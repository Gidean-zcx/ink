"""Exercise Bluetooth settings without changing the existing headset model."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-bluetooth'
area = build / 'test-out' / ('bluetooth' if not a.binary else 'bluetooth-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
for phase in ('write', 'restart'):
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/bluetooth.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / phase),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=180,
        env={**os.environ, 'INK_BT_RESTART': str(int(phase == 'restart')), 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / (phase + '.log')).write_text(result.stdout + result.stderr)
    path = area / phase / 'metrics.json'
    data = json.loads(path.read_text()) if path.exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
