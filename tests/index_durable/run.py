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
p.add_argument('--review-baseline', default='de1c411e', help='negative control before ID-H1 was fixed')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/index-durable'
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
flags = {}
for line in (build / 'CMakeFiles/sim-index-durable.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)


def compile_baseline(commit, name):
    source = area / (name + '.c')
    source.write_bytes(subprocess.check_output(['git', 'show', commit + ':system/services/file/file_service.c'], cwd=ROOT))
    link = shlex.split((build / 'CMakeFiles/sim-index-durable.dir/link.txt').read_text())
    obj = area / (name + '.o')
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
                    '-c', str(ROOT / 'tests/file_index_3/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    binary = area / (name + '-bin')
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    return binary


def have(commit):
    return subprocess.run(['git', 'cat-file', '-e', commit + '^{commit}'], cwd=ROOT, capture_output=True).returncode == 0


# CI checks out one commit: the baseline comparisons and the v1 migration need history, so they run where it exists.
baseline = compile_baseline(a.baseline, 'baseline') if have(a.baseline) else None
if not baseline:
    print('SKIP baseline latency comparison and v1 migration: commit', a.baseline, 'is not in this clone', flush=True)
summary = {}


def run(name, binary, path=None, mode='', old_id=0, expect_fail=False):
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
    if expect_fail:
        assert result.returncode == 0 and metrics.get('test.finished') == 1 and metrics.get('test.fail') == 7, (name, metrics, result.stderr[-3000:])
        assert len(data.get('warnings', [])) == 7 and all(
            '100 overwrites add zero' in str(w) or 'known index loads add zero card size queries' in str(w)
            for w in data['warnings']), data.get('warnings')
    else:
        assert result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0) and not data.get('warnings'), (
            name, result.returncode, result.stderr[-3000:], data.get('warnings'))
    return metrics


for name, binary in [('baseline', baseline), ('fixed', build / 'sim-index-durable')]:
    if not binary:
        continue
    metrics = run(name, binary)
    summary[name] = {op: metrics['test.durable.' + op + '_ms'] for op in ('create', 'rename', 'delete')}
    print(name, json.dumps(summary[name]), flush=True)
for op in ('create', 'rename', 'delete'):
    new = summary['fixed'][op]
    assert new['count'] == 5, 'five real refresh samples per operation'
    if baseline:
        old = summary['baseline'][op]
        assert old['count'] == 5 and new['max'] <= old['max'] + 100, (op, 'latency increase exceeds 100 ms', old, new)
if baseline:
    legacy = area / 'migration'
    old_id = int(run('seed', baseline, legacy, 'seed')['test.durable.old_id']['max'])
    run('upgrade', build / 'sim-index-durable', legacy, 'upgrade', old_id)
    run('reboot', build / 'sim-index-durable', legacy, 'upgrade', old_id)
    print('PASS v1 migration and restart preserve id', old_id, flush=True)
review_baseline = compile_baseline(a.review_baseline, 'review-baseline') if have(a.review_baseline) else None
if not review_baseline:
    print('SKIP negative control before the review fix: commit', a.review_baseline, 'is not in this clone', flush=True)
for name, binary in [('before-review', review_baseline), ('after-review', build / 'sim-index-durable')]:
    if not binary:
        continue
    metrics = run(name, binary, mode='overwrite', expect_fail=name == 'before-review')
    summary[name] = {kind: {key: metrics[f'test.durable.{kind}.{key}']['max']
                           for key in ('markers', 'runs', 'notifications')} for kind in ('open', 'atomic')}
    print(name, json.dumps(summary[name]), flush=True)
run('mutations', build / 'sim-index-durable', mode='mutations')
print('PASS new open, rename, copy and deletion marker boundaries', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
