import csv, hashlib, json, re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/dragonbox-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw_checked=set()
def check(obj):
 if isinstance(obj,list):
  for v in obj:check(v)
 elif isinstance(obj,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256'),('extracted_text_work_path','extracted_text_sha256')]:
   if obj.get(pk):
    p=R/obj[pk];assert sha(p)==obj[hk],p;raw_checked.add(str(p.relative_to(R)))
  if obj.get('archived_artifact') and obj.get('sha256'):assert sha(L/obj['archived_artifact'])==obj['sha256']
  if obj.get('path') and obj.get('sha256'):
   p=(R if obj['path'].startswith('work/') else L)/obj['path'];assert sha(p)==obj['sha256'],p
  if obj.get('visual_audit_path'):assert sha(L/obj['visual_audit_path'])==obj['visual_audit_sha256']
  for v in obj.values():
   if isinstance(v,(list,dict)):check(v)

E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()));bye={x['evidence_id']:x for x in E};byf={x['flow_id']:x for x in F}
newE=[];newF=[];names=['dragonbox-controls-deepening','anki-controls-deepening','hiby-controls-deepening']
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-coverage.csv').open()))
assert len(newE)==16 and len(newF)==3
assert all(x==bye[x['evidence_id']] and not x['hands_on'] for x in newE)
assert all(x==byf[x['flow_id']] and x['hands_on']=='false' for x in newF)
assert sorted(x['depth'] for x in newF)==['D1','D2','D2']
check(newE)
for e in newE:
 assert not e.get('derived_from') or e['derived_from'] in bye
 assert set(e.get('corroborating_evidence_ids',[]))<=set(bye)

audit=j(W/'applied-reviewed-updates.json');fids=set();eids=set();field_count=0
for u in audit['changes']:
 row=(byf if u['key']=='flow_id' else bye)[u[u['key']]]
 if u['whole_record']:assert row==u['after']
 else:assert row[u['field']]==u['after']
 if u['key']=='flow_id':
  fids.add(u['flow_id']);field_count+=len(u['changed_fields']) if u['whole_record'] else 1
 else:
  eids.add(u['evidence_id']);check(row.get('additional_captures',[]))
for f in audit['files']:
 assert sha(R/f['path'])==f['after_sha256']
 assert sha(W/'reviewed-before'/Path(f['path']).name)==f['before_sha256']
assert len(fids)==19 and field_count==75 and len(eids)==11
for fix in j(W/'reviewed-evidence-schema-fix.json'):assert bye[fix['evidence_id']]==fix['after']

index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for value in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',value))<=set(bye),(p,value)
   assert set(re.findall(r'F-M\d{3}-\d{2}',value))<=set(byf),(p,value)
assert len(counts)==32 and sum(counts.values())==2785
for name,count in zip(names,[27,60,145]):
 p=L/f'batches/{name}-inventory.csv';rows=list(csv.DictReader(p.open()));key=next(iter(rows[0]));assert len(rows)==len({r[key] for r in rows})==count
 assert b'\r' not in p.read_bytes() and p.read_bytes().endswith(b'\n')
 for row in rows:
  if row.get('raw_work_path'):check(row)

requests=j(W/'sources.json');assert len(requests)==6
assert sum(x.get('status')==403 for x in requests)==4
assert sum(x.get('status')==200 for x in requests)==1
assert sum('error' in x for x in requests)==1
check(requests);check(j(W/'privacy-local-reparse.json'))
assert j(W/'privacy-local-reparse.json')['http_status'] is None
ankis=[]
for name in ['fetch-log.json','fetch-extra-log.json']:ankis.extend(j(R/'work/anki-controls-deepening'/name))
assert len(ankis)==16 and all(x['status']==200 for x in ankis);check(ankis)
hibys=j(R/'work/hiby-controls-deepening/source-manifest.json');check(hibys)
assert sum(x.get('http_status')==200 for x in hibys)==7
assert sum(x.get('http_status')==403 for x in hibys)==1
check(j(R/'work/hiby-controls-deepening/image-manifest.json'))
known_urls={e['url'] for e in E}
for e in E:
 for cap in e.get('additional_captures',[]):known_urls.add(cap.get('capture_url',''))
brief=(R/'docs/briefs/015-exploration-undo-and-stopping.md').read_text()
assert set(re.findall(r'\]\((https?://[^)]+)\)',brief))<=known_urls
report=dict(new_evidence=16,new_tasks=3,new_D2=2,new_D1=1,new_D4=0,old_tasks_amended=19,old_fields_amended=75,old_evidence_additional_captures=11,inventories=32,inventory_rows=2785,new_inventory_rows=232,new_archived_images=4,raw_files_checked=len(raw_checked),device_or_app_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
