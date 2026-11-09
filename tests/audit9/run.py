"""Run one boundary driver and retain frames when INK_KEEP_EVIDENCE is set."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--case", choices=("bt-ai", "ime-layout"), required=True)
parser.add_argument("--binary", type=Path)
args = parser.parse_args()
build = args.build.resolve()
binary = args.binary.resolve() if args.binary else build / ("sim-audit9-" + args.case)
area = build / "test-out" / ("audit9-" + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(area), "--end", "60s", "--quiet", "--no-random-outages",
           "--budget", str(ROOT / "tests/ui_scene.budget.json")]
if args.case == "bt-ai":
    command += ["--script", str(ROOT / "tests/bluetooth.script")]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                        env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
(area / "driver.log").write_text(result.stdout + result.stderr)
report = json.loads((area / "metrics.json").read_text())
metrics = report["metrics"]
assert result.returncode == 0 and metrics.get("test.finished") == 1 and metrics.get("test.fail", 0) == 0, (
    result.returncode, report.get("warnings"), result.stderr[-2000:])
assert not report.get("warnings"), report["warnings"]
print(f'{args.case}: PASS checks={metrics.get("test.pass")}')
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
