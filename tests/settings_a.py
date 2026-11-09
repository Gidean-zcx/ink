"""Two processes share the same synthetic flash to verify saved settings."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-settings-a'
area = build / 'test-out' / ('settings-a-' + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for phase in ('write', 'restart'):
    out = area / phase
    env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
    if phase == 'restart':
        env['INK_SETTINGS_RESTART'] = '1'
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/settings_a.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--no-random-outages', '--quiet'],
        cwd=ROOT, capture_output=True, text=True, env=env, timeout=90)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-2000:]}\n{data.get("warnings", [])}')
    if phase == 'write':
        snapshots = [json.loads(line) for line in (out / 'snapshots.ndjson').read_text().splitlines()]
        assert {item['page'] for item in snapshots} == {'SetVoice', 'SetStorage', 'SetAbout', 'SetPrivacy'}
        for item in snapshots:
            assert item['fields'] and item['pagination']['visible'] == len(item['fields'])
            for action in item['actions']:
                assert isinstance(json.loads(action['args_json']), dict)
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
