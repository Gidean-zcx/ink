import csv,json,re,hashlib
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/douyin-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=set();formal=set()
def check(x):
 if isinstance(x,list):
  for v in x:check(v)
 elif isinstance(x,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('decoded_work_path','decoded_sha256'),('text_work_path','text_sha256'),('extracted_work_path','extracted_sha256'),('raw_image_work_path','raw_image_sha256'),('html_shell_work_path','html_shell_sha256')]:
   if x.get(pk):
    p=R/x[pk];expected=x.get(hk)
    if expected is None and (x.get('path') or x.get('artifact')) and x.get('sha256'):expected=x['sha256']
    assert expected and sha(p)==expected,p;raw.add(p)
  for pk,hk in [('archived_artifact','sha256'),('visual_audit_path','visual_audit_sha256'),('artifact','sha256')]:
   if x.get(pk):p=L/x[pk];assert sha(p)==x[hk],p;formal.add(p)
  for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h;formal.add(L/p)
  if x.get('path') and x.get('sha256'):
   p=R/x['path'] if x['path'].startswith(('work/','docs/')) else L/x['path'];assert sha(p)==x['sha256'],p
   (formal if p.is_relative_to(L) else raw).add(p)
  for v in x.values():
   if isinstance(v,(dict,list)):check(v)
E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()));be={e['evidence_id']:e for e in E};bf={f['flow_id']:f for f in F}
names=['douyin-controls-deepening','hisense-controls-deepening','baicizhan-device-controls-deepening-2'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(newE)==11 and len(newF)==7
for e in newE:assert be[e['evidence_id']]==e and e['hands_on'] is False and e['supersedes'] is None
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json')
assert (a['old_tasks'],a['old_task_fields'],a['old_evidence_captures'])==(14,60,8)
for x in a['flow_changes']:assert bf[x['id']][x['field']]==x['after']
for x in a['evidence_changes']:assert be[x['id']][x['field']]==x['after'];check(be[x['id']].get('additional_captures',[]))
for x in a['files']:assert sha(R/x['backup'])==x['before_sha256'];assert sha(R/x['path'])==x['after_sha256']
for n in ['sources.json','image-captures.json','guide-crops.json','observed-artifacts.json']:check(j(W/n))
check(j(R/'work/hisense-controls-deepening/formal-manifest.json'));check(j(R/'work/baicizhan-device-controls-deepening-2/manifest.json'))
assert sha(R/'docs/requirements.md')==j(W/'baseline.json')['requirements_sha256']
for fid in ['F-M051-01','F-M055-01','F-M055-08','F-M022-01','F-M022-02','F-M022-03']:assert bf[fid]['depth']=='D1'
index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for value in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',value))<=set(be),(p,value)
   assert set(re.findall(r'F-M\d{3}-\d{2}',value))<=set(bf),(p,value)
assert (len(counts),sum(counts.values()))==(60,5563)
for name,n in zip(names,[105,114,45]):
 rows=list(csv.DictReader((L/f'batches/{name}-inventory.csv').open()));assert len(rows)==len({r[next(iter(r))] for r in rows})==n
assert len(E)==1316 and len(F)==803 and Counter(x['depth'] for x in F)=={'D2':709,'D1':79,'D4':15}
newimages={p for p in formal if p.suffix in ['.png','.jpg','.jpeg','.webp']};assert len(newimages)==25
report=dict(new_evidence=11,new_tasks=7,new_D2=7,new_D4=0,old_tasks_amended=14,old_fields_amended=60,old_evidence_additional_captures=8,inventories=60,inventory_rows=5563,new_inventory_rows=264,new_static_images=len(newimages),raw_files_checked=len(raw),actual_product_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
