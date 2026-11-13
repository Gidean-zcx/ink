"""calendar-3: run the service path, the migration (two boots on one flash image) or the pages; no evidence kept on success."""
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
p.add_argument('--suite', choices=('service', 'migrate', 'pages'), default='service')
p.add_argument('--binary', type=Path, help='a mutated executable (reverse controls)')
p.add_argument('--scene', default='', help='pages: default, big or child')
p.add_argument('--shots', type=Path, help='pages: copy the screenshots here')
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(parents=True, exist_ok=True)
area = Path(tempfile.mkdtemp(prefix=f'calendar-3-{a.suite}-', dir=build / 'test-out'))
(area / 'sd').mkdir()
binary = (a.binary or build / ('sim-calendar-3-pages' if a.suite == 'pages' else 'sim-calendar-3-service')).resolve()
phases = {'service': ['path'], 'migrate': ['legacy', 'migrate'], 'pages': a.scene.split(',') if a.scene else ['default', 'big', 'child']}[a.suite]
failed = None
for phase in phases:
    out = area / phase
    cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out), '--end', '900s', '--flash-image', str(area / ('user.img' if a.suite == 'migrate' else f'{phase}.img')),
           '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages', '--trace',
           '--budget', str(ROOT / 'tests/ui_scene.budget.json')]
    result = subprocess.run(cmd, env={**os.environ, 'INK_CALENDAR_PHASE': phase}, capture_output=True, text=True, timeout=600)
    (area / f'{phase}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    metrics = data.get('metrics', {})
    passed = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0)
    print(f'{phase}: exit={result.returncode}, pass={metrics.get("test.pass", 0)}, fail={metrics.get("test.fail", 0)}, finished={metrics.get("test.finished", 0)}', flush=True)
    for w in data.get('warnings', []):
        if 'CHECK FAILED' in str(w) or 'REFRESH' in str(w):
            print(str(w)[:400], flush=True)
    if a.suite == 'pages' and (out / 'refreshes.txt').exists():
        print((out / 'refreshes.txt').read_text(), flush=True)
    if passed and a.shots:
        a.shots.mkdir(parents=True, exist_ok=True)
        for png in sorted(out.glob('snap-*.png')):
            shutil.copyfile(png, a.shots / f'{png.stem[5:]}.png')
    if not passed:
        failed = f'{phase} failed; evidence: {area}\n{result.stderr[-1200:]}'
        break
if failed:
    raise SystemExit(failed)
if a.binary or os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', area)
else:
    shutil.rmtree(area)
print(f'PASS: calendar-3 {a.suite}')
