"""Run an audit10 regression with synthetic data and optional retained evidence."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--case", choices=("b10-01", "b10-02"), required=True)
parser.add_argument("--binary", type=Path)
args = parser.parse_args()
build = args.build.resolve()
binary = args.binary.resolve() if args.binary else build / ("sim-audit10-" + args.case)
out = build / "test-out" / binary.name
shutil.rmtree(out, ignore_errors=True)
out.mkdir(parents=True)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(out), "--flash-image", str(out / "user.img"), "--sd-dir", str(out / "sd"),
           "--end", "40s", "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                        env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
(out / "driver.log").write_text(result.stdout + result.stderr)
report = json.loads((out / "metrics.json").read_text())
metrics = report["metrics"]
passed = result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0) and not report.get("warnings")
print(f'{args.case}: {"PASS" if passed else "FAIL"} exit={result.returncode} '
      f'checks={metrics.get("test.pass", 0)} failures={metrics.get("test.fail", 0)}', flush=True)
if not passed:
    raise SystemExit(f'{report.get("warnings")}\n{result.stderr[-3000:]}')
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(out)
