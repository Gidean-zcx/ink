"""Measure storage priority, index writes and physical input with synthetic files."""
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
p.add_argument('--baseline', help='compile original storage, datastore and index from this commit')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('storage-order-baseline' if a.baseline else 'storage-order')
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / 'sim-storage-order'

if a.baseline:
    flags = {}
    for line in (build / 'CMakeFiles/sim-storage-order.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles/sim-storage-order.dir/link.txt').read_text())
    sources = [
        ('system/services/storage/storage_service.c', 'tests/storage_order/storage_boundary.c', 'INK_STORAGE_SOURCE'),
        ('system/services/data/datastore.c', 'tests/storage_order/data_boundary.c', 'INK_DATA_SOURCE'),
        ('system/services/file/file_service.c', 'tests/file_index_3/boundary.c', 'INK_INDEX_SOURCE')]
    for path, boundary, define in sources:
        text = subprocess.check_output(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT, text=True)
        if define == 'INK_STORAGE_SOURCE':
            # Signature adapter only: the original scheduler ignores the new priority hint.
            for name in ('ink_storage_append', 'ink_storage_write_borrow'):
                old = f'int {name}(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {{'
                assert text.count(old) == 1
                text = text.replace(old, old[:-3] + ', const unsigned *waiters) {\n    (void)waiters;')
        source = area / Path(path).name
        source.write_text(text)
        obj = source.with_suffix('.o')
        subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
            '-I' + str(ROOT / 'system/services/file'), '-DINK_ORDER_BASELINE=1',
            '-D' + define + '=' + json.dumps(str(source)), '-c', str(ROOT / boundary), '-o', str(obj)],
            cwd=build, check=True)
        old_obj = boundary + '.o'
        link = [str(obj) if word.endswith(old_obj) else word for word in link]
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)


def seed(path, count, nested=False):
    content = path / 'sd/content'
    content.mkdir(parents=True)
    for i in range(count):
        folder = content / f'SYNTH_DIR_{i:04d}' if nested else content
        folder.mkdir(exist_ok=True)
        (folder / f'SYNTH_{i:04d}.txt').write_bytes(b'X')


summary = {}
for mode, count in [('priority', 0), ('index', 1000), ('limit', 1024), ('pressure', 0), ('input', 1000), ('quiet', 0)]:
    path = area / mode
    seed(path, count, nested=mode == 'input')
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
        '--out', str(path / 'out'), '--end', '100s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env={**os.environ, 'INK_ORDER_MODE': mode, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=120)
    (path / 'stdout.log').write_text(result.stdout)
    (path / 'stderr.log').write_text(result.stderr)
    data = json.loads((path / 'out/metrics.json').read_text())
    m = data['metrics']
    summary[mode] = {k: v for k, v in m.items() if k.startswith(('test.order.', 'test.pass', 'test.fail', 'test.finished'))}
    summary[mode]['exit'] = result.returncode
    summary[mode]['failures'] = data.get('warnings', [])
    ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    # The unmodified baseline must fail the new priority, batching and pressure checks.
    expected_fail = bool(a.baseline and mode != 'input')
    if expected_fail:
        assert not ok and m.get('test.fail', 0), f'baseline {mode} must expose the original fault'
    elif not ok and not a.baseline:
        (area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
        raise SystemExit(f'FAIL {mode}: {summary[mode]}\n{result.stderr[-2000:]}')
    print(f'{"EXPECTED_FAIL" if expected_fail else "OBSERVED" if a.baseline else "PASS"} {mode}: '
          + json.dumps(summary[mode], ensure_ascii=False), flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
