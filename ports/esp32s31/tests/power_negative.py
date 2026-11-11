#!/usr/bin/env python3
"""Recompile one mutated driver, then run the actual board/HAL regression test."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
VENDOR = ROOT / "ports/esp32s31/vendor/power_874538e"


def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr[-2000:])


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    database = json.loads((build / "compile_commands.json").read_text())
    cases = [
        ("raw-soc", "bq27220", "gauge", None,
         "if (ok) out->soc = level;",
         "if (ok) ok = bq27220_read(dev, BQ27220_STATE_OF_CHARGE, &out->soc) == HAL_I2C_OK;"),
        ("rtc-vl", "pcf8563", "rtc", None, " || !trusted", ""),
        ("rtc-stop", "pcf8563", "rtc", "rtc.inc", "uint8_t ctrl1 = 0x00;", "uint8_t ctrl1 = 0x20;"),
        ("rtc-calendar", "pcf8563", "rtc", None, "rtc_tm.tm_hour > 23", "false"),
        ("rtc-flags", "pcf8563", "rtc", "rtc.inc", "~(kBitAf | kBitTf)", "~kBitAf"),
    ]
    for name, chip, test_case, adapter, old, new in cases:
        original = ROOT / f"drivers/chips/{chip}/{chip}.c"
        entry = next(e for e in database if Path(e["file"]) == original and "test-s31-vendor-power.dir" in str(e))
        with tempfile.TemporaryDirectory(prefix="power-negative-", dir=build) as directory:
            scratch = Path(directory)
            shutil.copytree(VENDOR, scratch / "vendor")
            source = scratch / original.name
            text = original.read_text().replace("../../../ports/esp32s31/vendor/power_874538e", (scratch / "vendor").as_posix())
            source.write_text(text)
            changed = scratch / "vendor" / adapter if adapter else source
            text = changed.read_text()
            assert text.count(old) == 1, (name, old)
            changed.write_text(text.replace(old, new))
            compile_args = entry.get("arguments") or shlex.split(entry["command"])
            obj_index = compile_args.index("-o") + 1
            old_object = compile_args[obj_index]
            compile_args[obj_index] = str(scratch / "driver.o")
            compile_args = [str(source) if a == str(original) else a for a in compile_args]
            run(compile_args, build)
            link = shlex.split((build / "CMakeFiles/test-s31-vendor-power.dir/link.txt").read_text())
            assert old_object in link
            link[link.index(old_object)] = str(scratch / "driver.o")
            link[link.index("-o") + 1] = str(scratch / "negative")
            run(link, build)
            result = subprocess.run([str(scratch / "negative"), test_case], capture_output=True, text=True)
            assert result.returncode != 0 and "Assertion" in result.stderr, (name, result.stdout, result.stderr)
            print(f"{name}: expected failure, exit={result.returncode}; {result.stderr.strip()[:220]}")
    print("5/5 reverse controls failed at their assertions; production sources unchanged")


if __name__ == "__main__":
    main()
