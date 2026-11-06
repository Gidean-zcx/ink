#!/usr/bin/env python3
"""Revert each integration fix in isolated objects; run serially after a build."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'ec74e15e'


def previous(path):
    return subprocess.check_output(['git', 'show', f'{BASELINE}:{path}'], cwd=ROOT, text=True)


def variant(build, name, target, sources, flags_target='ink_system'):
    folder = build / 'integration-reverse'
    folder.mkdir(exist_ok=True)
    flags = []
    for line in (build / f'CMakeFiles/{flags_target}.dir/flags.make').read_text().splitlines():
        if line.startswith(('C_DEFINES =', 'C_INCLUDES =', 'C_FLAGS =')):
            flags += shlex.split(line.split('=', 1)[1])
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    for index, (path, source) in enumerate(sources.items()):
        obj = folder / f'{name}-{index}.o'
        subprocess.run([link[0], *flags, '-I' + str(ROOT / Path(path).parent),
                        '-I' + str(ROOT / 'system/services/notify'), '-x', 'c', '-c', '-', '-o', str(obj)],
                       input=source, cwd=ROOT, text=True, check=True, capture_output=True)
        direct = f'CMakeFiles/{target}.dir/{path}.o'
        if direct in link: link[link.index(direct)] = str(obj)
        else: link.insert(1, str(obj))
    binary = folder / name
    link = [str(binary) if arg == target else arg for arg in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    return binary


def run(build, name, binary, script=None, budget=None, end=None):
    out = build / 'test-out' / f'integration-reverse-{name}'
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
               '--out', str(out), '--quiet', '--no-random-outages']
    if script: command += ['--script', str(ROOT / script)]
    if budget: command += ['--budget', str(ROOT / budget)]
    if end: command += ['--end', end]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    metrics = json.loads((out / 'metrics.json').read_text())
    record = {'case': name, 'exit': result.returncode, 'metrics': metrics['metrics'],
              'warnings': metrics.get('warnings', [])}
    (out / 'result.json').write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    return result, record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    args = parser.parse_args()
    build = args.build.resolve()
    header = previous('system/policy/watch.h')
    sources = {path: (ROOT / path).read_text().replace('#include "watch.h"', header)
               for path in ('system/policy/watch.c', 'system/policy/state.c', 'system/policy/settings.c')}
    binary = variant(build, 'watch', 'ink-sim', sources)
    result, record = run(build, 'watch', binary, end='2s')
    assert result.returncode == 2 and 'ui failed to start' in result.stderr, record
    print('watch: old capacity reproduces UI startup panic (exit 2)', flush=True)

    path = 'system/services/display/display_service.c'
    binary = variant(build, 'display', 'ink-sim', {path: previous(path)})
    result, record = run(build, 'display', binary, 'tests/scenarios/home_basic.script',
                         'tests/scenarios/home_basic.budget.json')
    assert result.returncode == 1 and record['metrics']['ui.action_to_visible_ms.TEXT']['p95'] > 1200, record
    print('display: old scheduler exceeds the unchanged page latency budget (exit 1)', flush=True)

    path = 'system/services/notify/notify_service.c'
    binary = variant(build, 'notify', 'sim-notify', {path: previous(path)})
    result, record = run(build, 'notify', binary, 'tests/integration/notify.script',
                         'tests/integration/notify.budget.json')
    assert result.returncode == 1 and record['metrics'].get('test.finished') == 1, record
    assert any('with a sound' in warning for warning in record['warnings']), record
    print('notify: old publication order fails the original sound assertion (exit 1)', flush=True)

    path = 'tests/integration/races_notify.c'
    source = (ROOT / path).read_text().replace('#include "../../system/services/notify/notify_service.c"',
                                               previous('system/services/notify/notify_service.c'))
    binary = variant(build, 'members', 'sim-races', {path: source}, 'sim-races')
    result, record = run(build, 'members', binary, 'tests/integration/races.script', 'tests/integration/races.budget.json')
    assert result.returncode == 1 and record['metrics'].get('test.finished') == 1, record
    assert any('inbox of who it is for' in warning for warning in record['warnings']), record
    print('members: old inbox handoff fails the unchanged race assertions (exit 1)', flush=True)

    path = 'system/policy/cap.c'
    binary = variant(build, 'manifest', 'sim-confirm', {path: previous(path)})
    result, record = run(build, 'manifest', binary, 'tests/integration/confirm.script', 'tests/integration/confirm.budget.json')
    assert result.returncode == 1 and record['metrics'].get('test.finished') == 1, record
    assert any('not in its tool list' in warning for warning in record['warnings']), record
    print('manifest: old event entries overflow the unchanged tool buffer (exit 1)', flush=True)

    path = 'ports/sim/hal/hal_display_sim.c'
    binary = variant(build, 'preview', 'sim-binary-preview', {path: previous(path)}, 'ink_port_sim')
    result, record = run(build, 'preview', binary, budget='tests/simulator/binary_preview.budget.json', end='8s')
    assert result.returncode == 1 and record['metrics'].get('test.finished') == 1, record
    assert record['metrics'].get('test.fail', 0) >= 2, record
    print('preview: ignoring the binary preview flag fails pixel preservation (exit 1)', flush=True)

    path = 'drivers/chips/tps65185/tps65185.c'
    binary = variant(build, 'pmic', 'pmic-retry-test', {path: previous(path)}, 'pmic-retry-test')
    result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=10)
    assert result.returncode == 3, result
    print('pmic: old transport fails the bounded read retry case (exit 3)', flush=True)
    print('All seven reversions failed as expected; product sources and budgets were unchanged.')


if __name__ == '__main__': main()
