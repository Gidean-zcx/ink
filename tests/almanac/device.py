"""Run almanac capability calls against a fresh simulated device, never hardware."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'almanac-evidence'
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
r = subprocess.run([str(build / 'sim-almanac'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                    '--script', str(ROOT / 'tests/almanac/scenario.script'),
                    '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
                    '--content', str(build / 'test-content'), '--out', str(area),
                    '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
                    '--quiet', '--no-random-outages'], capture_output=True, text=True, timeout=180,
                   env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(area / 'run.log').write_text(r.stdout + r.stderr)
data = json.loads((area / 'metrics.json').read_text()) if (area / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
print(f'exit={r.returncode}, pass={counts.get("test.pass",0)}, fail={counts.get("test.fail",0)}, finished={counts.get("test.finished",0)}')
for line in r.stdout.splitlines():
    if line.startswith('ALMANAC '):
        print(line)
if r.returncode or counts.get('test.fail', 0) or counts.get('test.finished') != 1 or data.get('warnings'):
    raise SystemExit(r.stderr[-9000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
