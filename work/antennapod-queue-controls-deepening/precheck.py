"""Read-only checks against the pre-merge baseline; do not run after merging."""
from pathlib import Path
import csv
import hashlib
import json
import re

R = Path(__file__).resolve().parents[2]
W = R / 'work/antennapod-queue-controls-deepening'
L = R / 'docs/research/interaction-library'
stem = L / 'batches/antennapod-queue-controls-deepening'
def j(path): return json.loads(path.read_text())
def jl(path): return [json.loads(x) for x in path.read_text().splitlines() if x.strip()]
def cs(path): return list(csv.DictReader(path.open()))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
master_e = {x['evidence_id']: x for x in jl(L / 'evidence.jsonl')}
master_f = {x['flow_id']: x for x in cs(L / 'coverage.csv')}
old_e = j(W / 'evidence-before.json')
old_f = j(W / 'coverage-before.json')
assert all(master_e[x['evidence_id']] == x for x in old_e)
assert all(master_f[x['flow_id']] == x for x in old_f)
updates = j(W / 'old-coverage-updates.json')
assert len(updates) == 5 and sum(len(x['changed_fields']) for x in updates) == 18
for u in updates:
    assert master_f[u['flow_id']] == u['before']
    actual = {k: {'before': u['before'][k], 'after': v} for k, v in u['after'].items() if v != u['before'][k]}
    assert actual == u['changed_fields']
ev = jl(Path(str(stem) + '-evidence.jsonl'))
flows = cs(Path(str(stem) + '-coverage.csv'))
inv = cs(Path(str(stem) + '-inventory.csv'))
assert len(ev) == 9 and len(flows) == 1 and len(inv) == 158
assert len({e['evidence_id'] for e in ev}) == 9
assert len({e['url'] for e in ev}) == 9
for e in ev:
    assert e['evidence_id'] not in master_e
    assert e['url'] not in {o['url'] for o in master_e.values()}
    assert e['supersedes'] is None and e['hands_on'] is False and e['visual_observed'] is False
    assert not Path(e['archived_artifact']).is_absolute() and not e['archived_artifact'].startswith('work/')
    assert sha(L / e['archived_artifact']) == e['sha256']
    assert sha(R / e['raw_work_path']) == e['raw_sha256']
assert flows[0]['flow_id'] == 'F-M040-13' and flows[0]['flow_id'] not in master_f
assert flows[0]['depth'] == 'D2' and flows[0]['visual_status'] == 'not_observed' and flows[0]['hands_on'] == 'false'
ids = set(master_e) | {e['evidence_id'] for e in ev}
for row in inv + flows + [u['after'] for u in updates]:
    assert set(filter(None, row['evidence_ids'].split(';'))) <= ids
assert len([x for x in inv if x['inventory_id'].startswith('H-')]) == 47
assert len([x for x in inv if x['inventory_id'].startswith('C-')]) == 27
for suffix in ('-inventory.csv', '-coverage.csv'):
    b = Path(str(stem) + suffix).read_bytes()
    assert b'\r' not in b and b'\\r\\n' not in b
    assert b.count(b'\n') == len(cs(Path(str(stem) + suffix))) + 1
caps = j(W / 'old-evidence-additional-captures.json')
assert len(caps) == 1
for c in caps:
    e = master_e[c['evidence_id']]
    assert e['url'] == c['expected_url'] and e.get('additional_captures', []) == c['before_additional_captures']
    for a in c['append_additional_captures']:
        assert sha(R / a['raw_work_path']) == a['raw_sha256']
        assert sha(L / a['archived_artifact']) == a['sha256']
tree = {x['path']: x['sha'] for x in j(R / 'work/antennapod-controls-deepening/tree.json')['tree']}
fetches = j(W / 'fetch-log.json')
assert len(fetches) == 9 and all(x['status'] == 200 for x in fetches)
for x in fetches + [j(W / 'local-source.json')]:
    p = R / x['raw_work_path']
    assert sha(p) == x['raw_sha256']
    if 'repo_path' in x:
        b = p.read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(b)).encode() + b'\0' + b).hexdigest()
        assert blob == x['verified_git_blob'] == tree[x['repo_path']]
assert len(j(W / 'read-ranges.json')) == 10
case = L / 'cases/M-040-antennapod.md'
s = case.read_text()
assert len(re.findall(r'^\| F-M040-\d+ \|', s, re.M)) == 13
for target in re.findall(r'\]\((\.\./[^)]+)\)', s):
    assert (case.parent / target).exists(), target
for m in ('formal-manifest.json', 'raw-manifest.json'):
    for x in j(W / m): assert sha(R / x['path']) == x['sha256']
assert sha(W / 'case-before.md') == j(W / 'baseline.json')['case_sha256']
print('PASS: 9E/1F; 5 old F/18 fields; 1 old E capture; 158 inventory; 10 sources/7 blobs; LF/hash/reference/baseline checks. Read-only; no state changed.')
