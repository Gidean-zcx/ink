#!/usr/bin/env python3
"""Turn the bench's console into a report: what was measured, against what the simulator assumes, what it answers."""
import argparse, json, os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def scrub(text):
    """Nothing that could identify the device or a network gets into the public report."""
    text = re.sub(r"\b([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b", "(隐去)", text)
    return re.sub(r"\d{10,}", "(隐去)", text)


def parse(path):
    rows = []
    lines = open(path, errors="replace").readlines()
    # The bench says everything again at the end. That copy is whole; the first may have holes.
    start = max((n for n, line in enumerate(lines) if "BENCH_REPLAY" in line), default=-1) + 1
    for line in lines[start:]:
        i = line.find("BENCH {")
        if i < 0:
            continue
        try:
            rows.append(json.loads(line[i + 6:]))
        except ValueError:
            rows.append({"test": "bad_line"})
    return rows


def kib(n):
    return f"{n / 1024:.1f} KiB"


def compare(rows, prof):
    """(item, measured, simulator's assumption, verdict) for whatever was measured."""
    out = []
    def one(test, **kw):
        return next((r for r in rows if r.get("test") == test and all(r.get(k) == v for k, v in kw.items())), None)
    boot, end = one("heap", when="boot"), one("heap", when="end")
    mem = prof["memory"]
    if boot:
        assumed = (mem["internal_heap_kib"] + mem["dma_heap_kib"]) * 1024
        out.append(("开机后内部内存可用", kib(boot["internal_free"]), f"{kib(assumed)}（悲观值）",
                    "比模型多，模型可以调宽" if boot["internal_free"] > assumed else "**比模型少，预算要重算**"))
        out.append(("PSRAM 可用", kib(boot["psram_free"]), f"{mem['external_heap_kib']} KiB", "相符" if boot["psram_free"] > mem["external_heap_kib"] * 900 else "**偏少**"))
    panel = one("heap", when="panel_done") or end
    if panel:
        floor = 32768
        out.append(("屏幕开过之后（Wi-Fi 已关），内部内存历史最低", kib(panel["internal_min"]), f"底线 {kib(floor)}",
                    "够" if panel["internal_min"] >= floor else "**低于底线，RK-04 成立**"))
        on, off = one("heap", when="wifi_on"), one("heap", when="wifi_off")
        if on and off:
            both = panel["internal_free"] - (off["internal_free"] - on["internal_free"])
            out.append(("【估算】Wi-Fi 和屏幕同时开，内部内存可用（没有同时开过）", kib(both), f"底线 {kib(floor)}",
                        "够，但这个固件没有 LVGL、TLS 和系统任务，真实系统要少很多" if both >= floor else "**低于底线**"))
    f = one("flash")
    if f and "erase_4k_us_avg" in f:
        typ, top = prof["flash"]["erase_sector_us"]["typ"], prof["flash"]["erase_sector_us"]["max"]
        out.append(("Flash 擦一个写过的扇区", f"平均 {f['erase_4k_us_avg'] / 1000:.1f} ms，最长 {f['erase_4k_us_max'] / 1000:.1f} ms",
                    f"典型 {typ / 1000:.0f} ms，最长 {top / 1000:.0f} ms", "在模型内" if f["erase_4k_us_max"] <= top else "**比模型慢**"))
        mbps = 4096 / f["read_4k_us_avg"] if f.get("read_4k_us_avg") else 0
        out.append(("Flash 读", f"{mbps:.1f} MB/s", f"{prof['flash']['read_mbps']} MB/s", "在模型内" if mbps >= prof["flash"]["read_mbps"] else "**比模型慢**"))
    c = one("crypto")
    if c:
        out.append((f"PBKDF2 {c['pbkdf2_iterations']} 次（开一次密码）", f"{c['pbkdf2_ms']:.0f} ms", "约 100 ms（设计估算）",
                    "合适" if c["pbkdf2_ms"] < 300 else "**太慢，迭代次数要调**"))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("console")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--restored", default="未知")
    ap.add_argument("--profile", default=os.path.join(ROOT, "simulator", "profiles", "metalio-plus.json"))
    a = ap.parse_args()
    rows = parse(a.console)
    done = any("BENCH_DONE" in l for l in open(a.console, errors="replace"))
    prof = json.load(open(a.profile))
    w = []
    w.append("# 自动上机测量\n")
    w.append("由 `tools/hw-bench/bench.py` 生成，下面的数字都是【实测】，来自这一台设备的这一次运行。判断是工具按固定规则给的。\n")
    tests = sorted({r.get("test", "?") for r in rows})
    w.append(f"- 跑完：{'是' if done else '**没有，中途停了，下面只有停之前的结果**'}；结果 {len(rows)} 条，涉及 {len(tests)} 类。")
    w.append(f"- 设备已复原并和备份核对：{a.restored}。\n")
    w.append("## 和模拟器的假设对比\n")
    w.append("| 项目 | 实测 | 模拟器假设 | 结论 |\n| --- | --- | --- | --- |")
    for item in compare(rows, prof):
        w.append("| " + " | ".join(item) + " |")
    w.append("\n## 屏幕\n")
    w.append("| 动作 | 用时 |\n| --- | --- |")
    for r in rows:
        if r.get("test") == "panel":
            w.append(f"| {r['what']} | {r['ms']} ms{'（出错）' if r.get('err') else ''} |")
    init = next((r for r in rows if r.get("test") == "panel_init"), {})
    if init:
        w.append(f"\n初始化：{'成功' if init.get('ok') else '**失败**'}，占内部内存 {init.get('internal_cost', '?')} 字节。环境温度见下表。")
    w.append("\n## 其他结果\n")
    w.append("| 项目 | 结果 |\n| --- | --- |")
    for r in rows:
        t = r.get("test")
        if t in ("heap", "panel", "panel_init", "flash", "crypto", "bad_line"):
            continue
        body = ", ".join(f"{k}={v}" for k, v in r.items() if k != "test")
        w.append(f"| {t} | {scrub(body).replace('|', '/')} |")
    w.append("\n## 内部内存随时间\n")
    w.append("| 时点 | 可用 | 最大连续块 | 历史最低 | DMA 可用 | PSRAM 可用 |\n| --- | --- | --- | --- | --- | --- |")
    for r in rows:
        if r.get("test") == "heap":
            w.append(f"| {r['when']} | {kib(r['internal_free'])} | {kib(r['internal_largest'])} | {kib(r['internal_min'])} | {kib(r['dma_free'])} | {kib(r['psram_free'])} |")
    text = "\n".join(w) + "\n"
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, "w", encoding="utf-8").write(text)
    print(f"{a.out}: {len(rows)} results, finished={done}")


if __name__ == "__main__":
    main()
