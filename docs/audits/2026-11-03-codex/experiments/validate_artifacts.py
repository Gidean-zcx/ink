#!/usr/bin/env python3
"""Check audit artifacts only; do not mutate product sources or git state."""
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
UPSTREAM = Path("/Users/bytedance/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus")
HEAD = "3a435a59812b9d3d40fbe04358c9b17ef4eb15bf"
UPSTREAM_HEAD = "690a023c66862af103a3187464373ee7c5efcd14"
OUTPUT = HERE / "validation.json"
LIMIT = 1_000_000
checks = []
errors = []

def check(name, ok, details=None):
    row = {"check": name, "passed": bool(ok)}
    if details is not None:
        row["details"] = details
    checks.append(row)
    if not ok:
        errors.append(name)

def git(*args, cwd=ROOT):
    return subprocess.run(["git", *args], cwd=cwd, text=True, capture_output=True)

findings = json.loads((AUDIT / "findings.json").read_text())
report = (AUDIT / "report.md").read_text()
required_keys = {"id", "severity", "area", "title", "evidence_type", "locations",
                 "impact", "recommendation", "reproduce"}
check("findings_schema", isinstance(findings, list) and all(
    set(f) == required_keys and isinstance(f["locations"], list) and f["locations"]
    and all(isinstance(x, str) and x for x in f["locations"])
    and all(isinstance(f[k], str) and f[k] for k in required_keys - {"locations"})
    for f in findings))
ids = [f["id"] for f in findings]
check("ids_unique_and_contiguous", ids == [f"AUD2-{n:03}" for n in range(1, 25)])
counts = Counter(f["severity"] for f in findings)
check("severity_counts", counts == {"严重": 5, "高": 10, "中": 8, "低": 1}, dict(counts))
check("area_and_evidence_values", all(
    f["area"] in "ABCDEFGHI" and len(f["area"]) == 1
    and f["evidence_type"] in {"复现", "代码核查", "文档核查", "推断"}
    for f in findings))
check("report_sections_A_to_I", re.findall(r"^## ([A-I])\.", report, re.M) == list("ABCDEFGHI"))
check("I_is_last_top_level_section", re.findall(r"^## (.+)$", report, re.M)[-1].startswith("I."))
check("report_findings_match", all(
    report.count(f'### {f["id"]}｜{f["severity"]}｜{f["title"]}') == 1
    and all(loc in report for loc in f["locations"]) for f in findings))
check("no_known_exit_code_typo", "exit=-15" not in report and "。。" not in report)

location_results = []
for f in findings:
    for loc in f["locations"]:
        path_text, number = loc.rsplit(":", 1)
        line_number = int(number)
        if path_text.startswith("upstream@690a023:"):
            relative = path_text.split(":", 1)[1]
            result = git("show", UPSTREAM_HEAD + ":" + relative, cwd=UPSTREAM)
            lines = result.stdout.splitlines() if result.returncode == 0 else []
        else:
            file = ROOT / path_text
            lines = file.read_text().splitlines() if file.is_file() else []
        location_results.append({"id": f["id"], "location": loc,
                                 "valid": 1 <= line_number <= len(lines)})
check("source_files_and_line_numbers", all(x["valid"] for x in location_results),
      {"checked": len(location_results), "invalid": [x for x in location_results if not x["valid"]]})

required_artifacts = [
    "README.md", "enable_objc.cmake", "build_probes.py", "run_probes.py",
    "probe.c", "memory_probe.c", "gui_restart_probe.py", "collect_results.py",
    "validate_artifacts.py", "configure.log", "configure-second.log",
    "configure-workaround.log", "build.log", "ctest.log", "asan-configure.log",
    "asan-build.log", "asan-ctest.log", "probe-build.log", "probe-results.json",
    "memory.log", "gui-restart.log", "gui-restart-results.json",
    "evidence-summary.json", "ai_held_dismiss.script", "ai_held_control.script"
]
modes = ["storage_cache", "tca_shadow", "input_fault", "touch_blocks_power",
         "camera_cut", "camera_timeout", "audio_unpowered", "sleep_rails",
         "display_fault", "nt26_unicode", "ai_held_dismiss", "ai_held_control"]
required_artifacts += [name + ".log" for name in modes]
check("required_artifacts_exist", all((HERE / p).is_file() for p in required_artifacts),
      {"count": len(required_artifacts)})

bad_links = []
checked_links = 0
for md in [AUDIT / "report.md", HERE / "README.md"]:
    for target in re.findall(r"\[[^\]\n]+\]\(([^)\n]+)\)", md.read_text()):
        if target.startswith(("http://", "https://", "#")):
            continue
        checked_links += 1
        path = (md.parent / target.split("#", 1)[0].strip("<>")).resolve()
        if path != OUTPUT and not path.exists():
            bad_links.append({"file": str(md.relative_to(AUDIT)), "target": target})
check("markdown_local_links", not bad_links, {"checked": checked_links, "invalid": bad_links})
referenced = set(re.findall(r"experiments/([a-zA-Z0-9_.-]+\.(?:py|c|cmake|script|json|log|md))", report))
check("report_experiment_references", all((HERE / p).exists() or HERE / p == OUTPUT for p in referenced),
      {"checked": len(referenced)})

probe_results = json.loads((HERE / "probe-results.json").read_text())
check("all_12_probe_modes_completed", {p["name"] for p in probe_results} == set(modes)
      and len(probe_results) == 12 and all(p["exit_code"] == 0 for p in probe_results))
check("probe_excerpts_match_logs", all(
    all(line in (HERE / (p["name"] + ".log")).read_text() for line in p.get("output", []))
    for p in probe_results))
gui = json.loads((HERE / "gui-restart-results.json").read_text())
pairs = sorted(set((x["boots"], x["pipe_fds"]) for x in gui["samples"]))
check("gui_restart_observation", pairs == [(1, 4), (2, 6), (3, 8), (4, 10)]
      and gui["exit_code"] == 0 and all(x["lsof_rc"] == 0 for x in gui["samples"]),
      {"boots_and_pipe_fds": pairs, "process_exit_code": gui["exit_code"]})
for name, elapsed in [("ctest.log", "11.98"), ("asan-ctest.log", "14.11")]:
    log = (HERE / name).read_text()
    check(name + "_35_passed", "100% tests passed out of 35" in log
          and f"=  {elapsed} sec" in log)
memory = (HERE / "memory.log").read_text()
check("memory_observation", all(s in memory for s in [
    "returned_pool=0", "reported_free=59136 tlsf_free=58336 overstatement=800",
    "configured_dma=0 reported_total=1008",
    "live=0 host_bytes=2097152 failed_allocs=0"]))
summary = json.loads((HERE / "evidence-summary.json").read_text())
audio_metric = "audio.mic.captured_ms"
check("ai_control_metrics", summary["audit-scenarios/ai_held_dismiss"]["metrics"][audio_metric] == 61965
      and summary["audit-scenarios/ai_held_control"]["metrics"][audio_metric] == 3000)
check("original_configure_failure_preserved", all(
    "CMAKE_OBJC_COMPILE_OBJECT" in (HERE / name).read_text()
    for name in ["configure.log", "configure-second.log"]))

syntax_errors = []
for path in HERE.glob("*.py"):
    try:
        compile(path.read_text(), str(path), "exec")
    except SyntaxError as exc:
        syntax_errors.append({"path": path.name, "line": exc.lineno})
check("python_syntax_without_cache_writes", not syntax_errors, syntax_errors)

current = git("rev-parse", "HEAD")
official = git("rev-parse", "HEAD", cwd=UPSTREAM)
check("root_commit_fixed", current.returncode == 0 and current.stdout.strip() == HEAD)
check("upstream_commit_fixed", official.returncode == 0 and official.stdout.strip() == UPSTREAM_HEAD)
working = git("diff", "--exit-code", "--quiet")
staged = git("diff", "--cached", "--exit-code", "--quiet")
check("tracked_product_changes_absent", working.returncode == 0 and staged.returncode == 0,
      {"working_diff_exit": working.returncode, "staged_diff_exit": staged.returncode})
status = git("status", "--porcelain")
status_lines = status.stdout.splitlines()
allowed_prefix = "?? docs/audits/2026-11-03-codex/"
check("visible_changes_only_in_audit_directory", status.returncode == 0
      and all(x.startswith(allowed_prefix) for x in status_lines), status_lines)

files = sorted(p for p in AUDIT.rglob("*") if p.is_file() and p != OUTPUT)
check("no_symlink_artifacts", all(not p.is_symlink() for p in files))
manifest = [{"path": str(p.relative_to(AUDIT)), "bytes": p.stat().st_size,
             "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
check("each_artifact_under_1MB", all(x["bytes"] < LIMIT for x in manifest),
      {"count_excluding_validation": len(manifest),
       "largest_bytes": max(x["bytes"] for x in manifest), "limit_bytes": LIMIT})

# Limited formatting scan: report counts only; never print matching content.
patterns = {
    "mac_address_format": r"(?<![0-9A-Fa-f])(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}(?![0-9A-Fa-f])",
    "imei_or_iccid_length_decimal": r"(?<![A-Za-z0-9.])(?:[0-9]{15}|[0-9]{19,20})(?![A-Za-z0-9.])",
    "api_secret_format": r"sk-[A-Za-z0-9_-]{20,}",
    "private_key_block": r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
    "bearer_credential": r"(?i)authorization\s*:\s*bearer\s+[A-Za-z0-9_.+/-]{12,}",
    "sensitive_label_value": r"(?im)\b(?:ssid|bssid|imei|iccid|api[_-]?key|password|access[_-]?token|refresh[_-]?token)\b\s*[:=]\s*[\"']?[A-Za-z0-9/+_.:@-]{8,}",
}
sensitive_counts = {}
for label, pattern in patterns.items():
    sensitive_counts[label] = sum(len(re.findall(pattern, p.read_text(errors="replace"))) for p in files)
check("limited_sensitive_format_scan", not any(sensitive_counts.values()), sensitive_counts)

data = {
    "generated_at_utc": datetime.now(timezone.utc).isoformat(),
    "status": "passed" if not errors else "failed",
    "scope": "Artifact integrity checks only; not a semantic or security proof.",
    "audit_commit": current.stdout.strip(),
    "upstream_commit": official.stdout.strip(),
    "findings": {"total": len(findings), "by_severity": dict(counts)},
    "checks": checks,
    "files_excluding_this_validation_json": manifest,
    "errors": errors,
}
encoded = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
if len(encoded.encode()) >= LIMIT:
    raise RuntimeError("Validation manifest would exceed the per-file size limit")
OUTPUT.write_text(encoded)
print(json.dumps({"status": data["status"], "checks": len(checks),
                  "files_including_validation": len(files) + 1,
                  "errors": errors}, ensure_ascii=False))
sys.exit(0 if not errors else 1)

