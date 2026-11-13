"""control-1: run the control centre walks (touch, ai, kid, bar, pictures at two text sizes) and check from the
display trace that a toggled tile refreshes only its own area and a new minute only the status bar's time.

python3 tests/control_center.py --build build [--case touch] [--save DIR]; --save copies the pictures (clean frames)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--build', type=Path, required=True)
ap.add_argument('--binary', type=Path)
ap.add_argument('--case')
ap.add_argument('--save', type=Path)
a = ap.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-control-center'
RUNS = [('touch', None), ('ai', None), ('kid', None), ('bar', None), ('shots', '1'), ('shots', '3')]
WAVES = ('GC16', 'GL16', 'DU', 'A2')


def inside(trace, metrics, mark, what):
    """Every refresh between control.<mark>.before and .after lies in test.<what>.* (native pixels, inclusive)."""
    t = {e['name']: e['ts'] for e in trace if e.get('name', '').startswith('control.' + mark + '.')}
    t0, t1 = t.get('control.%s.before' % mark), t.get('control.%s.after' % mark)
    box = [metrics.get('test.%s.%s' % (what, edge), {}).get('max') for edge in ('x1', 'y1', 'x2', 'y2')]
    if t0 is None or t1 is None or None in box:
        return False, 'marks or bounds missing'
    updates = [e['args'] for e in trace if e.get('name') in WAVES and t0 <= e.get('ts', -1) <= t1 and 'args' in e]
    outside = [u for u in updates if u['x'] < box[0] - 1 or u['y'] < box[1] - 1 or
               u['x'] + u['w'] - 1 > box[2] + 1 or u['y'] + u['h'] - 1 > box[3] + 1]
    return bool(updates) and not outside, '%d refreshes, %d outside %s: %s' % (len(updates), len(outside), box, outside[:3])


ok = True
for case, text in RUNS:
    if a.case and case != a.case:
        continue
    name = case + ('-' + text if text else '')
    out = build / 'test-out' / ('control-center-' + name)
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    env = {**os.environ, 'INK_CONTROL_CASE': case}
    if text:
        env['INK_CONTROL_TEXT'] = text
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                        '--end', '400s', '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
                        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
                        '--quiet', '--no-random-outages', '--trace'], env=env, capture_output=True, text=True, timeout=240)
    (out / 'test.log').write_text(r.stdout + r.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    passed = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0)
    print('control-center[%s]: exit=%d pass=%s fail=%s' % (name, r.returncode, m.get('test.pass', 0), m.get('test.fail', 0)), flush=True)
    if not passed:
        for w in data.get('warnings', []):
            print('  ', str(w)[:300])
        if not data:
            print(r.stderr[-3000:])
    if case in ('touch', 'bar') and (out / 'trace.json').exists():  # judged even when an earlier check failed
        trace = json.loads((out / 'trace.json').read_text())
        if isinstance(trace, dict):
            trace = trace.get('traceEvents', [])
        good, why = inside(trace, m, 'tile', 'tile') if case == 'touch' else inside(trace, m, 'minute', 'clock')
        print('  %s: %s' % ('a toggled tile refreshes only itself' if case == 'touch' else 'a new minute refreshes only the time',
                            'yes' if good else 'NO'), why, flush=True)
        passed &= good
    if a.save and passed:
        a.save.mkdir(parents=True, exist_ok=True)
        for png in out.glob('clean-*.png'):
            shutil.copyfile(png, a.save / png.name[len('clean-'):])
    ok &= passed
    if passed and not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(out)
raise SystemExit(0 if ok else 1)
