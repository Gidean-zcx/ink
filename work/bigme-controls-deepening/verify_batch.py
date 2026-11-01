import csv,json,re,hashlib
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/bigme-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=set();formal=set()
def check(x):
 if isinstance(x,list):
  for v in x:check(v)
 elif isinstance(x,dict):
  for pk,hk in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256'),('extracted_work_path','extracted_sha256'),('failed_body_work_path','failed_body_sha256')]:
   if x.get(pk):
    p=R/x[pk];expected=x.get(hk)
    if expected is None and pk=='raw_work_path' and x.get('path') and x.get('sha256'):
     # Moaan observed_artifacts uses one digest for the byte-identical raw/formal copies.
     expected=x['sha256']
    assert expected and sha(p)==expected,p;raw.add(p)
  for pk,hk in [('archived_artifact','sha256'),('visual_audit_path','visual_audit_sha256'),('audit_artifact','audit_sha256')]:
   if x.get(pk):
    p=L/x[pk];assert sha(p)==x[hk],p;formal.add(p)
  for p,h in x.get('artifact_hashes',{}).items():assert sha(L/p)==h;formal.add(L/p)
  if x.get('path') and x.get('sha256'):
   p=R/x['path'] if x['path'].startswith(('work/','docs/')) else L/x['path'];assert sha(p)==x['sha256'],p
   (formal if p.is_relative_to(L) else raw).add(p)
  for v in x.values():
   if isinstance(v,(dict,list)):check(v)
E=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];F=list(csv.DictReader((L/'coverage.csv').open()));be={e['evidence_id']:e for e in E};bf={f['flow_id']:f for f in F}
names=['bigme','moaan','baicizhan'];newE=[];newF=[]
for name in names:
 newE.extend(json.loads(s) for s in (L/f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines())
 newF.extend(csv.DictReader((L/f'batches/{name}-controls-deepening-coverage.csv').open()))
assert len(newE)==5 and len(newF)==3
for e in newE:assert be[e['evidence_id']]==e and e['hands_on'] is False
for f in newF:assert bf[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
check(newE);a=j(W/'applied-reviewed-updates.json')
assert (a['old_tasks'],a['old_task_fields'],a['old_evidence_captures'],a['old_evidence_other_fields'])==(17,61,21,3)
for x in a['flow_changes']:assert bf[x['id']][x['field']]==x['after']
for x in a['evidence_changes']:
 assert be[x['id']][x['field']]==x['after'];check(be[x['id']].get('additional_captures',[]))
for x in a['files']:
 assert sha(R/x['backup'])==x['before_sha256'];assert sha(R/x['path'])==x['after_sha256']
for n in ['captures.json','image-captures.json','news-capture.json','gif-captures.json','gif-frame-manifest.json']:check(j(W/n))
check(j(R/'work/moaan-controls-deepening/formal-manifest.json'))
# Frozen helper manifests are checked as available; their before-checks must not run after merge.
bp=R/'work/baicizhan-controls-deepening'
for p in bp.glob('*manifest*.json'):check(j(p))
if (bp/'manifest.sha256').exists():
 for line in (bp/'manifest.sha256').read_text().splitlines():
  h,p=line.split('  ',1);assert sha(R/p)==h
assert j(W/'captures.json')[0]['status']==403
assert not (W/'raw/manual.pdf').read_bytes().startswith(b'%PDF')
assert bf['F-M011-06']['depth']==bf['F-M011-07']['depth']==bf['F-M016-01']['depth']=='D1'
index=L/'inventories/README.md';paths={(index.parent/p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',index.read_text())};counts={}
for p in paths:
 rows=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rows)
 for row in rows:
  assert None not in row and None not in row.values(),p
  for value in row.values():
   assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}',value))<=set(be),(p,value)
   assert set(re.findall(r'F-M\d{3}-\d{2}',value))<=set(bf),(p,value)
assert (len(counts),sum(counts.values()))==(57,5299)
for name,n in zip(names,[65,82,55]):
 rows=list(csv.DictReader((L/f'batches/{name}-controls-deepening-inventory.csv').open()));assert len(rows)==len({r[next(iter(r))] for r in rows})==n
assert len(E)==1305 and len(F)==796 and Counter(x['depth'] for x in F)=={'D2':702,'D1':79,'D4':15}
report=dict(new_evidence=5,new_tasks=3,new_D2=3,new_D4=0,old_tasks_amended=17,old_fields_amended=61,old_evidence_additional_captures=21,old_evidence_fields_corrected=3,inventories=57,inventory_rows=5299,new_inventory_rows=202,new_static_images=6,raw_files_checked=len(raw),actual_product_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
