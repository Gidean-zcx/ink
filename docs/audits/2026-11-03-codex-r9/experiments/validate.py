#!/usr/bin/env python3
"""Validate audit deliverables and the read-only boundary; not a product test."""
import json
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = HERE.parent
ROOT = HERE.parents[3]
HEAD = "31642df9fb101e79041d03b1edb215174c6dc116"

def read(name):
    return json.loads((HERE / name).read_text())

def cmd(argv):
    p = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, check=False)
    return dict(command=argv, exit_code=p.returncode, output=p.stdout.strip(), stderr=p.stderr.strip())

commands = [cmd(["git", "rev-parse", "HEAD"]), cmd(["git", "diff", "--exit-code", "HEAD", "--"]),
            cmd(["git", "ls-files", "--others", "--exclude-standard"])]
assert all(x["exit_code"] == 0 for x in commands)
assert commands[0]["output"] == HEAD
assert not commands[1]["output"]
prefix = str(BASE.relative_to(ROOT)) + "/"
assert all(x.startswith(prefix) for x in commands[2]["output"].splitlines())

files = []
json_count = 0
for path in sorted(BASE.rglob("*")):
    if not path.is_file() or path.name == "artifact-validation.json":
        continue
    size = path.stat().st_size
    assert size < 1_000_000, path
    assert path.suffix not in (".log", ".png", ".wav", ".elf", ".o", ".a"), path
    if path.suffix == ".json":
        json.loads(path.read_text())
        json_count += 1
    files.append(dict(path=str(path.relative_to(BASE)), bytes=size))

links = []
for path in [BASE / "report.md", HERE / "README.md"]:
    for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", path.read_text()):
        if "://" in target or target.startswith("#"):
            continue
        local = (path.parent / target.split("#")[0]).resolve()
        assert local.exists() or local == HERE / "artifact-validation.json", (path, target)
        links.append(dict(source=str(path.relative_to(BASE)), target=target))

verification = json.loads((BASE / "verification.json").read_text())
assert {x["id"] for x in verification} == {"AUD9-001", "AUD9-002", "AUD2-023"}
assert all(x["verdict"] in ["已修复", "部分修复", "未修复", "回归"] for x in verification)
findings = json.loads((BASE / "findings.json").read_text())
required = {"id", "severity", "area", "title", "evidence_type", "locations", "impact", "recommendation", "reproduce"}
assert len(findings) == 1 and findings[0]["id"] == "AUD10-001" and findings[0]["severity"] == "中"
assert all(required <= x.keys() for x in findings)
report = (BASE / "report.md").read_text()
assert "状态：已完成" in report and "待核实" not in report and "正在完成" not in report
assert all(x["id"] in report for x in findings + verification)

suite_steps = read("suite-steps.json")
assert len(suite_steps) == 9 and all(x["exit_code"] == 0 for x in suite_steps)
suite_evidence = read("suite-evidence.json")
assert [len(x["tests"]) for x in suite_evidence["suites"]] == [43, 42]
for filename, count in [("pq-results.json", 23), ("pq-extended-results.json", 552)]:
    data = read(filename)
    assert len(data) == 4 and all(x["exit_code"] == 0 for x in data)
    assert all(any("total=" + str(count) in line for line in x["output"]) for x in data if x["label"] == "run")
for filename, count in [("pq-cost-results.json", 5), ("pq-cost-extended-results.json", 10)]:
    data = read(filename)
    assert len(data) == 3 and all(x["exit_code"] == 0 for x in data)
    assert all(len(x["regions"]) == count for x in data if x["label"] != "compile")
results = read("results.json")
assert len(results) == 22 and all(x["exit_code"] == 0 for x in results)
assert all(any(" end " + x["name"] in s for s in x["output"]) for x in results), "a probe did not finish"
assert all(x["exit_code"] == 0 for x in read("probe-builds.json"))
metrics = read("metrics-evidence.json")["runs"]
assert len(metrics) == 22 and all(not x["warnings"] for x in metrics)
comparisons = read("comparison.json")["comparisons"]
assert len(comparisons) == 12
assert all(not x["warnings_before"] and not x["warnings_after"] and
           x["budget_violations_before"] == 0 and x["budget_violations_after"] == 0 for x in comparisons)
default = [x for x in comparisons if "pqbn" not in x["name"]]
assert len(default) == 6 and all(not x["all_metric_differences"] for x in default)
audio = read("audio-integrity.json")["runs"]
assert len(audio) == 6 and all(x["equal_to_control"] and x["samples"] == 352000 and x["underruns"] == 0 for x in audio)

out = dict(status="passed", head=HEAD, tracked_files_unchanged=True, untracked_files_only_in_audit=True,
           all_files_under_1MB=True, json_count=json_count, files=files, links=links,
           commands=commands, suites=[43, 42], historical_scenarios=5,
           kernel_cases_per_mode=575, extended_direct=540, extended_periodic=12,
           final_probe_combinations=22, historical_comparisons=12, default_identical=6,
           identical_wav_files=6, findings_by_severity={"严重":0,"高":0,"中":1,"低":0},
           note="仅检查交付及只读边界；不替代软件测试，不声称关闭历史F-6或其他范围外事项。")
(HERE / "artifact-validation.json").write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n")
print(json.dumps({k: out[k] for k in ["status", "head", "json_count", "all_files_under_1MB", "suites", "final_probe_combinations", "findings_by_severity"]}, ensure_ascii=False))
