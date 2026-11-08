"""Durable confirmation tests on shared flash images, with source-only counterexamples."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build')
p.add_argument('--mutant', choices=['member', 'version', 'commit'])
a = p.parse_args()
build = (ROOT / a.build).resolve()
work = Path(tempfile.mkdtemp(prefix='confirm-durable-', dir=build))
flags = {}
for line in (build / 'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-media.dir/link.txt').read_text())
defines = []
if a.mutant:
    path = 'system/policy/cap.c' if a.mutant == 'version' else 'system/policy/confirm.c'
    source = (ROOT / path).read_text()
    old, new = {
        'member': ('ink_json_eq(j, ink_json_find(j, 0, "binding.member"), ink_member_current()->id)', 'true'),
        'version': ('page_gen != ink_ui_content_version()', 'false'),
        'commit': ('journal(h->token, hub ? "accepted" : "running", NULL, NULL)', '1'),
    }[a.mutant]
    assert source.count(old) == 1, 'counterexample must remove exactly one guard'
    target = work / Path(path).name
    target.write_text(source.replace(old, new))
    macro = 'CAP' if a.mutant == 'version' else 'CONFIRM'
    defines.append('-DINK_TEST_' + macro + '_SOURCE=' + json.dumps(str(target)))
objects = []
for n, source in enumerate(['regression.c', 'confirm_boundary.c', 'cap_boundary.c']):
    obj = work / f'{n}.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'],
                    *flags['C_FLAGS'], '-Wno-unused-function', '-c', str(ROOT / 'tests/confirm_durable' / source), '-o', str(obj)],
                   cwd=build, check=True)
    objects.append(str(obj))
index = next(i for i, word in enumerate(link) if '/tests/integration/' in word and word.endswith('.o'))
link[index] = objects[0]
index = next(i for i, word in enumerate(link) if word.endswith('.a'))
link[index:index] = objects[1:]
binary = work / 'regression'
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)

def boot(path, label, phase, cut=None, expected=None):
    path.mkdir(exist_ok=True)
    (path / 'sd').mkdir(exist_ok=True)
    out = path / label
    env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'INK_CONFIRM_PHASE': phase}
    if cut:
        env['INK_CONFIRM_CUT'] = cut
    if expected:
        env['INK_CONFIRM_EXPECT'] = expected
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                             '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                             '--out', str(out), '--end', '200s' if phase == 'core' else '35s', '--quiet',
                             '--no-random-outages', '--flash-image', str(path / 'user.img'), '--sd-dir', str(path / 'sd')],
                            cwd=ROOT, capture_output=True, text=True, timeout=120, env=env)
    (path / (label + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    complete = m.get('test.cut') == 1 and m.get('power.reboot') == 1 if cut else m.get('test.finished') == 1
    ok = result.returncode == 0 and complete and not m.get('test.fail', 0)
    print(path.name, label, 'PASS' if ok else 'FAIL', 'exit', result.returncode,
          'checks', m.get('test.pass', 0), 'failures', m.get('test.fail', 0), flush=True)
    if not ok:
        for warning in data.get('warnings', []):
            if 'CHECK FAILED' in str(warning):
                print(warning, flush=True)
        print(result.stderr[-1000:], flush=True)
    return ok

ok = boot(work / 'core', 'run', 'core')
for mode in ['', '-auto']:
    for cut in ['before-running', 'after-running', 'before-done', 'after-done']:
        path = work / (cut + mode)
        ok = boot(path, 'cut', 'cut' + mode, cut=cut) and ok
        ok = boot(path, 'restart', 'restart' + mode, expected=cut) and ok
print('Evidence:', work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
