from pathlib import Path
from urllib.parse import urlparse
import json,csv,re,hashlib
L=Path('docs/research/interaction-library');W=Path('work/sony-controls-deepening');B=L/'batches'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
E=[json.loads(x) for x in (B/'sony-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(E)==18 and len({r['evidence_id'] for r in E})==18
for r in E:
 assert r['hands_on'] is False
 assert urlparse(r['url']).netloc=='helpguide.sony.net' and '/dmp/1301/' in r['url']
 for p,h,base in [('raw_work_path','raw_sha256',Path('.')),('extracted_work_path','extracted_sha256',Path('.')),('archived_artifact','sha256',L)]:
  assert r[p] and not r[p].startswith('/')
  assert sha(base/r[p])==r[h],r['evidence_id']+':'+h
 if r['visual_observed']:
  assert r['derivative_page'] in [22,111]
  assert sha(r['derivative_source_work_path'])==r['derivative_source_sha256']
rows={}
for name,count in [('coverage',6),('inventory',82)]:
 p=B/f'sony-controls-deepening-{name}.csv';data=p.read_bytes();assert b'\\n' not in data and b'\\r' not in data and b'\r' not in data
 rr=list(csv.DictReader(p.open()));assert len(rr)==count and data.count(b'\n')==count+1;rows[name]=rr
 assert all(None not in r for r in rr)
known={r['evidence_id'] for r in E}
for p in [L/'evidence.jsonl',B/'audio-evidence.jsonl',B/'audio-inventory-evidence.jsonl']:
 if p.exists():known.update(json.loads(x)['evidence_id'] for x in p.read_text().splitlines() if x.strip())
for r in rows['coverage']:
 assert r['depth']=='D2' and r['hands_on']=='false'
 assert set(r['evidence_ids'].split(';'))<=known
assert [r['flow_id'] for r in rows['coverage']]==[f'F-M036-{i:02}' for i in range(11,17)]
case=L/'cases/M-036-sony.md';text=case.read_text();ids=re.findall(r'^\| (F-M036-\d+) \|',text,re.M);assert len(ids)==16 and len(set(ids))==16
assert '\n\n| F-M036-11' not in text
assert set(re.findall(r'E-(?:SONY|SYCD)-\d+',text))<=known
for target in re.findall(r'\]\(([^)]+)\)',text):
 if not target.startswith(('https://','http://','#')):assert (case.parent/target.split('#')[0]).exists(),target
master={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open())}
proposals=json.loads((W/'old-coverage-updates.json').read_text());assert len(proposals)==6
for x in proposals:
 assert master[x['flow_id']]==x['before'],x['flow_id']+' stale before'
 assert x['changed_fields']=={k:{'before':x['before'][k],'after':x['after'][k]} for k in x['before'] if x['before'][k]!=x['after'][k]}
 assert x['after']['hands_on']=='false'
assert len({r['article_url'] for r in rows['inventory']})==82
assert all('未自动' not in r['locator'] for r in rows['inventory'])
result={'status':'PASS','evidence':18,'new_flows':6,'existing_flow_proposals':6,'case_flows':16,'inventory':82,'visual_pages':[22,111],'sha256_checks':'all_match','csv_physical_newlines':'coverage7/inventory83','master_before':'all_6_match','hands_on':False}
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
