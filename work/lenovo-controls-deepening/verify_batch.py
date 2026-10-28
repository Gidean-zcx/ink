import csv,hashlib,json,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/lenovo-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=set();formal=set()
def check(x):
 if isinstance(x,list):
  for v in x:check(v)
 elif isinstance(x,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256'),('extracted_work_path','extracted_sha256'),('extracted_text_work_path','extracted_text_sha256')]:
   if x.get(pk):
    p=R/x[pk];assert sha(p)==x[hk],p;raw.add(str(p.relative_to(R)))
  for pk,hk in [('archived_artifact','sha256'),('note_artifact','note_sha256')]:
   if x.get(pk):
    p=L/x[pk];assert sha(p)==x[hk],p;formal.add(str(p.relative_to(L)))
  if x.get('visual_audit_path'):
   digest=x.get('visual_audit_sha256')
   if digest is None and x.get('archived_artifact')==x['visual_audit_path']:digest=x['sha256']
   assert digest and sha(L/x['visual_audit_path'])==digest
  for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h;formal.add(p)
  if x.get('path') and x.get('sha256'):
   p=R/x['path'] if x['path'].startswith(('work/','docs/')) else L/x['path'];assert sha(p)==x['sha256'],p
   if str(p).startswith(str(L)):formal.add(str(p.relative_to(L)))
  for v in x.values():
   if isinstance(v,(list,dict)):check(v)
E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()))
be={e['evidence_id']:e for e in E};bf={f['flow_id']:f for f in F}
names=['lenovo-controls-deepening','shanling-controls-deepening','ibasso-controls-deepening'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(newE)==5 and len(newF)==14
for e in newE:
 assert be[e['evidence_id']]==e and e['hands_on'] is False
 assert not e.get('derived_from') or e['derived_from'] in be
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json')
for x in a['flow_changes']:assert bf[x['id']][x['field']]==x['after']
for x in a['evidence_changes']:
 assert be[x['id']][x['field']]==x['after'];check(x['after'])
for x in a['files']:
 assert sha(R/x['path'])==x['after_sha256'];assert sha(R/x['backup'])==x['before_sha256']
assert a['old_tasks']==12 and a['old_task_fields']==44 and a['old_evidence_captures']==8
index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for v in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',v))<=set(be),(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=set(bf),(p,v)
assert len(counts)==38 and sum(counts.values())==3572,(len(counts),sum(counts.values()))
for name,count in zip(names,[127,114,129]):
 p=L/f'batches/{name}-inventory.csv';rows=list(csv.DictReader(p.open()));key=next(iter(rows[0]));assert len(rows)==len({x[key] for x in rows})==count
 assert b'\\r\\n' not in p.read_bytes()
for name,file in [('shanling-controls-deepening','captures.json'),('ibasso-controls-deepening','fetch-log.json')]:check(j(R/f'work/{name}/{file}'))
for name,file in [('shanling-controls-deepening','rendered.json'),('ibasso-controls-deepening','render-manifest.json')]:check(j(R/f'work/{name}/{file}'))
known={e['url'] for e in E}
for e in E:
 for cap in e.get('additional_captures',[]):known.add(cap.get('capture_url',''))
brief=(R/'docs/briefs/017-action-targets-and-lifecycle.md').read_text();assert set(re.findall(r'\]\((https?://[^)]+)\)',brief))<=known
assert len(E)==1236 and len(F)==764
report=dict(new_evidence=5,new_tasks=14,new_D2=14,new_D4=0,old_tasks_amended=12,old_fields_amended=44,old_evidence_additional_captures=8,inventories=38,inventory_rows=3572,new_inventory_rows=370,new_archived_images=len([p for p in formal if p.endswith('.png')]),raw_files_checked=len(raw),actual_product_operations=0,errors=[])
assert report['new_archived_images']==10,report
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
