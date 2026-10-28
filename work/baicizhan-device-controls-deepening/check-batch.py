from pathlib import Path
import hashlib,json,csv,re
root=Path.cwd(); base=Path('work/baicizhan-device-controls-deepening');lib=Path('docs/research/interaction-library');bn='baicizhan-device-controls-deepening'
case=lib/'cases/M-022-baicizhan-device.md';ep=lib/'batches'/f'{bn}-evidence.jsonl';cp=lib/'batches'/f'{bn}-coverage.csv';up=base/'existing-coverage-updates.csv'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
e=[json.loads(x) for x in ep.read_text().splitlines() if x];assert len(e)==1 and e[0]['evidence_id']=='E-BDCD-01'
assert e[0]['visual_observed'] is False and e[0]['hands_on'] is False and e[0]['archived_artifact'] is None
assert sha(Path(e[0]['raw_work_path']))==e[0]['raw_sha256'];assert e[0]['http_status']==200
assert not list(base.glob('*.jpg'));assert len(e[0]['failed_image_requests'])==3
assert all(x['http_status']==403 for x in e[0]['failed_image_requests'])
assert list(csv.DictReader(cp.open()))==[]
updates=list(csv.DictReader(up.open()));assert len(updates)==4 and {x['flow_id'] for x in updates}=={'F-M022-02'}
old={r['flow_id']:r for r in json.loads((base/'existing-coverage-before.json').read_text())}
for r in updates:assert old[r['flow_id']][r['field']]==r['before']
flows=re.findall(r'^\| (F-M022-\d+)',case.read_text(),flags=re.M);assert flows==['F-M022-01','F-M022-02','F-M022-03']
assert set(re.findall(r'E-BDCD-\d+',case.read_text()))=={'E-BDCD-01'}
collisions=[]
for p in (lib/'batches').glob('*-evidence.jsonl'):
 for line in p.read_text().splitlines():
  if line.strip() and json.loads(line).get('evidence_id')=='E-BDCD-01':collisions.append(str(p))
assert collisions==[str(ep)],collisions
baseline=json.loads((base/'baseline.json').read_text());protected={p:sha(Path(p))==v for p,v in baseline.items() if p!=str(case)};assert all(protected.values()),protected
files=[case,ep,cp,up,base/'research-log.md']
for p in files:
 b=p.read_bytes();b.decode('utf-8');assert b.endswith(b'\n');assert b'\r' not in b;assert b'\\r\\n' not in b
 assert all(not line.endswith((' ','\t')) for line in b.decode().splitlines()),p
for label,target in re.findall(r'\[([^\]]+)\]\(([^)]+)\)',case.read_text()):
 if target.startswith(('https://','http://')):continue
 p=Path(target) if target.startswith('/') else case.parent/target
 assert p.exists(),(label,target)
result={'status':'PASS','new_evidence_count':1,'new_flow_count':0,'case_flows':flows,'case_depth':'three existing flows remain D1','old_field_proposal_count':4,'images_observed':0,'image_http_statuses':[403,403,403],'old_batches_unchanged':protected,'raw_hashes':{x['path']:sha(Path(x['path'])) for x in json.loads((base/'fetch-results.json').read_text())},'output_hashes':{str(p):sha(p) for p in files},'master_modified_by_this_batch':False,'checks':'UTF-8; LF; duplicate E; F identifiers; before values; local Markdown links; raw SHA; four old batch SHA; no image artifact or depth upgrade'}
(base/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
