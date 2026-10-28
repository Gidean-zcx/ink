"""Read-only checks before parent merges old rows; do not run after merge."""
from pathlib import Path
import csv, hashlib, json, re
W = Path('work/bbdc-learning-controls-deepening')
L = Path('docs/research/interaction-library')
B = 'bbdc-learning-controls-deepening'
readj = lambda p: json.loads(Path(p).read_text())
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows = lambda p: list(csv.DictReader(Path(p).open()))
records = lambda p: [json.loads(s) for s in Path(p).read_text().splitlines() if s.strip()]
baseline = readj(W/'baseline.json')
old_e = {r['evidence_id']:r for r in records(L/'evidence.jsonl')}
old_f = {r['flow_id']:r for r in rows(L/'coverage.csv')}
assert all(old_e[r['evidence_id']]==r for r in baseline['evidence'])
assert all(old_f[r['flow_id']]==r for r in baseline['coverage'])
assert sha(W/'case-before.md')=='5ff40f5ab1b1141b6ae561cfd9ee11d0a929da6ef02af5b40f1b30da777867c2'
es = records(L/f'batches/{B}-evidence.jsonl')
assert len(es)==3 and len({e['evidence_id'] for e in es})==3
other_ids = set(old_e)
other_urls = {e['url'] for e in old_e.values()}
for p in (L/'batches').glob('*evidence.jsonl'):
    if p.name != B+'-evidence.jsonl':
        for e in records(p):
            other_ids.add(e['evidence_id']); other_urls.add(e['url'])
for e in es:
    assert e['evidence_id'] not in other_ids and e['url'] not in other_urls
    assert e['product_id']=='M-018' and e['supersedes'] is None and not e['hands_on']
    assert not e['archived_artifact'].startswith(('work/','/'))
    assert sha(L/e['archived_artifact'])==e['sha256']
    assert sha(e['raw_work_path'])==e['raw_sha256']
    if e['visual_observed']:
        assert sha(L/e['visual_audit_path'])==e['visual_audit_sha256']
        for a in e['additional_artifacts']: assert sha(L/a['path'])==a['sha256']
updates = readj(W/'existing-coverage-updates.json')
assert len(updates)==2 and sum(len(u['changes']) for u in updates)==8
for u in updates:
    r = next(r for r in rows(u['source_batch']) if r['flow_id']==u['flow_id'])
    assert r==old_f[u['flow_id']]
    assert all(r[k]==v['before'] for k,v in u['changes'].items())
    assert u['replacement_row']=={**r,**{k:v['after'] for k,v in u['changes'].items()}}
    assert u['replacement_row']['depth']=='D1'
assert readj(W/'existing-evidence-additional-captures.json')==[]
assert rows(L/f'batches/{B}-coverage.csv')==[]
inv = rows(L/f'batches/{B}-inventory.csv'); assert len(inv)==9
all_e = set(old_e)|{e['evidence_id'] for e in es}
for r in inv:
    assert set(filter(None,r['evidence_ids'].split(';')))<=all_e
    assert set(filter(None,r['flow_ids'].split(';')))<=set(old_f)
assert len(readj(W/'candidate-register.json'))==6
case = L/'cases/M-018-bbdc.md'
for link in re.findall(r'\]\(([^)]+)\)',case.read_text()):
    if not link.startswith(('http:','https:','#')): assert (case.parent/link).exists(),link
for p in [L/f'batches/{B}-coverage.csv',L/f'batches/{B}-inventory.csv']:
    assert b'\r' not in p.read_bytes() and b'\\r\\n' not in p.read_bytes()
print('PASS: 3 new E / 0 new F; 2 old F / 8 fields; baseline rows unchanged; hashes/paths/IDs/LF verified.')
