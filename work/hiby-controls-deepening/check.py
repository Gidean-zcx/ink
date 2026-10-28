from pathlib import Path
import json,csv,re,hashlib
from urllib.parse import urlsplit,urlunsplit,unquote
W=Path('work/hiby-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';C=L/'cases/M-035-hiby.md'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def norm(u):
 s=urlsplit(u);return urlunsplit((s.scheme,s.netloc.lower(),unquote(s.path).rstrip('/'),'',''))
E=[json.loads(x) for x in (B/'hiby-controls-deepening-evidence.jsonl').read_text().splitlines()];assert len(E)==4
old=[]
for p in L.rglob('*.jsonl'):
 if p.name!='hiby-controls-deepening-evidence.jsonl':old.extend(json.loads(x) for x in p.read_text().splitlines() if x.strip())
ids={x['evidence_id'] for x in old if x.get('evidence_id')};urls={norm(x['url']) for x in old if x.get('url')}
assert len({e['evidence_id'] for e in E})==4 and all(e['evidence_id'] not in ids and norm(e['url']) not in urls for e in E)
for e in E:
 assert e['hands_on'] is False
 assert sha(e['raw_work_path'])==e['raw_sha256'];assert sha(L/e['archived_artifact'])==e['sha256']
 if 'extracted_work_path' in e:assert sha(e['extracted_work_path'])==e['extracted_sha256']
 for p,h in e.get('artifact_hashes',{}).items():assert sha(L/p)==h
 for r in e.get('additional_raw_files',[]):assert sha(r['raw_work_path'])==r['raw_sha256']
known=ids|{e['evidence_id'] for e in E};result={}
for kind,num in [('coverage',1),('inventory',145)]:
 p=B/f'hiby-controls-deepening-{kind}.csv';b=p.read_bytes();assert b'\r' not in b and b'\\r' not in b and b'\\n' not in b
 rows=list(csv.DictReader(p.open()));assert len(rows)==num and b.count(b'\n')==num+1
 for r in rows:assert None not in r and set(r['evidence_ids'].split(';'))<=known
 if kind=='coverage':assert rows[0]['flow_id']=='F-M035-10' and rows[0]['depth']=='D2' and rows[0]['hands_on']=='false'
 result[kind+'_rows']=num;result[kind+'_physical_lf']=b.count(b'\n')
masterF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())};U=json.loads((W/'old-coverage-updates.json').read_text());assert len(U)==5
for x in U:
 assert masterF[x['flow_id']]==x['before'],x['flow_id'];assert x['changed_fields']=={k:{'before':x['before'][k],'after':x['after'][k]} for k in x['before'] if x['before'][k]!=x['after'][k]}
assert next(x['after']['depth'] for x in U if x['flow_id']=='F-M035-04')=='D1'
assert next(x['after']['depth'] for x in U if x['flow_id']=='F-M035-05')=='D1'
masterE={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};R=json.loads((W/'old-evidence-additional-captures.json').read_text());assert len(R)==4
for x in R:
 assert masterE[x['evidence_id']]==x['before'],x['evidence_id'];c=x['after']['additional_captures'][-1]
 assert sha(c['raw_work_path'])==c['raw_sha256'];assert sha(c['extracted_work_path'])==c['extracted_sha256'];assert sha(L/c['archived_artifact'])==c['sha256']
s=C.read_text();fs=re.findall(r'^\| (F-M035-\d+)(?: \|| )',s,re.M);assert len(fs)==10 and len(set(fs))==10
assert '\n\n| F-M035-10' not in s
assert set(re.findall(r'E-(?:HBY|HBD|HBCD)-\d+',s))<=known
for u in re.findall(r'\]\(([^)]+)\)',s):
 if not u.startswith(('http://','https://','#')):assert (C.parent/u.split('#')[0]).exists(),u
for p in [C,L/'artifacts/hiby-controls-deepening/source-notes.md']:assert all(line==line.rstrip() for line in p.read_text().splitlines())
m=json.loads((W/'source-manifest.json').read_text());assert sum(x['http_status']==200 for x in m)==7 and sum(x['http_status']==403 for x in m)==1
assert [e['evidence_id'] for e in E if e['visual_observed']]==['E-HBCD-04']
result.update(status='PASS',new_E=4,new_F=1,case_F=10,old_F_updates=5,old_fields=sum(len(x['changed_fields']) for x in U),old_E_captures=4,hashes='all_match',before='all_match',new_url_collisions=0,static_images=2,visual_sources=1,hands_on=False)
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
