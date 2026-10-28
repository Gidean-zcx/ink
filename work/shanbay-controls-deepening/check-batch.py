from pathlib import Path
import json,csv,re,hashlib
root=Path('/home/workspace/reader');base=root/'docs/research/interaction-library';w=root/'work/shanbay-controls-deepening'
case=base/'cases/M-019-shanbay.md';ep=base/'batches/shanbay-controls-deepening-evidence.jsonl';cp=base/'batches/shanbay-controls-deepening-coverage.csv'
e=[json.loads(l) for l in ep.read_text().splitlines()];c=list(csv.DictReader(cp.open()));oldc=list(csv.DictReader((base/'batches/vocab-coverage.csv').open()))
ids=[r['evidence_id'] for r in e]
assert len(e)==12 and len(ids)==len(set(ids))
assert all(r['product_id']=='M-019' and r['hands_on'] is False and r['visual_observed'] is False for r in e)
otherids=set()
for p in (base/'batches').glob('*evidence.jsonl'):
 if p==ep:continue
 for line in p.read_text().splitlines():otherids.add(json.loads(line)['evidence_id'])
assert not set(ids)&otherids
assert [r['flow_id'] for r in c]==['F-M019-10','F-M019-11']
assert not set(r['flow_id'] for r in c)&set(r['flow_id'] for r in oldc)
for r in e:
 p=root/r['raw_work_path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==r['raw_sha256'],r['evidence_id']
for p,h in json.loads((w/'baseline.json').read_text()).items():
 if '/batches/' in p:assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
oldf={r['flow_id']:r for r in oldc};proposed=list(csv.DictReader((w/'existing-coverage-updates.csv').open()))
assert len(proposed)==8
for r in proposed:
 changed={k for k in r if r[k]!=oldf[r['flow_id']][k]}
 assert changed=={'evidence_ids','known_branches','gaps'},(r['flow_id'],changed)
refs=set(re.findall(r'E-(?:SBY|SBCD)-\d{2}',case.read_text()))
assert refs<=otherids|set(ids),refs-otherids-set(ids)
for r in c+proposed:assert set(r['evidence_ids'].split(';'))<=otherids|set(ids)
for p in [case,ep,cp,w/'existing-coverage-updates.csv']:
 b=p.read_bytes();assert b.endswith(b'\n') and b'\r' not in b,p
 for i,l in enumerate(b.decode().splitlines(),1):assert l.rstrip()==l,(p,i)
 if p.suffix=='.csv':assert b'\\n' not in b and b'\\r' not in b,p
for u in re.findall(r'\]\(([^)]+)\)',case.read_text()):assert u.startswith('https://'),u
for n in [7,8]:
 p=w/f'ielts-{n}-blocked-response.html';assert p.read_bytes().startswith(b'<script>')
print('PASS: 12 evidence; 2 new flows; 8 old-row proposals only evidence_ids/known_branches/gaps; references/raw hashes; old vocab batches unchanged; LF and whitespace; static observation all false')
for p in [case,ep,cp]:print(str(p.relative_to(root)),len(p.read_bytes()),'bytes',hashlib.sha256(p.read_bytes()).hexdigest())
