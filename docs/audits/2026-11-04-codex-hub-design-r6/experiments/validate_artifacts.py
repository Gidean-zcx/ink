"""Validate this audit's deliverables without changing any other directory."""

import ast
import json
import re
import subprocess
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
PREFIX = AUDIT.relative_to(ROOT).as_posix() + "/"
REVIEW = "4ba1656c7d040cbb2e63d25875c8111fe5e2ce8f"
CODE = "e57fbc5b771f9c3cf5194412d6c868fff6679627"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True)


assert git("rev-parse", "HEAD").strip() == REVIEW
# An attached/detached change would be visible here without mutating a ref.
branch = subprocess.run(["git", "symbolic-ref", "--quiet", "--short", "HEAD"],
                        cwd=ROOT, capture_output=True, text=True)
assert branch.returncode == 1  # detached HEAD, as at the start
assert not git("diff", "--name-only", "HEAD", "--")
whitespace = subprocess.run(["git", "diff", "--check"], cwd=ROOT,
                            capture_output=True, text=True)
assert whitespace.returncode == 0, whitespace.stdout + whitespace.stderr

status = git("status", "--porcelain=v1", "--untracked-files=all", "-z")
changes = [entry for entry in status.split("\0") if entry]
assert all(entry[:3] == "?? " and entry[3:].startswith(PREFIX) for entry in changes)

findings = json.loads((AUDIT / "findings.json").read_text())
verification = json.loads((AUDIT / "verification.json").read_text())
results = json.loads((HERE / "results.json").read_text())
report = (AUDIT / "report.md").read_text()
assert isinstance(findings, list) and isinstance(verification, list)
fkeys = {"id", "severity", "area", "title", "evidence_type", "locations", "impact", "recommendation"}
vkeys = {"id", "verdict", "evidence", "note"}
assert all(set(f) == fkeys for f in findings)
assert all(set(v) == vkeys for v in verification)
assert [f["id"] for f in findings] == ["HUB6-001", "HUB6-002"]
assert [v["id"] for v in verification] == ["HUB5-001", "HUB5-002", "HUB4-003"]
assert Counter(f["severity"] for f in findings) == {"严重": 1, "高": 1}
assert Counter(v["verdict"] for v in verification) == {"已解决": 3}
assert re.findall(r"^### (HUB6-\d{3})｜", report, re.M) == [f["id"] for f in findings]
assert "严重 1 条、高 1 条" in report
assert "已解决 3、部分解决 0、未解决 0、改出了新问题 0" in report
assert "状态：完成" in report and results["review_commit"] == REVIEW
assert not any(s in report for s in ["待完成处理", "待核查；", "待核实", "状态：审查中", "尚未执行实验"])

checked_refs = []
for f in findings:
    for location in f["locations"]:
        path, point = location.rsplit(":", 1)
        text = (ROOT / path).read_text()
        if re.fullmatch(r"\d+(?:-\d+)?", point):
            nums = [int(x) for x in point.split("-")]
            assert 1 <= min(nums) <= max(nums) <= len(text.splitlines()), location
        else:
            assert point in json.loads(text), location
        checked_refs.append(location)

# Every fully spelled out file:line reference in the report must resolve.
for path, line in re.findall(r"`((?:docs|system)/[A-Za-z0-9_./-]+):(\d+)`", report):
    text = git("show", CODE + ":" + path) if path.startswith("system/") else (ROOT / path).read_text()
    assert 1 <= int(line) <= len(text.splitlines()), (path, line)
    checked_refs.append(path + ":" + line)

anchors = [
    ("docs/hub/hub-architecture.md", 413, "人确认之前不取最终的快照和日志"),
    ("docs/hub/device-protocol.md", 301, "有就回 `manifest`"),
    ("docs/hub/hub-architecture.md", 1019, "令牌还绑定当时的 `hub_instance`"),
    ("docs/hub/hub-architecture.md", 1493, "这一轮是人开口或打字发起的"),
    ("docs/hub/hub-architecture.md", 1494, "中枢拿转写文字和联系人比对"),
    ("docs/hub/hub-architecture.md", 1495, "要发的内容里有没有这些文字"),
    ("docs/hub/device-protocol.md", 178, "高风险的能力（`ask_by_hand`）一律弹卡"),
]
for path, line, fragment in anchors:
    assert fragment in (ROOT / path).read_text().splitlines()[line - 1]

links_checked = []
files_checked = []
destination = HERE / "validation.json"
for path in sorted(AUDIT.rglob("*")):
    if not path.is_file():
        continue
    assert path.stat().st_size < 1_000_000, path
    if path.name == "TASK.md":
        continue  # existing task is read-only and was not authored here
    text = path.read_text()
    assert text.endswith("\n"), path
    assert all(line.rstrip() == line for line in text.splitlines()), path
    if path.suffix == ".json":
        json.loads(text)
    elif path.suffix == ".py":
        ast.parse(text, filename=str(path))
    elif path.suffix == ".md":
        for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
            if "://" not in target and not target.startswith("#"):
                resolved = (path.parent / target.split("#", 1)[0]).resolve()
                # This one result is created only after all checks succeed.
                assert resolved.is_file() or resolved == destination, target
                links_checked.append(str(path.relative_to(ROOT)) + " -> " + target)
    if path.name != "validation.json":
        files_checked.append({"path": str(path.relative_to(ROOT)), "bytes": path.stat().st_size})

output = {
    "status": "pass",
    "review_commit": REVIEW,
    "device_code_commit": CODE,
    "verification_counts": dict(Counter(v["verdict"] for v in verification)),
    "finding_counts": dict(Counter(f["severity"] for f in findings)),
    "git_diff_check_exit": whitespace.returncode,
    "tracked_changes": [],
    "all_changes_inside_audit_directory": True,
    "all_files_below_1mb": True,
    "json_schema_and_report_counts": "pass",
    "source_anchors_checked": len(anchors),
    "references_checked": sorted(set(checked_refs)),
    "local_links_checked": links_checked,
    "artifact_sizes_excluding_this_result": files_checked,
}
destination.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n")
assert destination.is_file() and destination.stat().st_size < 1_000_000
print("PASS: schemas, counts, references, local links, syntax, whitespace, file sizes, fixed HEAD, and audit-only writes.")
print(json.dumps({"verification": output["verification_counts"], "findings": output["finding_counts"]}, ensure_ascii=False))
