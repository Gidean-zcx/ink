"""Exercise the real product in six fresh boots; optional bounded negative controls use local copies."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--negative', choices=('radio', 'page'))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='emergency-call-', dir=build))
binary = build / 'sim-emergency-call'
if a.negative:
    db = json.loads((build / 'compile_commands.json').read_text())
    path = 'system/services/comm/comm_service.c' if a.negative == 'radio' else 'tests/emergency_call/emergency_call.c'
    entry = next(e for e in db if e['file'] == str(ROOT / path))
    source = (ROOT / ('system/services/comm/comm_service.c' if a.negative == 'radio' else 'system/apps/phone/phone.c')).read_text()
    old, new = ('if (temporary) r = hal_cell_radio(true, 0) < 0 ? -1 : 0;', '') if a.negative == 'radio' else (
        'if (emergency_changed(id, a)) return;', '(void)emergency_changed;')
    assert source.count(old) == 1
    mutant = work / 'mutant.c'
    mutant.write_text(source.replace(old, new))
    driver = mutant
    if a.negative == 'page':
        text = (ROOT / path).read_text()
        for old_path, target in (('../../system/apps/phone/phone.c', mutant),
                                 ('../../system/boot/main.c', ROOT / 'system/boot/main.c'),
                                 ('../integration/whole_product.h', ROOT / 'tests/integration/whole_product.h')):
            text = text.replace('"' + old_path + '"', '"' + str(target) + '"')
        driver = work / 'driver.c'
        driver.write_text(text)
    obj = work / 'mutant.o'
    command = shlex.split(entry['command'])
    original_object = command[command.index('-o') + 1]
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(ROOT / path))] = str(driver)
    subprocess.run(command, cwd=build, check=True, capture_output=True)
    link = shlex.split((build / 'CMakeFiles/sim-emergency-call.dir/link.txt').read_text())
    if a.negative == 'page':
        link = [str(obj) if x == original_object else x for x in link]
    else:
        # An explicit object takes precedence over the same member in the static archive.
        link.insert(1, str(obj))
    binary = work / 'sim-negative'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)

for phase in (['airplane'] if a.negative == 'radio' else ['kid'] if a.negative else ['lock', 'gate', 'kid', 'airplane', 'nosim', 'en']):
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
        expected = 'emergency AT call connects with RF on' if a.negative == 'radio' else 'emergency page tracks the active call'
        assert not passed and expected in result.stdout + result.stderr + json.dumps(metrics.get('warnings', [])), result.stderr[-4000:]
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
