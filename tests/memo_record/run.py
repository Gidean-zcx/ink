"""Voice memos: the main run, then a power cut while recording and the boot after it (same flash and card)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--phase", choices=("main", "cut", "cost"))
args = parser.parse_args()
build = args.build.resolve()
out_root = build / "test-out" / "memo-record"
shutil.rmtree(out_root, ignore_errors=True)
out_root.mkdir(parents=True)
runs = [args.phase] if args.phase else ["main", "cut"]
ok = True
for run in runs:
    phases = ["cut", "after"] if run == "cut" else [run]
    for phase in phases:
        out = out_root / phase
        result = subprocess.run([str(build / "sim-memo-record"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
            "--script", str(ROOT / "tests/memo_record/memo_record.script"), "--budget", str(ROOT / "tests/memo_record/budget.json"),
            "--assets", str(build / "assets"), "--content", str(build / "test-content"),
            "--flash-image", str(out_root / (run + ".img")), "--sd-dir", str(out_root / (run + "-sd")),
            "--out", str(out), "--quiet", "--no-random-outages"],
            env={**os.environ, "INK_MR_PHASE": phase}, capture_output=True, text=True, timeout=600)
        data = json.loads((out / "metrics.json").read_text(errors="replace")) if (out / "metrics.json").exists() else {}
        counts = data.get("metrics", {})
        passed = counts.get("test.finished") == 1 and not counts.get("test.fail", 0) and (phase == "cut" or result.returncode == 0)
        print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
              "failures", counts.get("test.fail", 0), flush=True)
        if not passed:
            for warning in data.get("warnings", []): print(warning)
            print(result.stdout[-2000:], result.stderr[-3000:])
            ok = False
            break
raise SystemExit(0 if ok else 1)
