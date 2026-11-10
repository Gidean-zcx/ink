"""Airplane mode and power saving across restarts: write, restart, restart again on the same flash."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-airplane-mode'
area = build / 'test-out' / ('airplane-mode' if not a.binary else 'airplane-mode-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
for phase in ('write', 'restart', 'again'):
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/airplane_mode.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / phase),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=240,
        env={**os.environ, 'INK_AIRPLANE_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / (phase + '.log')).write_text(result.stdout + result.stderr)
    path = area / phase / 'metrics.json'
    data = json.loads(path.read_text()) if path.exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    shots = {'write': ('control', 'statusbar', 'network', 'call', 'sms', 'ai'), 'restart': ('restart',), 'again': ()}[phase]
    for shot in shots:
        if counts.get('test.snapshot.' + shot) != 1 or not (area / phase / (shot + '.json')).exists():
            raise SystemExit(f'{phase}: no {shot} snapshot')
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
