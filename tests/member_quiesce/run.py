"""Compile review boundaries and run deterministic member handoff regressions."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--build', default='build')
parser.add_argument('--baseline', help='Use only the old central policy/transport sources')
parser.add_argument('modes', nargs='*')
args = parser.parse_args()
build = (ROOT / args.build).resolve()
work = Path(tempfile.mkdtemp(prefix='member-quiesce-', dir=build))
flags = {}
for line in (build / 'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-media.dir/link.txt').read_text())
defines = []
extras = [ROOT / 'tests/services_finish/member_data_boundary.c',
          ROOT / 'tests/services_finish/member_time_boundary.c',
          ROOT / 'tests/services_finish/member_notify_boundary.c',
          ROOT / 'tests/device_events/member_boundary.c',
          ROOT / 'tests/services_finish/cap_boundary.c',
          ROOT / 'tests/member_quiesce/hub_boundary.c',
          ROOT / 'tests/device_events/hub_observer.c']
if args.baseline:
    for macro, path in [('CAP', 'system/policy/cap.c'), ('MEMBER', 'system/policy/members.c'),
                        ('HUB', 'system/services/hub/hub_link.c')]:
        source = work / Path(path).name
        source.write_bytes(subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=ROOT))
        defines.append('-DINK_TEST_' + macro + '_SOURCE=' + json.dumps(str(source)))
objects = []
for number, source in enumerate([ROOT / 'tests/member_quiesce/regression.c', *extras]):
    obj = work / f'{number}.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'],
                    *flags['C_FLAGS'], '-Wno-unused-function', '-c', str(source), '-o', str(obj)],
                   cwd=build, check=True)
    objects.append(str(obj))
index = next(i for i, word in enumerate(link) if '/tests/integration/' in word and word.endswith('.o'))
link[index] = objects[0]
index = next(i for i, word in enumerate(link) if word.endswith('.a'))
link[index:index] = objects[1:]
binary = work / 'regression'
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
hub = work / 'hub.json'
hub.write_text(json.dumps({'rules': [
    {'method': 'hello', 'think_ms': 40, 'result': {'proto': 1, 'session': 'synthetic'}},
    {'method': 'ai.ask', 'think_ms': 100, 'result': {'done': True}},
]}))
failed = []
for mode in args.modes or ['data', 'alarm', 'reminder', 'cancel', 'events', 'publication', 'upload', 'queued', 'idle']:
    out = work / mode
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                             '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                             '--out', str(out), '--hub', str(hub), '--end', '45s', '--quiet',
                             '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=60,
                            env={**os.environ, 'INK_QUIESCE_MODE': mode, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (work / (mode + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    metrics = data.get('metrics', {})
    ok = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0)
    print(mode, 'PASS' if ok else 'FAIL', 'exit', result.returncode,
          'checks', metrics.get('test.pass'), 'failures', metrics.get('test.fail', 0),
          {k: v for k, v in metrics.items() if k.startswith('test.switch.')}, flush=True)
    if not ok:
        failed.append(mode)
        for warning in data.get('warnings', []):
            if 'CHECK FAILED' in str(warning):
                print(warning, flush=True)
print('Evidence:', work.relative_to(ROOT), 'failed:', failed)
raise SystemExit(bool(failed))
