"""Long text messages (sms-multipart): receive, restart in the middle, send, compose; INK_KEEP_EVIDENCE keeps the pictures."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = (a.binary or build / 'sim-sms-multipart').resolve()
area = build / 'test-out' / ('sms-multipart' if not a.binary else 'sms-multipart-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
pictures = {'rx': 'incomplete', 'compose': 'compose-parts'}
for phase in ('rx', 'restart-1', 'restart-2', 'tx', 'compose'):
    image = area / ('restart.img' if phase.startswith('restart') else phase + '.img')
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/sms_multipart' / ('sms_multipart.script' if phase == 'rx' else 'short.script')), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / phase),
        '--flash-image', str(image), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=300, env={**os.environ, 'INK_SMS_PHASE': phase})
    (area / (phase + '.log')).write_text(result.stdout + result.stderr)
    path = area / phase / 'metrics.json'
    data = json.loads(path.read_text()) if path.exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    if phase in pictures and not (area / phase / f'snap-{pictures[phase]}.png').exists():
        raise SystemExit(f'{phase}: no {pictures[phase]} picture')
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
