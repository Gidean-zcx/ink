#!/usr/bin/env python3
# coding: utf-8
"""Check this research packet offline; never import or execute upstream code."""
import argparse
import json
import re
import subprocess
from pathlib import Path

packet = Path(__file__).resolve().parent
repo = packet.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--baseline", help="Optional local history check, not a ctest dependency")
args = parser.parse_args()
sources = json.loads((packet / "source-evidence.json").read_text(encoding="utf-8"))
registry = json.loads((packet / "registry-evidence.json").read_text(encoding="utf-8"))
assert registry["business_code"] == 0
records = registry["records"]
assert len(records) == registry["reported_total"] == 53
assert len({r["id"] for r in records}) == 53
assert {s["id"] for s in sources["sources"]} == {f"S{i:02d}" for i in range(1, 23)}
files = {}
archive_files = {}
for source in sources["sources"]:
    for entry in source.get("files", []):
        assert re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])
        files[entry["url"]] = entry["lines"]
    for archive in source.get("archives", []):
        assert archive["name"].endswith("source.zip")
        assert re.fullmatch(r"[0-9a-f]{64}", archive["sha256"])
        assert archive["member_count"] in (142, 203)
        archive_files.update({f["path"]: f["lines"] for f in archive["files"]})
assert sources["vendor_head_before"] == sources["vendor_head_after"]
assert len(sources["moji_launcher_lineage"]["identical_paths"]) == 220
anchors = 0
archive_anchors = 0
for path in packet.glob("*.md"):
    text = path.read_text(encoding="utf-8")
    for name, start, end in re.findall(r"`([^`]+):(\d+)–(\d+)`", text):
        if name in archive_files:
            assert 1 <= int(start) <= int(end) <= archive_files[name], name
            archive_anchors += 1
    for target in re.findall(r"\]\(([^\s)]+)\)", text):
        if target.startswith(("https://", "http://")):
            if "#L" in target:
                base, fragment = target.split("#", 1)
                match = re.fullmatch(r"L(\d+)-L(\d+)", fragment)
                assert match and base in files, target
                assert 1 <= int(match[1]) <= int(match[2]) <= files[base], target
                anchors += 1
            continue
        name, _, fragment = target.partition("#")
        dest = path.parent / name if name else path
        assert dest.is_file(), target
        if fragment:
            titles = re.findall(r"^##\s+(.+)$", dest.read_text(encoding="utf-8"), re.M)
            assert fragment in {t.lower() for t in titles}, target
    assert "待完成" not in text
assert anchors == 96, anchors
assert archive_anchors == 7, archive_anchors
for path in packet.iterdir():
    if path.is_file():
        assert path.stat().st_size < 5_000_000
report = (packet / "REPORT.md").read_text(encoding="utf-8")
for name in ("CrossMux", "CrossInk", "Mogo", "PageRest", "Pixel", "小智畅享版",
             "墨集", "固件口袋", "lnkboot", "BoBoOS", "ModernClassicOS", "布丁",
             "ThingOS", "录音机", "kiikoread", "AgentUI", "ESP-ADF", "ESP-Brookesia",
             "五子棋", "贪吃蛇", "记忆", "Eink4Boy", "Peanut-GB", "retro-go", "ardmos", "森林"):
    assert name in report, name
if args.baseline:
    def git(*parts):
        return subprocess.run(["git", *parts], cwd=repo, check=True, capture_output=True).stdout
    changed = git("diff", "--name-only", args.baseline).decode().splitlines()
    assert changed and all(p.startswith("docs/tasks/replan-media/") for p in changed), changed
    diff = git("diff", args.baseline).decode()
    private = re.compile("/" + "Users/|/" + "private/tmp|" + r"[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}")
    assert not private.search(diff), "Privacy pattern found in baseline diff"
    print("Scope: task directory only; product code 0 lines; privacy matches 0")
print("Registry: 53 unique records; all named candidates covered")
print("Sources: 22 fixed sources; 96 file/line anchors verified; 2 source ZIPs")
print("Links, JSON, file sizes, vendor HEAD and 220 matching blobs: PASS")
print("No product builds, tests, ASan, S31 compile or gcccheck run (research exemption)")
