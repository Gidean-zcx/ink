"""Verify legacy migration and exact-length records across real process restarts."""
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
p.add_argument('--baseline', help='manual negative control; not needed by ctest')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('index-compact-baseline' if a.baseline else 'index-compact')
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / 'sim-index-compact'
if a.baseline:
    source = area / 'file_service.c'
    source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':system/services/file/file_service.c'], cwd=ROOT))
    flags = {}
    for line in (build / 'CMakeFiles/sim-index-compact.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-index-compact.dir/link.txt').read_text())
    obj = area / 'boundary.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
                    '-c', str(ROOT / 'tests/index_compact/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x.endswith('/tests/index_compact/boundary.c.o') else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)

summary = {}
for schema in (1, 2):
    path = area / f'v{schema}'
    content = path / 'sd/content'
    private = content / '.m/owner'
    private.mkdir(parents=True)
    for i, file in enumerate((content / 'SYNTH_OLD.txt', content / '中文合成.txt', private / 'SYNTH_PRIVATE.txt')):
        file.write_bytes(b'X' * (i + 1))
        os.utime(file, (1, 1))
    ids = None
    for stage in (f'seed{schema}', 'unchanged', 'rewrite', 'mixed', 'upgrade', 'reboot'):
        out = path / stage
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
            '--out', str(out), '--end', '60s', '--quiet', '--no-random-outages'], cwd=ROOT,
            env={**os.environ, 'INK_COMPACT_STAGE': stage, 'INK_COMPACT_SCHEMA': str(schema), 'UBSAN_OPTIONS': 'halt_on_error=1'},
            capture_output=True, text=True, timeout=60)
        (path / f'{stage}.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text())
        m = data['metrics']
        ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
        summary[f'v{schema}-{stage}'] = {k: v for k, v in m.items() if k.startswith('test.')}
        if a.baseline:
            assert not ok and m.get('test.fail', 0) > 0 and result.returncode == 0 and m.get('test.finished') == 1 and all(
                any(message in str(w) for message in ('155-byte UTF-8 path', 'every upgraded record',
                    'completion', 'completed legacy or mixed restart', 'unchanged legacy and mixed',
                    'writing one legacy record'))
                for w in data.get('warnings', [])), (stage, result.stderr[-2000:], data.get('warnings'))
        elif not ok:
            raise SystemExit(f'FAIL v{schema}-{stage}: exit={result.returncode} {data.get("warnings")}\n{result.stderr[-2000:]}')
        current = [m[f'test.compact.id_{i}']['max'] for i in range(3)]
        if ids is None:
            ids = current
        assert current == ids and all(ids), 'migration and restart preserve every original id'
        print(f'{"NEGATIVE_CONTROL" if a.baseline else "PASS"} v{schema}-{stage}: ids={current} '
              f'checks={m.get("test.pass")} failures={m.get("test.fail", 0)}', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
