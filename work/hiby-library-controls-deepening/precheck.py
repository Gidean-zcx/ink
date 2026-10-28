"""Read-only pre-merge baseline/content checks. Do not use after merge."""
from pathlib import Path
import csv, hashlib, json, re
R=Path(__file__).resolve().parents[2];W=R/'work/hiby-library-controls-deepening';L=R/'docs/research/interaction-library'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cs(p):return list(csv.DictReader(p.open()))
E={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
F={x['flow_id']:x for x in cs(L/'coverage.csv')}
assert all(E[x['evidence_id']]==x for x in j(W/'evidence-before.json'))
assert all(F[x['flow_id']]==x for x in j(W/'coverage-before.json'))
u=j(W/'old-coverage-updates.json');assert len(u)==4 and sum(len(x['changed_fields']) for x in u)==11
for x in u:
 assert F[x['flow_id']]==x['before']
 assert x['changed_fields']=={k:{'before':x['before'][k],'after':v} for k,v in x['after'].items() if v!=x['before'][k]}
for x in j(W/'old-evidence-additional-captures.json'):
 assert E[x['evidence_id']]['url']==x['expected_url']
 assert E[x['evidence_id']].get('additional_captures',[])==x['before_additional_captures']
 for c in x['append_additional_captures']:
  assert sha(R/c['raw_work_path'])==c['raw_sha256'] and sha(L/c['archived_artifact'])==c['sha256']
  assert sha(R/c['extracted_work_path'])==c['extracted_sha256'] and not c['hands_on'] and not c['visual_observed']
stem=L/'batches/hiby-library-controls-deepening'
assert Path(str(stem)+'-evidence.jsonl').read_bytes()==b''
for suffix,n in [('-coverage.csv',1),('-inventory.csv',145)]:
 p=Path(str(stem)+suffix);rows=cs(p);assert len(rows)==n and p.read_bytes().count(b'\n')==n+1 and b'\r' not in p.read_bytes()
 for x in rows:assert set(x['evidence_ids'].split(';'))<=set(E)
flow=cs(Path(str(stem)+'-coverage.csv'))[0]
assert flow['flow_id']=='F-M035-11' and flow['flow_id'] not in F and flow['hands_on']=='false' and flow['visual_status']=='not_observed'
assert len(re.findall(r'^\| F-M035-\d+', (L/'cases/M-035-hiby.md').read_text(),re.M))==11
for m in ['formal-manifest.json','raw-manifest.json']:
 for x in j(W/m):assert sha(R/x['path'])==x['sha256']
assert sha(W/'case-before.md')==j(W/'baseline.json')['case_sha256']
print('PASS: 0E/1F; 4 old F/11 fields; 4 old E captures; 145 inventory; 5 formal/8 raw hashes; baseline/LF/references. Read-only.')
