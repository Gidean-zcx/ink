"""Exercise the real product; bounded controls compile copies without changing product sources."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from policy import check

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--negative', choices=('radio', 'page', 'ownership', 'entry', 'prefix'))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='emergency-call-', dir=build))
policy = (ROOT / 'system/policy/cap.c').read_text()
if a.negative == 'prefix':
    try:
        check(policy.replace('bool restricted =', 'bool phone_locked = !strncmp(c->id, "phone.", 6);\n    bool restricted =', 1))
    except AssertionError as error:
        print('PASS: reintroducing the prefix exception fails the unchanged check:', error)
        shutil.rmtree(work)
        raise SystemExit(0)
    raise AssertionError('name-prefix control unexpectedly passed')
check(policy)
print('PASS: lock gate and public entry use capability declarations', flush=True)
db = json.loads((build / 'compile_commands.json').read_text())
base_link = shlex.split((build / 'CMakeFiles/sim-emergency-call.dir/link.txt').read_text())


def compile_copy(path, source, name):
    entry = next(e for e in db if e['file'] == str(ROOT / path))
    source_path, obj = work / (name + '.c'), work / (name + '.o')
    source_path.write_text(source)
    command = shlex.split(entry['command'])
    original_object = command[command.index('-o') + 1]
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(ROOT / path))] = str(source_path)
    result = subprocess.run(command, cwd=build, capture_output=True, text=True)
    (work / (name + '-compile.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stderr
    return obj, original_object


link = base_link[:]
if a.negative:
    file = 'system/services/comm/comm_service.c' if a.negative == 'radio' else 'system/policy/cap.c' if a.negative == 'entry' else 'system/apps/phone/phone.c'
    source = (ROOT / file).read_text()
    old, new = {
        'radio': ('if (temporary) r = hal_cell_radio(true, 0) < 0 ? -1 : 0;', ''),
        'page': ('if (emergency_changed(id, a)) return;', '(void)emergency_changed;'),
        'ownership': ('{.id = "phone.emergency_open", .app = NULL', '{.id = "phone.emergency_open", .app = "phone"'),
        'entry': ('return c && c->locked_entry;', 'return false; (void)c;'),
    }[a.negative]
    assert source.count(old) == 1
    source = source.replace(old, new)
    if file.endswith('/phone.c'):
        mutant = work / 'phone.c'
        mutant.write_text(source)
        file = 'tests/emergency_call/emergency_call.c'
        source = (ROOT / file).read_text()
        for old_path, target in (('../../system/apps/phone/phone.c', mutant),
                                 ('../../system/boot/main.c', ROOT / 'system/boot/main.c'),
                                 ('../integration/whole_product.h', ROOT / 'tests/integration/whole_product.h')):
            source = source.replace('"' + old_path + '"', '"' + str(target) + '"')
    obj, original = compile_copy(file, source, 'negative')
    if file.startswith('tests/'):
        link = [str(obj) if item == original else item for item in link]
    else:
        link.insert(1, str(obj))

# The shipped policy makes Phone mandatory. This fixture removes only that
# exemption so a stored disabled-phone policy reaches the real app ownership gate.
source = (ROOT / 'system/policy/members.c').read_text()
start = source.index('int ink_member_app_mode(')
end = source.index('\n}', start)
body = source[start:end]
assert body.count('!strcmp(app, "phone") || ') == 1
source = source[:start] + body.replace('!strcmp(app, "phone") || ', '', 1) + source[end:]
start = source.index('static void cap_kid(')
end = source.index('\n}', start)
body = source[start:end]
assert body.count(' || !strcmp(app, "phone")') == 1
source = source[:start] + body.replace(' || !strcmp(app, "phone")', '', 1) + source[end:]
fixture, _ = compile_copy('system/policy/members.c', source, 'disabled-phone')
phases = {'radio': ['airplane'], 'page': ['kid'], 'ownership': ['kid-disabled'], 'entry': ['gate']}.get(a.negative,
    ['lock', 'gate', 'kid', 'airplane', 'nosim', 'en', 'kid-disabled'])
for phase in phases:
    binary = build / 'sim-emergency-call'
    if a.negative or phase == 'kid-disabled':
        phase_link = link[:]
        if phase == 'kid-disabled':
            phase_link.insert(1, str(fixture))
        binary = work / ('sim-' + phase)
        phase_link[phase_link.index('-o') + 1] = str(binary)
        result = subprocess.run(phase_link, cwd=build, capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
    out = work / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/emergency_call/calls.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(work / (phase + '.img')), '--sd-dir', str(work / (phase + '-sd')),
        '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=120,
        env={**os.environ, 'INK_EMERGENCY_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (work / (phase + '.log')).write_text(result.stdout + result.stderr)
    metrics_path = out / 'metrics.json'
    metrics = json.loads(metrics_path.read_text(errors='replace')) if metrics_path.exists() else {}
    counts = metrics.get('metrics', {})
    passed = not result.returncode and counts.get('test.finished') == 1 and not counts.get('test.fail') and not metrics.get('warnings')
    print(phase, 'PASS' if passed else 'FAIL', 'exit', result.returncode,
          'checks', counts.get('test.pass'), 'failures', counts.get('test.fail', 0), flush=True)
    if a.negative:
        expected = {'radio': 'emergency AT call connects with RF on', 'page': 'emergency page tracks the active call',
                    'ownership': 'AI opens the public entry from a locked surface', 'entry': 'the public-entry marker works without a phone name'}[a.negative]
        assert not passed and counts.get('test.finished') == 1 and expected in result.stdout + result.stderr + json.dumps(metrics.get('warnings', [])), result.stderr[-4000:]
        print('PASS: removing the fix fails the unchanged regression:', expected, flush=True)
    elif not passed:
        print('Evidence:', work.relative_to(ROOT), flush=True)
        raise SystemExit(json.dumps(metrics.get('warnings', []), ensure_ascii=False) + result.stderr[-4000:])
    else:
        assert all((out / ('snap-' + name + '.png')).exists() for name in ('entry', 'keypad', 'active', 'returned'))
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', work.relative_to(ROOT))
else:
    shutil.rmtree(work)
