"""Reproduce the restore failure without changing product sources or the fixed power-cut positions."""
import argparse
import json
import os
import shutil
import sys
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description="Rebuild changed units at the task baseline and cross-check restore data.")
p.add_argument('--build', type=Path, required=True)
p.add_argument('--current-data', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'factory-baseline'
area.mkdir(exist_ok=True)
base = '2228df7de99de9fea15fddc22868a0de1caae770'
commands = []

def original(unit):
    return subprocess.check_output(['git', 'show', f'{base}:{unit}'], text=True)

def flags(target):
    text = (build / f'CMakeFiles/{target}.dir/flags.make').read_text()
    result = []
    for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
        result += shlex.split(re.search('^' + key + r' = (.*)$', text, re.M)[1])
    return result

def run(command, **kwargs):
    commands.append(command)
    result = subprocess.run(command, cwd=build, capture_output=True, text=True, **kwargs)
    if result.returncode:
        print(result.stderr[-3000:])
        raise SystemExit(result.returncode)
    return result

link = shlex.split((build / 'CMakeFiles/sim-backup-restore.dir/link.txt').read_text())
objects = []
for unit in ('system/policy/system_actions.c', 'system/services/backup/backup_service.c',
             'system/services/file/file_service.c', 'system/ui/shell/settings_pages.c', 'system/ui/shell/boot_flow.c'):
    obj = area / (Path(unit).stem + '.o')
    run([link[0], *flags('ink_system'), '-iquote', str((root / unit).parent), '-x', 'c', '-', '-c', '-o', str(obj)], input=original(unit))
    objects.append(str(obj))
main = area / 'main.c'
main.write_text(original('system/boot/main.c'))
unit = 'tests/backup_restore/device.c'
source = original(unit).replace('#include "../../system/boot/main.c"', f'#include "{main}"')
obj = area / 'device.o'
run([link[0], *flags('sim-backup-restore'), '-iquote', str((root / unit).parent), '-x', 'c', '-', '-c', '-o', str(obj)], input=source)
for i, part in enumerate(link):
    if part.endswith('/tests/backup_restore/device.c.o'):
        link[i] = str(obj)
link[link.index('-o') + 1] = str(area / 'sim-backup-baseline')
index = next(i for i, part in enumerate(link) if part.endswith('libink_system.a'))
link[index:index] = objects
run(link)
result = subprocess.run([sys.executable,
    str(root / 'tests/backup_restore/run.py'), '--build', str(build), '--binary', str(area / 'sim-backup-baseline')],
    cwd=root, text=True, capture_output=True, timeout=180)
(area / 'backup-restore.log').write_text(result.stdout + result.stderr)
print('baseline backup-restore exit=', result.returncode)
assert result.returncode == 0, (result.stdout + result.stderr)[-3000:]
match = re.search(r'Evidence: (.+)', result.stdout)
assert match, 'run with INK_KEEP_EVIDENCE=1'
baseline_data = root / match[1]

def probe(source, binary, label, fails):
    target = area / label
    if target.exists():
        shutil.rmtree(target)
    target.mkdir()
    shutil.copyfile(source / 'empty/user.img', target / 'user.img')
    shutil.copytree(source / 'empty/sd', target / 'sd')
    peer = target / 'hub'
    shutil.copytree(source / 'hub', peer)
    for phase in ('flash-cut', 'flash-verify'):
        out = target / phase
        cmd = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
            '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
            '--end', '500s', '--quiet', '--no-random-outages', '--flash-image', str(target / 'user.img'), '--sd-dir', str(target / 'sd')]
        r = subprocess.run(cmd, text=True, capture_output=True, timeout=90,
            env={**os.environ, 'INK_BACKUP_PHASE': phase, 'INK_BACKUP_CUT': '45', 'INK_SIM_BACKUP_DIR': str(peer)})
        (target / (phase + '.log')).write_text(r.stdout + r.stderr)
        d = json.loads((out / 'metrics.json').read_text())
        m = d['metrics']
        assert r.returncode == 0, r.stderr
        if phase == 'flash-cut':
            assert m.get('flash.torn_ops', 0) > 0, 'power cut must actually tear a write'
        else:
            assert bool(m.get('test.fail', 0)) == fails, d['warnings']
            assert bool(m.get('backup.restored', 0)) != fails, m
            print(label, 'expected restore failure' if fails else 'restore passed', flush=True)
probe(a.current_data.resolve(), area / 'sim-backup-baseline', 'baseline-on-new-flash', True)
probe(baseline_data, build / 'sim-backup-restore', 'current-on-baseline-flash', False)
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
