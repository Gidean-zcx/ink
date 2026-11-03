#!/usr/bin/env python3
"""Fit the iss timing model and turn kernel measurements into cost classes.

Called by tools/iss/calibrate.sh once everything is built:

  calib.py --iss build/iss --elf-dir build/elf --base timing/esp32s31.cfg \
           --profile ../../simulator/profiles/metalio-plus.json --runs build/runs --out out

Steps
  1. CoreMark fit: evaluate a box of physically plausible 5-stage in-order
     parameters, pick the point closest to the anchor (ties -> slower).
  2. PSRAM fit: choose the line-fill time so that the iss memcpy rates are no
     faster than measured ESP32-P4 rates (P4-parity); raise the write-back
     time if needed. The same procedure against ESP32-S3 rates gives the slow
     corner.
  3. Run the kernels for the fitted model (-O2, -Os, Zb/Zc builds) and for the
     ideal and slow corners.
  4. Write out/esp32s31.fitted.cfg, out/costs.json and out/report.md.
Output is deterministic: no timestamps, no absolute paths.
"""
import argparse
import itertools
import json
import math
import os
import subprocess
import sys

ANCHOR = 3.43  # CoreMark/MHz per core (Espressif material, unverified on our board)
MARGIN = 1.3   # "until calibrated" CPU factor (performance-modeling.md section 5)
MHZ = 320.0

# Physically plausible box for a 5-stage single-issue in-order RV32 core.
BOX = [
    ("lat.load_sub", [2, 3]),
    ("lat.mul", [1, 2]),
    ("br.mispredict", [2, 3]),
    ("br.jal_nobtb", [1, 3]),
]
SLOW_CORE = {"lat.load_sub": 3, "lat.mul": 2, "br.mispredict": 3, "br.jal_nobtb": 3, "dcache.hit_extra": 1,
             "mem.sram_extra": 1}
IDEAL_CORE = {"lat.load_word": 1, "lat.load_sub": 1, "lat.mul": 1, "br.mispredict": 0, "br.jal_nobtb": 0,
              "br.taken_correct": 0}

MIB = 1.048576  # MiB/s -> MB/s
# ESP32-P4, hex PSRAM 200 MHz, standard memcpy, 8-16 MiB buffers (developer-portal discussion #353)
P4_RATES = {"psram_to_internal.copy_byte": 105.15 * MIB, "internal_to_psram.copy_byte": 84.86 * MIB,
            "psram.copy_byte": 50.96 * MIB}
# ESP32-S3, octal PSRAM, memcpy (project-x51/esp32-s3-memorycopy, as quoted in performance-modeling.md)
S3_RATES = {"psram_to_internal.copy_byte": 56.8, "internal_to_psram.copy_byte": 32.5, "psram.copy_byte": 21.2}

# cost class -> (regions, unit, how) ; how: "max" of cycles/unit over the regions, "aux" = cycles/aux unit
CLASSES = [
    ("sram.copy_byte", ["sram.copy_byte"], "byte", "max", "memcpy SRAM->SRAM (newlib)"),
    ("psram_to_internal.copy_byte", ["psram_to_internal.copy_byte"], "byte", "max",
     "memcpy PSRAM->SRAM, 256 KiB stream, cold D-cache"),
    ("internal_to_psram.copy_byte", ["internal_to_psram.copy_byte"], "byte", "max",
     "memcpy SRAM->PSRAM, write-allocate + write-back charged"),
    ("psram.copy_byte", ["psram.copy_byte"], "byte", "max", "memcpy PSRAM->PSRAM, 256 KiB"),
    ("glyph.blit", ["glyph.blit@psram"], "glyph", "max",
     "24 px A8 glyph blended into the PSRAM gray8 framebuffer (full page, glyph cache in PSRAM)"),
    ("epd.convert_px", ["epd.convert_px@sram"], "px", "max",
     "8bpp->4bpp pack, compute only (hal_display_sim charges the PSRAM traffic separately)"),
    ("text.break_char", ["text.break_char"], "char", "max", "UTF-8 decode + width + greedy CJK line breaking"),
    ("text.layout_char", ["text.layout_char"], "char", "max",
     "break + glyph-cache hash lookup + positioned draw list (no rasterization)"),
    ("json.byte", ["json.byte"], "byte", "max", "cJSON parse + delete of a 14 KB response, nodes in PSRAM"),
    ("zlib.inflate_byte", ["zlib.inflate_byte"], "out byte", "max", "miniz tinfl, zlib stream, PSRAM in/out"),
    ("crc32.byte", ["crc32.byte@sram", "crc32.byte@psram"], "byte", "max", "byte-wise table CRC-32, max(SRAM, PSRAM data)"),
    ("heap.op", ["tlsf.op@sram", "tlsf.op@psram"], "op", "max",
     "raw TLSF malloc or free (max of SRAM/PSRAM pools); ESP-IDF heap_caps/multi_heap locking NOT included"),
]
NEW_CLASSES = [
    ("glyph.px", ["glyph.blit@psram"], "px", "aux", "same run as glyph.blit, per glyph-cell pixel"),
    ("fill.px", ["fill.px@psram"], "px", "max", "gray8 rectangle fill (memset per row) into the PSRAM framebuffer"),
    ("psram.set_byte", ["psram.set_byte"], "byte", "max", "memset to PSRAM, write-backs charged"),
    ("sram.set_byte", ["sram.set_byte"], "byte", "max", "memset to internal SRAM"),
]
NOT_MEASURED = {
    "lvgl.px": "not measured (LVGL itself is not run); proxies: fill.px, glyph.px",
    "littlefs.op": "not measured (dominated by flash timing, modeled in hal_storage_sim)",
    "icache.miss": "model input, not a measurement: flash.fill (DIO 80 MHz)",
}


def fmt(v):
    if isinstance(v, float) and not v.is_integer():
        return ("%.6f" % v).rstrip("0").rstrip(".")
    return str(int(v))


class Iss:
    def __init__(self, args):
        self.iss, self.base, self.runs = args.iss, args.base, args.runs
        os.makedirs(self.runs, exist_ok=True)

    def run(self, elf, sets, tag):
        out = os.path.join(self.runs, tag + ".json")
        cmd = [self.iss, "-q", "-c", self.base]
        for k in sorted(sets):
            cmd += ["-s", "%s=%s" % (k, fmt(sets[k]))]
        cmd += ["-o", out, elf]
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        if p.returncode != 0 or not os.path.exists(out):
            sys.exit("calib: iss failed (%s): %s" % (tag, p.stderr.strip()))
        with open(out) as f:
            d = json.load(f)
        if d["error"] or d["exit_code"] != 0:
            sys.exit("calib: %s: %s" % (tag, d["error"] or "exit %d" % d["exit_code"]))
        d["by_name"] = {r["name"]: r for r in d["regions"]}
        return d

    def dump_config(self, sets):
        cmd = [self.iss, "-c", self.base]
        for k in sorted(sets):
            cmd += ["-s", "%s=%s" % (k, fmt(sets[k]))]
        cmd += ["--dump-config"]
        return subprocess.run(cmd, stdout=subprocess.PIPE, check=True, universal_newlines=True).stdout


def coremark_per_mhz(d):
    r = d["by_name"]["coremark"]
    return r["units"] * 1e6 / r["cycles"], r


def fit_core(iss, cm_elf):
    rows = []
    keys = [k for k, _ in BOX]
    for combo in itertools.product(*[v for _, v in BOX]):
        sets = dict(zip(keys, combo))
        d = iss.run(cm_elf, sets, "coremark-box-" + "-".join(str(c) for c in combo))
        cm, r = coremark_per_mhz(d)
        rows.append({"params": sets, "coremark_per_mhz": cm, "cycles_per_iter": r["cycles"] / r["units"],
                     "cpi": r["stats"]["cpi"], "instr_per_iter": r["stats"]["instret"] / r["units"]})
    # closest to the anchor; on a tie prefer the slower point
    rows.sort(key=lambda x: (round(abs(x["coremark_per_mhz"] - ANCHOR), 9), -x["cycles_per_iter"]))
    return rows[0], rows


def psram_fit(iss, elf, core, rates, tag):
    d = iss.run(elf, core, tag + "-probe")
    f0, w0 = d["config"]["psram.fill"], d["config"]["psram.writeback"]

    def parts(name):
        r = d["by_name"][name]
        s = r["stats"]["dcache"]["psram"]
        m, wb = s["miss"], s["writeback"]
        return r["units"], r["cycles"] - f0 * m - w0 * wb, m, wb

    u, c0, m, wb = parts("psram_to_internal.copy_byte")
    target = u * MHZ / rates["psram_to_internal.copy_byte"]
    fill = max(f0, (target - c0 - w0 * wb) / m)
    fill = math.ceil(fill * 10) / 10  # round up: never faster than the target
    wback = w0
    for name in ("internal_to_psram.copy_byte", "psram.copy_byte"):
        u, c0, m, wb = parts(name)
        target = u * MHZ / rates[name]
        if c0 + fill * m + wback * wb < target:
            wback = math.ceil((target - c0 - fill * m) / wb * 10) / 10
    return fill, wback


def mbps(cyc_per_byte):
    return MHZ / cyc_per_byte if cyc_per_byte else 0.0


def class_value(d, regions, how):
    vals = []
    for name in regions:
        r = d["by_name"].get(name)
        if r is None:
            return None
        if how == "aux":
            vals.append(r["cycles"] / r["aux_units"])
        else:
            vals.append(r["cycles"] / r["units"])
    return max(vals)


def nice(v):
    """Round a proposed profile value: integers above 100, one decimal above 10, else two."""
    if v is None:
        return None
    if v >= 100:
        return int(math.ceil(v))
    if v >= 10:
        return math.ceil(v * 10) / 10
    return math.ceil(v * 100) / 100


def r3(v):
    return None if v is None else round(v, 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iss", required=True)
    ap.add_argument("--elf-dir", required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--runs", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--meta", required=True, help="JSON with toolchain/source provenance")
    args = ap.parse_args()
    iss = Iss(args)
    E = lambda n: os.path.join(args.elf_dir, n)
    with open(args.meta) as f:
        meta = json.load(f)

    # 1. CoreMark fit
    best, grid = fit_core(iss, E("coremark-idf.elf"))
    core = best["params"]
    cm_points = {}
    for name, elf in (("idf", "coremark-idf.elf"), ("zbzc", "coremark-zbzc.elf")):
        for corner, sets in (("fitted", core), ("ideal", IDEAL_CORE), ("slow", SLOW_CORE)):
            cm, r = coremark_per_mhz(iss.run(E(elf), sets, "coremark-%s-%s" % (name, corner)))
            cm_points["%s/%s" % (name, corner)] = {
                "coremark_per_mhz": round(cm, 4), "cpi": round(r["stats"]["cpi"], 4),
                "instr_per_iter": round(r["stats"]["instret"] / r["units"], 1),
                "residual": round(cm / ANCHOR - 1, 4)}

    # 2. PSRAM fits (fitted core vs ESP32-P4 rates; slow core vs ESP32-S3 rates)
    k_o2 = E("kernels-O2.elf")
    nofetch = {"icache.enable": 0}  # same conditions as the warm kernel runs below
    fill_p4, wb_p4 = psram_fit(iss, k_o2, dict(core, **nofetch), P4_RATES, "psram-fit-p4")
    slow_core = dict(SLOW_CORE)
    fill_s3, wb_s3 = psram_fit(iss, k_o2, dict(slow_core, **nofetch), S3_RATES, "psram-fit-s3")
    fitted = dict(core, **{"psram.fill": fill_p4, "psram.writeback": wb_p4})
    slow = dict(slow_core, **{"psram.fill": fill_s3, "psram.writeback": wb_s3})
    ideal = dict(IDEAL_CORE)

    with open(os.path.join(args.out, "esp32s31.fitted.cfg"), "w") as f:
        f.write("# Generated by tools/iss/calibrate.sh (scripts/calib.py) from timing/esp32s31.cfg.\n")
        f.write("# Fitted: %s; PSRAM fill/write-back from ESP32-P4 memcpy parity.\n"
                % ", ".join("%s=%s" % (k, fmt(v)) for k, v in sorted(core.items())))
        f.write(iss.dump_config(fitted))

    # 3. kernel runs. Warm (steady-state) costs use an ideal I-cache so that they
    # do not depend on how this small test binary happens to be laid out (a hot
    # loop that fits the 16 KiB I-cache can still thrash a 2-way set by
    # coincidence); flash fetch is measured by the "#cold" regions of the run
    # with the real I-cache, and charged in the system model via icache.miss.
    warm = {"icache.enable": 0}
    runs = {
        "fitted": iss.run(k_o2, dict(fitted, **warm), "kernels-O2-fitted"),
        "fitted_ic": iss.run(k_o2, fitted, "kernels-O2-fitted-icache"),
        "Os": iss.run(E("kernels-Os.elf"), dict(fitted, **warm), "kernels-Os-fitted"),
        "zbzc": iss.run(E("kernels-O2-zbzc.elf"), dict(fitted, **warm), "kernels-O2-zbzc-fitted"),
        "ideal": iss.run(k_o2, dict(ideal, **warm), "kernels-O2-ideal"),
        "slow": iss.run(k_o2, dict(slow, **warm), "kernels-O2-slow"),
    }
    fitted_cm = cm_points["idf/fitted"]["coremark_per_mhz"]
    copy_rates = {}
    for corner in ("fitted", "slow", "ideal"):
        copy_rates[corner] = {n: round(mbps(class_value(runs[corner], [n], "max")), 1)
                              for n in P4_RATES}

    with open(args.profile) as f:
        guesses = json.load(f).get("costs_cycles", {})

    def entry(cls, regions, unit, how, note):
        raw = class_value(runs["fitted"], regions, how)
        slow_v = class_value(runs["slow"], regions, how)
        e = {"unit": unit, "regions": regions, "note": note,
             "raw": r3(raw), "with_margin": r3(raw * MARGIN),
             "proposed": nice(raw * MARGIN),
             "pessimistic": nice(max(raw * MARGIN, slow_v)),  # also covers the slow corner
             "ideal": r3(class_value(runs["ideal"], regions, how)),
             "slow": r3(slow_v),
             "Os": r3(class_value(runs["Os"], regions, how)),
             "zbzc": r3(class_value(runs["zbzc"], regions, how))}
        if cls in guesses:
            e["current_guess"] = guesses[cls]
            e["proposed_vs_guess"] = round(e["proposed"] / guesses[cls], 3) if guesses[cls] else None
        return e

    costs = {c[0]: entry(*c) for c in CLASSES}
    new = {c[0]: entry(*c) for c in NEW_CLASSES}
    regions = {}
    for name in runs["fitted"]["by_name"]:
        cold = name.endswith("#cold")  # cold regions only make sense with the real I-cache
        r = runs["fitted_ic" if cold else "fitted"]["by_name"][name]
        ric = runs["fitted_ic"]["by_name"][name]
        s = r["stats"]
        regions[name] = {
            "units": r["units"], "aux_units": r["aux_units"], "cycles_per_unit": r3(r["cycles"] / r["units"]),
            "instr_per_unit": r3(s["instret"] / r["units"]), "cpi": round(s["cpi"], 3),
            "psram_miss_per_unit": r3(s["dcache"]["psram"]["miss"] / r["units"]),
            "psram_wb_per_unit": r3(s["dcache"]["psram"]["writeback"] / r["units"]),
            "with_icache": r3(ric["cycles"] / ric["units"]),
            "flash_icache_miss": ric["stats"]["icache"]["flash"]["miss"],
            "slow": None if cold else r3(runs["slow"]["by_name"][name]["cycles"] / r["units"]),
            "ideal": None if cold else r3(runs["ideal"]["by_name"][name]["cycles"] / r["units"]),
        }

    doc = {
        "_comment": "Generated by tools/iss/calibrate.sh. Cycles per unit at 320 MHz on the ESP32-S31 iss model. "
                    "'raw' = fitted model, kernels -O2, hot code (ideal I-cache; flash fetch is charged separately "
                    "via icache.miss); 'with_margin' = raw x 1.3 (performance-modeling.md: CPU x1.3 until calibrated "
                    "on hardware); 'proposed' = with_margin rounded up for the profile. 'ideal'/'slow' = bounding "
                    "corners; 'Os'/'zbzc' = build sensitivities. Not applied to simulator/profiles/.",
        "anchor": {"coremark_per_mhz_per_core": ANCHOR,
                   "source": "Espressif material (ESP32-S31 datasheet v0.5: CoreMark 2195.20 dual-core @320 MHz = "
                             "6.86/MHz, assumed linear per core), unverified on our board"},
        "fit": {"params": core, "coremark_per_mhz": fitted_cm, "residual": cm_points["idf/fitted"]["residual"],
                "box": {k: v for k, v in BOX}, "coremark": cm_points,
                "psram": {"fitted": {"fill": fill_p4, "writeback": wb_p4, "target_MBps": {k: round(v, 1) for k, v in P4_RATES.items()}},
                          "slow": {"fill": fill_s3, "writeback": wb_s3, "target_MBps": S3_RATES},
                          "iss_MBps": copy_rates}},
        "margin": MARGIN,
        "costs_cycles": costs,
        "new_classes": new,
        "not_measured": NOT_MEASURED,
        "proposed_costs_cycles": dict(sorted({**{k: v["proposed"] for k, v in costs.items()},
                                              **{k: v["proposed"] for k, v in new.items()}}.items())),
        "proposed_costs_cycles_pessimistic": dict(sorted({**{k: v["pessimistic"] for k, v in costs.items()},
                                                          **{k: v["pessimistic"] for k, v in new.items()}}.items())),
        "regions": dict(sorted(regions.items())),
        "provenance": meta,
    }
    with open(os.path.join(args.out, "costs.json"), "w") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False, sort_keys=False)
        f.write("\n")
    write_report(os.path.join(args.out, "report.md"), doc, grid, meta)
    print("calib: CoreMark/MHz fitted %.3f (residual %+.1f%%), PSRAM fill %.1f / write-back %.1f cycles"
          % (fitted_cm, 100 * cm_points["idf/fitted"]["residual"], fill_p4, wb_p4))


def write_report(path, doc, grid, meta):
    L = []
    w = L.append
    fit = doc["fit"]
    w("# ISS 校准报告（自动生成）")
    w("")
    w("由 `tools/iss/calibrate.sh` 生成，请勿手改；方法与解读见 `docs/research/simulator/iss-calibration.md`。")
    w("单位：ESP32-S31 内核周期（320 MHz）。raw = 拟合模型、内核 -O2、热代码（理想 I-cache，flash 取指另由 "
      "icache.miss 计）；×1.3 = 校准前的悲观系数；建议值 = ×1.3 后向上取整。")
    w("")
    w("## 1. CoreMark 拟合")
    w("")
    w("锚点 %.2f CoreMark/MHz（%s）。" % (doc["anchor"]["coremark_per_mhz_per_core"], doc["anchor"]["source"]))
    w("")
    w("可信参数盒内的全部组合（CoreMark 用乐鑫组件的编译选项 -O3 -fjump-tables -ftree-switch-conversion，"
      "代码与数据放内部 SRAM）：")
    w("")
    w("| lat.load_sub | lat.mul | br.mispredict | br.jal_nobtb | CoreMark/MHz | 残差 | CPI | 指令/迭代 |")
    w("|---|---|---|---|---|---|---|---|")
    for g in grid:
        p = g["params"]
        w("| %s | %s | %s | %s | %.3f | %+.1f%% | %.3f | %.0f |" % (
            p["lat.load_sub"], p["lat.mul"], p["br.mispredict"], p["br.jal_nobtb"], g["coremark_per_mhz"],
            100 * (g["coremark_per_mhz"] / ANCHOR - 1), g["cpi"], g["instr_per_iter"]))
    w("")
    w("选中：%s → **%.3f CoreMark/MHz，残差 %+.1f%%**（偏慢一侧）。" % (
        ", ".join("%s=%s" % (k, fmt(v)) for k, v in sorted(fit["params"].items())), fit["coremark_per_mhz"],
        100 * fit["residual"]))
    w("")
    w("| 构建 / 角点 | CoreMark/MHz | 残差 | CPI | 指令/迭代 |")
    w("|---|---|---|---|---|")
    for k, v in fit["coremark"].items():
        w("| %s | %.3f | %+.1f%% | %.3f | %.0f |" % (k, v["coremark_per_mhz"], 100 * v["residual"], v["cpi"],
                                                   v["instr_per_iter"]))
    w("")
    w("idf = IDF 对 S31 的 -march（rv32imafc + zaamo/zalrsc，去掉 GCC 不会生成的 xesploop/xespv）；"
      "zbzc = 另加数据手册列出的 Zba/Zbb/Zbs/Zcb/Zcmp。ideal = 理想单发射流水线（无任何停顿）；"
      "slow = SiFive U54 类比参数 + 内部 SRAM 访问与 D-cache 命中各多 1 周期。")
    w("")
    w("## 2. PSRAM 拟合")
    w("")
    ps = fit["psram"]
    w("| 角点 | 行填充（周期） | 写回（周期） | 目标 P→S / S→P / P→P（MB/s） | ISS 结果 P→S / S→P / P→P（MB/s） |")
    w("|---|---|---|---|---|")
    order = ("psram_to_internal.copy_byte", "internal_to_psram.copy_byte", "psram.copy_byte")
    for corner, label in (("fitted", "拟合（ESP32-P4 同等）"), ("slow", "慢角（ESP32-S3 同等）")):
        t = ps[corner]["target_MBps"]
        i = ps["iss_MBps"][corner]
        w("| %s | %.1f | %.1f | %s | %s |" % (label, ps[corner]["fill"], ps[corner]["writeback"],
                                          " / ".join("%.1f" % t[o] for o in order),
                                          " / ".join("%.1f" % i[o] for o in order)))
    i = ps["iss_MBps"]["ideal"]
    w("| 理论（IDF 时序，ideal 角点用） | 96.0 | 85.0 | — | %s |" % " / ".join("%.1f" % i[o] for o in order))
    w("")
    w("## 3. 成本类（profile 现有键）")
    w("")
    w("建议值 = raw×1.3 向上取整（预期情形）；悲观值 = max(raw×1.3, slow) 向上取整（同时覆盖慢角）。")
    w("")
    w("| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | 现猜测 | 建议/猜测 | ideal | slow | -Os | Zb/Zc |")
    w("|---|---|---|---|---|---|---|---|---|---|---|---|")

    def row(k, e):
        g = e.get("current_guess")
        return "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            k, e["unit"], fnum(e["raw"]), fnum(e["with_margin"]), fnum(e["proposed"]), fnum(e["pessimistic"]),
            fnum(g), ("%.2f" % e["proposed_vs_guess"]) if e.get("proposed_vs_guess") else "—", fnum(e["ideal"]),
            fnum(e["slow"]), fnum(e["Os"]), fnum(e["zbzc"]))

    for k, e in doc["costs_cycles"].items():
        w(row(k, e))
    for k, v in doc["not_measured"].items():
        w("| %s | — | — | — | — | — | — | — | — | — | — | — |" % k)
    w("")
    for k, v in doc["not_measured"].items():
        w("- `%s`：%s" % (k, v))
    w("")
    w("## 4. 建议新增的成本类")
    w("")
    w("| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | ideal | slow | -Os | Zb/Zc |")
    w("|---|---|---|---|---|---|---|---|---|---|")
    for k, e in doc["new_classes"].items():
        w("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            k, e["unit"], fnum(e["raw"]), fnum(e["with_margin"]), fnum(e["proposed"]), fnum(e["pessimistic"]),
            fnum(e["ideal"]), fnum(e["slow"]), fnum(e["Os"]), fnum(e["zbzc"])))
    w("")
    w("## 5. 全部测量区段（拟合模型，-O2）")
    w("")
    w("| 区段 | 单位数 | 周期/单位 | 指令/单位 | CPI | PSRAM 缺失/单位 | PSRAM 写回/单位 | 真实 I-cache 时 | flash I-miss | ideal | slow |")
    w("|---|---|---|---|---|---|---|---|---|---|---|")
    for k, r in doc["regions"].items():
        w("| %s | %d | %s | %s | %.2f | %s | %s | %s | %d | %s | %s |" % (
            k, r["units"], fnum(r["cycles_per_unit"]), fnum(r["instr_per_unit"]), r["cpi"],
            fnum(r["psram_miss_per_unit"]), fnum(r["psram_wb_per_unit"]), fnum(r["with_icache"]),
            r["flash_icache_miss"], fnum(r["ideal"]), fnum(r["slow"])))
    w("")
    w("稳态区段按理想 I-cache 计（热代码，结果不受本测试程序偶然的代码布局影响）；“真实 I-cache 时”一列是同一区段在"
      "16 KiB 两路 I-cache + DIO 80 MHz flash 下的结果，flash I-miss 为该次运行的缺失次数。`#cold` 区段从空 cache "
      "开始（代码经 flash XIP 取指），只作参考。")
    w("")
    w("## 6. 运行信息")
    w("")
    for k, v in meta.items():
        w("- %s: `%s`" % (k, v))
    w("")
    with open(path, "w") as f:
        f.write("\n".join(L))


def fnum(v):
    if v is None:
        return "—"
    if isinstance(v, int):
        return str(v)
    if abs(v) >= 1000:
        return "%.0f" % v
    if abs(v) >= 100:
        return "%.1f" % v
    if abs(v) >= 10:
        return "%.2f" % v
    return "%.3f" % v


if __name__ == "__main__":
    main()
