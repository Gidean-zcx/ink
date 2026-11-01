import csv,hashlib,json,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/time-vocabulary-deepening'
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
names=['time-vocabulary-deepening','youdao-controls-deepening','ak-directory-deepening'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(newE)==6 and len(newF)==3
for e in newE:
 assert be[e['evidence_id']]==e
 assert not e.get('derived_from') or e['derived_from'] in be
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json');migration=j(W/'visual-audit-migration.json')
for x in a['flow_changes']:assert bf[x['id']][x['field']]==x['after']
for x in a['evidence_changes']:
 assert be[x['id']][x['field']]==x['after'];check(x['after'])
mf={x['path']:x for x in migration['files']}
for x in a['files']:
 assert sha(R/x['backup'])==x['before_sha256']
 if x['path'] in mf:assert mf[x['path']]['before_sha256']==x['after_sha256']
 else:assert sha(R/x['path'])==x['after_sha256']
for x in migration['files']:
 assert sha(R/x['path'])==x['after_sha256'] and sha(R/x['backup'])==x['before_sha256']
for x in migration['changes']:assert be[x['id']][x['field']]==x['after']
assert a['old_tasks']==6 and a['old_task_fields']==27 and a['old_evidence_captures']==11
index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for v in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',v))<=set(be),(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=set(bf),(p,v)
assert len(counts)==41 and sum(counts.values())==3877,(len(counts),sum(counts.values()))
for name,count in zip(names,[83,20,202]):
 p=L/f'batches/{name}-inventory.csv';rows=list(csv.DictReader(p.open()));key=next(iter(rows[0]));assert len(rows)==len({x[key] for x in rows})==count
 assert b'\\r\\n' not in p.read_bytes()
assert len(list(csv.DictReader((L/'batches/youdao-controls-deepening-discovery.csv').open())))==8
b=L/'artifacts/time-vocabulary-deepening/browser';states=j(b/'observed-states.json');trace=j(b/'trace-actions.json');check(j(b/'raw-trace-manifest.json'))
assert len(states)==31 and sum(x['image_reviewed'] for x in states)==14
assert len([x for x in states if x.get('archived_image')])==12
for x in states:
 for key in ['image','metadata','html']:
  p=R/x[f'raw_{key}_path'];assert sha(p)==x[f'raw_{key}_sha256'];raw.add(str(p.relative_to(R)))
 if x.get('archived_image'):assert sha(L/x['archived_image'])==x['raw_image_sha256']
assert len(trace['actions'])==81 and not any(x['error'] for x in trace['actions'])
assert len(j(b/'browser-errors.json'))==14 and all(x['status']==200 for x in j(b/'document-statuses.json'))
by={x['capture'].split('-')[0]:x for x in states}
media=lambda n:by[n]['media'][2]
for first,last in [('11','12'),('13','14'),('14','15')]:
 assert media(first)['paused'] is False and media(last)['paused'] is False
 assert media(last)['time']>media(first)['time']
assert media('16')['paused'] and media('17')['paused'] and media('16')['time']==media('17')['time']
assert media('18')['paused'] and media('18')['time']==0
assert bf['F-M047-03']['depth']=='D4'
known={e['url'] for e in E}
for e in E:
 for cap in e.get('additional_captures',[]):known.add(cap.get('capture_url',''))
brief=(R/'docs/briefs/018-reading-controls-and-source-completeness.md').read_text();assert set(re.findall(r'\]\((https?://[^)]+)\)',brief))<=known
assert len(E)==1242 and len(F)==767
report=dict(new_evidence=6,new_tasks=3,new_D2=3,old_task_D1_to_D4=1,old_tasks_amended=6,old_fields_amended=27,old_evidence_additional_captures=11,audit_metadata_fields_migrated=4,inventories=41,inventory_rows=3877,new_inventory_rows=305,new_archived_images=len([p for p in formal if p.endswith('.png')]),raw_files_checked=len(raw),browser_captures=31,reviewed_browser_images=14,archived_browser_images=12,recorded_browser_calls=81,errors=[])
assert report['new_archived_images']==17,report
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
