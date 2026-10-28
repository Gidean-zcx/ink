import csv,hashlib,json,re
from collections import Counter
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/espresso-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=set();formal=set()
casefix=j(W/'case-status-fix.json')
assert sha(R/casefix['backup'])==casefix['before_sha256']
assert sha(R/casefix['path'])==casefix['after_sha256']
assert (R/casefix['backup']).read_text().replace(casefix['before_text'],casefix['after_text'])==(R/casefix['path']).read_text()
def check(x):
 if isinstance(x,list):
  for v in x:check(v)
 elif isinstance(x,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256'),('extracted_work_path','extracted_sha256'),('structure_path','structure_sha256')]:
   if x.get(pk):
    p=R/x[pk];assert sha(p)==x[hk],p;raw.add(p)
  for pk,hk in [('archived_artifact','sha256'),('note_artifact','note_sha256'),('visual_audit_path','visual_audit_sha256')]:
   if x.get(pk):
    p=L/x[pk];digest=x.get(hk) or (x.get('sha256') if x.get('archived_artifact')==x[pk] else None);assert digest and sha(p)==digest,p;formal.add(p)
  for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h;formal.add(L/p)
  if x.get('path') and x.get('sha256'):
   p=R/x['path'] if x['path'].startswith(('work/','docs/')) else L/x['path']
   if x['path']==casefix['path']:assert x['sha256']==casefix['before_sha256'] and sha(p)==casefix['after_sha256']
   else:assert sha(p)==x['sha256'],p
   if p.is_relative_to(L):formal.add(p)
   else:raw.add(p)
  for v in x.values():
   if isinstance(v,(list,dict)):check(v)
E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()))
be={e['evidence_id']:e for e in E};bf={f['flow_id']:f for f in F}
names=['espresso-controls-deepening','toutiao-controls-deepening','hanvon-controls-deepening'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(newE)==8 and len(newF)==1
for e in newE:
 assert be[e['evidence_id']]==e and e['hands_on'] is False
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json');fix=j(W/'visual-status-fix.json')
overrides={(x['flow_id'],x['field']):x for x in fix['changes']};fixfiles={x['path']:x for x in fix['files']}
assert len(overrides)==4
for x in a['flow_changes']:
 override=overrides.get((x['id'],x['field']))
 if override:assert x['after']==override['before']
 assert bf[x['id']][x['field']]==(override['after'] if override else x['after'])
for x in a['evidence_changes']:
 assert be[x['id']][x['field']]==x['after'];check(x['after'])
for x in a['files']:
 assert sha(R/x['backup'])==x['before_sha256']
 if x['path'] in fixfiles:
  y=fixfiles[x['path']];assert x['after_sha256']==y['before_sha256'];assert sha(R/y['backup'])==y['before_sha256'];assert sha(R/y['path'])==y['after_sha256']
 else:assert sha(R/x['path'])==x['after_sha256'],x['path']
assert (a['old_tasks'],a['old_task_fields'],a['old_evidence_captures'])==(14,60,5)
assert bf['F-M045-01']['depth']=='D2' and all(bf[f'F-M008-{i:02}']['depth']=='D1' for i in range(1,7))

cl=j(W/'field-clarification.json');notes=L/'artifacts/espresso-controls-deepening/source-notes.md'
assert sha(W/'source-notes-before-field-clarification.md')==cl['source_before_sha256'];assert sha(notes)==cl['source_after_sha256']
assert 'dateFirstPublished和datePublished；没有单独核定更新字段语义' in notes.read_text()
for x in cl['files']:
 assert sha(R/x['path'])==x['after_sha256'];assert sha(W/(Path(x['path']).name+'.before-field-clarification'))==x['before_sha256']

index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for v in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',v))<=set(be),(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=set(bf),(p,v)
assert (len(counts),sum(counts.values()))==(51,4786),(len(counts),sum(counts.values()))
for name,count in zip(names,[61,22,53]):
 p=L/f'batches/{name}-inventory.csv';rows=list(csv.DictReader(p.open()));key=next(iter(rows[0]));assert len(rows)==len({x[key] for x in rows})==count
 assert b'\\r\\n' not in p.read_bytes()
creator=list(csv.DictReader((L/'artifacts/toutiao-controls-deepening/creator-help-index.csv').open()))
assert len(creator)==73
for name,file in [
 ('espresso-controls-deepening','captures.json'),('espresso-controls-deepening','image-captures.json'),('espresso-controls-deepening','observed-images.json'),
 ('toutiao-controls-deepening','captures.json'),('toutiao-controls-deepening','file-hashes.json'),
 ('hanvon-controls-deepening','raw-manifest.json'),('hanvon-controls-deepening','formal-manifest.json')]:check(j(R/f'work/{name}/{file}'))
hc=[json.loads(line) for line in (R/'work/hanvon-controls-deepening/captures.jsonl').read_text().splitlines()];check(hc)
assert len(hc)==4 and Counter(x['http_status'] for x in hc)=={200:3,403:1}
assert len(j(W/'captures.json'))==3 and all(x['status']==200 for x in j(W/'captures.json'))
assert len(j(W/'observed-images.json'))==7 and all(x['visual_observed'] for x in j(W/'observed-images.json'))
versions=j(W/'store-versions.json');assert len(versions)==25
assert next(x for x in versions if x['version']=='3.31.0')['date'].startswith('Thu Aug 15 2024')
assert next(x for x in versions if x['version']=='3.42.0')['date'].startswith('Thu Apr 16 2026')
images={p for p in formal if p.suffix.lower() in ['.png','.jpg','.jpeg'] and any('/artifacts/'+name+'/' in str(p) for name in names)}
assert len(images)==7
known={e['url'] for e in E}
for e in E:
 for cap in e.get('additional_captures',[]):known.add(cap.get('capture_url',''))
brief=(R/'docs/briefs/021-reading-feedback-and-data-reset.md').read_text();assert set(re.findall(r'\]\((https?://[^)]+)\)',brief))<=known
assert len(E)==1290 and len(F)==788 and Counter(x['depth'] for x in F)=={'D2':694,'D1':79,'D4':15}
report=dict(new_evidence=8,new_tasks=1,new_D2=1,new_D4=0,old_tasks_amended=14,old_fields_amended=60,old_evidence_additional_captures=5,canonical_visual_status_fields=4,inventories=51,inventory_rows=4786,new_function_source_rows=136,additional_creator_index_rows=73,new_static_images=7,raw_files_checked=len(raw),actual_product_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
