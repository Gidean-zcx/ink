"""phone-3 acceptance: the product at the default text size, the largest one and as a child; --reverse rebuilds copies
with a fix taken out (product sources untouched) and requires the matching assertion to fail."""
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
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
out = build / 'test-out' / 'phone-3'


def run(binary, sets, where):
    results = {}
    for name in sets:
        target = where / name
        shutil.rmtree(target, ignore_errors=True)
        command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'), '--end', '900s',
                   '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
                   '--content', str(build / 'test-content'), '--flash-image', str(where / f'{name}.img'),
                   '--sd-dir', str(where / f'{name}-sd'), '--out', str(target), '--quiet', '--no-random-outages']
        for leftover in (where / f'{name}.img',):
            leftover.unlink(missing_ok=True)
        result = subprocess.run(command, env=dict(os.environ, INK_PHONE3_SET=name), capture_output=True, text=True, errors="replace", timeout=900)
        metrics = json.loads((target / 'metrics.json').read_bytes().decode('utf-8', 'replace')) if (target / 'metrics.json').exists() else {}
        results[name] = (result, metrics)
    return results


if not a.reverse:
    for source in ('system/apps/phone/phone.c', 'system/apps/phone/vendor/lilyphone/call_card.c'):
        assert 'ink_ui_circle(' not in (ROOT / source).read_text(), f'{source} draws no circle'
    out.mkdir(parents=True, exist_ok=True)
    for name, (result, metrics) in run(build / 'sim-phone-3', ('1', '3', 'kid'), out).items():
        m = metrics.get('metrics', {})
        assert result.returncode == 0 and m.get('test.finished') == 1, (name, result.returncode, result.stderr[-3000:], metrics.get('warnings'))
        refresh = {k[len('test.refresh.'):]: v for k, v in m.items() if k.startswith('test.refresh.')}
        print(f'{name}: {m.get("test.pass")} assertions; refreshes {json.dumps(refresh, sort_keys=True)}')
    raise SystemExit(0)

# Reverse controls: compile a changed copy with the real flags and link it in front of the libraries.
db = json.loads((build / 'compile_commands.json').read_text())
link = shlex.split((build / 'CMakeFiles/sim-phone-3.dir/link.txt').read_text())
work = Path(tempfile.mkdtemp(prefix='phone-3-reverse-', dir=build))


def compile_copy(path, source, name, unit=None):
    entry = next(e for e in db if e['file'] == str(ROOT / (unit or path)))
    copy, obj = work / (name + Path(path).suffix), work / (name + '.o')
    copy.write_text(source)
    command = shlex.split(entry['command'])
    command[command.index('-o') + 1] = str(obj)
    if unit:  # phone.c is included by the test driver: point the driver at the copy
        command += ['-DINK_PHONE_SOURCE="' + str(copy) + '"']
    else:
        command[command.index(str(ROOT / path))] = str(copy)
    command += ['-iquote', str((ROOT / path).parent)]
    result = subprocess.run(command, cwd=build, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return obj


cases = [
    ('accepted-is-connected', 'system/apps/phone/phone.c',
     'snprintf(P.status, sizeof P.status, "%s", P.incoming ? TR_NOOP("来电") : TR_NOOP("正在拨号"));',
     'snprintf(P.status, sizeof P.status, "%s", P.incoming ? TR_NOOP("来电") : TR_NOOP("通话中")); if (!P.incoming) P.started = ink_now_us();',
     'accepted by the modem, not connected'),
    ('busy-is-plain-end', 'ports/sim/hal/hal_cellular_sim.c',
     "ev->end = line[0] == 'B' ? HAL_CELL_END_BUSY : HAL_CELL_END_NO_ANSWER;",
     "ev->end = line[0] == 'B' ? HAL_CELL_END_HANGUP : HAL_CELL_END_NO_ANSWER;",
     'call-ended-busy: 对方忙或拒接'),
]
for name, path, old, new, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(old) == 1, name
    variant = link[:]
    if path.endswith('phone.c'):
        obj = compile_copy(path, source.replace(old, new), name, 'tests/phone_3/phone_3.c')
        variant = [str(obj) if item.endswith('tests/phone_3/phone_3.c.o') else item for item in variant]
    else:
        obj = compile_copy(path, source.replace(old, new), name)

        variant.insert(next(i for i, item in enumerate(variant) if item.endswith('.a')), str(obj))
    binary = work / name
    variant[variant.index('-o') + 1] = str(binary)
    linked = subprocess.run(variant, cwd=build, capture_output=True, text=True)
    assert linked.returncode == 0, linked.stderr
    result, metrics = run(binary, ('1',), work / (name + '-out'))['1']
    warnings = '\n'.join(metrics.get('warnings', []))
    assert result.returncode != 0 and expected in warnings, (name, result.returncode, warnings[-2000:])
    print(f'PASS: taking out the fix ({name}) fails "{expected}"')
shutil.rmtree(work)
