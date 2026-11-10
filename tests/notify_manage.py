"""Run synthetic notification UI/AI checks and restart on the same flash."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-notify-manage'
area = build / 'notify-manage-evidence' / binary.name
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
frames = ['notifications-zh', 'notifications-en', 'channels-zh', 'channels-en',
          'channels-zh-large', 'channels-en-large']
for phase in ('main', 'restart'):
    out = area / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/notify_manage.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=180,
        env={**os.environ, 'INK_NOTIFY_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    (out / 'run.log').write_text(result.stdout + result.stderr + '\n' + '\n'.join(data.get('warnings', [])))
    counts = data.get('metrics', {})
    print(f'{phase}: exit={result.returncode}, pass={counts.get("test.pass", 0)}, '
          f'fail={counts.get("test.fail", 0)}, finished={counts.get("test.finished", 0)}', flush=True)
    if result.returncode or counts.get('test.fail', 0) or counts.get('test.finished') != 1 or data.get('warnings'):
        raise SystemExit((out / 'run.log').read_text()[-12000:])
    if phase == 'main':
        for name in frames:
            assert (out / f'snap-{name}.png').is_file(), name
            snap = json.loads((out / f'{name}.json').read_text())
            assert snap['page'] == ('Notifications' if name.startswith('notifications') else 'SetNotifications')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
