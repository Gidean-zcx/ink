"""Exercise the real settings pages across two simulator processes."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-settings-b'
area = build / 'test-out' / ('settings-b-' + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for phase in ('write', 'restart'):
    out = area / phase
    env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
    if phase == 'restart':
        env['INK_SETTINGS_B_RESTART'] = '1'
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/settings_b.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, env=env, timeout=180)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    if phase == 'write':
        for page in ['SetMembers', 'SetKid', 'Wallpaper', 'Records', 'AiMemory']:
            snap = json.loads((out / f'{page}.json').read_text())
            assert snap['page'] == page and snap['fields']
        before = Image.open(out / 'snap-HomeBefore.png').convert('L')
        after = Image.open(out / 'snap-HomeAfter.png').convert('L')
        assert before.tobytes() == after.tobytes(), 'selecting a wallpaper must leave home unchanged'
        locks = [Image.open(out / f'snap-{name}.png').convert('L') for name in ('LockPage', 'LockClock', 'LockImage')]
        assert len({im.tobytes() for im in locks}) == 3, 'three lock styles have different screenshots'
        assert len(locks[2].crop((10, 400, 30, 800)).getcolors(256) or []) > 2, 'wallpaper is visible'
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
