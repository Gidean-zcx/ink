import csv
import hashlib
import json
import re
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/ground-controls-deepening'
def read_json(path):
    return json.loads(path.read_text())
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

checked_paths = set()
def check_sources(obj):
    if isinstance(obj, list):
        for value in obj:
            check_sources(value)
    elif isinstance(obj, dict):
        for path_key, hash_key in [('raw_work_path','raw_sha256'), ('extracted_work_path','extracted_sha256'), ('extracted_text_work_path','extracted_text_sha256')]:
            if obj.get(path_key):
                path = R / obj[path_key]
                assert path.is_file(), path
                assert sha(path) == obj[hash_key], path
                checked_paths.add(str(path.relative_to(R)))
        if obj.get('parsed_text_path'):
            assert (R / obj['parsed_text_path']).is_file()
        if obj.get('archived_artifact') and obj.get('sha256'):
            path = L / obj['archived_artifact']
            assert sha(path) == obj['sha256'], path
        for value in obj.values():
            if isinstance(value, (dict, list)):
                check_sources(value)

evidence = [json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()]
flows = list(csv.DictReader((L/'coverage.csv').open()))
bye = {e['evidence_id']:e for e in evidence}
byf = {f['flow_id']:f for f in flows}
new_e = []
new_f = []
names = ['ground-controls-deepening', 'yoto-controls-deepening', 'shanbay-controls-deepening-2']
for name in names:
    new_e.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
    new_f.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(new_e) == 38 and len(new_f) == 10
assert all(e == bye[e['evidence_id']] and not e['hands_on'] for e in new_e)
assert all(f == byf[f['flow_id']] and f['depth'] == 'D2' for f in new_f)
check_sources(new_e)

changed_flows = set()
field_count = 0
for item in read_json(W/'applied-existing-updates.json'):
    assert byf[item['flow_id']][item['field']] == item['after']
    changed_flows.add(item['flow_id'])
    field_count += 1
changed_evidence = set()
for item in read_json(W/'applied-existing-evidence-updates.json'):
    assert bye[item['evidence_id']] == item['after']
    check_sources(item['after'].get('additional_captures', []))
    changed_evidence.add(item['evidence_id'])

audit = read_json(W/'applied-helper-updates.json')
for item in audit['changes']:
    if 'flow_id' in item:
        row = byf[item['flow_id']]
        if 'field' in item:
            assert row[item['field']] == item['after']
            field_count += 1
        else:
            assert row == item['after']
            field_count += len(item['changed_fields'])
        changed_flows.add(item['flow_id'])
    else:
        row = bye[item['evidence_id']]
        assert (row[item['field']] == item['after']) if 'field' in item else row == item['after']
        check_sources(row.get('additional_captures', []))
        changed_evidence.add(item['evidence_id'])
for item in audit['files']:
    assert sha(R/item['path']) == item['after_sha256']
    assert sha(W/'reviewed-before'/Path(item['path']).name) == item['before_sha256']
assert len(changed_flows) == 21 and field_count == 80 and len(changed_evidence) == 20

# Validate every indexed inventory, including evidence prefixes containing digits.
index = L/'inventories/README.md'
paths = {(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)', index.read_text())}
counts = {}
for path in paths:
    rows = list(csv.DictReader(path.open()))
    counts[str(path.relative_to(L))] = len(rows)
    for row in rows:
        assert None not in row and None not in row.values(), path
        for value in row.values():
            assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}', value)) <= set(bye), (path, value)
            assert set(re.findall(r'F-M\d{3}-\d{2}', value)) <= set(byf), (path, value)
assert len(counts) == 29 and sum(counts.values()) == 2553
for name, count in zip(names, [76, 123, 68]):
    path = L/f'batches/{name}-inventory.csv'
    rows = list(csv.DictReader(path.open()))
    key = next(iter(rows[0]))
    ids = {row[key] for row in rows}
    assert len(rows) == len(ids) == count
    assert b'\r' not in path.read_bytes() and path.read_bytes().endswith(b'\n')
    for row in rows:
        assert not row.get('parent_id') or row['parent_id'] in ids, (path, row)

html_count = 0
for name in ['sources-initial.json','sources-categories.json','sources-selected.json','sources-more.json','sources-feedback-category.json']:
    sources = read_json(W/name)
    assert all(source['status'] == 200 for source in sources)
    check_sources(sources)
    html_count += len(sources)
assert html_count == 35
images = read_json(W/'image-manifest.json')
assert len(images) == 3
for image in images:
    assert image['status'] == 200
    assert sha(R/image['path']) == image['sha256']
    assert sha(L/'artifacts/ground-controls-deepening'/image['name']) == image['sha256']
attempt = read_json(L/'artifacts/ground-controls-deepening/browser-access-attempt.json')
assert attempt['navigation_status'] == 403 and attempt['product_actions'] == attempt['screenshots'] == 0
brief = (R/'docs/briefs/014-news-learning-offline-evidence.md').read_text()
assert set(re.findall(r'\]\((https?://[^)]+)\)', brief)) <= {e['url'] for e in evidence}

report = dict(new_evidence=38,new_tasks=10,new_D2=10,new_D4=0,old_tasks_amended=21,
              old_fields_amended=80,existing_evidence_additional_captures=20,
              inventories=29,inventory_rows=2553,new_inventory_rows=267,
              ground_public_html=35,new_archived_static_images=3,new_device_operations=0,
              raw_files_checked=len(checked_paths),errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
