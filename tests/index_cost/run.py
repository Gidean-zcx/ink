"""Reproduce file.index cost without changing product code or its CMake targets."""
import argparse
import csv
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', help='manual comparison of the file service from a Git commit')
p.add_argument('--cases', nargs='+', choices=['cold', 'prepare', 'warm', 'probe', 'control', 'typ', 'max'],
               default=['cold', 'prepare', 'warm', 'probe', 'control', 'typ', 'max'])
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('index-cost-baseline' if a.baseline else 'index-cost')
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/sim-storage-order.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-storage-order.dir/link.txt').read_text())
source = (ROOT / 'system/services/storage/storage_service.c').read_text()
start = '            int r = run_job(&job);'
end = '            ink_probe_value("storage.write_ms", (double)(osal_now_us() - t0) / 1000.0);'
assert source.count(start) == source.count(end) == 1, 'timing hooks must match one storage job boundary'
source = source.replace(start, '            ink_cost_begin(job.path, job.kind, job.len, job.queued_us, t0);\n' + start)
source = source.replace(end, '            ink_cost_end(r);\n' + end)
instrumented = area / 'storage_service.c'
instrumented.write_text(source)
for old, new in [('tests/storage_order/device.c', 'device.c'),
                 ('tests/storage_order/data_boundary.c', 'data_boundary.c'),
                 ('tests/storage_order/storage_boundary.c', 'storage_boundary.c')]:
    obj = area / (new + '.o')
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(ROOT / 'system/services/storage'),
                    '-DINK_COST_STORAGE_SOURCE=' + json.dumps(str(instrumented)),
                    '-c', str(ROOT / 'tests/index_cost' / new), '-o', str(obj)], cwd=build, check=True)
    assert sum(x.endswith(old + '.o') for x in link) == 1
    link = [str(obj) if x.endswith(old + '.o') else x for x in link]
if a.baseline:
    baseline = area / 'file_service.c'
    baseline.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':system/services/file/file_service.c'], cwd=ROOT))
    obj = area / 'file_boundary.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(baseline)),
                    '-c', str(ROOT / 'tests/file_index_3/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    assert sum(x.endswith('/tests/file_index_3/boundary.c.o') for x in link) == 1
    link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
binary = area / 'sim-index-cost'
link[link.index('-o') + 1] = str(binary)
# Expose existing NOR counters from a build-directory copy; no timing change.
hal = area / 'hal_storage_sim.c'
hal.write_text((ROOT / 'ports/sim/hal/hal_storage_sim.c').read_text() + '''
void ink_cost_flash_counts(uint64_t out[3]) {
    out[0] = FL.erases; out[1] = FL.progs; out[2] = FL.prog_bytes;
}
''')
hal_flags = {}
for line in (build / 'CMakeFiles/ink_port_sim.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        hal_flags[key] = shlex.split(value)
hal_obj = area / 'hal_storage_sim.o'
subprocess.run([link[0], *hal_flags['C_DEFINES'], *hal_flags['C_INCLUDES'], *hal_flags['C_FLAGS'],
                '-c', str(hal), '-o', str(hal_obj)], cwd=build, check=True)
link.insert(next(i for i, x in enumerate(link) if x.endswith('.a')), str(hal_obj))
subprocess.run(link, cwd=build, check=True, capture_output=True)


def seed(path, count):
    content = path / 'sd/content'
    content.mkdir(parents=True)
    for i in range(count):
        file = content / f'SYNTH_{i:04d}.txt'
        file.write_bytes(b'SYNTH')
        os.utime(file, (1, 1))


def overlap(event, intervals):
    return sum(max(0, min(event['end_us'], x['end_us']) - max(event['begin_us'], x['begin_us']))
               for x in intervals)


def stats(events):
    duration = [(x['end_us'] - x['begin_us']) / 1000 for x in events]
    return {'count': len(events), 'bytes': sum(x.get('bytes', 0) for x in events),
            'erases': sum(x.get('erases', 0) for x in events),
            'program_bytes': sum(x.get('program_bytes', 0) for x in events),
            'total_ms': round(sum(duration), 3), 'min_ms': round(min(duration, default=0), 3),
            'max_ms': round(max(duration, default=0), 3),
            'each_ms': [round(x, 3) for x in duration]}


def summarize(events):
    writes = [x for x in events if x['type'] == 'storage']
    index = [x for x in writes if x['path'].startswith('/user/sys/file.index.') and x['job'] in (0, 1)]
    metadata = [x for x in writes if x['path'].startswith('/user/sys/file.index_ready.') and x['job'] in (0, 1)]
    waits = [x for x in events if x['type'] == 'wait']
    external = [x for x in waits if x['name'] != 'file.index' and x['name'] != 'file.index_ready']
    result = {name: stats(rows) for name, rows in [
        ('index', index), ('append', [x for x in index if x['job'] == 1]),
        ('compact', [x for x in index if x['job'] == 0]), ('completion', metadata),
        ('external_waits', external)]}
    result['external_overlap_ms'] = [round(overlap(x, index) / 1000, 3) for x in external]
    result['queued_overlap_ms'] = [round(overlap({**x, 'end_us': x['begin_us'], 'begin_us': x['queued_us']}, index) / 1000, 3)
                                   for x in writes if x not in index and x not in metadata and x['job'] in (0, 1)]
    result['phases'] = {}
    phases = [x for x in events if x['type'] == 'phase' and x['name'] != 'probe-request']
    for i, phase in enumerate(phases[:-1]):
        end_us = phases[i + 1]['begin_us']
        selected = [x for x in index if phase['begin_us'] <= x['begin_us'] < end_us]
        result['phases'][phase['name']] = {
            'append': stats([x for x in selected if x['job'] == 1]),
            'compact': stats([x for x in selected if x['job'] == 0]),
            'completion': stats([x for x in metadata if phase['begin_us'] <= x['begin_us'] < end_us])}
    return result


summary = {}
for case in a.cases:
    path = area / case
    if path.exists():
        shutil.rmtree(path)
    path.mkdir()
    if case == 'warm':
        if not (area / 'prepare/user.img').exists():
            raise SystemExit('warm requires a completed prepare case in this evidence directory')
        shutil.copytree(area / 'prepare/sd', path / 'sd')
        shutil.copyfile(area / 'prepare/user.img', path / 'user.img')
    else:
        seed(path, 0 if case == 'control' else 1000)
    mode = 'hour' if case in ('cold', 'warm') else case if case in ('probe', 'control') else 'boot'
    timing = case if case in ('typ', 'max') else 'tail'
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'),
               '--flash-image', str(path / 'user.img'), '--out', str(path / 'out'),
               '--end', '3600s' if mode == 'hour' else '60s', '--quiet', '--no-random-outages',
               '--seed', '1', '--flash-timing', timing]
    completed = subprocess.run(command, cwd=ROOT, env={**os.environ, 'INK_COST_MODE': mode,
                               'UBSAN_OPTIONS': 'halt_on_error=1'}, capture_output=True, text=True, timeout=300)
    (path / 'stdout.log').write_text(completed.stdout)
    (path / 'stderr.log').write_text(completed.stderr)
    metrics = json.loads((path / 'out/metrics.json').read_text())
    m = metrics['metrics']
    if completed.returncode or m.get('test.finished') != 1 or m.get('test.fail', 0) or metrics.get('warnings'):
        raise SystemExit(f'FAIL {case}: exit={completed.returncode}, finished={m.get("test.finished")}, '
                         f'failures={m.get("test.fail")}, warnings={metrics.get("warnings")}\n{completed.stderr[-3000:]}')
    events = [json.loads(line[5:]) for line in completed.stdout.splitlines() if line.startswith('COST ')]
    assert events and all(x.get('result', 0) == 0 for x in events), 'all measured jobs and durability waits must succeed'
    (path / 'events.json').write_text(json.dumps(events, ensure_ascii=False, indent=2) + '\n')
    summary[case] = summarize(events)
    summary[case]['checks'] = m['test.pass']
    summary[case]['final_index_records'] = m.get('test.cost.final_index_records', {}).get('max')
    summary[case]['physical_files'] = sum(1 for x in (path / 'sd/content').rglob('*')
                                          if x.is_file() and not x.name.startswith('.'))
    if mode == 'hour':
        assert summary[case]['physical_files'] == 1000, 'declared one-hour workload must leave 1000 real files'
    summary[case]['flash'] = {k: v for k, v in m.items() if k.startswith('flash.') and not isinstance(v, dict)}
    (area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    print(f'PASS {case}: ' + json.dumps({k: v for k, v in summary[case].items() if k not in ('phases', 'flash')}, ensure_ascii=False), flush=True)

if 'probe' in summary and 'control' in summary:
    assert max(summary['probe']['external_overlap_ms'], default=0) > 1000, 'controlled waiter must overlap compaction by over one second'
    assert max(summary['control']['external_overlap_ms'], default=0) == 0, 'no-index control must have no index blocking'
with (area / 'measurements.csv').open('w', newline='') as output:
    fields = ['case', 'type', 'name', 'job', 'bytes', 'queued_us', 'begin_us', 'end_us',
              'duration_us', 'queue_us', 'index_overlap_us', 'result', 'erases', 'program_bytes']
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator='\n')
    writer.writeheader()
    for case in summary:
        events = json.loads((area / case / 'events.json').read_text())
        intervals = [x for x in events if x['type'] == 'storage' and
                     x['path'].startswith('/user/sys/file.index.') and x['job'] in (0, 1)]
        for event in events:
            row = {key: event[key] for key in fields if key in event}
            row.update(case=case, name=event.get('name', event.get('path')))
            if 'end_us' in event:
                row['duration_us'] = event['end_us'] - event['begin_us']
                if 'queued_us' in event:
                    row['queue_us'] = event['begin_us'] - event['queued_us']
                    queued = {**event, 'begin_us': event['queued_us'], 'end_us': event['begin_us']}
                    row['index_overlap_us'] = overlap(queued, intervals)
                else:
                    row['index_overlap_us'] = overlap(event, intervals)
            writer.writerow(row)
print('Evidence: ' + str(area.relative_to(ROOT)), flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
