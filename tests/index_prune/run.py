"""Exercise complete, interrupted and full card rescans and their real restarts."""
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
p.add_argument('--baseline', help='manual negative control; ctest needs no history')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('index-prune-baseline' if a.baseline else 'index-prune')
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / 'sim-index-prune'
if a.baseline:
    source = area / 'file_service.c'
    source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':system/services/file/file_service.c'], cwd=ROOT))
    flags = {}
    for line in (build / 'CMakeFiles/sim-index-prune.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-index-prune.dir/link.txt').read_text())
    obj = area / 'boundary.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
        '-c', str(ROOT / 'tests/index_prune/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x.endswith('/tests/index_prune/boundary.c.o') else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)

summary = {}
for case in ('complete', 'remove', 'swap', 'full'):
    path = area / case
    content = path / 'sd/content'
    for root in ('', '.m/owner', '.m/kid'):
        (content / root).mkdir(parents=True, exist_ok=True)
    full = case == 'full'
    for i in range(1024 if full else 60):
        root = '' if i < (1000 if full else 30) else '.m/owner' if i < (1016 if full else 50) else '.m/kid'
        file = content / root / f'SYNTH_{i:04d}.txt'
        file.write_bytes(b'X' * (i + 1))
        os.utime(file, (1000000 + i, 1000000 + i))
    previous = None
    for stage in ('exercise', 'restart'):
        env = {**os.environ, 'INK_PRUNE_MODE': case, 'UBSAN_OPTIONS': 'halt_on_error=1'}
        if previous:
            env['INK_PRUNE_OLD_IDS'] = ','.join(str(int(previous[f'test.prune.old_{j}']['max'])) for j in range(25))
            env['INK_PRUNE_LARGEST'] = str(int(previous['test.prune.largest']['max']))
        out = path / stage
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
            '--out', str(out), '--end', '240s', '--quiet', '--no-random-outages'], cwd=ROOT,
            env=env, capture_output=True, text=True, timeout=100)
        (path / f'{stage}.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text())
        m = data['metrics']
        ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
        summary[f'{case}-{stage}'] = {k: v for k, v in m.items() if k.startswith('test.')}
        if a.baseline:
            assert not ok and result.returncode == 0 and m.get('test.finished') == 1 and m.get('test.fail', 0) > 0, (case, stage, data.get('warnings'))
            allowed = ('pruned index equals', 'absent from the database', 'loses its cached thumbnail',
                       'complete card rescan', 'new file gets a fresh id')
            assert all(any(s in str(w) for s in allowed) for w in data.get('warnings', [])), data.get('warnings')
        elif not ok:
            raise SystemExit(f'FAIL {case}-{stage}: exit={result.returncode} {data.get("warnings")}\n{result.stderr[-2000:]}')
        else:
            assert m['test.index.max_hold_us']['max'] < 250000, 'index batches must release edits within 250 ms'
            if previous:
                for key, value in m.items():
                    if key.startswith(('test.prune.renamed_', 'test.prune.new_')):
                        assert value == previous[key], 'restart must retain the newly durable file ids'
        print(f'{"EXPECTED_FAIL" if a.baseline else "PASS"} {case}-{stage}: '
              f'checks={m.get("test.pass", 0)} failures={m.get("test.fail", 0)}', flush=True)
        previous = m
        if a.baseline:
            break
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
