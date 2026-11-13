#!/usr/bin/env python3
"""tools/card_files.py (auto-provision-1): what it refuses, a header that does not change, verify, which Python makes
the font, and the real build's list against its header. No fontTools needed."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/card_files.py"
sys.path.insert(0, str(ROOT / "tools"))
import card_files  # noqa: E402


def tool(*args):
    return subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, text=True)


def manifest(where, entries):
    for e in entries:
        e.setdefault("size", 10)
        e.setdefault("sha256", hashlib.sha256(e["path"].encode()).hexdigest())
        e.setdefault("app", "mogo")
        e.setdefault("file", "files/0")
    where.write_text(json.dumps({"version": 1, "digest": card_files.digest(entries), "entries": entries}))
    return where


def check(cond, what):
    if not cond:
        raise SystemExit(f"FAIL: {what}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", type=Path)
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        out = tmp / "ink/card_files_manifest.h"
        refused = {
            "outside /content/fonts": [{"path": "/user/sys/x"}],
            "..": [{"path": "/content/../x"}],
            "a member's folder": [{"path": "/content/.m/owner/x"}],
            ".part": [{"path": "/content/fonts/x.part"}],
            "twice": [{"path": "/content/fonts/a"}, {"path": "/content/fonts/a"}],
            "over 5 MiB": [{"path": "/content/fonts/a", "size": 5 * 1024 * 1024 + 1}],
            "9 entries": [{"path": f"/content/fonts/f{n}"} for n in range(9)],
        }
        for why, entries in refused.items():
            r = tool("header", "--manifest", manifest(tmp / "bad.json", entries), "--out", out)
            check(r.returncode != 0 and not out.exists(), f"header refuses {why}: {r.stdout}{r.stderr}")
            spec = tmp / "spec.json"
            spec.write_text(json.dumps({"version": 1, "entries": [{"path": e["path"], "app": "mogo", "make": "mogo_font"} for e in entries]}))
            if why != "over 5 MiB":  # a size exists only once the file is made
                r = tool("bundle", "--spec", spec, "--noto", tmp / "none.ttc", "--out", tmp / "bundle")
                check(r.returncode != 0 and "card files:" in r.stderr, f"bundle refuses {why} before making anything: {r.stderr}")
        good = manifest(tmp / "good.json", [{"path": "/content/fonts/a.ttf"}, {"path": "/content/.system/b", "size": 4}])
        check(tool("header", "--manifest", good, "--out", out).returncode == 0, "a good list")
        first = out.read_bytes()
        out.unlink()
        check(tool("header", "--manifest", good, "--out", out).returncode == 0 and out.read_bytes() == first, "the same header twice")
        check(tool("verify", "--manifest", good, "--header", out).returncode == 0, "verify: they agree")
        text = first.decode()
        digest = json.loads(good.read_text())["digest"]
        for why, changed in (("digest", text.replace(digest, "0" * 16)), ("an entry", text.replace(" 4u,", " 5u,"))):
            bad = tmp / "bad.h"
            bad.write_text(changed)
            check(changed != text and tool("verify", "--manifest", good, "--header", bad).returncode != 0, f"verify sees {why}")
        no, yes = tmp / "no-fonttools", tmp / "has-fonttools"
        no.write_text("#!/bin/sh\nexit 1\n")
        yes.write_text("#!/bin/sh\nexit 0\n")
        os.chmod(no, 0o755)
        os.chmod(yes, 0o755)
        r = tool("python", no, yes)
        check(r.returncode == 0 and r.stdout.strip() == str(yes), f"INK_FONT_PYTHON: the second when the first has no fontTools: {r.stdout}")
        check(tool("python", no, tmp / "missing").returncode != 0, "INK_FONT_PYTHON: none at all fails")
    if a.build:
        r = tool("verify", "--manifest", a.build / "card-files/manifest.json", "--header", a.build / "generated/ink/card_files_manifest.h")
        check(r.returncode == 0, f"the build's list and header: {r.stdout}{r.stderr}")
        print(r.stdout.strip())
    print("card files tool: refusals, stable header, verify, Python choice: PASS")


if __name__ == "__main__":
    main()
