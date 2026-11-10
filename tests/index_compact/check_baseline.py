"""Manual baseline and review negative controls; never changes working sources."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--revert', choices=('strip', 'lock', 'sms', 'navigation', 'all'))
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
area = root / 'work/index-compact' / ((f'reverse-{a.revert}-' if a.revert else 'baseline-check-') + build.name)
area.mkdir(parents=True, exist_ok=True)
def build_flags(target):
    flags = {}
    for line in (build / f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    return flags

link = shlex.split((build / 'CMakeFiles/sim-configured.dir/link.txt').read_text())
sources = {}
if a.revert:
    changes = [
        ('strip', 'system/ui/ui_task.c', '#define STRIP_ROWS 128', '#define STRIP_ROWS 96'),
        ('lock', 'system/ui/ui_task.c', '    if (ink_power_transition_pending()) return true;\n', ''),
        ('sms', 'system/apps/sms/sms.c', '"message.received", received, NULL, INK_EXEC_DIRECT', '"message.received", received, NULL, INK_EXEC_BG'),
        ('navigation', 'system/ui/shell/navigation.c', '    if (ctx && !strcmp(id, "power.state")) return; /* No status-bar field changes. */\n', ''),
        ('navigation', 'system/ui/shell/navigation.c', 'strncmp(id, "time.", 5) && strcmp(id, "notify.unread")',
         'strncmp(id, "time.", 5) && strcmp(id, "power.state") && strcmp(id, "notify.unread")'),
    ]
    for fix, name, before, after in changes:
        if a.revert not in (fix, 'all'):
            continue
        text = sources.get(name, (root / name).read_text())
        assert text.count(before) == 1, (fix, 'negative control must change exactly one site')
        sources[name] = text.replace(before, after)
else:
    name = 'system/services/file/file_service.c'
    sources[name] = subprocess.check_output(['git', 'show', '5e1e3964b5aafae3587b805c2edcf13922cccd44:' + name], cwd=root, text=True)
objects = []
for name, text in sources.items():
    flags = build_flags('sim-sms' if name == 'system/apps/sms/sms.c' else 'ink_system')
    source = area / Path(name).name
    source.write_text(text)
    if name == 'system/apps/sms/sms.c':
        text = (root / 'tests/sms/sms.c').read_text().replace('"../../system/apps/sms/sms.c"', json.dumps(str(source)))
        source = area / 'sms-test.c'
        source.write_text(text)
    obj = source.with_suffix('.o')
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I' + str(root / Path(name).parent), '-I' + str(root / 'tests/sms'), '-I' + str(root / 'simulator'),
        '-c', str(source), '-o', str(obj)], cwd=build, check=True)
    objects.append(obj)
tests = {t['name']: t['command'] for t in json.loads(subprocess.check_output(
    [str(Path(sys.executable).with_name('ctest')), '--test-dir', str(build), '--show-only=json-v1'], cwd=root))['tests']}
summary = {}
for test, target in [('scenario-reader_basic-metalio-plus-pqbn', 'sim-configured'),
                     ('sms', 'sim-sms'), ('settings-b', 'sim-settings-b'), ('confirm-pages', 'sim-confirm-pages')]:
    fixes = {'strip': 'sim-configured', 'sms': 'sim-sms', 'lock': 'sim-settings-b', 'navigation': 'sim-confirm-pages'}
    if a.revert and a.revert != 'all' and target != fixes[a.revert]:
        continue
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    binary = area / ('baseline-' + target)
    link[link.index('-o') + 1] = str(binary)
    for obj in objects:
        if obj.stem == 'sms-test':
            if target == 'sim-sms':
                link = [str(obj) if x.endswith('/tests/sms/sms.c.o') else x for x in link]
        else:
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
    evidence = result.stdout + result.stderr
    if test == 'confirm-pages':
        evidence += json.dumps(json.loads((area / 'ConfirmWeb/metrics.json').read_text()).get('warnings', []))
    (area / (test + '.log')).write_text(evidence)
    summary[test] = {'exit': result.returncode, 'command': cmd}
    print(test, 'exit', result.returncode, result.stdout[-800:], result.stderr[-1800:], flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
expected = 1 if a.revert else 0
assert all(row['exit'] == expected for row in summary.values()), 'every control must have its expected exit status'
if a.revert:
    messages = {'sim-configured': 'ui.action_to_first_change_ms.p95', 'sim-sms': 'capture is settled',
                'sim-settings-b': 'image style has no labels over the photo', 'sim-confirm-pages': 'screenshot is fully presented'}
    for test, row in summary.items():
        target = 'sim-configured' if test.startswith('scenario-') else 'sim-' + test
        assert messages[target] in (area / (test + '.log')).read_text(), 'fail for the original regression, not another error'
