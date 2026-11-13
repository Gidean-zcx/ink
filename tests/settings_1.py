"""settings-1: the settings home and its search after AOSP, one registry behind rows, search and settings.open, and a
changed value that redraws only its own row (D-127). The walk runs at the default and the largest text size; the
refresh areas are read from the default run's display trace. --shots DIR keeps each step's picture as <name>-<size>.png."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
STATUS_BAR = 108  # the status bar is redrawn on every settings change by navigation.c (control-1's, not this page's)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--sizes', default='1,3')
p.add_argument('--shots', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-settings-1'
area = build / 'test-out' / ('settings-1-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
ok = True
for size in a.sizes.split(','):
    out = area / f'size-{size}'
    trace = size == '1'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/settings_1.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'] + (['--trace'] if trace else []),
        env={**os.environ, 'INK_SETTINGS1_TEXT': size, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=900)
    (area / f'size-{size}.log').write_text(run.stdout + run.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    passed = run.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0)
    for warning in data.get('warnings', []):
        if 'CHECK FAILED' in str(warning):
            print(warning)
    if trace:
        events = json.loads((out / 'trace.json').read_text()) if (out / 'trace.json').exists() else []
        marks = {e['name']: e['ts'] for e in events if str(e.get('name', '')).startswith('settings1.')}
        y1, y2 = m.get('settings1.row_y1', -1), m.get('settings1.row_y2', -1)
        start, end = marks.get('settings1.refresh.before', float('inf')), marks.get('settings1.refresh.after', 0)
        # Panel x is the page's y (the panel is turned 90 degrees): an update's band down the page.
        bands = [(e['args']['x'], e['args']['x'] + e['args']['w']) for e in events
                 if e.get('name') in ('DU', 'GC16', 'GL16', 'A2') and 'args' in e and start <= e.get('ts', -1) < end]
        body = [b for b in bands if b[1] > STATUS_BAR]
        in_row = y1 >= 0 and bool(body) and all(y1 - 2 <= lo and hi <= y2 + 2 for lo, hi in body)
        print(f'size {size}: the changed value redraws only its row: {in_row} (row {y1}..{y2}, bands {bands})', flush=True)
        passed &= in_row
    if a.shots:
        a.shots.mkdir(parents=True, exist_ok=True)
        for picture in sorted(out.glob('snap-*.png')):
            shutil.copyfile(picture, a.shots / f'{picture.stem[5:]}-{size}.png')
    print(f'size {size}: the home has {m.get("settings1.home_pages")} pages, a child\'s home {m.get("settings1.kid_pages")}', flush=True)
    print(f'size {size}:', 'PASS' if passed else 'FAIL', 'exit', run.returncode, 'checks', m.get('test.pass', 0),
          'failures', m.get('test.fail', 0), flush=True)
    if not passed and run.returncode:
        print(run.stderr[-2000:])
    ok &= passed
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
elif not ok:
    print('Evidence:', area, flush=True)
raise SystemExit(0 if ok else 1)
