from pathlib import Path
import json,csv,re,hashlib,collections
W=Path('work/shanbay-controls-deepening-2');L=Path('docs/research/interaction-library');name='shanbay-controls-deepening-2';B=L/'batches';A=L/'artifacts'/name
E=B/(name+'-evidence.jsonl');C=B/(name+'-coverage.csv');I=B/(name+'-inventory.csv');P=L/'cases/M-019-shanbay.md';N=A/'source-notes.md'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
e=[json.loads(x) for x in E.read_text().splitlines()];c=list(csv.DictReader(C.open()));inv=list(csv.DictReader(I.open()));olde=[]
for p in L.rglob('*.jsonl'):
 if p==E:continue
 for line in p.read_text().splitlines():
  if line.strip():olde.append(json.loads(line))
ids={x['evidence_id'] for x in olde};urls={x['url'].rstrip('/') for x in olde if 'url' in x};newids={x['evidence_id'] for x in e}
assert len(e)==7 and len(newids)==7 and not ids&newids
assert not ({x['url'].rstrip('/') for x in e}&urls)
assert len(c)==0 and len(inv)==68
s=P.read_text();assert set(re.findall('F-M019-\\d{2}',s))=={f'F-M019-{n:02}' for n in range(1,12)}
for r in e:
 assert r['hands_on'] is False and r['visual_observed'] is False
 assert sha(r['raw_work_path'])==r['raw_sha256']
 assert not r['archived_artifact'].startswith('work/') and sha(L/r['archived_artifact'])==r['sha256']
 for raw in r.get('additional_raw_sources',[]):assert sha(raw['raw_work_path'])==raw['raw_sha256']
for r in inv:
 assert set(r['evidence_ids'].split(';'))<=ids|newids
 for f in r['flow_ids'].split(';'):assert not f or f in s
assert len({r['item_id'] for r in inv})==68
count=collections.Counter(r['kind'] for r in inv);assert count['help_column']==12 and count['help_article']==34
old={r['flow_id']:r for r in json.loads((W/'old-coverage.json').read_text())}
u=json.loads((W/'existing-coverage-updates.json').read_text());assert len(u)==26 and u==list(csv.DictReader((W/'existing-coverage-updates.csv').open()))
for r in u:assert old[r['flow_id']][r['field']]==r['before'] and r['field']!='depth'
cap=json.loads((W/'additional-captures-updates.json').read_text());assert len(cap)==7
oe={r['evidence_id']:r for r in json.loads((W/'old-evidence.json').read_text())}
for r in cap:
 assert r['before']==oe[r['evidence_id']].get('additional_captures')
 new=r['after'][-1];assert not new['network_refetched'] and sha(new['raw_work_path'])==new['raw_sha256'] and new['sha256']==sha(N)
for p in [C,I,W/'existing-coverage-updates.csv']:
 raw=p.read_bytes();assert b'\r' not in raw and b'\\r\\n' not in raw and raw.endswith(b'\n')
base=json.loads((W/'baseline.json').read_text());unchanged=[]
for p,h in base.items():
 if p.startswith('batches/'):
  assert sha(L/p)==h;unchanged.append(p)
for p in [P,N]:
 for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
  if target.startswith(('https:','http:','#')):continue
  t=target.split('#')[0];dest=Path(t) if t.startswith('/') else p.parent/t;assert dest.exists(),(p,target)
# Catch isolated Markdown table rows introduced in prose.
lines=s.splitlines()
for i,line in enumerate(lines):
 if line.startswith('|') and not (i and lines[i-1].startswith('|')):
  assert i+1<len(lines) and lines[i+1].startswith('|'),('isolated table row',i+1)
files=[P,E,C,I,N,W/'existing-coverage-updates.json',W/'existing-coverage-updates.csv',W/'additional-captures-updates.json']+sorted(W.glob('*.html'))+sorted(W.glob('*.json'))+sorted(W.glob('*.js'))
manifest={str(p):sha(p) for p in files if p.name not in ['hash-manifest.json','validation.json']};(W/'hash-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
res=dict(result='PASS',new_E=7,new_F=0,case_F=11,old_depth_unchanged=True,inventory=dict(count),old_F_field_changes=26,old_E_additional_capture_suggestions=7,old_batches_unchanged=unchanged,sha256='match',CSV='UTF-8/LF',visual_observed=False,hands_on=False)
(W/'validation.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n');print(json.dumps(res,ensure_ascii=False,indent=2))
