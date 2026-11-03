#!/usr/bin/env python3
"""Extract compact evidence from completed local runs; does not run product tests."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
HEAD = "0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3"
BEFORE = "89b112620730902f0fa1594e83ffb146b08749be"


def read(path):
    return json.loads(path.read_text())


def write(name, data):
    (HERE / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n")


def relative(path):
    return str(path.relative_to(ROOT))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None


def metric_evidence(directory, full=False):
    path = directory / "metrics.json"
    data = read(path)
    metrics = data["metrics"]
    prefixes = ("audio.", "ai.", "input.", "reader.", "ui.action_to_visible_ms", "power.unlock", "power.lock_to",
                "display.power", "display.pmic.", "display.redraw", "display.updates.",
                "display.update_ms.", "display.busy", "display.pqbn.cleans", "test.",
                "board.shared_", "storage.forced", "hub.requests.")
    exact = {"power.sleeps", "power.sleep_refused", "power.sleep_deferred", "power.off",
             "mem.internal.failed_allocs", "mem.external.failed_allocs", "mem.dma.failed_allocs"}
    selected = metrics if full else {k: v for k, v in metrics.items()
                                     if k.startswith(prefixes) or k in exact}
    return {"metrics_file": relative(path), "status": data.get("status"),
            "virtual_time_s": data.get("virtual_time_s"), "metrics": selected,
            "warnings": data.get("warnings"), "budget_violations": data.get("budget_violations"),
            "final_png_sha256": digest(directory / "final.png")}


results = read(HERE / "results.json")
rows = []
for row in results:
    out = Path(row["command"][row["command"].index("--out") + 1])
    rows.append({k: row[k] for k in ("name", "sanitizer", "pqbn", "before", "exit_code")})
    rows[-1].update(metric_evidence(out))
write("metrics-evidence.json", {"head": HEAD, "before": BEFORE, "runs": rows})


def compare(label, old_dir, new_dir, old_output=None, new_output=None):
    old, new = metric_evidence(old_dir, True), metric_evidence(new_dir, True)
    keys = old["metrics"].keys() | new["metrics"].keys()
    diff = {k: {"before": old["metrics"].get(k), "after": new["metrics"].get(k)}
            for k in sorted(keys) if old["metrics"].get(k) != new["metrics"].get(k)}
    result = {"name": label, "before_file": old["metrics_file"], "after_file": new["metrics_file"],
              "all_metric_differences": diff, "warnings_before": old["warnings"],
              "warnings_after": new["warnings"], "budget_violations_before": old["budget_violations"],
              "budget_violations_after": new["budget_violations"],
              "final_png_before_sha256": old["final_png_sha256"],
              "final_png_after_sha256": new["final_png_sha256"],
              "final_png_byte_equal": old["final_png_sha256"] == new["final_png_sha256"]}
    if old_output is not None:
        result["probe_output_equal"] = old_output == new_output
    return result


comparisons = []
for name in ("reader_basic-metalio-plus", "standby-metalio-plus", "ai_ask-metalio-plus",
             "ai_ask-metalio-plus-pqbn"):
    comparisons.append(compare(name, ROOT / "build-audit/before/test-out" / name,
                               ROOT / "build-audit/test-out" / name))
for old in results:
    if not old["before"] or old["name"] not in ("r4_normal", "r4_normal_activity", "r4_service_normal"):
        continue
    new = next(r for r in results if r["name"] == old["name"] and r["pqbn"] == old["pqbn"]
               and not r["before"] and not r["sanitizer"])
    old_dir = Path(old["command"][old["command"].index("--out") + 1])
    new_dir = Path(new["command"][new["command"].index("--out") + 1])
    comparisons.append(compare(new_dir.name, old_dir, new_dir, old["output"], new["output"]))
write("comparison.json", {"head": HEAD, "before": BEFORE,
                          "note": "固定配置的虚拟时间比较；不代表实机测量。未出现的计数器在解读时按0，原始差异保留null。",
                          "comparisons": comparisons})

suites = []
for directory, count in (("build-audit", 42), ("build-audit-asan", 41)):
    log = ROOT / directory / "ctest.log"
    lines = log.read_text().splitlines()
    test_lines = [s for s in lines if re.search(r"\d+/\d+ Test\s+#\d+:", s)]
    summaries = [s for s in lines if "tests passed out of" in s or "Total Test time" in s]
    assert len(test_lines) == count and all("Passed" in s for s in test_lines)
    suites.append({"directory": directory, "log": relative(log), "observed_exit_code": 0,
                   "tests": test_lines, "summary": summaries})
selected = [metric_evidence(ROOT / "build-audit/test-out" / name) for name in
            ("selftest-board", "selftest-board-pqbn", "ai_mic_unavailable-metalio-plus",
             "reader_basic-metalio-plus", "standby-metalio-plus", "ai_ask-metalio-plus",
             "ai_ask-metalio-plus-pqbn", "gui-smoke/out")]
write("suite-evidence.json", {"head": HEAD, "environment": {
    "os": "macOS 15.7.9 (24G830)", "arch": "arm64", "compiler": "Apple clang 17.0.0",
    "cmake": "3.28.3", "ctest": "4.4.4", "probe_python": "3.9.6"},
    "suites": suites, "selected_scenarios": selected})
print(json.dumps({"probe_records": len(rows), "comparisons": len(comparisons),
                  "formal_tests": [42, 41]}, ensure_ascii=False))
