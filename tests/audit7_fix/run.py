"""Exercise four batch7 regressions; history is used only with explicit --baseline."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=['b7-01', 'b7-02', 'b7-03', 'b7-04'], required=True)
p.add_argument('--baseline', help='manual negative control; requires the specified commit')
a = p.parse_args()
build = a.build.resolve()
phone = a.case in ('b7-01', 'b7-02')
target = 'sim-phone-calls' if phone else 'sim-audit7-file'
binary = build / target
area = build / 'test-out' / ('audit7-' + a.case + ('-baseline' if a.baseline else ''))
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)

if a.baseline:
    def flags(name):
        result = {}
        for line in (build / f'CMakeFiles/{name}.dir/flags.make').read_text().splitlines():
            if ' = ' in line:
                key, value = line.split(' = ', 1)
                result[key] = shlex.split(value)
        return result

    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())

    def compile_source(relative, name, driver=None, define=None):
        source = area / (name + '.c')
        source.write_bytes(subprocess.check_output(['git', 'show', f'{a.baseline}:{relative}'], cwd=ROOT))
        f = flags(target if driver else 'ink_system')
        extra = ['-I' + str((ROOT / relative).parent)]
        if driver:
            extra += ['-D' + define + '=' + json.dumps(str(source))]
            source = ROOT / driver
        obj = area / (name + '.o')
        subprocess.run([link[0], *f['C_DEFINES'], *f['C_INCLUDES'], *f['C_FLAGS'], *extra,
                        '-c', str(source), '-o', str(obj)], cwd=build, check=True)
        return str(obj)

    if phone:
        obj = compile_source('system/apps/phone/phone.c', 'phone', 'tests/phone_calls/phone_calls.c', 'INK_PHONE_SOURCE')
        link = [obj if x.endswith('/tests/phone_calls/phone_calls.c.o') else x for x in link]
        for relative in ('system/services/comm/comm_service.c', 'system/policy/confirm.c'):
            obj = compile_source(relative, Path(relative).stem)
            link.insert(next(i for i, x in enumerate(link) if x.endswith('.a')), obj)
    else:
        obj = compile_source('system/services/file/file_service.c', 'file', 'tests/audit7_fix/file_boundary.c', 'INK_INDEX_SOURCE')
        link = [obj if x.endswith('/tests/audit7_fix/file_boundary.c.o') else x for x in link]
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)

content = area / 'sd/content'
content.mkdir(parents=True)
if a.case == 'b7-04':
    directory = content / 'SYNTHETIC_DIR'
    directory.mkdir()
    for i in range(300):
        (directory / f'FILE_{i:03}.txt').write_bytes(b'X')
else:
    (content / 'SYNTHETIC_OLD.txt').write_bytes(b'XXX')
host_file = area / 'SYNTHETIC_HOST.txt'
host_file.write_bytes(b'XXXXX')
failed = False
summary = {}
modes = {'b7-01': ['audit7-rebind'], 'b7-02': ['audit7-privacy'],
         'b7-03': ['usb-cut', 'usb-reboot'], 'b7-04': ['directory']}[a.case]
for mode in modes:
    out = area / mode
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--sd-dir', str(area / 'sd'),
               '--flash-image', str(area / 'user.img'), '--out', str(out), '--end', '240s',
               '--quiet', '--no-random-outages']
    if phone:
        command += ['--script', str(ROOT / 'tests/phone_calls/calls.script')]
    result = subprocess.run(command, cwd=ROOT, env={**os.environ, 'INK_PHONE_PHASE': mode,
        'INK_AUDIT7_MODE': mode, 'INK_AUDIT7_HOST_FILE': str(host_file), 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=180)
    (area / (mode + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    metrics = data.get('metrics', {})
    ok = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0) and not data.get('warnings')
    if mode == 'usb-cut':
        ok = ok and metrics.get('test.power_cut') == 1 and metrics.get('usb.copies_from_host') == 1
    if a.case == 'b7-02':
        ok = ok and '246802' not in result.stdout + result.stderr
    summary[mode] = {'exit': result.returncode, 'passed': metrics.get('test.pass'), 'failed': metrics.get('test.fail', 0),
                     'warnings': data.get('warnings', []), 'measurements': {k: v for k, v in metrics.items() if k.startswith('test.audit7.')}}
    print('PASS' if ok else 'FAIL', a.case, mode, json.dumps(summary[mode], ensure_ascii=False), flush=True)
    failed |= not ok
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not failed and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(1 if failed else 0)
