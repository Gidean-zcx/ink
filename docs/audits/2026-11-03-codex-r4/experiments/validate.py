#!/usr/bin/env python3
"""Check the delivered audit evidence; this is not a product acceptance test."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
OUTPUT = HERE / "artifact-validation.json"
EXPECTED_HEAD = "89b112620730902f0fa1594e83ffb146b08749be"
checks = []


def check(name, passed, detail):
    checks.append(dict(name=name, passed=bool(passed), detail=detail))


def read_json(path):
    return json.loads(path.read_text())


def git(*args):
    p = subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True)
    return p.returncode, p.stdout.strip()


def key(row):
    return tuple(row.get(k, False) for k in ("name", "sanitizer", "pqbn", "before"))


head_rc, head = git("rev-parse", "HEAD")
diff_rc, diff = git("diff", "--exit-code")
index_rc, index_diff = git("diff", "--cached", "--exit-code")
branch_rc, branch = git("branch", "--show-current")
status_rc, status = git("status", "--short", "--untracked-files=all")
prefix = "?? " + str(AUDIT.relative_to(ROOT)) + "/"
check("object_and_read_only_scope", head_rc == diff_rc == index_rc == branch_rc == status_rc == 0
      and head == EXPECTED_HEAD and not branch and not diff and not index_diff
      and all(line.startswith(prefix) for line in status.splitlines()),
      dict(head=head, detached=not branch, diff_exit=diff_rc, index_diff_exit=index_rc,
           status_lines=status.splitlines(), ignored_build_outputs_excluded=True))

verification = read_json(AUDIT / "verification.json")
findings = read_json(AUDIT / "findings.json")
report = (AUDIT / "report.md").read_text()
expected_ids = {"AUD4-001", "AUD4-002", "AUD4-003", "AUD4-004", "AUD2-019", "AUD2-023", "AUD3-002", "N3-3"}
verdicts = Counter(r["verdict"] for r in verification)
check("verification_schema_and_coverage", len(verification) == 8
      and {r["id"] for r in verification} == expected_ids
      and verdicts == {"已修复": 5, "部分修复": 3}
      and all(set(r) == {"id", "verdict", "evidence_type", "note"} for r in verification), dict(verdicts))
fields = {"id", "severity", "area", "title", "evidence_type", "locations", "impact", "recommendation", "reproduce"}
severity = Counter(r["severity"] for r in findings)
check("findings_schema_and_severity", [r["id"] for r in findings] == [f"AUD5-{i:03}" for i in range(1, 5)]
      and all(set(r) == fields and all(r.values()) for r in findings)
      and severity == {"严重": 1, "高": 2, "中": 1}, dict(severity))
bad_locations = []
for row in findings:
    for location in row["locations"]:
        name, line = location.rsplit(":", 1)
        path = ROOT / name
        if not path.is_file() or not 1 <= int(line) <= len(path.read_text().splitlines()):
            bad_locations.append(location)
check("source_locations_resolve", not bad_locations, bad_locations)
check("report_matches_structured_rows", all(f"| {r['id']} | {r['verdict']} |" in report for r in verification)
      and all(report.count(f"### {r['id']}（{r['severity']}）") == 1 for r in findings), "8 verification rows; 4 unique finding headings")
check("report_complete", all(report.count(h) == 1 for h in ("## 一页结论", "## 修复核实表", "## 新问题", "## 分组核查记录", "## 命令、覆盖与限制"))
      and not any(x in report for x in ("当前状态：进行中", "完成后填写", "尚未完成核查", "后续逐组补充", "对照另补")), "No initial placeholders or duplicate sections")

bad_links = []
for path in (AUDIT / "report.md", HERE / "README.md"):
    for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", path.read_text()):
        if target.startswith(("http://", "https://", "#")):
            continue
        dest = (path.parent / target.split("#", 1)[0]).resolve()
        if dest != OUTPUT and not dest.exists():
            bad_links.append(dict(source=str(path.relative_to(ROOT)), target=target))
check("local_markdown_links", not bad_links, bad_links)

results = read_json(HERE / "results.json")
metrics = read_json(HERE / "metrics-evidence.json")
by_id = {key(r): r for r in results}
metric_by_id = {key(r): r for r in metrics}
counts = dict(total=len(results), current_regular=sum(not r["sanitizer"] and not r.get("before", False) for r in results),
              sanitizer=sum(r["sanitizer"] for r in results), before=sum(r.get("before", False) for r in results))
check("experiment_inventory", counts == dict(total=54, current_regular=34, sanitizer=7, before=13)
      and len(by_id) == 54 and all(r["exit_code"] == 0 for r in results), counts)
check("metric_inventory", len(metrics) == 54 and set(by_id) == set(metric_by_id), "One metric summary per experiment configuration")
evidence_errors = []
san_errors = []
for ident, row in by_id.items():
    cmd = row["command"]
    out = Path(cmd[cmd.index("--out") + 1])
    log = (out / "run.log").read_text(errors="replace")
    actual = read_json(out / "metrics.json")
    summary = metric_by_id[ident]
    if not all(s in log for s in row["output"]):
        evidence_errors.append([list(ident), "snippet absent from run.log"])
    if any(actual["metrics"].get(k) != v for k, v in summary["metrics"].items()) or actual["warnings"] != summary["warnings"]:
        evidence_errors.append([list(ident), "metric summary mismatch"])
    if row["sanitizer"] and any(s in log for s in ("ERROR: AddressSanitizer", "runtime error:", "SUMMARY: UndefinedBehaviorSanitizer")):
        san_errors.append(list(ident))
check("runtime_evidence_matches", not evidence_errors, evidence_errors)
check("selected_sanitizer_runs", not san_errors, dict(runs=7, diagnostics=san_errors, leak_sanitizer=False, thread_sanitizer=False))


def observed(name, pq=False):
    return "\n".join(by_id[(name, False, pq, False)]["output"])


def measured(name):
    return metric_by_id[(name, False, False, False)]["metrics"]


check("AUD5_001_recorded_counterexample", all(observed("r4_p05_preoff", pq).count("ok=1 p05=0 ready=0") == 3 for pq in (False, True))
      and "id=4 ok=1 p05=1 ready=1 latency_us=0" in observed("r4_p05_preoff", True), "Both profiles: three false successes; PQ-BN restored same-target update has zero waveform latency")
failed_audio = measured("r4_service_failed")
normal_audio = measured("r4_service_normal")
check("AUD5_002_recorded_counterexample", failed_audio["audio.start_without_chip"] == 9
      and failed_audio["audio.speaker.played_ms"] == 0 and normal_audio["audio.speaker.played_ms"] == 480,
      dict(failed_played_ms=failed_audio["audio.speaker.played_ms"], normal_played_ms=normal_audio["audio.speaker.played_ms"]))
ai = measured("r4_ai_failed")
check("AUD5_003_recorded_counterexample", ai["audio.mic.captured_ms"] == 0 and ai["ai.mic_unavailable"] == 1
      and ai["hub.requests.ai.ask"] == 1 and ai["ai.answers"] == 1, "Zero capture with one ai.ask and answer")
board = read_json(HERE / "board-results.json")
board_ok = True
for row in board:
    actual = read_json(ROOT / row["path"])
    board_ok &= (row["test_pass"] == actual["metrics"].get("test.pass", 0)
                 and row["test_fail"] == actual["metrics"].get("test.fail", 0)
                 and row["budget_violations"] == actual["budget_violations"])
check("AUD5_004_recorded_counterexample", board_ok and board[0]["test_pass"] == 65 and board[0]["budget_violations"] == 1
      and board[1]["test_pass"] == 66 and board[1]["budget_violations"] == 0, board)
check("restore_observations", all("cap=-1 play=-1 busy=0" in observed(name)
      and "cfg=c0,e3 keys=4 p14=1" in observed(name) and "write=2400 samples=9600" in observed(name)
      for name in ("r4_touch_restore", "r4_audio_restore")), "Failure releases resources; later restore and retry succeed")
check("gpio2_observations", measured("r4_gpio2_accel")["input.gpio2_rechecks"] == 10
      and measured("r4_gpio2_normal").get("input.gpio2_rechecks", 0) == 0, "Fault: 10 rechecks; normal: 0")

comparison = read_json(HERE / "comparison.json")
normal_ok = len(comparison["comparisons"]) == 5
for row in comparison["comparisons"]:
    paths = []
    for before in (True, False):
        cmd = by_id[(row["name"], False, row["pqbn"], before)]["command"]
        paths.append(Path(cmd[cmd.index("--out") + 1]) / "final.png")
    normal_ok &= row["final_png_identical"] and paths[0].read_bytes() == paths[1].read_bytes()
    normal_ok &= not row["warnings_before"] and not row["warnings_after"]
check("normal_path_comparisons", normal_ok, "Five before/after pairs; identical final PNG bytes and no warnings")
images = read_json(HERE / "image-evidence.json")
image_ok = all(hashlib.sha256((ROOT / c["file"]).read_bytes()).hexdigest() == c["sha256"] for r in images for c in r["comparisons"])
image_ok &= all(c["pixels_changed_from_before"] == 0 and c["mean_gray"] == 255 for c in images[1]["comparisons"])
image_ok &= all(c["pixels_changed_from_before"] == 16384 for c in images[0]["comparisons"])
check("image_evidence_integrity", image_ok, "Image SHA-256 values match local PNGs; recorded pixel statistics match reported claims")
check("original_ctest_summaries", "100% tests passed out of 40" in (ROOT / "build-audit/ctest.log").read_text()
      and "100% tests passed out of 39" in (ROOT / "build-audit-asan/ctest.log").read_text(), "40 regular and 39 ASan/UBSan; not evidence against untested counterexamples")

manifest = []
for path in sorted(AUDIT.rglob("*")):
    if path.is_file() and path != OUTPUT:
        data = path.read_bytes()
        manifest.append(dict(path=str(path.relative_to(AUDIT)), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
check("artifact_size_and_content_scope", all(r["bytes"] <= 1000000 and Path(r["path"]).suffix in {".md", ".json", ".py", ".c"} for r in manifest),
      dict(files_excluding_this_validation=len(manifest), largest_bytes=max(r["bytes"] for r in manifest), build_logs_included=False))
payload = dict(audit_commit=head, purpose="Audit package consistency only; confirmed product defects remain open",
               all_checks_passed=all(r["passed"] for r in checks), checks=checks, manifest=manifest)
OUTPUT.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(dict(checks=len(checks), passed=sum(r["passed"] for r in checks), failures=[r["name"] for r in checks if not r["passed"]],
                      output=str(OUTPUT.relative_to(ROOT))), ensure_ascii=False))
raise SystemExit(0 if payload["all_checks_passed"] else 1)
