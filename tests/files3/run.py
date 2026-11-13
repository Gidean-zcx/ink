"""files-3 walks in the simulator: the reference path by touch and by the AI, sort, cards, favourites, a child,
empty and error states. Refresh counts come from the display metrics; the marked row's refreshes from the trace.

python3 tests/files3/run.py --build build [--case walk] [--text 3] [--save DIR] [--mutant keep-both]
--mutant keep-both makes "keep both" overwrite in the file service: the walk must then fail."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--build', type=Path, required=True)
ap.add_argument('--case', action='append')
ap.add_argument('--text')
ap.add_argument('--save', type=Path)
ap.add_argument('--mutant', choices=('keep-both',))
a = ap.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='files3-', dir=build))
binary = build / 'sim-files3'
CASES = a.case or ['walk', 'ai', 'sort', 'delete', 'fav', 'child', 'states', 'flipper']
WAVES = ('GC16', 'GL16', 'DU', 'A2')
if a.mutant:
    caps = (ROOT / 'system/services/file/file_caps.c').read_text()
    old = 'how == HOW_KEEP ? 1 : how == HOW_REPLACE ? 2 : 0'
    assert caps.count(old) == 1
    (work / 'file_caps.c').write_text(caps.replace(old, 'how == HOW_KEEP ? 2 : how == HOW_REPLACE ? 2 : 0'))
    flags = {}
    for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-files3.dir/link.txt').read_text())
    obj = work / 'file_caps.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], '-I', str(ROOT / 'system/services/file'), *flags['C_FLAGS'],
                    '-c', str(work / 'file_caps.c'), '-o', str(obj)], cwd=build, check=True)
    link.insert(link.index('-o'), str(obj))
    binary = work / 'sim-files3'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    CASES = ['walk']

ok = True
for case in CASES:
    name = case + ('-' + a.text if a.text else '')
    out = work / name
    env = {**os.environ, 'INK_FILES3_CASE': case}
    if a.text:
        env['INK_FILES3_TEXT'] = a.text
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                        '--script', str(ROOT / 'tests/files3/files3.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
                        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--sd-dir', str(work / (name + '-sd')),
                        '--flash-image', str(work / (name + '.img')), '--out', str(out), '--quiet', '--no-random-outages', '--trace'],
                       env=env, capture_output=True, text=True, timeout=400)
    (work / (name + '.log')).write_text(r.stdout + r.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    passed = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0)
    print('files3[%s]: exit=%d pass=%s fail=%s' % (name, r.returncode, m.get('test.pass', 0), m.get('test.fail', 0)), flush=True)
    for w in data.get('warnings', [])[:12]:
        print('   ', str(w)[:240])
    if not data:
        print(r.stderr[-2000:])
    counts = {k.split('.')[-1]: v.get('max') for k, v in m.items() if k.startswith('files3.refresh.') and isinstance(v, dict)}
    if counts:
        print('  refreshes per action (display dispatches):', json.dumps(counts, ensure_ascii=False), flush=True)
    if case == 'walk' and not a.mutant and (out / 'trace.json').exists():
        trace = json.loads((out / 'trace.json').read_text())
        trace = trace.get('traceEvents', trace) if isinstance(trace, dict) else trace
        t = {e['name']: e['ts'] for e in trace if e.get('name', '').startswith('files3.mark.')}
        box = [m.get('test.mark.%s' % edge, {}).get('max') for edge in ('x1', 'y1', 'x2', 'y2')]
        updates = [e['args'] for e in trace if e.get('name') in WAVES and t.get('files3.mark.before', 1e30) <= e.get('ts', -1) <= t.get('files3.mark.after', -1) and 'args' in e]
        pad = 16  # the panel driver widens a refresh to its 8-pixel alignment, plus the row outline
        outside = [u for u in updates if None in box or u['x'] < box[0] - pad or u['y'] < box[1] - pad or u['x'] + u['w'] - 1 > box[2] + pad or u['y'] + u['h'] - 1 > box[3] + pad]
        good = bool(updates) and not outside
        print('  a tick refreshes only its row and the count:', 'yes' if good else 'NO', '%d updates, outside %s' % (len(updates), outside[:2]), flush=True)
        passed &= good
    if a.save and passed:
        a.save.mkdir(parents=True, exist_ok=True)
        for png in out.glob('clean-*.png'):
            shutil.copyfile(png, a.save / png.name[len('clean-'):])
    ok &= passed
if a.mutant:
    print('mutant %s: the walk %s' % (a.mutant, 'FAILED as it must' if not ok else 'PASSED (the test does not see it)'))
    ok = not ok
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(work)
else:
    print('Evidence:', work)
raise SystemExit(0 if ok else 1)
