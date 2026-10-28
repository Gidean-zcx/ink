import csv,json,re,hashlib
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/gemini-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=set();formal=set()
def check(x):
 if isinstance(x,list):
  for v in x:check(v)
 elif isinstance(x,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256'),('extracted_work_path','extracted_sha256'),('structure_path','structure_sha256')]:
   if x.get(pk):
    p=R/x[pk];assert sha(p)==x[hk],p;raw.add(p)
  for pk,hk in [('archived_artifact','sha256'),('visual_audit_path','visual_audit_sha256')]:
   if x.get(pk):
    p=L/x[pk];digest=x.get(hk) or (x.get('sha256') if x.get('archived_artifact')==x[pk] else None);assert digest and sha(p)==digest,p;formal.add(p)
  for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h;formal.add(L/p)
  if x.get('path') and x.get('sha256'):
   p=R/x['path'] if x['path'].startswith(('work/','docs/')) else L/x['path'];assert sha(p)==x['sha256'],p
   (formal if p.is_relative_to(L) else raw).add(p)
  for v in x.values():
   if isinstance(v,(dict,list)):check(v)
E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()));be={e['evidence_id']:e for e in E};bf={f['flow_id']:f for f in F}
names=['gemini','palma','ireader'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-controls-deepening-coverage.csv').open()))
assert len(newE)==10 and len(newF)==5
for e in newE:assert be[e['evidence_id']]==e and e['hands_on'] is False
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json');fix=j(W/'date-semantics-fix.json');fixkey=(fix['evidence_id'],fix['field'])
for x in a['flow_changes']:assert bf[x['id']][x['field']]==x['after']
for x in a['evidence_changes']:
 if (x['id'],x['field'])==fixkey:
  assert x['after']==fix['before'];assert be[x['id']][x['field']]==fix['after']
 else:assert be[x['id']][x['field']]==x['after']
 check(be[x['id']].get('additional_captures',[]))
files={x['path']:x for x in a['files']}
for x in fix['files']:
 assert files[x['path']]['after_sha256']==x['before_sha256'];assert sha(R/x['backup'])==x['before_sha256'];assert sha(R/x['path'])==x['after_sha256']
for x in a['files']:
 assert sha(R/x['backup'])==x['before_sha256']
 expected=next((y['after_sha256'] for y in fix['files'] if y['path']==x['path']),x['after_sha256']);assert sha(R/x['path'])==expected
check(j(W/'captures.json'));check(j(R/'work/palma-controls-deepening/formal-manifest.json'))
for line in (R/'work/ireader-controls-deepening/manifest.sha256').read_text().splitlines():
 h,p=line.split('  ',1);assert sha(R/p)==h
invfix=j(W/'inventory-enrichment.json');assert sha(R/invfix['backup'])==invfix['before_sha256'];assert sha(R/invfix['path'])==invfix['after_sha256']
index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for value in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',value))<=set(be),(p,value)
   assert set(re.findall(r'F-M\d{3}-\d{2}',value))<=set(bf),(p,value)
assert (len(counts),sum(counts.values()))==(54,5097)
for name,n in zip(names,[129,137,45]):
 rows=list(csv.DictReader((L/f'batches/{name}-controls-deepening-inventory.csv').open()));assert len(rows)==len({r[next(iter(r))] for r in rows})==n
known={e['url'] for e in E};brief=(R/'docs/briefs/022-ai-actions-and-resume-conditions.md').read_text();assert set(re.findall(r'\]\((https?://[^)]+)\)',brief))<=known
assert len(E)==1300 and len(F)==793 and Counter(x['depth'] for x in F)=={'D2':699,'D1':79,'D4':15}
report=dict(new_evidence=10,new_tasks=5,new_D2=5,new_D4=0,old_tasks_amended=17,old_fields_amended=60,old_evidence_additional_captures=19,old_evidence_locator_fields_corrected=1,app_release_date_semantics_corrected=1,inventories=54,inventory_rows=5097,new_inventory_rows=311,new_static_images=5,raw_files_checked=len(raw),actual_product_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
