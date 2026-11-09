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
p.add_argument('--mutant', choices=['member', 'version', 'commit', 'ack', 'sent', 'receipt', 'expiry', 'failure', 'deadline'])
p.add_argument('--case', choices=['all', 'core', 'receipts', 'capacity', 'failure'], default='all')
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
    path = 'system/policy/cap.c' if a.mutant in ('version', 'ack', 'sent') else 'system/policy/confirm.c'
    source = (ROOT / path).read_text()
    replacements = {
        'member': [
            ('strcmp(h->member, ink_member_current()->id) || h->epoch != ink_member_epoch()', 'false', 1),
            ('.member = h->member', '.member = NULL', 1),
        ],
        'version': [('page_gen != ink_ui_content_version()', 'false', 1)],
        'commit': [('journal(h->token, hub ? "accepted" : "running", NULL, NULL, NULL)', '1', 1)],
        'ack': [('ink_confirm_ack(token[i], ver[i]);', '(void)token[i], (void)ver[i];', 1)],
        'sent': [('e->delivered && e->seq > ev_acked', 'e->seq > ev_acked', 1)],
        'receipt': [('rc->status == INK_RC_DONE || rc->status == INK_RC_UNDONE || rc->status == INK_RC_PARTIAL', 'rc->status == INK_RC_DONE', 1)],
        'expiry': [('expires <= 0 || osal_now_us() >= (uint64_t)expires', 'false', 1)],
        'failure': [('confirmed_done(h, &rc); return;', 'osal_free(h); return;', 1)],
        'deadline': [('ink_db_wait(CF.db, ver, remaining)', 'ink_db_wait(CF.db, ver, 5000000)', 1)],
    }[a.mutant]
    for old, new, count in replacements:
        assert source.count(old) == count, 'counterexample must remove the intended guards'
        source = source.replace(old, new)
    target = work / Path(path).name
    target.write_text(source)
    macro = 'CAP' if a.mutant in ('version', 'ack', 'sent') else 'CONFIRM'
    defines.append('-DINK_TEST_' + macro + '_SOURCE=' + json.dumps(str(target)))
objects = []
for n, source in enumerate(['../integration/dictation.c' if a.mutant == 'deadline' else 'regression.c', 'confirm_boundary.c', 'cap_boundary.c']):
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

if a.mutant == 'deadline':
    line = next(x for x in (build / 'CTestTestfile.cmake').read_text().splitlines()
                if x.startswith('add_test(integration-dictation '))
    command = shlex.split(line[len('add_test('):-1])[1:]
    command[0] = str(binary)
    out = work / 'dictation'
    command[command.index('--out') + 1] = str(out)
    result = subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=120)
    (work / 'dictation.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text(errors='replace'))
    print('deadline reverse:', result.returncode, data.get('warnings'), 'Evidence:', work.relative_to(ROOT), flush=True)
    raise SystemExit(0 if result.returncode == 0 and data['metrics'].get('test.finished') == 1 and not data['metrics'].get('test.fail', 0) else 1)

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
                             '--out', str(out), '--end', '2000s' if phase == 'capacity' else '200s' if phase == 'core' else '35s', '--quiet',
                             '--no-random-outages', '--flash-image', str(path / 'user.img'), '--sd-dir', str(path / 'sd')],
                            cwd=ROOT, capture_output=True, text=True, timeout=120, env=env)
    (path / (label + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
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

ok = True
for phase in ['core', 'receipts', 'failure']:
    if a.case in ('all', phase):
        ok = boot(work / phase, 'run', phase) and ok
if a.case in ('all', 'capacity'):
    ok = boot(work / 'capacity', 'run', 'capacity') and ok
    ok = boot(work / 'capacity', 'restart', 'restart-capacity') and ok
for mode in ['', '-auto'] if a.case == 'all' else []:
    for cut in ['before-running', 'after-running', 'before-done', 'after-done']:
        path = work / (cut + mode)
        ok = boot(path, 'cut', 'cut' + mode, cut=cut) and ok
        ok = boot(path, 'restart', 'restart' + mode, expected=cut) and ok
print('Evidence:', work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
