import csv
import hashlib
import json
import re
from pathlib import Path

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
work = root / 'work/inoreader-controls-deepening'
new = []
for name in ['inoreader', 'quizlet', 'audible']:
    new.extend(json.loads(line) for line in (lib / f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines())
assert len(new) == 73
raw = {}
derived = set()
images = set()
for evidence in new:
    for record in [evidence] + evidence.get('additional_raw_sources', []):
        path = root / record['raw_work_path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == record['raw_sha256'], path
        raw[str(path.relative_to(root))] = record['raw_sha256']
    for key in ['source_notes_path', 'archived_artifact']:
        if evidence.get(key):
            assert (lib / evidence[key]).is_file(), evidence[key]
    if evidence.get('parsed_text_path'):
        assert (root / evidence['parsed_text_path']).is_file()
    if evidence.get('extracted_work_path'):
        path = root / evidence['extracted_work_path']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == evidence['extracted_sha256'], path
        derived.add(str(path.relative_to(root)))
    for rel, digest in evidence.get('artifact_hashes', {}).items():
        assert hashlib.sha256((lib / rel).read_bytes()).hexdigest() == digest, rel
        images.add(rel)
assert len(images) == 2

manifest = json.loads((root / 'work/audible-controls-deepening/source-manifest.json').read_text())
for record in manifest:
    data = (root / record['path']).read_bytes()
    assert len(data) == record['bytes'], record['path']
    assert hashlib.sha256(data).hexdigest() == record['sha256'], record['path']
assert len(manifest) == 35

index = lib / 'inventories/README.md'
paths = []
for rel in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)', index.read_text()):
    path = (index.parent / rel).resolve()
    if path not in paths:
        paths.append(path)
evidence_ids = {json.loads(line)['evidence_id'] for line in (lib / 'evidence.jsonl').read_text().splitlines()}
flows = list(csv.DictReader((lib / 'coverage.csv').open()))
flow_ids = {row['flow_id'] for row in flows}
counts = {}
for path in paths:
    rows = list(csv.DictReader(path.open()))
    counts[str(path.relative_to(lib))] = len(rows)
    for row in rows:
        assert None not in row and None not in row.values(), path
        for value in row.values():
            assert set(re.findall(r'E-[A-Z]+-\d{2}', value)) <= evidence_ids, (path, value)
            assert set(re.findall(r'F-M\d{3}-\d{2}', value)) <= flow_ids, (path, value)
assert len(counts) == 17 and sum(counts.values()) == 1317, counts

for name, count in [('inoreader', 316), ('quizlet', 68), ('audible', 53)]:
    path = lib / f'batches/{name}-controls-deepening-inventory.csv'
    data = path.read_bytes()
    assert b'\r' not in data and data.endswith(b'\n'), path
    rows = list(csv.DictReader(path.open()))
    assert len(rows) == count
    if name == 'inoreader':
        ids = {row['entry_id'] for row in rows}
        assert len(ids) == count
        assert all(not row['parent_id'] or row['parent_id'] in ids for row in rows)
    elif name == 'quizlet':
        ids = {row['order'] for row in rows}
        assert len(ids) == count
        assert all(not row['parent_order'] or row['parent_order'] in ids for row in rows)
    else:
        assert len({row['inventory_id'] for row in rows}) == count

audit = json.loads((work / 'applied-existing-updates.json').read_text())
by_flow = {row['flow_id']: row for row in flows}
for update in audit['updates']:
    assert by_flow[update['flow_id']][update['field']] == update['after'], update['flow_id']
assert len({u['flow_id'] for u in audit['updates']}) == 18
report = dict(new_evidence=73, distinct_raw_files_checked=len(raw), derived_extracts_hashed=len(derived), audible_manifest_files=35, static_images=2, inventories=17, inventory_rows=1317, new_directory_rows=437, new_tasks=16, existing_tasks_updated=18, existing_fields_updated=len(audit['updates']), errors=[])
(work / 'verification-result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False))
