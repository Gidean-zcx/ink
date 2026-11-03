#!/usr/bin/env python3
"""Validate audit artifact consistency and the read-only product boundary."""
import collections
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
EXPECTED_HEAD = "0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3"
REL = "docs/audits/2026-11-03-codex-r5/"


def command(args):
    r = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    assert r.returncode == 0, (args, r.returncode, r.stderr)
    return r.stdout.strip()


def read(path):
    return json.loads(path.read_text())


head = command(["git", "rev-parse", "HEAD"])
assert head == EXPECTED_HEAD
assert command(["git", "diff", "--exit-code"]) == ""
assert command(["git", "diff", "--cached", "--exit-code"]) == ""
status = command(["git", "status", "--porcelain", "--untracked-files=all"])
assert all(s.startswith("?? " + REL) for s in status.splitlines()), status

verification = read(AUDIT / "verification.json")
assert [v["id"] for v in verification] == ["AUD5-001", "AUD5-002", "AUD5-003", "AUD5-004", "AUD2-023", "N4-1"]
for v in verification:
    assert set(v) == {"id", "verdict", "evidence_type", "note"}
    assert v["verdict"] in {"已修复", "部分修复", "未修复", "回归"}
    assert v["evidence_type"] and v["note"]

findings = read(AUDIT / "findings.json")
assert [f["id"] for f in findings] == ["AUD6-001", "AUD6-002", "AUD6-003"]
locations = []
for f in findings:
    assert set(f) == {"id", "severity", "area", "title", "evidence_type", "locations", "impact", "recommendation", "reproduce"}
    assert f["severity"] in {"严重", "高", "中", "低"}
    assert f["locations"]
    for loc in f["locations"]:
        name, line = loc.rsplit(":", 1)
        assert 0 < int(line) <= len((ROOT / name).read_text().splitlines()), loc
        locations.append(loc)

results = read(HERE / "results.json")
keys = [(r["name"], r["before"], r["sanitizer"], r["pqbn"]) for r in results]
assert len(keys) == len(set(keys)) == 53
assert all(r["exit_code"] == 0 for r in results)
error_pattern = re.compile(r"ERROR: (?:AddressSanitizer|UndefinedBehaviorSanitizer)|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer)")
san_logs = []
for r in results:
    if r["sanitizer"]:
        out = Path(r["command"][r["command"].index("--out") + 1])
        san_logs.append(out / "run.log")
san_logs.append(ROOT / "build-audit-asan/Testing/Temporary/LastTest.log")
for log in san_logs:
    assert not error_pattern.search(log.read_text(errors="replace")), log

links = []
for markdown in (AUDIT / "report.md", HERE / "README.md"):
    for target in re.findall(r"\]\(([^)]+)\)", markdown.read_text()):
        if re.match(r"https?://", target):
            continue
        local = target.split("#", 1)[0]
        if local:
            # This validation result is generated below, so its report link may
            # legitimately be absent on the first run.
            destination = markdown.parent / local
            assert destination.exists() or destination == HERE / "artifact-validation.json", (markdown, target)
            links.append(str((markdown.parent / local).relative_to(ROOT)))

sizes = {}
for file in sorted(AUDIT.rglob("*")):
    if not file.is_file() or file.name == "artifact-validation.json":
        continue
    size = file.stat().st_size
    assert size <= 1_000_000, (file, size)
    sizes[str(file.relative_to(AUDIT))] = size
    if file.suffix == ".json":
        read(file)

summary = {"head": head, "product_tracked_diff_empty": True, "staged_diff_empty": True,
           "all_untracked_files_under_audit": True, "file_bytes": sizes,
           "verification_counts": dict(collections.Counter(v["verdict"] for v in verification)),
           "new_finding_counts": dict(collections.Counter(f["severity"] for f in findings)),
           "validated_source_locations": locations, "validated_local_links": links,
           "unique_probe_combinations": len(results), "probe_processes_exit_zero": len(results),
           "note": "探针进程完成不等于行为通过；3条新问题均在exit=0的反例中观察到。",
           "sanitizer_logs_checked": [str(p.relative_to(ROOT)) for p in san_logs],
           "sanitizer_error_matches": 0}
serialized = json.dumps(summary, ensure_ascii=False, indent=2) + "\n"
assert len(serialized.encode()) <= 1_000_000
(HERE / "artifact-validation.json").write_text(serialized)
assert all((ROOT / link).exists() for link in links)
print(json.dumps({"validation": "ok", "files": len(sizes),
                  "verification": summary["verification_counts"],
                  "new_findings": summary["new_finding_counts"]}, ensure_ascii=False))
