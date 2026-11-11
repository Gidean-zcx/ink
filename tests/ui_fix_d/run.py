"""Manual ui-fix-d regression, screenshots and baseline negative controls; no Git history is needed for after."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {
    'network': 'system/ui/shell/navigation.c',
    'memo': 'system/apps/memo/memo.c',
    'boot': 'system/ui/shell/boot_flow.c',
}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--phase', choices=('before', 'after', 'reverse'), default='after')
p.add_argument('--baseline', help='Required only for manual before/reverse runs')
p.add_argument('--case', choices=tuple(SOURCES))
p.add_argument('--size', choices=('100', '120'))
a = p.parse_args()
if a.phase != 'after' and not a.baseline:
    p.error('--baseline is required for a manual before/reverse run')
build = a.build.resolve()
work = build / 'ui-fix-d' / a.phase
work.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/sim-memo.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
template = shlex.split((build / 'CMakeFiles/sim-memo.dir/link.txt').read_text())
summary = []
all_ok = True
for case in ([a.case] if a.case else SOURCES):
    defines = []
    if a.phase != 'after':
        original = work / f'{case}-product.c'
        original.write_bytes(subprocess.check_output(['git', 'show', f'{a.baseline}:{SOURCES[case]}'], cwd=ROOT))
        defines.append('-DINK_UI_FIX_D_SOURCE=' + json.dumps(str(original)))
    if a.phase == 'before':
        defines.append('-DINK_UI_FIX_D_BEFORE=1')
    driver = ROOT / 'tests/ui_fix_d' / f'{case}.c'
    obj = work / f'{case}.o'
    command = [template[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I' + str(ROOT / Path(SOURCES[case]).parent), *defines, '-c', str(driver), '-o', str(obj)]
    subprocess.run(command, cwd=build, check=True)
    # Preserve the exact standalone compile command for the repository's GCC check.
    database = json.loads((build / 'compile_commands.json').read_text())
    database = [entry for entry in database if entry['file'] != str(driver)]
    database.append({'file': str(driver), 'directory': str(build), 'arguments': command})
    (build / 'compile_commands.json').write_text(json.dumps(database))
    link = [str(obj) if word.endswith('/tests/memo/memo.c.o') else word for word in template]
    binary = work / case
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    for size in ([a.size] if a.size else ('100', '120')):
        out = work / f'{case}-{size}'
        if out.exists():
            shutil.rmtree(out)
        env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
        env.pop('INK_UI_FIX_D_LARGE', None)
        if size == '120':
            env['INK_UI_FIX_D_LARGE'] = '1'
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/i18n_apps.script'), '--assets', str(build / 'assets'),
            '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages'],
            cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
        (work / f'{case}-{size}.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
        metrics = data.get('metrics', {})
        passed = result.returncode == 0 and metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) == 0 and not data.get('warnings')
        expected = not passed if a.phase == 'reverse' else passed
        if a.phase == 'reverse':
            expected = metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0 and any('CHECK FAILED' in item for item in data.get('warnings', []))
        row = {'case': case, 'size': size, 'exit': result.returncode, 'checks': metrics.get('test.pass', 0),
            'failures': metrics.get('test.fail', 0), 'finished': metrics.get('test.finished', 0), 'expected': expected}
        row['failed_checks'] = [item.split(') ', 1)[-1] for item in data.get('warnings', []) if 'CHECK FAILED' in item]
        summary.append(row)
        print(a.phase, json.dumps(row), flush=True)
        all_ok &= expected
        if not expected:
            print(result.stderr[-6000:], data.get('warnings', []), flush=True)
        if a.phase != 'reverse':
            target = ROOT / 'docs/tasks/ui-fix-d/screenshots' / f'{a.phase}-{size}'
            target.mkdir(parents=True, exist_ok=True)
            for source in sorted(out.glob('snap-*.png')):
                destination = target / source.name.removeprefix('snap-')
                shutil.copyfile(source, destination)
                if destination.stat().st_size > 300 * 1024:
                    raise SystemExit(f'PNG exceeds 300 KB: {destination.name}')
                row.setdefault('screenshots', []).append({'name': destination.name,
                    'bytes': destination.stat().st_size, 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
(work / 'results.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(0 if all_ok else 1)
