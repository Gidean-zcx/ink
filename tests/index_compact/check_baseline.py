"""Relink failing full-suite cases with the exact task baseline file service."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
area = root / 'work/index-compact' / ('baseline-check-' + build.name)
area.mkdir(parents=True, exist_ok=True)
source = area / 'file_service.c'
source.write_bytes(subprocess.check_output(['git', 'show', '5e1e3964b5aafae3587b805c2edcf13922cccd44:system/services/file/file_service.c'], cwd=root))
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = shlex.split(v)
obj = area / 'file_service.o'
link = shlex.split((build / 'CMakeFiles/sim-configured.dir/link.txt').read_text())
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                '-I' + str(root / 'system/services/file'), '-c', str(source), '-o', str(obj)], cwd=build, check=True)
tests = {t['name']: t['command'] for t in json.loads(subprocess.check_output(
    [str(Path(sys.executable).with_name('ctest')), '--test-dir', str(build), '--show-only=json-v1'], cwd=root))['tests']}
summary = {}
for test, target in [('scenario-reader_basic-metalio-plus-pqbn', 'sim-configured'),
                     ('sms', 'sim-sms'), ('settings-b', 'sim-settings-b'), ('confirm-pages', 'sim-confirm-pages')]:
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    binary = area / ('baseline-' + target)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, x in enumerate(link) if x.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    cmd = list(tests[test])
    if test == 'confirm-pages':
        cmd = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
               '--script', str(root / 'tests/confirm_pages/pages.script'), '--budget', str(root / 'tests/ui_scene.budget.json'),
               '--hub', str(root / 'tests/ui_shell.hub.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(area / 'ConfirmWeb'), '--quiet', '--no-random-outages']
    elif test == 'settings-b':
        cmd += ['--binary', str(binary)]
    else:
        cmd = [str(binary) if x == str(build / target) else x for x in cmd]
        if test == 'sms':
            cmd[-1] = str(area / 'sms')
        else:
            cmd[cmd.index('--out') + 1] = str(area / 'reader')
    result = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=180,
                            env={**os.environ, 'INK_KEEP_EVIDENCE': '1', 'UBSAN_OPTIONS': 'halt_on_error=1', 'INK_CONFIRM_SCENE': 'ConfirmWeb'})
    (area / (test + '.log')).write_text(result.stdout + result.stderr)
    summary[test] = {'exit': result.returncode, 'command': cmd}
    print(test, 'exit', result.returncode, result.stdout[-800:], result.stderr[-1800:], flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
assert all(row['exit'] == 0 for row in summary.values()), 'baseline must pass all four checks'
