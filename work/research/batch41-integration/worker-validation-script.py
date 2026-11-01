import csv,json,hashlib,re
from pathlib import Path
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'
# refs
rp=list(csv.DictReader((lib/'reference-points.csv').open()))
headers=list(rp[0].keys()) if rp else []
expected=['reference_id','product_id','question_tags','flow_ids','evidence_ids','source_locator','artifact_path','precondition','action','visible_result','evidence_basis','persistence_result','unknown_branches','version_scope','checked_on']
# master ids
flows={}
for p in [lib/'coverage.csv',*sorted((lib/'batches').glob('*-coverage.csv'))]:
 if not p.exists(): continue
 for r in csv.DictReader(p.open()):
  if r.get('flow_id'): flows[r['flow_id']]=r
E={}
for line in (lib/'evidence.jsonl').open():
 x=json.loads(line); E[x['evidence_id']]=x
missing=[]
for r in rp:
 for f in filter(None,r['flow_ids'].split(';')):
  if f not in flows:missing.append((r['reference_id'],'F',f))
 for e in filter(None,r['evidence_ids'].split(';')):
  if e not in E:missing.append((r['reference_id'],'E',e))
 p=lib/r['artifact_path']
 if not p.exists():missing.append((r['reference_id'],'artifact',str(p)))
empty=[(r['reference_id'],k) for r in rp for k in expected if not (r.get(k) or '').strip()]
# requirements
reqsha=hashlib.sha256((root/'docs/requirements.md').read_bytes()).hexdigest()
# new E
ids=[f'E-INRS-{i:02d}' for i in range(1,43)]+[f'E-GNRS-{i:02d}' for i in range(1,5)]
new=[E.get(i) for i in ids]; absent=[i for i,x in zip(ids,new) if x is None]
hand=[(x['evidence_id'],x.get('hands_on')) for x in new if x and x.get('hands_on') is not False]
visual=[x for x in new if x and x.get('visual_observed') is True]
image_paths=[]
for x in visual:
 for k,v in (x.get('artifact_hashes') or {}).items():
  if Path(k).suffix.lower() in {'.png','.jpg','.jpeg','.webp'}: image_paths.append((x['evidence_id'],k,v))
missing_images=[]; bad_hash=[]
for eid,p,h in image_paths:
 q=lib/p
 if not q.exists():missing_images.append((eid,p))
 elif hashlib.sha256(q.read_bytes()).hexdigest()!=h:bad_hash.append((eid,p))
# new flow relative baseline manifest frozen before tree coverage ids
before=root/'work/research/batch41-integration/before/docs/research/interaction-library/coverage.csv'
bflows=set()
if before.exists():
 for r in csv.DictReader(before.open()):bflows.add(r['flow_id'])
else:
 # use all batch41 before coverage parts if master absent
 for p in (root/'work/research/batch41-integration/before/docs/research/interaction-library/batches').glob('*-coverage.csv'):
  for r in csv.DictReader(p.open()):bflows.add(r['flow_id'])
newflows=sorted(set(flows)-bflows)
print(json.dumps({'rp_count':len(rp),'headers':headers,'headers_exact':headers==expected,'rp_unique':len({r['reference_id'] for r in rp})==len(rp),'rp_missing_refs':missing,'rp_empty_fields':empty,'requirements_sha256':reqsha,'requirements_match':reqsha=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087','new_e_absent':absent,'new_e_nonfalse_hands_on':hand,'visual_e_ids':[x['evidence_id'] for x in visual],'image_count':len(image_paths),'image_unique_count':len({p for _,p,_ in image_paths}),'missing_images':missing_images,'bad_image_hash':bad_hash,'new_flows':newflows,'new_flow_records':{f:flows[f] for f in newflows}},ensure_ascii=False,indent=2))
