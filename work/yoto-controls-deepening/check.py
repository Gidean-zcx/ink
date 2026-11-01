from pathlib import Path
import json,csv,re,hashlib
from urllib.parse import urlsplit,urlunsplit,unquote
W=Path('work/yoto-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';C=L/'cases/M-037-yoto.md'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def norm(u):
 s=urlsplit(u);return urlunsplit((s.scheme,s.netloc.lower(),unquote(s.path).rstrip('/'),'',''))
new=[json.loads(s) for s in (B/'yoto-controls-deepening-evidence.jsonl').read_text().splitlines()]; assert len(new)==14
allold=[]
for p in L.rglob('*.jsonl'):
 if p.name!='yoto-controls-deepening-evidence.jsonl':allold += [json.loads(s) for s in p.read_text().splitlines() if s.strip()]
oldurls={norm(x['url']) for x in allold if x.get('url')};oldids={x['evidence_id'] for x in allold if x.get('evidence_id')}
assert all(x['evidence_id'] not in oldids and norm(x['url']) not in oldurls for x in new)
assert len({norm(x['url']) for x in new})==14
for x in new:
 assert x['hands_on'] is False and x['visual_observed'] is False
 for p,h,base in [('raw_work_path','raw_sha256',Path('.')),('extracted_work_path','extracted_sha256',Path('.')),('archived_artifact','sha256',L)]:assert sha(base/x[p])==x[h],(x['evidence_id'],p)
 if 'capture_url' in x:assert '/categories/' in x['capture_url']
result={};known=oldids|{x['evidence_id'] for x in new}
for kind,num in [('coverage',4),('inventory',123)]:
 p=B/f'yoto-controls-deepening-{kind}.csv';bb=p.read_bytes();assert b'\r' not in bb and b'\\r' not in bb and b'\\n' not in bb
 rows=list(csv.DictReader(p.open()));assert len(rows)==num and bb.count(b'\n')==num+1
 for r in rows:
  assert None not in r and set(r['evidence_ids'].split(';'))-known<=set([''])
 if kind=='coverage':assert [r['flow_id'] for r in rows]==[f'F-M037-{n}' for n in range(16,20)] and all(r['hands_on']=='false' and r['depth']=='D2' for r in rows)
 result[kind+'_rows']=num;result[kind+'_physical_lf']=bb.count(b'\n')
masterF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())};updates=json.loads((W/'old-coverage-updates.json').read_text());assert len(updates)==6
for x in updates:
 assert masterF[x['flow_id']]==x['before'],x['flow_id']
 assert x['changed_fields']=={k:{'before':x['before'][k],'after':x['after'][k]} for k in x['before'] if x['before'][k]!=x['after'][k]}
masterE={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};recap=json.loads((W/'old-evidence-additional-captures.json').read_text());assert len(recap)==7
for x in recap:
 assert masterE[x['evidence_id']]==x['before'],x['evidence_id']
 c=x['after']['additional_captures'][-1]
 for p,h,base in [('raw_work_path','raw_sha256',Path('.')),('extracted_work_path','extracted_sha256',Path('.')),('extracted_text_work_path','extracted_text_sha256',Path('.')),('archived_artifact','sha256',L)]:assert sha(base/c[p])==c[h]
s=C.read_text();ids=re.findall(r'^\| (F-M037-\d+)(?: \|| )',s,re.M);assert len(ids)==19 and len(set(ids))==19,ids
assert '\n\n| F-M037-16' not in s
assert set(re.findall(r'E-(?:YOD|YOT|YTCD)-\d+',s))<=known
for link in re.findall(r'\]\(([^)]+)\)',s):
 if not link.startswith(('http://','https://','#')):assert (C.parent/link.split('#')[0]).exists(),link
manifest=json.loads((W/'source-manifest.json').read_text());assert len(manifest)==6 and all(x['http_status']==200 for x in manifest)
assert all(x['http_status']==403 and x['file_saved'] is False for x in json.loads((W/'image-manifest.json').read_text()))
result.update(status='PASS',new_E=14,new_F=4,case_F=19,old_F_updates=6,old_E_captures=7,old_fields=sum(len(x['changed_fields']) for x in updates),url_duplicates=0,all_hashes_match=True,all_before_match=True,visual_observed=False,hands_on=False)
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
