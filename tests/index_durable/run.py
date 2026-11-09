"""Compare identical file operations under display activity with the task baseline."""
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
p.add_argument('--baseline', default='1705e7ca4c9e3fde0ff8465f636893a8d9567db4')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/index-durable'
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
source = area / 'file_service.c'
source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':system/services/file/file_service.c'], cwd=ROOT))
flags = {}
for line in (build / 'CMakeFiles/sim-index-durable.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-index-durable.dir/link.txt').read_text())
obj = area / 'baseline.o'
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
                '-c', str(ROOT / 'tests/file_index_3/boundary.c'), '-o', str(obj)], cwd=build, check=True)
baseline = area / 'baseline-bin'
link[link.index('-o') + 1] = str(baseline)
link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
subprocess.run(link, cwd=build, check=True, capture_output=True)
summary = {}


def run(name, binary, path=None, mode='', old_id=0):
    path = path or area / name
    content = path / 'sd/content'
    if not content.exists():
        content.mkdir(parents=True)
        (content / 'SYNTHETIC_OLD.txt').write_bytes(b'XXX')
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
        '--out', str(path / name), '--end', '300s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', **({'INK_DURABLE_MODE': mode,
             'INK_DURABLE_OLD_ID': str(old_id)} if mode else {})}, capture_output=True, text=True, timeout=180)
    (path / (name + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((path / name / 'metrics.json').read_text())
    metrics = data.get('metrics', {})
    assert result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0) and not data.get('warnings'), (
        name, result.returncode, result.stderr[-3000:], data.get('warnings'))
    return metrics


for name, binary in [('baseline', baseline), ('fixed', build / 'sim-index-durable')]:
    metrics = run(name, binary)
    summary[name] = {op: metrics['test.durable.' + op + '_ms'] for op in ('create', 'rename', 'delete')}
    print(name, json.dumps(summary[name]), flush=True)
for op in ('create', 'rename', 'delete'):
    old, new = summary['baseline'][op], summary['fixed'][op]
    assert old['count'] == new['count'] == 5, 'five real refresh samples per operation'
    assert new['max'] <= old['max'] + 100, (op, 'latency increase exceeds 100 ms', old, new)
legacy = area / 'migration'
old_id = int(run('seed', baseline, legacy, 'seed')['test.durable.old_id']['max'])
run('upgrade', build / 'sim-index-durable', legacy, 'upgrade', old_id)
run('reboot', build / 'sim-index-durable', legacy, 'upgrade', old_id)
print('PASS v1 migration and restart preserve id', old_id, flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
