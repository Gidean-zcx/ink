"""Run audit5 regressions using real capabilities and persistent simulated flash/card."""
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
p.add_argument('--case', choices=['confirmation', 'index', 'receipts'], required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--baseline', help='compile this case against one product file from the given commit')
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-audit5-fix'
area = build / 'test-out' / ('audit5-' + a.case + ('-baseline' if a.baseline else ''))
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
failed = False

if a.baseline:
    relative = {'confirmation': 'system/policy/confirm.c', 'index': 'system/services/file/file_service.c',
                'receipts': 'system/ui/shell/ai_pages.c'}[a.case]
    source = area / Path(relative).name
    source.write_bytes(subprocess.check_output(['git', 'show', f'{a.baseline}:{relative}'], cwd=ROOT))
    target = 'sim-audit5-fix' if a.case == 'index' else 'ink_system'
    flags = {}
    for line in (build / f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-audit5-fix.dir/link.txt').read_text())
    obj = area / 'baseline.o'
    extra = ['-I' + str((ROOT / relative).parent)]
    if a.case == 'index':
        extra += ['-DINK_INDEX_SOURCE=' + json.dumps(str(source))]
        source = ROOT / 'tests/file_index_3/boundary.c'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], *extra,
                    '-c', str(source), '-o', str(obj)], cwd=build, check=True)
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    if a.case == 'index':
        link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
    else:
        link.insert(next(i for i, x in enumerate(link) if x.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)


def run(path, mode, old_id=0):
    global failed
    out = path / mode
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
        '--out', str(out), '--end', '80s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env={**os.environ, 'INK_AUDIT5_MODE': mode, 'INK_AUDIT5_OLD_ID': str(old_id),
             'UBSAN_OPTIONS': 'halt_on_error=1'}, capture_output=True, text=True, timeout=120)
    (path / (mode + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    if mode.endswith('-cut'):
        ok = ok and m.get('test.power_cut') == 1
    print('PASS' if ok else 'FAIL', mode, 'exit', result.returncode, 'checks', m.get('test.pass'),
          'failures', m.get('test.fail', 0), flush=True)
    if not ok:
        failed = True
        print(result.stderr[-3000:], data.get('warnings', []), flush=True)
    return int(m.get('test.old_id', {}).get('max', 0))


if a.case == 'index':
    for kind in ('create', 'rename'):
        path = area / kind
        content = path / 'sd/content'
        content.mkdir(parents=True)
        (content / 'SYNTHETIC_OLD.txt').write_bytes(b'XXX')
        old_id = run(path, kind + '-cut')
        run(path, kind + '-reboot', old_id)
else:
    run(area, a.case)
if not failed and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(1 if failed else 0)
