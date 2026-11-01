"""One-time reviewed merge of Yoto and Shanbay proposals; do not rerun."""
import csv
import hashlib
import io
import json
from pathlib import Path

ROOT = Path('/home/workspace/reader')
WORK = ROOT / 'work/ground-controls-deepening'
LIB = ROOT / 'docs/research/interaction-library'
AUDIT = WORK / 'applied-helper-updates.json'
assert not AUDIT.exists(), 'Already applied; do not rerun'

def read_json(path):
    return json.loads((ROOT / path).read_text())

tables = {}
for pattern, key in [('*-coverage.csv', 'flow_id'), ('*-evidence.jsonl', 'evidence_id')]:
    for path in sorted((LIB / 'batches').glob(pattern)):
        raw = path.read_text()
        rows = list(csv.DictReader(io.StringIO(raw))) if key == 'flow_id' else [json.loads(s) for s in raw.splitlines() if s.strip()]
        tables[path] = {'rows': rows, 'key': key, 'raw': raw}

changes = []
def apply(item, key, whole):
    found = []
    for path, tab in tables.items():
        if tab['key'] != key:
            continue
        for row in tab['rows']:
            if row[key] != item[key]:
                continue
            if whole:
                assert row in [item['before'], item['after']], (item[key], path, 'before mismatch')
                row.clear()
                row.update(item['after'])
            else:
                field = item['field']
                assert row.get(field) in [item['before'], item['after']], (item[key], field, path, 'before mismatch')
                row[field] = item['after']
            found.append(str(path.relative_to(ROOT)))
    assert found, ('Missing', item[key])
    changes.append({**item, 'updated_files': found})

for item in read_json('work/yoto-controls-deepening/old-coverage-updates.json'):
    apply(item, 'flow_id', True)
for item in read_json('work/yoto-controls-deepening/old-evidence-additional-captures.json'):
    apply(item, 'evidence_id', True)
for item in read_json('work/shanbay-controls-deepening-2/existing-coverage-updates.json'):
    apply(item, 'flow_id', False)
for item in read_json('work/shanbay-controls-deepening-2/additional-captures-updates.json'):
    apply(item, 'evidence_id', False)

changed_files = sorted({f for change in changes for f in change['updated_files']})
before_dir = WORK / 'reviewed-before'
before_dir.mkdir(exist_ok=True)
file_audit = []
for rel in changed_files:
    assert (ROOT / rel).read_text() == tables[ROOT / rel]['raw'], ('Concurrent change', rel)
for rel in changed_files:
    path = ROOT / rel
    tab = tables[path]
    before = path.read_bytes()
    assert path.read_text() == tab['raw']
    backup = before_dir / path.name
    if not backup.exists():
        backup.write_bytes(before)
    original_before = backup.read_bytes()
    if tab['key'] == 'flow_id':
        stream = io.StringIO(newline='')
        writer = csv.DictWriter(stream, fieldnames=list(tab['rows'][0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(tab['rows'])
        after = stream.getvalue().encode()
    else:
        after = ''.join(json.dumps(row, ensure_ascii=False) + '\n' for row in tab['rows']).encode()
    path.write_bytes(after)
    file_audit.append({'path': rel, 'before_sha256': hashlib.sha256(original_before).hexdigest(), 'after_sha256': hashlib.sha256(after).hexdigest()})

result = {'checked_on': '2026-10-27', 'recovered_partial_write': 'Initial newline-format assertion stopped after four files; second strict-before attempt stopped before writes; recovery accepts only exact before/after and preserves original backups.', 'flow_ids': sorted({c['flow_id'] for c in changes if 'flow_id' in c}),
          'field_changes': 23 + 26, 'evidence_ids': sorted({c['evidence_id'] for c in changes if 'evidence_id' in c}),
          'files': file_audit, 'changes': changes}
AUDIT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k: v for k, v in result.items() if k not in ['changes', 'files']}, ensure_ascii=False))
