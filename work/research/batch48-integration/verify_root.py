import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch48-integration'
S = R / 'work/research/batch48-maimemo-device'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def csvrows(p):
    return list(csv.DictReader(p.open()))

flows = csvrows(L / 'coverage.csv')
evidence = [json.loads(x) for x in (L / 'evidence.jsonl').read_text().splitlines()]
rp = csvrows(L / 'reference-points.csv')
fm = {x['flow_id']: x for x in flows}
em = {x['evidence_id']: x for x in evidence}
assert len(flows) == 963 and len(evidence) == 1696 and len(rp) == 106
assert Counter(x['depth'] for x in flows) == {'D1': 78, 'D2': 865, 'D4': 20}
assert len({x['url'] for x in evidence}) == 1525
assert len({x['reference_id'] for x in rp}) == len(rp)
for x in rp:
    assert len(x) == 15 and all(x.values()), x['reference_id']
    assert all(k in fm and fm[k]['product_id'] == x['product_id'] for k in x['flow_ids'].split(';'))
    assert all(k in em for k in x['evidence_ids'].split(';'))
    if x['reference_id'].startswith('RP-M023-'):
        assert all(em[k]['product_id'] == 'M-023' for k in x['evidence_ids'].split(';'))
    assert (L / x['artifact_path']).exists(), x['reference_id']
assert sum(x['product_id'] == 'M-023' for x in rp) == 8
assert sum(x['product_id'] == 'M-023' for x in evidence) == 6

case = (L / 'cases/M-023-maimemo-device.md').read_text()
assert set(re.findall(r'F-M023-\d+', case)) == {f'F-M023-{i:02d}' for i in range(1, 8)}
mf = [x for x in flows if x['product_id'] == 'M-023']
assert len(mf) == 7 and all(x['hands_on'] == 'false' for x in mf)
assert all(fm[f'F-M023-{i:02d}']['depth'] == 'D2' for i in range(1, 7))
assert fm['F-M023-07']['depth'] == 'D1'
owners = {x['flow_id']: [] for x in mf}
for p in (L / 'batches').glob('*-coverage.csv'):
    for x in csvrows(p):
        if x['flow_id'] in owners:
            owners[x['flow_id']].append(str(p.relative_to(R)))
assert all(len(x) == 1 for x in owners.values())
assert all(owners[f'F-M023-{i:02d}'][0].endswith('/vocab-coverage.csv') for i in range(1, 6))
assert owners['F-M023-06'][0].endswith('/vocab-device-deepening-coverage.csv')
assert owners['F-M023-07'][0].endswith('/maimemo-device-controls-coverage.csv')
for update in json.loads((W / 'old-flow-updates.json').read_text()):
    assert fm[update['before']['flow_id']] == update['after']
    assert update['before']['depth'] == update['after']['depth']
for update in json.loads((W / 'old-evidence-updates.json').read_text()):
    assert em[update['before']['evidence_id']] == update['after']

audit = json.loads((L / 'artifacts/maimemo-device-controls/capture-audit.json').read_text())
fetch = json.loads((S / 'fetch.json').read_text())
elements = json.loads((S / 'elements.json').read_text())
assert audit['http'] == fetch
assert sha(R / audit['raw_work_path']) == fetch['sha256'] == '8efad5579fd525c6d65240800272dfd99ec96b7a1435b6a0ecd8b0c1710f9044'
assert (R / audit['raw_work_path']).stat().st_size == 117802
assert sha(R / audit['extracted_work_path']) == audit['extracted_sha256']
assert len(audit['all_source_images']) == 26 and audit['images_viewed'] == 24 and audit['images_archived'] == 21
assert len(audit['observations']) == 24
archived = []
for x in audit['observations']:
    assert x['status'] == 200 and x['hands_on'] is False and x['continuous_sequence'] is False
    assert sha(R / x['path']) == x['sha256'] and (R / x['path']).stat().st_size == x['bytes']
    assert x['url'] == audit['all_source_images'][x['index'] - 1]['src']
    if x.get('archived_artifact'):
        assert sha(L / x['archived_artifact']) == x['sha256']
        archived.append(x['index'])
assert archived == [1, 2, 3, 4, 5, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]
record = em['E-MMDS-01']
assert record['derived_from'] == 'E-MMOH-01' and record['url'] == em['E-MMDVC-01']['url']
assert record['hands_on'] is False and record['visual_observed'] is True
assert sha(L / record['archived_artifact']) == record['sha256']
for rel, digest in record['artifact_hashes'].items():
    assert sha(L / rel) == digest, rel
capture = em['E-MMDVC-01']['additional_captures'][-1]
for key, digest in [('raw_work_path', 'raw_sha256'), ('extracted_work_path', 'extracted_sha256')]:
    assert sha(R / capture[key]) == capture[digest]
assert sha(L / capture['archived_artifact']) == capture['sha256']

inv = csvrows(L / 'batches/maimemo-device-controls-inventory.csv')
assert len(inv) == 41 and sum(x['entry_type'] == 'group' for x in inv) == 5
ids = {x['entry_id'] for x in inv}
assert len(ids) == 41
for row in inv:
    assert not row['parent_id'] or row['parent_id'] in ids
    assert all(k in em for k in row['evidence_ids'].split(';'))
    assert all(k in fm for k in row['related_flow_ids'].split(';') if k)
assert all(not row['related_flow_ids'] for row in inv if 'Q10' in row['title'])
bluetooth = next(row for row in inv if 'Q11' in row['title'])
assert bluetooth['related_flow_ids'] == 'F-M023-04' and '未来声明' in bluetooth['gaps']
# Q11 is linked as a limitation of the existing audio task, not as a delivered task.
assert '未来功能' in case and '蓝牙耳机' in case
helper = json.loads((W / 'helper-source-check.json').read_text())
assert helper['ok'] and helper['inventory']['exact_order_title_anchor_match']

reqsha = sha(R / 'docs/requirements.md')
assert reqsha == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs = sorted((R / 'docs/briefs').glob('[0-9]*.md'))
idx = (R / 'docs/briefs/README.md').read_text()
assert len(briefs) == 48 and all(p.name in idx for p in briefs)
old = json.loads((W / 'before-manifest.json').read_text())
paths = [R / 'README.md', R / 'AGENTS.md', *sorted((R / 'docs').rglob('*'))]
now = {str(p.relative_to(R)): sha(p) for p in paths if p.is_file()}
new = sorted(set(now) - set(old))
modified = sorted(k for k in set(now) & set(old) if now[k] != old[k])
deleted = sorted(set(old) - set(now))
assert not deleted
assert 'docs/requirements.md' not in modified and 'AGENTS.md' not in modified
for rel in new + modified:
    if rel.endswith('.md'):
        assert all(line.rstrip() == line for line in (R / rel).read_text().splitlines()), rel
result = dict(status='passed', total_flows=963, total_evidence=1696, total_reference_points=106,
              M023_flows=7, M023_depths={'D2': 6, 'D1': 1}, M023_evidence=6, M023_reference_points=8,
              source_headings=41, images_observed=24, images_archived=21, all_briefs_indexed=48,
              flow_owners=owners, requirements_unchanged=True, requirements_sha256=reqsha,
              new_formal_files=new, modified_formal_files=modified, deleted_formal_files=deleted,
              semantic_review='Root rechecked key image 09/11/14/23 and full body; retained stage, direction, timer-unit and static-result limits. Other prior observations retained with their audit.',
              scope='Document and evidence consistency; no product behavior or learning efficacy verified')
(W / 'root-final-verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
(W / 'final-manifest.json').write_text(json.dumps(now, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ['status', 'total_flows', 'total_evidence', 'total_reference_points', 'M023_depths', 'requirements_unchanged']}, ensure_ascii=False))
print('formal files:', len(new), 'new,', len(modified), 'modified,', len(deleted), 'deleted')
