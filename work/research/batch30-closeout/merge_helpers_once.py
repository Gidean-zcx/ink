from pathlib import Path
import copy
import csv
import json

R = Path('/home/workspace/reader')
B = R / 'docs/research/interaction-library/batches'
W = R / 'work/research/batch30-closeout'
assert not (W / 'helpers-merged.json').exists(), 'one-time merge already applied'
for name in ['hiby-library-controls-deepening', 'bbdc-learning-controls-deepening']:
    assert (R / 'work' / name / 'HANDOFF.md').exists(), name

changes = json.loads((R / 'work/hiby-library-controls-deepening/old-coverage-updates.json').read_text())
for x in json.loads((R / 'work/bbdc-learning-controls-deepening/existing-coverage-updates.json').read_text()):
    after = x['replacement_row']
    before = copy.deepcopy(after)
    for k, v in x['changes'].items():
        assert after[k] == v['after']
        before[k] = v['before']
    changes.append(dict(flow_id=x['flow_id'], before=before, after=after))

updates = {}
hits = []
for p in B.glob('*-coverage.csv'):
    rows = list(csv.DictReader(p.open()))
    modified = False
    for i, f in enumerate(rows):
        for c in changes:
            if f['flow_id'] == c['flow_id']:
                assert f == c['before'], f['flow_id']
                rows[i] = c['after']
                modified = True
                hits.append(f['flow_id'])
    if modified:
        updates[p] = rows
assert len(hits) == len(changes) == len(set(hits)) == 6

echanges = json.loads((R / 'work/hiby-library-controls-deepening/old-evidence-additional-captures.json').read_text())
assert json.loads((R / 'work/hiby-library-controls-deepening/old-evidence-field-updates.json').read_text()) == []
assert json.loads((R / 'work/bbdc-learning-controls-deepening/existing-evidence-additional-captures.json').read_text()) == []
eupdates = {}
ehits = []
for p in B.glob('*-evidence.jsonl'):
    rows = [json.loads(s) for s in p.read_text().splitlines() if s.strip()]
    modified = False
    for e in rows:
        for c in echanges:
            if e['evidence_id'] == c['evidence_id']:
                assert e.get('additional_captures', []) == c['before_additional_captures']
                assert e['url'] == c['expected_url']
                e['additional_captures'] = c['before_additional_captures'] + c['append_additional_captures']
                modified = True
                ehits.append(e['evidence_id'])
    if modified:
        eupdates[p] = rows
assert len(ehits) == len(echanges) == len(set(ehits)) == 4

# All preconditions have passed. Preserve affected source batches before writing.
backup = W / 'premerge'
backup.mkdir()
for p in [*updates, *eupdates]:
    (backup / p.name).write_bytes(p.read_bytes())
for p, rows in updates.items():
    with p.open('w', newline='') as h:
        writer = csv.DictWriter(h, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
for p, rows in eupdates.items():
    p.write_text(''.join(json.dumps(x, ensure_ascii=False) + '\n' for x in rows))
out = dict(old_flows=hits, changed_fields=sum(sum(c['before'][k] != c['after'][k] for k in c['before']) for c in changes), old_evidence_added_captures=ehits)
(W / 'helpers-merged.json').write_text(json.dumps(out, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(out, ensure_ascii=False))
