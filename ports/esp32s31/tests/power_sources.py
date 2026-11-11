#!/usr/bin/env python3
"""Check vendor copies; the optional glue count is independent of CTest history."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
VENDOR = ROOT / "ports/esp32s31/vendor/power_874538e"
PRODUCT = ("drivers/chips/bq27220/bq27220.c", "drivers/chips/bq27220/bq27220.h", "drivers/chips/pcf8563/pcf8563.c")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline")
    args = parser.parse_args()
    entries = json.loads((VENDOR / "sources.json").read_text())
    for entry in entries:
        assert hashlib.sha256((VENDOR / entry["path"]).read_bytes()).hexdigest() == entry["sha256"], entry["path"]
    print(f"{len(entries)} unchanged vendor files: SHA-256 verified")
    if args.baseline:
        original = {line.strip() for e in entries for line in (VENDOR / e["path"]).read_text().splitlines()}
        adaptation = 0
        for path in (VENDOR / "binding.h", VENDOR / "gauge.inc", VENDOR / "rtc.inc"):
            count = sum(bool(line.strip()) and line.strip() not in original and not line.lstrip().startswith(("/*", "//", "*"))
                        for line in path.read_text().splitlines())
            adaptation += count
            print(f"{path.name}: {count} new/adapted non-original lines")
        diff = subprocess.check_output(["git", "diff", "--numstat", args.baseline, "--", *PRODUCT], cwd=ROOT, text=True)
        additions = sum(int(line.split("\t")[0]) for line in diff.splitlines())
        total = additions + adaptation
        print(f"product additions={additions}; vendor adaptation={adaptation}; glue={total}/250")
        assert total <= 250


if __name__ == "__main__":
    main()
