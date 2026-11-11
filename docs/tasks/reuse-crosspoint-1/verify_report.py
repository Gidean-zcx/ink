#!/usr/bin/env python3
"""Offline checks for this research deliverable; no product tests or network."""
import argparse
import collections
import json
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', default='6bf4b0014b963558d747c2a4cc2a7bc3e2ca167c')
args = parser.parse_args()
folder = pathlib.Path(__file__).resolve().parent
repo = folder.parents[2]
prefix = str(folder.relative_to(repo)) + '/'

def git(*words):
    return subprocess.check_output(['git', '-C', str(repo), *words], text=True)

def require(ok, message):
    if not ok:
        raise SystemExit('FAIL: ' + message)

report = (folder / 'REPORT.md').read_text()
selected = re.findall(r'^\| `([^`]+)`', (folder / 'picks.md').read_text(), re.M)
mapping = json.loads((folder / 'picks-mapping.json').read_text())
ids = [row['id'] for row in mapping]
require(len(set(selected)) == len(selected), 'duplicate source selection')
require(collections.Counter(ids) == collections.Counter(selected), 'selection coverage')
for row in mapping:
    require(set(row['depends_on']) <= set(selected), 'unknown feature dependency')
    require(row['effort'] in ('小于半天', '1～2 天', '更多'), 'effort category')
    require(len(re.findall(r'^\| `' + re.escape(row['id']) + '` ', report, re.M)) == 1, 'report row ' + row['id'])
    require(row['source'] in report and row['limits'] in report, 'mapping/report drift ' + row['id'])
print('selection: %d rows, %d unique; mapping: %d; missing: 0; extra: 0' % (len(selected), len(set(selected)), len(ids)))

queue = json.loads((folder / 'port-queue.json').read_text())
qids = {row['id'] for row in queue}
require(len(qids) == len(queue), 'duplicate task ID')
external = {'ui-input-2', 'ui-audit-2', 'comm-ux-2', 'port-cell-3', 'port-display-3', 'port-board-2', 'port-wifi-2', 'port-bt-audio-2', 'port-power-2'}
owners = {}
seen = set()
for row in queue:
    require(row['hours'] in (1, 2), 'task timebox')
    require(set(row['depends_on']) <= qids | external, 'unknown task dependency')
    require(set(row['depends_on']) & qids <= seen, 'task order/cycle')
    for name in row['writes']:
        require(not name.startswith('/') and '..' not in pathlib.PurePosixPath(name).parts, 'unsafe write path')
        for previous, owner in owners.items():
            left, right = name.removesuffix('/**'), previous.removesuffix('/**')
            require(left != right and not left.startswith(right + '/') and not right.startswith(left + '/'), 'write conflict ' + owner + '/' + row['id'])
        owners[name] = row['id']
    seen.add(row['id'])
    require('| `' + row['id'] + '` ' in report or '`' + row['id'] + '` ' in report, 'task absent in report')
print('queue: %d tasks, %d hours; ordered DAG; overlapping write sets: 0' % (len(queue), sum(row['hours'] for row in queue)))

evidence = json.loads((folder / 'source-evidence.json').read_text())
require(evidence['baseline'] == args.baseline, 'baseline mismatch')
require(len({s['id'] for s in evidence['sources']}) == len(evidence['sources']), 'source IDs')
require(evidence['vendor_original_head_before'] == evidence['vendor_original_head_after'], 'vendor HEAD moved')
for source in evidence['sources']:
    require(re.fullmatch('[0-9a-f]{40}', source['commit']) is not None, 'unfixed source')
    for item in source['retrieved_files']:
        require(re.fullmatch('[0-9a-f]{64}', item['sha256']) is not None, 'file hash')
print('provenance: %d pinned sources, %d retrieved file hashes; vendor HEAD unchanged' % (len(evidence['sources']), evidence['retrieved_file_count']))

links = re.findall(r'\]\(([^)]+)\)', report)
local_count = 0
for target in links:
    if '://' in target or target.startswith('#'):
        continue
    local_count += 1
    require((folder / target.split('#')[0]).exists(), 'missing local link ' + target)
for heading in ('验收表', '没做到的', '和设计不一致的', '改过的已有测试', '要用户定的', '顺手发现', '新增的机制', '要改的文档'):
    require('## ' + heading in report, 'missing section ' + heading)
known_sources = {item['url'] for source in evidence['sources'] for item in source['retrieved_files']}
blob_links = [target for target in links if target.startswith('https://github.com/') and '/blob/' in target]
require(all(target in known_sources for target in blob_links), 'source link without retrieved evidence')
print('document links: %d local targets exist; %d fixed source links have retrieved evidence; required sections present' % (local_count, len(blob_links)))

names = git('diff', '--name-status', args.baseline, '--').splitlines()
require(all(line.startswith('A\t' + prefix) for line in names), 'changed existing/out-of-scope file')
tracked = git('diff', args.baseline + '..HEAD', '--')
private = re.compile(r'[/]Users[/]|[/]private[/]tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}')
credentials = re.compile(r'(?:sk|ghp)[-][A-Za-z0-9]{20,}|AKIA[A-Z0-9]{16}')
require(not private.search(tracked) and not credentials.search(tracked), 'private data in committed diff')
for file in folder.iterdir():
    if file.is_file():
        require(file.stat().st_size < 5 * 1024 * 1024, 'oversize file')
        body = file.read_text()
        require(not private.search(body) and not credentials.search(body), 'private data in ' + file.name)
require(not git('diff', '--check', args.baseline), 'whitespace errors')
print('scope: additions only in task directory; product code: 0 lines; privacy scan: 0 matches; diff check: clean')
