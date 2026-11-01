from pathlib import Path
import json,csv,re,hashlib
from urllib.parse import urlparse
W=Path('work/fiio-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';C=L/'cases/M-034-fiio.md'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
E=[json.loads(l) for l in (B/'fiio-controls-deepening-evidence.jsonl').read_text().splitlines()];assert len(E)==15
assert len(set(x['evidence_id'] for x in E))==15
for x in E:
 assert x['hands_on'] is False and urlparse(x['url']).scheme=='https'
 for path,hsh,base in [('raw_work_path','raw_sha256',Path('.')),('extracted_work_path','extracted_sha256',Path('.')),('archived_artifact','sha256',L)]:
  assert x[path] and not x[path].startswith('/');assert sha(base/x[path])==x[hsh],(x['evidence_id'],hsh)
 for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h
known={x['evidence_id'] for x in E}
for p in [L/'evidence.jsonl',B/'audio-evidence.jsonl']:
 known.update(json.loads(l)['evidence_id'] for l in p.read_text().splitlines() if l.strip())
result={}
for kind,num in [('coverage',6),('inventory',97)]:
 p=B/f'fiio-controls-deepening-{kind}.csv';data=p.read_bytes();assert b'\r' not in data and b'\\r' not in data and b'\\n' not in data
 rows=list(csv.DictReader(p.open()));assert len(rows)==num and data.count(b'\n')==num+1
 for r in rows:
  assert None not in r
  assert set(r['evidence_ids'].split(';'))<=known
 if kind=='coverage':
  assert [r['flow_id'] for r in rows]==[f'F-M034-{n:02}' for n in range(10,16)]
  assert all(r['depth']=='D2' and r['hands_on']=='false' for r in rows)
 result[kind+'_rows']=num;result[kind+'_physical_lf']=num+1
text=C.read_text();ids=re.findall(r'^\| (F-M034-\d+) \|',text,re.M);assert len(ids)==15 and len(set(ids))==15
assert '\n\n| F-M034-10' not in text
assert set(re.findall(r'E-(?:FIIO|FICD)-\d+',text))<=known
for target in re.findall(r'\]\(([^)]+)\)',text):
 if not target.startswith(('http://','https://','#')):assert (C.parent/target.split('#')[0]).exists(),target
updates=json.loads((W/'old-coverage-updates.json').read_text());assert len(updates)==7
master={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open())}
for x in updates:
 assert master[x['flow_id']]==x['before'],x['flow_id']+' stale before'
 assert x['changed_fields']=={k:{'before':x['before'][k],'after':x['after'][k]} for k in x['before'] if x['before'][k]!=x['after'][k]}
 assert x['after']['hands_on']=='false'
manifest=json.loads((W/'source-manifest.json').read_text());assert len(manifest)==14 and all(r['http_status']==200 for r in manifest)
assert [r['observed_pages'] for r in E if r['visual_observed']]==[[29,30,31,33]]
# No E-FICD IDs in older batches/master prior to root merge.
collisions=[]
for p in L.rglob('*.jsonl'):
 if p.name=='fiio-controls-deepening-evidence.jsonl':continue
 for line in p.read_text().splitlines():
  if line.strip():
   obj=json.loads(line)
   if obj.get('evidence_id','').startswith('E-FICD-'):collisions.append(str(p))
assert not collisions,collisions
result.update(status='PASS',evidence=15,new_F=6,case_F=15,old_proposals=7,sha='all_match',before='all_7_match',static_source_count=1,static_pages=[29,30,31,33],hands_on=False)
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
