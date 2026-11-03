#!/usr/bin/env python3
"""Validate this audit package and its observations; passing is not product acceptance."""
import collections
import json
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
HEAD = "95587e5a5f641fa4fd44c313f366e69f244faeb1"
checks = []

def check(name, ok, detail):
    checks.append(dict(name=name, passed=bool(ok), detail=detail))

def read(name):
    return json.loads((AUDIT / name).read_text())

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

check("fixed_commit", git("rev-parse", "HEAD") == HEAD, HEAD)
changes = git("diff", "--name-only") + git("diff", "--cached", "--name-only")
check("tracked_files_unchanged", not changes, changes or "no tracked changes")
untracked = git("ls-files", "--others", "--exclude-standard").splitlines()
check("untracked_scope", all(x.startswith("docs/audits/2026-11-03-codex-r3/") for x in untracked), untracked)
files = [p for p in AUDIT.rglob("*") if p.is_file()]
check("file_sizes", all(p.stat().st_size <= 1_000_000 for p in files),
      {"files": len(files), "largest_bytes": max(p.stat().st_size for p in files)})
check("no_build_logs_or_media", not any(p.suffix in (".log", ".png", ".wav", ".a", ".o") for p in files),
      "runtime artifacts remain in allowed build directories")
for p in files:
    if p.suffix == ".json":
        json.loads(p.read_text())
check("json_parse", True, "all audit JSON files parse")
for p in files:
    if p.suffix == ".py":
        compile(p.read_text(), str(p), "exec")  # no __pycache__ output
check("python_syntax", True, "all experiment scripts parse")

rows = read("verification.json")
needed = {f"AUD3-{i:03}" for i in range(1, 5)} | {f"AUD2-{i:03}" for i in (10, 12, 16, 19, 20, 22, 23, 24)}
check("verification_ids", len(rows) == 12 and {r["id"] for r in rows} == needed, sorted(needed))
check("verification_schema", all(set(r) == {"id", "verdict", "evidence_type", "note"} and
      r["verdict"] in ("已修复", "部分修复", "未修复", "回归") for r in rows),
      dict(collections.Counter(r["verdict"] for r in rows)))
findings = read("findings.json")
schema = {"id", "severity", "area", "title", "evidence_type", "locations", "impact", "recommendation", "reproduce"}
check("findings_schema", all(set(f) == schema for f in findings) and
      [f["id"] for f in findings] == [f"AUD4-{i:03}" for i in range(1, 5)],
      dict(collections.Counter(f["severity"] for f in findings)))
bad_locations = []
for f in findings:
    for location in f["locations"]:
        path, line = location.rsplit(":", 1)
        p = ROOT / path
        if not p.is_file() or not 1 <= int(line) <= len(p.read_text().splitlines()):
            bad_locations.append(location)
check("finding_locations_exist", not bad_locations, bad_locations)
report = (AUDIT / "report.md").read_text()
check("report_complete", "审计进行中" not in report and "最终结论待" not in report and
      all(f["title"] in report for f in findings) and
      all(f"| {r['id']} | {r['verdict']} |" in report for r in rows),
      "all verdicts and finding titles rendered")
broken_links = []
for p in (AUDIT / "report.md", HERE / "README.md"):
    for target in re.findall(r"\]\(([^)]+)\)", p.read_text()):
        target = target.split("#", 1)[0]
        if target and "://" not in target and not (p.parent / target).exists():
            broken_links.append(str(p.relative_to(AUDIT)) + ": " + target)
check("local_markdown_links", not broken_links, broken_links)

for build, n in (("build-audit", 40), ("build-audit-asan", 39)):
    log = (ROOT / build / "ctest.log").read_text()
    check(build + "_ctest", f"100% tests passed out of {n}" in log, f"{n}/{n}")
layout = read("experiments/layout-evidence.json")
check("layout_sources", len(layout["code_files"]) == 85 and
      all(x["code_identical"] for x in layout["code_files"]) and not layout["added"] and not layout["deleted"],
      "85 changed code pairs; no additions/deletions")
check("layout_headers", len(layout["header_checks"]) == 12 and all(x["passed"] for x in layout["header_checks"]), "12/12")
history = read("experiments/history-evidence.json")
check("history_builds_tests", history["complete"] and len(history["commands"]) == 6 and
      all(x["exit_code"] == 0 for x in history["commands"]), "configure/build/ctest each revision exit=0")
check("history_artifacts", history["metrics_pairs"] == 39 and history["png_pairs"] == 410 and
      all(not history[k] for k in ("metrics_only_before", "metrics_only_after", "metrics_differences",
                                  "png_only_before", "png_only_after", "png_differences")),
      "39 metrics and 410 PNG pairs identical")
runs = read("experiments/results.json")
check("probe_processes", len(runs) == 27 and all(r["exit_code"] == 0 for r in runs),
      "27 configurations ended normally; defects are judged by observations")
index = {(r["name"], r["sanitizer"], r.get("pqbn", False)): "\n".join(r["output"]) for r in runs}
def output(name, san=False, pq=False):
    return index[(name, san, pq)]

check("timer_delete_observation", all("rearm rc=-3" in output("r2_timer_rearm_delete", san) and
      "observation complete calls=1" in output("r2_timer_rearm_delete", san) for san in (False, True)), "normal and ASan")
check("AUD4-001_observation", "panel rails cut ready=0" in output("r3_display_transient") and
      "callback id=1 ok=1 count=1" in output("r3_display_transient") and
      "callback id=1 ok=0 count=1" in output("r3_display_transient", pq=True) and
      all("VCOM cut ready=0" in output("r3_display_vcom", pq=pq) and
          "callback id=1 ok=1 count=1" in output("r3_display_vcom", pq=pq) for pq in (False, True)),
      "EPDiy rail transient and both VCOM transients falsely succeed; PQ rail control fails")
check("PQ_callbacks_once", all(output("r3_display_multi", pq=True).count(f"callback id={i} ok=0 count=1") == 1
      for i in range(1, 5)) and "callback id=5 ok=1 count=1" in output("r3_display_multi", pq=True) and
      "after cut pending=0" in output("r3_display_multi", pq=True), "4 failures then successful new request")
check("AUD4-002_observation", all("after volume cfg=00,80 keys=1" in output("r3_wake_touch", san) and
      "after touch release cfg=c0,e3 keys=2" in output("r3_wake_touch", san) for san in (False, True)),
      "normal and ASan")
for identifier, name in (("AUD4-003", "r3_wake_audio"), ("AUD4-004", "r3_boot_audio")):
    check(identifier + "_observation", all("capture start rc=0 mode=0 clock=0" in output(name, san) and
          "capture observed samples=0 mode=0 clock=0 power=1 busy=1" in output(name, san)
          for san in (False, True)), "normal and ASan; powered but no audio clock/samples")
san_errors = []
for r in runs:
    if not r["sanitizer"]:
        continue
    out = Path(r["command"][r["command"].index("--out") + 1])
    log = (out / "run.log").read_text()
    if re.search(r"ERROR: AddressSanitizer|runtime error:|SUMMARY: (AddressSanitizer|UndefinedBehaviorSanitizer)", log):
        san_errors.append(r["name"])
check("sanitizer_logs", not san_errors, {"configurations": 5, "errors": san_errors, "leak_check": False})

result = {"audit_commit": HEAD, "all_checks_passed": all(x["passed"] for x in checks),
          "meaning": "audit package and observations are consistent; product still has 1 critical and 3 high findings",
          "checks": checks}
(HERE / "artifact-validation.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps({"all_checks_passed": result["all_checks_passed"], "checks": len(checks),
                  "failed": [x["name"] for x in checks if not x["passed"]]}, ensure_ascii=False))
raise SystemExit(0 if result["all_checks_passed"] else 1)
