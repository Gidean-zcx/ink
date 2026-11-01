from pathlib import Path
import csv
import hashlib
import json
import re
from urllib.parse import unquote

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch30-closeout'
flows = {r['flow_id']: r for r in csv.DictReader((L / 'coverage.csv').open())}
evidence = {e['evidence_id']: e for e in map(json.loads, (L / 'evidence.jsonl').read_text().splitlines())}
verified = 0
for name, fn in [('meebook-reading-controls-deepening', 'manifest.json'), ('xiaoyuan-camera-preview', 'manifest.json'), ('hiby-library-controls-deepening', 'formal-manifest.json'), ('hiby-library-controls-deepening', 'raw-manifest.json')]:
    d = json.loads((R / 'work' / name / fn).read_text())
    items = d.items() if isinstance(d, dict) else [(x['path'], x['sha256']) for x in d]
    for p, h in items:
        assert hashlib.sha256((R / p).read_bytes()).hexdigest() == h, p
        verified += 1
for line in (R / 'work/bbdc-learning-controls-deepening/frozen-manifest.sha256').read_text().splitlines():
    h, name = line.split(maxsplit=1)
    p = R / name.lstrip('* ')
    assert hashlib.sha256(p.read_bytes()).hexdigest() == h, str(p)
    verified += 1

changes = []
for name, fn in [('meebook-reading-controls-deepening', 'coverage-updates-applied.json'), ('xiaoyuan-camera-preview', 'coverage-update-applied.json'), ('hiby-library-controls-deepening', 'old-coverage-updates.json')]:
    changes.extend(json.loads((R / 'work' / name / fn).read_text()))
field_count = 0
for c in changes:
    assert flows[c['flow_id']] == c['after'], c['flow_id']
    field_count += sum(c['before'][k] != c['after'][k] for k in c['before'])
for c in json.loads((R / 'work/bbdc-learning-controls-deepening/existing-coverage-updates.json').read_text()):
    assert flows[c['flow_id']] == c['replacement_row']
    field_count += len(c['changes'])
    changes.append(c)
assert len(changes) == 11 and field_count == 35
for c in json.loads((R / 'work/hiby-library-controls-deepening/old-evidence-additional-captures.json').read_text()):
    e = evidence[c['evidence_id']]
    assert e['url'] == c['expected_url']
    assert e['additional_captures'] == c['before_additional_captures'] + c['append_additional_captures']

p = L / 'inventories/README.md'
paths = set()
for target in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)', p.read_text()):
    paths.add((p.parent / unquote(target)).resolve())
rows_count = 0
used_e = set()
used_f = set()
for p in paths:
    assert p.exists(), p
    rows = list(csv.DictReader(p.open()))
    rows_count += len(rows)
    for row in rows:
        text = ' '.join(str(v) for v in row.values())
        es = set(re.findall(r'E-[A-Z]+-\d{2}', text))
        fs = set(re.findall(r'F-M\d{3}-\d{2}', text))
        assert es <= set(evidence), (p, es - set(evidence))
        assert fs <= set(flows), (p, fs - set(flows))
        used_e |= es
        used_f |= fs
assert len(paths) == 76 and rows_count == 7306, (len(paths), rows_count)
requirement_hash = hashlib.sha256((R / 'docs/requirements.md').read_bytes()).hexdigest()
assert requirement_hash == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
print(json.dumps(dict(status='PASS', manifest_entries_checked=verified, old_flows=len(changes), old_fields=field_count, inventory_csvs=len(paths), inventory_rows=rows_count, distinct_inventory_e=len(used_e), distinct_inventory_f=len(used_f), requirements_sha256=requirement_hash), ensure_ascii=False))
