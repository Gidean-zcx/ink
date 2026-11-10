"""Run one batch12 audit regression (synthetic data) and keep the evidence only when asked."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--case", choices=("m1", "m2", "m3", "l7"), required=True)
parser.add_argument("--binary", type=Path)
args = parser.parse_args()
build = args.build.resolve()
binary = args.binary.resolve() if args.binary else build / "sim-audit12-fix"
out = build / "test-out" / ("audit12-" + args.case)
shutil.rmtree(out, ignore_errors=True)
out.mkdir(parents=True)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(out), "--flash-image", str(out / "user.img"), "--sd-dir", str(out / "sd"),
           "--end", "180s", "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=240,
                        env={**os.environ, "INK_AUDIT12_CASE": args.case, "UBSAN_OPTIONS": "halt_on_error=1"})
(out / "driver.log").write_text(result.stdout + result.stderr)
path = out / "metrics.json"
report = json.loads(path.read_text(errors="replace")) if path.exists() else {}
metrics = report.get("metrics", {})
passed = result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0) and not report.get("warnings")
print(f'{args.case}: {"PASS" if passed else "FAIL"} exit={result.returncode} '
      f'checks={metrics.get("test.pass", 0)} failures={metrics.get("test.fail", 0)}', flush=True)
if not passed:
    raise SystemExit(f'{report.get("warnings")}\n{result.stderr[-3000:]}')
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(out)
