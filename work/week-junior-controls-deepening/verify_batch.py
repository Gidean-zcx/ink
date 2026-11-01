import json,csv,hashlib,zipfile
from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';e=[json.loads(x) for x in (lib/'evidence.jsonl').read_text().splitlines()];flows=list(csv.DictReader((lib/'coverage.csv').open()));eids={x['evidence_id'] for x in e};fids={x['flow_id'] for x in flows}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
states=json.loads((lib/'artifacts/week-junior-controls-deepening/browser/observed-states.json').read_text());assert len(states)==14
for x in states:assert digest(lib/x['image'])==x['sha256']
s={x['capture']:x for x in states};assert [s[x]['page_number'] for x in ['01-initial','02-next','06-jump14','08-result15','11-front','12-reopen-same-context','13-new-context','14-last-page']]==['1','2','14','14','1','14','1','32']
errors=[];actions=0
for name in ['trace-actions.json','fresh-context-trace-actions.json']:
 data=json.loads((lib/'artifacts/week-junior-controls-deepening/browser'/name).read_text());actions+=len(data['actions']);errors += [x for x in data['actions'] if x['error']]
assert len(errors)==3,len(errors)
for x in json.loads((lib/'artifacts/week-junior-controls-deepening/browser/raw-trace-manifest.json').read_text()):assert digest(root/x['path'])==x['sha256']
for i in ['E-WJCD-06','E-WJCD-07','E-WJCD-08']:
 x=next(x for x in e if x['evidence_id']==i);assert digest(root/x['raw_work_path'])==x['raw_sha256']
paths=[*sorted((lib/'inventories').glob('*.csv')),lib/'artifacts/pocketbook-controls-deepening/functional-inventory.csv'];count=0
for p in paths:
 for r in csv.DictReader(p.open()):
  count+=1
  for k in ['evidence_id','evidence_ids']:
   if r.get(k):assert set(r[k].split(';'))<=eids,(p,r)
  for k in ['related_flow_ids','flow_ids']:
   if r.get(k):assert set(r[k].split(';'))<=fids,(p,r)
assert count==240,count
# Operation records must have screenshots and direct action-result pairs, not just a label.
for x in e:
 if x.get('operation_method')=='automated_public_browser':
  assert len([a for a in x['artifact_hashes'] if a.endswith('.png')])==14
  assert x['operation_audit_sha256']==digest(lib/x['operation_audit_path'])
print(json.dumps({'screenshots_verified':len(states),'ui_action_log_entries':actions,'recorded_failed_operations':len(errors),'inventory_files':len(paths),'inventory_rows':count,'raw_traces_and_activity_pdfs':'hashes_match','errors':[]},ensure_ascii=False,indent=2))
