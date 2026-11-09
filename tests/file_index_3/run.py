"""Exercise automatic card indexing, persistence, card removal and real storage UI."""
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
p.add_argument('--baseline')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('file-index-3-baseline' if a.baseline else 'file-index-3')
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / 'sim-file-index-3'
if a.baseline:
    source = area / 'file_service.c'
    source.write_bytes(subprocess.check_output(['git', 'show', f'{a.baseline}:system/services/file/file_service.c'], cwd=ROOT))
    flags = {}
    for line in (build / 'CMakeFiles/sim-file-index-3.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles/sim-file-index-3.dir/link.txt').read_text())
    obj = area / 'baseline.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
        '-c', str(ROOT / 'tests/file_index_3/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)


def seed(path):
    content = path / 'sd/content'
    content.mkdir(parents=True)
    for i in range(1000):
        (content / f'SYNTH_{i:04d}.txt').write_bytes(b'X')
    for member, size in [('owner', 17), ('kid', 11)]:
        target = content / '.m' / member
        target.mkdir(parents=True)
        (target / 'SYNTH_PRIVATE.txt').write_bytes(b'X' * size)


summary = {}


def run(path, stage, negative=False):
    out = path / stage
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
        '--out', str(out), '--end', '200s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env={**os.environ, 'INK_INDEX_3_STAGE': 'legacy' if a.baseline and stage == 'compare' else stage, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=150)
    (path / f'{stage}.stdout.log').write_text(result.stdout)
    (path / f'{stage}.stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    m = data.get('metrics', {})
    ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    summary[stage] = {k: v for k, v in m.items() if k.startswith(('test.index.', 'file.index.'))}
    if negative:
        assert not ok and m.get('test.fail', 0) > 0, 'baseline must fail automatic-index acceptance'
    elif not ok:
        raise SystemExit(f'FAIL {stage} exit={result.returncode}\n{result.stderr[-2000:]}\n{data.get("warnings")}')
    print(f'{"EXPECTED_FAIL" if negative else "PASS"} {stage} checks={m.get("test.pass")} {summary[stage]}', flush=True)
    if not a.baseline and stage in ('boot', 'ui', 'remove', 'swap'):
        assert m['test.index.max_hold_us']['max'] < 250000, 'batch edit hold must stay under 250 ms in simulator'
    return m


if a.baseline:
    path = area / 'comparison'
    seed(path)
    old = run(path, 'compare')
    current = json.loads((build / 'test-out/file-index-3/summary.json').read_text())
    assert current['compare']['test.index.writes']['max'] * 10 <= old['test.index.writes']['max'], 'writes must fall by an order of magnitude'
    path = area / 'negative'
    seed(path)
    run(path, 'boot', negative=True)
else:
    path = area / 'persist'
    seed(path)
    first = run(path, 'boot')
    second = run(path, 'reboot')
    assert first['test.index.first_id']['max'] == second['test.index.first_id']['max'], 'actual restart retains ids'
    for stage in ('ui', 'remove', 'swap'):
        path = area / stage
        seed(path)
        run(path, stage)
    path = area / 'comparison'
    seed(path)
    run(path, 'compare')
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
