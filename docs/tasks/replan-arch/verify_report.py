#!/usr/bin/env python3
"""Offline evidence and document checks; not a product test or build."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
BASE = "d09b79195e2dc57908e33219216048e529e082c6"
PREFIX = "docs/tasks/replan-arch/"
OSS = Path(os.environ["INK_DEPS"]).parent / "ink-hw/upstream/oss"

def git(*args, directory=ROOT):
    return subprocess.check_output(["git", "-C", str(directory), *args], text=True)

def strip_comments(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)

evidence = json.loads((HERE / "source-evidence.json").read_text())
assert evidence["ink_baseline"] == BASE
texts = {}
ids = set()
for item in evidence["files"]:
    project, path, rev = item["project"], item["path"], item["revision"]
    assert item["id"] not in ids
    ids.add(item["id"])
    directory = ROOT if project == "ink" else ROOT / "work/replan-arch/freeink-sdk" if project == "freeink-sdk" else OSS / project
    data = subprocess.check_output(["git", "-C", str(directory), "show", rev + ":" + path])
    if project != "freeink-sdk":
        assert data == (directory / path).read_bytes(), item["id"]
    assert hashlib.sha256(data).hexdigest() == item["sha256"], item["id"]
    assert len(data.splitlines()) == item["lines"]
    assert all(1 <= start <= end <= item["lines"] for start, end in item["ranges"])
    texts[(project, path)] = data.decode()
for project, rev in evidence["revisions"].items():
    if project not in ("ink", "freeink-sdk"):
        assert git("rev-parse", "HEAD", directory=OSS / project).strip() == rev
sdk = git("ls-tree", evidence["revisions"]["crosspoint-reader"], "freeink-sdk",
          directory=OSS / "crosspoint-reader").split()[2]
assert sdk == evidence["revisions"]["freeink-sdk"]

api = evidence["api_inventory"]
cp = strip_comments(texts[("crosspoint-reader", "lib/GfxRenderer/GfxRenderer.cpp")])
assert sorted(set(re.findall(r"\bdisplay\.(\w+)\s*\(", cp))) == api["crosspoint_gfx_haldisplay"]
app = strip_comments(texts[("kiikoread", "main/app/app.h")])
assert re.findall(r"\(\*(\w+)\)", app) == api["kiikoread_app_callbacks"]
epd = set()
for path in ("main/ui/ui_kit.c", "main/book/book_layout.c", "main/apps/app_book.c"):
    epd.update(re.findall(r"\b(epd_\w+)\s*\(", strip_comments(texts[("kiikoread", path)])))
assert sorted(epd) == api["kiikoread_graphics_calls"]
book = strip_comments(texts[("kiikoread", "main/apps/app_book.c")])
assert sorted(set(re.findall(r"\b((?:update_display|display_)\w+)\s*\(", book))) == api["kiikoread_reader_display_calls"]
assert [len(v) for v in api.values()] == [22, 23, 14, 23]
reader = texts[("ink", "apps/reader/reader.c")].split("static const ink_cap_t BOOK_ACTIONS[] = {", 1)[1].split("};", 1)[0]
assert len(re.findall(r"\.id\s*=", reader)) == 14

probe = json.loads((HERE / "probe-result.json").read_text())
assert [item["exit"] for item in probe["runs"]] == [1, 0, 1, 0]
assert "cst836u.h" in probe["runs"][0]["output"]
assert "read_pico_transfer_get_status" in probe["runs"][2]["output"]
assert probe["runs"][1]["output"] == ""
assert probe["compat_header_lines"] == len((HERE / "probe/compat.h").read_text().splitlines()) == 55
assert len(probe["undefined_symbols"]) == 34
for path, info in probe["sources"].items():
    item = next(item for item in evidence["files"] if item["project"] == "kiikoread" and item["path"] == path) if path.endswith(".c") else None
    if item:
        assert item["sha256"] == info["sha256"]
    data = (OSS / "kiikoread" / path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == info["sha256"]

report = (HERE / "REPORT.md").read_text()
for title in ("结论", "验收表", "没做到的", "和设计不一致的", "改过的已有测试", "要用户定的", "顺手发现", "新增的机制", "要改的文档"):
    assert "## " + title in report
assert all("| A" + str(i) + " " in report for i in range(1, 8))
assert "待核查" not in report and "待执行" not in report
links = 0
for markdown in HERE.rglob("*.md"):
    for target in re.findall(r"\]\(([^)]+)\)", markdown.read_text()):
        if "://" in target or target.startswith("#"):
            continue
        dest = markdown.parent / unquote(target.split("#", 1)[0])
        assert dest.exists() or dest == HERE / "verification.txt", (markdown.name, target)
        links += 1

changed = set(git("diff", "--name-only", BASE).splitlines())
changed.update(git("ls-files", "--others", "--exclude-standard").splitlines())
assert all(path.startswith(PREFIX) for path in changed), sorted(changed)
files = [path for path in HERE.rglob("*") if path.is_file()]
assert all(path.stat().st_size <= 5 * 1024 * 1024 for path in files)
private_pattern = re.compile(r"/" + r"Users/|/" + r"private/tmp|(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}")
commit_diff = git("diff", BASE + "..HEAD")
assert not private_pattern.search(commit_diff), "privacy match in committed diff"
assert all(not private_pattern.search(path.read_text()) for path in files), "privacy match in task files"
check = subprocess.run(["git", "diff", "--check", BASE], cwd=ROOT, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
assert check.returncode == 0, check.stdout
assert 1216 * 684 == 831744
assert 5 * 1024 * 1024 * .08 == 419430.4

lines = [
    "C2/C3 verification: PASS (documents and compile evidence only)",
    "Baseline: " + BASE,
    "Source hashes and cited line bounds: " + str(len(evidence["files"])) + " PASS",
    "Fixed OSS heads and CrossPoint SDK pin: PASS; existing snapshots unchanged",
    "Active API counts: CrossPoint display 22; kiikoread callbacks 23 / graphics 14 / display 23",
    "Existing reader body capabilities: 14",
    "Probe evidence: source hashes PASS; compiler exits 1 / 0 / 1 / 0; 34 unresolved symbols",
    "Probe limitation: compile only, no service implementation, link, rendering or device run",
    "Local document links: " + str(links) + " PASS",
    "Changed file scope: task directory only; product code 0 lines; existing tests unchanged",
    "Individual task files <= 5 MiB: PASS",
    "Privacy pattern scan, baseline..HEAD and task files: 0 matches",
    "git diff --check baseline: exit 0",
    "Resource arithmetic: PASS; 8% is the task assumption, not a measured image",
    "Main build / full ctest / ASan / UBSan / S31 / gcccheck.py: NOT RUN (research task exemption)",
    "Commands: taskpolicy -c utility python3 docs/tasks/replan-arch/probe/run.py",
    "Commands: taskpolicy -c utility python3 docs/tasks/replan-arch/verify_report.py",
    "Commands: git diff --check baseline; git diff --stat baseline..HEAD; privacy scan of git diff baseline..HEAD",
]
output = "\n".join(lines) + "\n"
(HERE / "verification.txt").write_text(output)
print(output, end="")
