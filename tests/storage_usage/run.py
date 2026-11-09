"""Run real storage capabilities and the production settings presenter."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-storage-usage'
area = build / 'test-out' / ('storage-usage-' + binary.name)
if area.exists():
    shutil.rmtree(area)
out = area / 'device'
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/settings_a.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--no-random-outages', '--quiet'], cwd=ROOT, capture_output=True, text=True, timeout=150,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
out.mkdir(parents=True, exist_ok=True)
(out / 'stdout.log').write_text(result.stdout)
(out / 'stderr.log').write_text(result.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
    raise SystemExit(f'FAIL exit={result.returncode}\n{result.stderr[-8000:]}\n{data.get("warnings", [])}')
target = build / 'storage-compare' / 'SetStorage'
target.mkdir(parents=True, exist_ok=True)
shutil.copyfile(out / 'snap-scene.png', target / 'snap-scene.png')
shutil.copyfile(out / 'metrics.json', target / 'metrics.json')
print(f'PASS checks={counts.get("test.pass")} exact categories, member isolation, indexing, cleanup, actual touch, AI, child gate')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
