#!/usr/bin/env python3
"""Summarize tools/iss/bench/run_pqbn.sh output: PQ-BN v4 cost per 12 ms phase on the ISS
(ESP32-S31 model) next to the research's RV32IM estimates, and whether one or two scan tasks can
produce the drive lines in time (line mode).

Scan check per phase (same rules as the device simulator's feeder planner): row y is needed at
scan_start + max(0, y - 16) * line_time (two 8-line bounce buffers are filled when the scan starts);
the scan starts once the first `prefill` rows are ready; the executor(s) produce rows in order
(two executors: the next free one takes the next row); core 0 loses 10% to the LCD refill interrupt
once the scan runs. Reports the smallest prefill (rows) that avoids a late row, per phase, worst
over the scenario. Assumes the planner for the phase has finished before the scan (it runs on the
other core while the previous phase scans), and an unbounded line buffer.
"""
import glob
import json
import os
import sys

MHZ = 320.0
LEAD = 16
ISR = 0.10
SCEN = ["static", "ai_stream", "page_turn", "scroll_half", "scroll_full", "scroll_full_14fps", "covers_scroll"]
LABEL = {"static": "静止页", "ai_stream": "AI 流式出字", "page_turn": "翻页", "scroll_half": "半屏列表滚动",
         "scroll_full": "全屏滚动 28 fps", "scroll_full_14fps": "全屏滚动 14 fps", "covers_scroll": "封面网格滚动"}
# research: rvbench4_auto8k.txt (drive-plane mode, budget 8000, auto motion), ms per phase avg / worst
RESEARCH_PLANE = {"static": (0.32, 0.37), "ai_stream": (1.15, 2.54), "page_turn": (6.38, 8.43), "scroll_half": (6.96, 14.22),
                  "scroll_full": (9.29, 21.93), "scroll_full_14fps": (7.77, 22.13), "covers_scroll": (9.47, 22.20)}
# research: rvbench4_linemode.txt (unlimited budget, no auto motion): worst drive line us
RESEARCH_LINE_WORST = {"static": 0.1, "ai_stream": 3.1, "page_turn": 16.2, "scroll_half": 11.6, "scroll_full": 19.6,
                       "scroll_full_14fps": 19.6, "covers_scroll": 19.6}


def parse(path):
    P, R, T = [], {}, {}
    with open(path) as f:
        for line in f:
            if line.startswith("P "):
                v = line.split()
                P.append({"ph": int(v[1]), "cyc": int(v[2]), "planned": int(v[3]), "words": int(v[4]),
                          "active": int(v[5]), "rsum": int(v[6]), "rmax": int(v[7]), "motion": int(v[8]),
                          "backlog": int(v[9]), "dirty": int(v[10]) if len(v) > 10 else 0,
                          "mdirty": int(v[11]) if len(v) > 11 else 0, "motion0": int(v[12]) if len(v) > 12 else 0})
            elif line.startswith("R "):
                v = line.split()
                R[int(v[1])] = [int(x) for x in v[2:]]
            elif line.startswith("T "):
                v = line.split()
                T[int(v[1])] = [int(x) for x in v[2:]]
    return P, R, T


def late(costs_us, prefill, line_us, executors):
    """True if some row is not ready when the scan needs it."""
    n = len(costs_us)
    ready = [0.0] * n
    free = [0.0] * executors
    scan = None
    for y in range(n):
        f = min(range(executors), key=lambda i: free[i])
        speed = (1.0 - ISR) if (f == 0 and scan is not None and free[f] >= scan) else 1.0
        free[f] += costs_us[y] / speed
        ready[y] = free[f]
        if scan is None and y == prefill - 1:
            scan = max(ready[: prefill])
    if scan is None:
        scan = max(ready)
    for y in range(n):
        if ready[y] > scan + max(0, y - LEAD) * line_us + 1e-9:
            return True
    return False


def min_prefill(costs_us, line_us, executors):
    lo, hi = 2 * 8, len(costs_us)
    if not late(costs_us, lo, line_us, executors):
        return lo
    if late(costs_us, hi, line_us, executors):
        return None
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if late(costs_us, mid, line_us, executors):
            lo = mid
        else:
            hi = mid
    return hi


def main(out):
    rows = []
    for s in SCEN:
        rec = {"scen": s}
        for corner in ("fitted", "slow"):
            P, R, _ = parse(os.path.join(out, "%s.line.%s.txt" % (s, corner)))
            Pp, _, _ = parse(os.path.join(out, "%s.plane.%s.txt" % (s, corner)))
            n = len(P)
            plan = [p["cyc"] / MHZ / 1000 for p in P]                    # ms
            rsum = [p["rsum"] / MHZ / 1000 for p in P]                   # ms
            rmax = max(p["rmax"] for p in P) / MHZ                       # us
            plane = [p["cyc"] / MHZ / 1000 for p in Pp]
            need = {}
            for line_us in (16.0, 17.0):
                for ex in (1, 2):
                    worst = 16
                    for ph, c in R.items():
                        costs = [x / MHZ for x in c]
                        if max(costs) < 1.0:
                            continue
                        m = min_prefill(costs, line_us, ex)
                        worst = 10 ** 9 if m is None else max(worst, m)
                    need[(line_us, ex)] = worst
            rec[corner] = {"plan_avg": sum(plan) / n, "plan_max": max(plan), "rows_avg": sum(rsum) / n,
                           "rows_max": max(rsum), "row_max_us": rmax, "plane_avg": sum(plane) / len(plane),
                           "plane_max": max(plane), "need": need,
                           "total_avg": (sum(plan) + sum(rsum)) / n, "total_max": max(a + b for a, b in zip(plan, rsum)),
                           "motion_max": max(p["motion"] for p in P)}
            rec[corner]["need"] = {"%gus_x%d" % k: v for k, v in need.items()}
        rows.append(rec)

    w = print
    w("# PQ-BN v4 在本仓库 ISS（ESP32-S31 模型）上的重测\n")
    w("条件：研究的全分辨率场景（1216×684，每 3 相一帧 UI），前 60 相；plan_budget 8000、auto_motion、"
      "每像素状态与帧缓冲在 PSRAM（经 D-cache），表与 tile 状态在内部 SRAM，代码在 flash；ESP-IDF S31 编译选项 -O2。"
      "拟合 = 本仓库校准的时序（CoreMark 3.08/MHz、PSRAM 按 P4 实测拟合），慢角 = 偏慢核参数 + 按 S3 实测拟合的 PSRAM。\n")
    w("## 1. 每相耗时（ms，单核 320 MHz；括号内为最坏一相）\n")
    w("| 场景 | 研究（驱动平面，简单顺序核估算） | 本 ISS 驱动平面·拟合 | 本 ISS 驱动平面·慢角 | 逐行：规划·拟合 | 逐行：生成驱动线·拟合 | 逐行：合计·慢角 |")
    w("| --- | --- | --- | --- | --- | --- | --- |")
    for r in rows:
        f, sl = r["fitted"], r["slow"]
        ra, rw = RESEARCH_PLANE[r["scen"]]
        w("| %s | %.2f (%.2f) | %.2f (%.2f) | %.2f (%.2f) | %.2f (%.2f) | %.2f (%.2f) | %.2f (%.2f) |" % (
            LABEL[r["scen"]], ra, rw, f["plane_avg"], f["plane_max"], sl["plane_avg"], sl["plane_max"],
            f["plan_avg"], f["plan_max"], f["rows_avg"], f["rows_max"], sl["total_avg"], sl["total_max"]))
    w("\n## 2. 逐行生成驱动线：最慢一行与所需预填\n")
    w("行时间 16 µs（研究）/ 17 µs（本仓库档案）；“所需预填”= 不出现迟到行的最小预填行数（各相取最坏）；"
      "单任务在 0 核（扣 10% 补缓冲中断），双任务谁空谁取。>684 表示整相都跟不上。\n")
    w("| 场景 | 最慢一行 µs：研究 / 拟合 / 慢角 | 单任务 16 µs | 单任务 17 µs | 双任务 17 µs | 慢角：单任务 17 µs | 慢角：双任务 17 µs |")
    w("| --- | --- | --- | --- | --- | --- | --- |")
    fmt = lambda v: ">684" if v >= 10 ** 9 else str(v)
    for r in rows:
        f, sl = r["fitted"], r["slow"]
        w("| %s | %.1f / %.1f / %.1f | %s | %s | %s | %s | %s |" % (
            LABEL[r["scen"]], RESEARCH_LINE_WORST[r["scen"]], f["row_max_us"], sl["row_max_us"],
            fmt(f["need"]["16us_x1"]), fmt(f["need"]["17us_x1"]), fmt(f["need"]["17us_x2"]),
            fmt(sl["need"]["17us_x1"]), fmt(sl["need"]["17us_x2"])))
    models = fit_models(out)
    w("\n## 3. 设备模拟器用的成本模型（最小二乘拟合，周期）\n")
    w("逐行：空闲带的行（整带没有在推的 tile）r_idle；有推送的带 r_band + r_tile × 本相在推的 tile 数。"
      "规划：p_base + p_px × 规划访问的像素 + p_mot × 收到 UI 改动的运动 tile + p_enter/p_exit × 进出运动模式的 tile + p_dirty × UI 标脏的 tile。\n")
    w("| 角点 | r_idle | r_band | r_tile | 逐行 R² | p_base | p_px | p_mot | p_enter | p_exit | p_dirty | 规划 R² |")
    w("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for corner, m in models.items():
        w("| %s | %.0f | %.0f | %.1f | %.4f | %.0f | %.1f | %.0f | %.0f | %.0f | %.1f | %.3f |" % (
            corner, m["r_idle"], m["r_band"], m["r_tile"], m["row_r2"], m["p_base"], m["p_px"], m["p_mot"],
            m["p_enter"], m["p_exit"], m["p_dirty"], m["plan_r2"]))
    json.dump({"scenarios": rows, "models": models}, open(os.path.join(out, "summary.json"), "w"), indent=1)


def fit_models(out):
    import numpy as np
    models = {}
    for corner in ("fitted", "slow"):
        X, Y, PX, PY = [], [], [], []
        for s in SCEN:
            P, R, T = parse(os.path.join(out, "%s.line.%s.txt" % (s, corner)))
            for p in P:
                ph = p["ph"]
                if ph == 1:
                    continue  # cold caches and the first band scan
                for y, c in enumerate(R[ph]):
                    n = T[ph][y // 4]
                    X.append((1.0 if n < 0 else 0.0, 1.0 if n >= 0 else 0.0, max(n, 0)))
                    Y.append(c)
                d = p["motion"] - p["motion0"]
                PX.append((1.0, p["planned"], p["mdirty"], max(d, 0), max(-d, 0), p["dirty"]))
                PY.append(p["cyc"])
        X, Y, PX, PY = map(np.array, (X, Y, PX, PY))
        co = np.linalg.lstsq(X, Y, rcond=None)[0]
        r2 = 1 - ((Y - X @ co) ** 2).sum() / ((Y - Y.mean()) ** 2).sum()
        # non-negative coefficients (a negative cost per event would under-predict in rare phases):
        # drop features that come out negative and refit
        keep = list(range(PX.shape[1]))
        while True:
            sub = np.linalg.lstsq(PX[:, keep], PY, rcond=None)[0]
            neg = [k for k, v in zip(keep, sub) if v < 0 and k != 0]
            if not neg:
                break
            keep.remove(neg[0])
        pc = np.zeros(PX.shape[1])
        pc[keep] = sub
        r2p = 1 - ((PY - PX @ pc) ** 2).sum() / ((PY - PY.mean()) ** 2).sum()
        models[corner] = {"r_idle": co[0], "r_band": co[1], "r_tile": co[2], "row_r2": r2, "p_base": pc[0], "p_px": pc[1],
                          "p_mot": pc[2], "p_enter": pc[3], "p_exit": pc[4], "p_dirty": pc[5], "plan_r2": r2p}
    return models


if __name__ == "__main__":
    main(sys.argv[1])
