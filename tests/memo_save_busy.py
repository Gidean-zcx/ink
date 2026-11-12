"""Continuous edits on the real memo app, with an optional isolated baseline control."""
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
binary = build / 'sim-memo-save-busy'
area = build / 'test-out' / ('memo-save-busy-baseline' if a.reverse else 'memo-save-busy')
shutil.rmtree(area, ignore_errors=True)
(area / 'sd').mkdir(parents=True)
if a.reverse:
    source = ROOT / 'system/services/storage/storage_service.c'
    old = subprocess.check_output(['git', 'show', '6171c2bf8d59238c95f2d548663f8b2ab3c6f43a:system/services/storage/storage_service.c'], cwd=ROOT, text=True)
    replacement = area / 'storage_service.c'
    replacement.write_text(old)
    test_source = ROOT / 'tests/memo_save_busy.c'
    test_copy = area / 'memo_save_busy.c'
    test_copy.write_text(test_source.read_text().replace('"../system/services/storage/storage_service.c"', '"' + str(replacement) + '"'))
    entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == test_source)
    command = shlex.split(entry['command'])
    obj = area / 'storage.o'
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(test_source))] = str(test_copy)
    command += ['-I' + str(source.parent), '-I' + str(test_source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-memo-save-busy.dir/link.txt').read_text())
    binary = area / 'sim-memo-save-busy'
    link[link.index('-o') + 1] = str(binary)
    link[link.index('CMakeFiles/sim-memo-save-busy.dir/tests/memo_save_busy.c.o')] = str(obj)
    subprocess.run(link, cwd=build, check=True)
r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / 'out'),
    '--end', '60s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
    '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')], capture_output=True, text=True, timeout=100)
(area / 'run.log').write_text(r.stdout + r.stderr)
d = json.loads((area / 'out/metrics.json').read_text())
m = d['metrics']
if a.reverse:
    passed = r.returncode != 0 and m.get('test.fail', 0) > 0 and any('busy memo save completes within three seconds' in w and 'rc=7' in w for w in d.get('warnings', []))
else:
    passed = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0)
summary = {'case': 'baseline' if a.reverse else 'current', 'exit': r.returncode, 'pass': m.get('test.pass', 0),
           'fail': m.get('test.fail', 0), 'expected': passed, 'save_us': m.get('test.memo_save_us'),
           'typing_ms': m.get('text.key_to_visible_ms'), 'storage_wait_ms': m.get('storage.wait_for_idle_ms')}
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(summary, ensure_ascii=False), flush=True)
if not passed:
    print('\n'.join(d.get('warnings', [])), r.stderr[-4000:])
if passed and not a.reverse and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(0 if passed else 1)
