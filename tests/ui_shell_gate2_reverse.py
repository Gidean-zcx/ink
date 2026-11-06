#!/usr/bin/env python3
"""Check the gate-2 regressions against isolated copies of the merged baseline."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "9ef1958b"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", default="build")
args = parser.parse_args()
build = (ROOT / args.build).resolve()
work = Path(tempfile.mkdtemp(prefix="ui-shell-gate2-reverse-", dir=build))


def run(label, command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                            env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
    output = result.stdout + result.stderr
    (work / (label + ".log")).write_text(output)
    return result.returncode, output


design = work / "system-services-design.md"
design.write_bytes(subprocess.check_output(
    ["git", "show", BASELINE + ":docs/system/system-services-design.md"], cwd=ROOT))
code, output = run("capability-doc", [sys.executable, str(ROOT / "tests/services_mid/check_capability_doc.py"),
                                      "--design", str(design)])
if code != 1 or not all(value in output for value in (
        "FOR_AI: checked=99 missing=4", "NOT_FOR_AI: checked=7 missing=1",
        "ai.panel.hide", "ai.panel.listen", "ai.panel.next", "ai.panel.prev", "confirm.page")):
    raise SystemExit("The old capability table did not reproduce the five omissions")
print("capability-doc: baseline rejected for all five missing entries (exit 1)", flush=True)

code, output = run("keyboard-snapshot", [sys.executable, str(ROOT / "tests/services_finish/run.py"),
                                         "--build", str(build), "--baseline", BASELINE,
                                         "keyboard_snapshot"])
if code != 1 or "keyboard_snapshot FAIL exit=0" not in output or "Evidence directory: " not in output:
    raise SystemExit("The old UI snapshot did not complete the failing regression")
evidence = ROOT / output.split("Evidence directory: ", 1)[1].splitlines()[0]
metrics = json.loads((evidence / "keyboard_snapshot-out/metrics.json").read_text())
if metrics["metrics"].get("test.finished") != 1 or metrics["metrics"].get("test.fail", 0) < 9:
    raise SystemExit("The old UI snapshot did not finish with the original nine failures")
for message in ("page and input share one snapshot", "input target identified", "password remains masked",
                "AI request retains the same page and keyboard snapshot",
                "AI panel keeps the underlying page and input context"):
    if not any(message in warning for warning in metrics.get("warnings", [])):
        raise SystemExit("The old UI snapshot did not fail: " + message)
print("keyboard-snapshot: baseline completed and failed the page, target, masking and AI context checks", flush=True)
print("PASS: both gate-2 reverse checks rejected the old behavior")
print("Evidence directory:", work.relative_to(ROOT))
