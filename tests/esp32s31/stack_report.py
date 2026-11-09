#!/usr/bin/env python3
"""Report compiler frames and known direct-call paths, never a stack safety bound."""
import argparse
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--host', type=Path, help='Optional ordinary simulator test-out directory')
args = parser.parse_args()
frames, edges, labels = {}, {}, {}
for path in sorted(args.build.rglob('*.ci')):
    for line in path.read_text().splitlines():
        node = re.match(r'node: \{ title: "([^"]+)" label: "([^"]+)"', line)
        edge = re.match(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', line)
        if node:
            name, label = node.groups()
            size = re.search(r'\\n(\d+) bytes \(([^)]+)\)', label)
            if size:
                frames[name] = int(size[1])
                labels[name] = label.split(r'\n')[0]
        if edge:
            source, target = edge.groups()
            edges.setdefault(source, set()).add(target)

memo, cycles = {}, set()
def longest(name, active):
    if name in active:
        cycles.add(name)
        return 0, []
    if name in memo:
        return memo[name]
    best = (0, [])
    for child in sorted(edges.get(name, ())):
        candidate = longest(child, active | {name})
        if candidate[0] > best[0]:
            best = candidate
    result = frames.get(name, 0) + best[0], [name] + best[1]
    memo[name] = result
    return result

tasks = {'storage': ('storage_service.c:storage_task', 4096),
         'hub': ('hub_link.c:hub_task', 6144),
         'ui': ('ui_task.c:ui_task', 10240),
         'display': ('display_service.c:service_task', 3072),
         'audio': ('audio_service.c:audio_task', 26624),
         'comm': ('comm_service.c:comm_task', 8192),
         'power': ('power_service.c:power_task', 3072),
         'exec.media': ('exec.c:exec_task', 8192),
         'exec.bg': ('exec.c:exec_task', 8192)}
host = {}
if args.host:
    for path in sorted(args.host.rglob('metrics.json')):
        data = json.loads(path.read_text())
        if data.get('profile') != 'metalio-e-ink4-plus':
            continue
        for task in tasks:
            value = data.get('metrics', {}).get('task.' + task + '.stack_used_est_bytes')
            if isinstance(value, (int, float)) and value > host.get(task, (0, ''))[0]:
                host[task] = value, str(path.relative_to(args.host))
for task, (suffix, configured) in tasks.items():
    matches = [n for n in frames if n.endswith(suffix)]
    if not matches:
        raise SystemExit('Missing compiler entry: ' + suffix)
    name = matches[0]
    used, chain = longest(name, set())
    row = {'task': task, 'entry_frame': frames[name], 'known_direct_chain': used,
        'configured': configured, 'path': [labels.get(n, n.rsplit(':', 1)[-1]) for n in chain]}
    if args.host:
        row['host_estimate'], row['host_case'] = host.get(task, (None, None))
    print(json.dumps(row, ensure_ascii=False))
print('Excluded: indirect callbacks, missing library frames, recursive repetitions, RTOS context; '
      'these figures are not complete upper bounds. Recursive nodes: ' + str(len(cycles)))
