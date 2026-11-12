"""Clock seconds acceptance; the optional baseline control is manual and isolated."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
binary = build / 'sim-timer-seconds'
area = build / 'test-out' / ('timer-seconds-baseline' if a.reverse else 'timer-seconds')
shutil.rmtree(area, ignore_errors=True)
(area / 'sd').mkdir(parents=True)
if a.reverse:
    source = ROOT / 'system/apps/clock/clock.c'
    old = subprocess.check_output(['git', 'show', '6171c2bf8d59238c95f2d548663f8b2ab3c6f43a:system/apps/clock/clock.c'], cwd=ROOT, text=True)
    replacement = area / 'clock.c'
    replacement.write_text(old)
    entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
    command = shlex.split(entry['command'])
    obj = area / 'clock.o'
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(replacement)
    command += ['-I' + str(source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-timer-seconds.dir/link.txt').read_text())
    binary = area / 'sim-timer-seconds'
    link[link.index('-o') + 1] = str(binary)
    link.insert(link.index('libink_system.a'), str(obj))
    subprocess.run(link, cwd=build, check=True)
r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / 'out'),
    '--end', '400s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
    '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')], capture_output=True, text=True, timeout=160)
(area / 'run.log').write_text(r.stdout + r.stderr)
d = json.loads((area / 'out/metrics.json').read_text())
m = d['metrics']
frames = list((area / 'out').glob('snap-*.png'))
if a.reverse:
    passed = r.returncode != 0 and m.get('test.fail', 0) > 0 and any('touch selects ninety seconds' in w for w in d.get('warnings', []))
else:
    passed = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and len(frames) == 6
print(f'timer-seconds {"baseline" if a.reverse else "current"}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, frames={len(frames)}, expected={passed}', flush=True)
if not passed:
    print('\n'.join(d.get('warnings', [])), r.stderr[-4000:])
if passed and not a.reverse and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(0 if passed else 1)
