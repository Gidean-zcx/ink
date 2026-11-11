"""Run real pages, synthetic flash and a restart; keep no successful evidence by default."""
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
p.add_argument('--binary', type=Path, help='manual reverse-control executable')
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(parents=True, exist_ok=True)
area = Path(tempfile.mkdtemp(prefix='calendar-events-', dir=build / 'test-out'))
(area / 'sd').mkdir()
for phase in ('create',) if a.binary else ('create', 'restart'):
    out = area / phase
    cmd = [str((a.binary or build / 'sim-calendar-events').resolve()),
           '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out), '--end', '300s', '--flash-image', str(area / 'user.img'),
           '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages',
           '--budget', str(ROOT / 'tests/ui_scene.budget.json')]
    result = subprocess.run(cmd, env={**os.environ, 'INK_CALENDAR_PHASE': phase},
                            capture_output=True, text=True, timeout=180)
    (area / f'{phase}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    metrics = data['metrics']
    passed = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0)
    print(f'{phase}: exit={result.returncode}, pass={metrics.get("test.pass", 0)}, fail={metrics.get("test.fail", 0)}, finished={metrics.get("test.finished", 0)}', flush=True)
    if not passed:
        print('\n'.join(str(w) for w in data.get('warnings', []) if 'CHECK FAILED' in str(w)), flush=True)
        raise SystemExit(f'FAIL; evidence: {area.relative_to(ROOT)}\n{result.stderr[-1500:]}')
    if phase == 'create':
        assert metrics.get('time.reminders_fired') == 1, 'only audible control fires'
        assert metrics.get('notify.alerted') == 1 and metrics.get('audio.speaker.played_ms', 0) >= 60, 'audible control reaches the real notification audio path'
        screenshots = build / 'calendar-events'
        screenshots.mkdir(exist_ok=True)
        for name in ('month', 'cross-day', 'all-day', 'editor'):
            shutil.copyfile(out / f'snap-{name}.png', screenshots / f'{name}.png')
if a.binary or os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', area.relative_to(ROOT))
else:
    shutil.rmtree(area)
print('PASS: silent events, calendar, cross-day, all-day, isolation, AI add/list, shared editor and restart')
