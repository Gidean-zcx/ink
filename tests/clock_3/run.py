"""clock-3 on the whole product: the reference paths by touch and by the AI, refresh counts, members, a restart, and the
pictures at the default text, the largest text and as a child.

--docs DIR also copies the clean pictures there (docs/tasks/clock-3, each under 300 KB).
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--docs', type=Path)
p.add_argument('--part', choices=('all', 'main', 'pictures'), default='all', help='main: the paths and a restart; pictures: the three sets')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('clock-3' if a.part == 'all' else f'clock-3-{a.part}')
shutil.rmtree(area, ignore_errors=True)
runs = [('main', '-normal', 'main'), ('restart', '-normal', 'main'), ('shots', '-normal', 'shots-normal'),
        ('shots', '-max', 'shots-max'), ('shots', '-child', 'shots-child')]
runs = [r for r in runs if a.part == 'all' or (a.part == 'main') == (r[0] != 'shots')]
failed, refreshes = False, []
for phase, suffix, flash in runs:
    out = area / f'{phase}{suffix}'
    (area / flash / 'sd').mkdir(parents=True, exist_ok=True)
    r = subprocess.run([str(build / 'sim-clock-3'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out), '--end', '900s',
        '--flash-image', str(area / flash / 'user.img'), '--sd-dir', str(area / flash / 'sd'), '--quiet', '--no-random-outages'],
        env={**os.environ, 'INK_CLOCK3_PHASE': phase, 'INK_CLOCK3_SUFFIX': suffix}, capture_output=True, text=True, timeout=560)
    (area / f'{phase}{suffix}.log').write_text(r.stdout + r.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    refreshes += re.findall(r'clock3 refreshes (.*): (\d+)', r.stderr)
    print(f'{phase}{suffix}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
    if r.returncode or m.get('test.fail', 0) or m.get('test.finished') != 1:
        failed = True
        print('\n'.join(data.get('warnings', [])[:30]) or r.stderr[-3000:])
print('refreshes per action:', ', '.join(f'{what} {n}' for what, n in refreshes))
if a.docs and not failed:
    a.docs.mkdir(parents=True, exist_ok=True)
    for picture in sorted(area.glob('*/clean-*.png')):
        target = a.docs / picture.name.replace('clean-', '')
        shutil.copyfile(picture, target)
        assert target.stat().st_size < 300 * 1024, target
    print(f'pictures: {a.docs}')
raise SystemExit(1 if failed else 0)
