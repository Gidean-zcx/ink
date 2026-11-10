"""Real touch UI, deadlines, synthetic member data and persisted restart."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--negative', action='store_true', help='manual reverse comparison only')
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(parents=True, exist_ok=True)
area = Path(tempfile.mkdtemp(prefix='reminder-edit-', dir=build / 'test-out'))
(area / 'sd').mkdir()
for phase in ('create',) if a.negative else ('create', 'restart'):
    out = area / phase
    cmd = [str(build / 'sim-reminder-edit'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
           '--end', '300s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
           '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')]
    result = subprocess.run(cmd, env={**os.environ, 'INK_REMINDER_PHASE': phase}, capture_output=True, text=True, timeout=180)
    (area / f'{phase}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    metrics = data['metrics']
    passed = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0)
    print(f'{phase}: exit={result.returncode}, pass={metrics.get("test.pass", 0)}, fail={metrics.get("test.fail", 0)}, finished={metrics.get("test.finished", 0)}', flush=True)
    if a.negative:
        assert not passed and metrics.get('test.fail', 0) > 0, 'reverse comparison must fail'
    elif not passed:
        print('\n'.join(str(w) for w in data.get('warnings', []) if 'CHECK FAILED' in str(w)), flush=True)
        raise SystemExit(f'FAIL; evidence: {area.relative_to(ROOT)}\n{result.stderr[-1500:]}')
    if phase == 'create' and passed:
        assert metrics.get('time.reminders_fired') == 2, 'both real reminders fire exactly once'
        assert metrics.get('notify.alerted', 0) >= 2 and metrics.get('audio.speaker.played_ms', 0) >= 120, 'both reminders use the audible notification path'
        screenshots = build / 'reminder-edit'
        screenshots.mkdir(exist_ok=True)
        for name in ('add', 'detail', 'modified', 'picker'):
            shutil.copyfile(out / f'snap-{name}.png', screenshots / f'{name}.png')
if a.negative or os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', area.relative_to(ROOT))
else:
    shutil.rmtree(area)
print('PASS: reverse comparison fails as expected' if a.negative else 'PASS: manual editing, AI interoperability, member isolation, ringing and restart')
