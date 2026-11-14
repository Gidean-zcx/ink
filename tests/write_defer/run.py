"""write-defer-1: typing at 3 and 1 keys a second while drafts and logs wait (type); a power cut after the hands stop
(pause) and one while typing without a pause (run), each followed by the boot after it on the same flash image."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--phase", choices=("type", "pause", "run"))
args = parser.parse_args()
build = args.build.resolve()
out_root = build / "test-out" / "write-defer"
shutil.rmtree(out_root, ignore_errors=True)
out_root.mkdir(parents=True)
ok = True
for phase in ([args.phase] if args.phase else ["type", "pause", "run"]):
    for step in ([phase] if phase == "type" else ["cut-" + phase, "after-" + phase]):
        out = out_root / step
        result = subprocess.run([str(build / "sim-write-defer"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
            "--script", str(ROOT / "tests/write_defer/typing.script"), "--budget", str(ROOT / "tests/write_defer/budget.json"),
            "--assets", str(build / "assets"), "--content", str(build / "test-content"),
            "--flash-image", str(out_root / (phase + ".img")), "--out", str(out), "--quiet", "--no-random-outages",
            "--flash-timing", "max"], env={**os.environ, "INK_WD_PHASE": step}, capture_output=True, text=True, timeout=600)
        data = json.loads((out / "metrics.json").read_text(errors="replace")) if (out / "metrics.json").exists() else {}
        counts = data.get("metrics", {})
        passed = counts.get("test.finished") == 1 and not counts.get("test.fail", 0) and (step.startswith("cut") or result.returncode == 0)
        print(step, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
              "failures", counts.get("test.fail", 0), "keys kept", counts.get("test.write_defer.keys_kept", "-"), flush=True)
        if not passed:
            for warning in data.get("warnings", []):
                print(warning)
            print(result.stdout[-2000:], result.stderr[-3000:])
            ok = False
            break
    if not ok:
        break
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(out_root, ignore_errors=True)
raise SystemExit(0 if ok else 1)
