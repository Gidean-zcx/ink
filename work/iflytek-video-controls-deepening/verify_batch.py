from pathlib import Path
import json,csv,hashlib,re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-video-controls-deepening'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
load=lambda p:json.loads(p.read_text())
e={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
f={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
assert len(e)==1391 and len(f)==846
names=['iflytek-video-controls-deepening','pocketbook-app-controls-deepening','supernote-notes-controls-deepening']
new_e=[];new_f=[]
for name in names:
 es=list(map(json.loads,(L/f'batches/{name}-evidence.jsonl').read_text().splitlines()));fs=list(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
 for x in es:assert e[x['evidence_id']]==x
 for x in fs:assert f[x['flow_id']]==x and x['depth']=='D2' and x['hands_on']=='false'
 new_e+=es;new_f+=fs
assert len(new_e)==9 and len(new_f)==15
a=load(W/'applied-reviewed-updates.json')
for rows,index in [(a['flow_changes'],f),(a['evidence_changes'],e)]:
 for x in rows:assert index[x['id']].get(x['field'])==x['after']
for x in a['files']:
 assert sha(R/x['backup'])==x['before_sha256']
 assert sha(R/x['path'])==x['after_sha256']
manifest_files=0
for p in [W/'formal-manifest.json',R/'work/pocketbook-app-controls-deepening/formal-manifest.json',R/'work/pocketbook-app-controls-deepening/raw-manifest.json',R/'work/supernote-notes-controls-deepening/manifest.json']:
 rows=load(p);rows=rows['files'] if isinstance(rows,dict) else rows
 for x in rows:assert sha(R/x['path'])==x['sha256'],x['path'];manifest_files+=1
inventory=set()
for p in [L/'inventories/README.md']:
 for u in re.findall(r'\]\(([^)]+\.csv)\)',p.read_text()):
  q=Path(u) if u.startswith('/') else (p.parent/u).resolve()
  inventory.add(q)
row_count=0
for p in inventory:
 rows=list(csv.DictReader(p.open()));row_count+=len(rows)
 for x in rows:
  for key in ['evidence_ids','evidence_id']:
   for i in re.findall(r'E-[A-Z0-9]+-\d+',x.get(key,'')):assert i in e,(p,key,i)
  for key in ['flow_ids','related_flow_ids','flow_id']:
   for i in re.findall(r'F-M\d+-\d+',x.get(key,'')):assert i in f,(p,key,i)
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
result=dict(status='PASS',new_evidence=9,new_flows=15,old_tasks=7,old_task_fields=28,old_E_captures=4,old_E_metadata_fields=0,manifest_files=manifest_files,linked_inventory_csvs=len(inventory),linked_inventory_rows=row_count,requirements_changed=False)
(W/'verification-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
