"""Run settings-gap touch/AI checks; optional baseline controls need local history."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = build / 'settings-gaps-evidence'
area.mkdir(parents=True, exist_ok=True)
binary = build / 'sim-settings-gaps'
summary = []
catalogue = (ROOT / 'docs/system/capabilities.md').read_text()
inventory = (ROOT / 'docs/tasks/settings-gaps/INVENTORY.md').read_text()
mutations = {line.split('|')[1].strip(' `') for line in catalogue.split('## 设置')[0].splitlines()
             if line.startswith('| `') and any(effect in line for effect in
                 (' / local_write', ' / local_destructive', ' / system', ' / external'))}
listed = inventory.split('## 全部非只读能力复核')[1].split('## 全部设置项复核')[0]
assert mutations == {line.split('|')[1].strip(' `') for line in listed.splitlines() if line.startswith('| `')}
settings = {line.split('|')[1].strip(' `') for line in catalogue.splitlines() if '`settings.get` / `settings.set`' in line}
assert settings == {line.split('|')[1].strip(' `') for line in inventory.split('## 全部设置项复核')[1].splitlines() if line.startswith('| `')}


def run(name, executable, expected=None):
    out = area / name
    shutil.rmtree(out, ignore_errors=True)
    result = subprocess.run([str(executable), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/settings_gaps.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(out / 'user.img'), '--sd-dir', str(out / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=180,
        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    (out / 'run.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    with (out / 'run.log').open('a') as log:
        log.write('\n'.join(data.get('warnings', [])) + '\n')
    counts = data.get('metrics', {})
    passed = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0) and not data.get('warnings')
    summary.append({'case': name, 'exit': result.returncode, 'passed': passed,
                    'checks': counts.get('test.pass', 0), 'failures': counts.get('test.fail', 0),
                    'finished': counts.get('test.finished', 0)})
    if expected:
        assert result.returncode and counts.get('test.finished') == 1 and counts.get('test.fail', 0) > 0, summary[-1]
        assert not passed and expected in (out / 'run.log').read_text(), (name, result.stderr[-4000:])
        print(f'{name}: expected failure ({expected})', flush=True)
    elif not passed:
        raise SystemExit(f'{name}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}')
    else:
        print(f'{name}: PASS checks={counts.get("test.pass")}', flush=True)


if not a.reverse:
    run('fixed', binary)
else:
    flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
    options = []
    for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
        options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
    for name, path, expected in (
        ('baseline-navigation', 'system/ui/shell/navigation.c', 'Wi-Fi touch target binds the existing capability'),
        ('baseline-files', 'system/apps/files/files.c', 'file actions snapshot exposes copy picker'),
    ):
        source = subprocess.run(['git', 'show', '5e1e3964b5aafae3587b805c2edcf13922cccd44:' + path],
                                cwd=ROOT, capture_output=True, text=True, check=True).stdout
        link = shlex.split((build / 'CMakeFiles/sim-settings-gaps.dir/link.txt').read_text())
        obj, executable = area / (name + '.o'), area / (name + '-sim')
        subprocess.run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                       input=source, text=True, cwd=build, check=True)
        link[link.index('-o') + 1] = str(executable)
        link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
        subprocess.run(link, cwd=build, check=True)
        run(name, executable, expected)
    log = (area / 'baseline-navigation/run.log').read_text()
    assert 'notification snapshot exposes clear' in log and 'clear touch target binds the existing capability' in log
    assert (area / 'baseline-navigation/notifications.txt').read_text().startswith('{"page":"Notifications"')
(area / ('reverse-summary.json' if a.reverse else 'summary.json')).write_text(json.dumps(summary, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
