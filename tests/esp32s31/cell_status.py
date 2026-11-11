"""Headless cellular status captures; fake radio only."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
out = build / 'test-out/cell-status'
shutil.rmtree(out, ignore_errors=True)
r = subprocess.run([str(build / 'sim-cell-status'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/esp32s31/cell_status.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages'],
    cwd=ROOT, capture_output=True, text=True, timeout=180)
(out / 'run.log').write_text(r.stdout + r.stderr)
m = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = m.get('metrics', {})
if r.returncode or counts.get('test.fail', 0) or counts.get('test.finished') != 1 or m.get('warnings'):
    raise SystemExit(f'cell-status failed ({r.returncode}): {r.stdout[-8000:]} {r.stderr[-3000:]} {m.get("warnings", [])}')
for name in ('wifi-4g-strong', 'wifi-4g-weak', 'airplane', 'no-sim', 'no-service'):
    assert (out / f'snap-cell-{name}.png').exists(), name
print(f'cell-status: PASS checks={counts.get("test.pass", 0)}; five screenshots')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(out)
