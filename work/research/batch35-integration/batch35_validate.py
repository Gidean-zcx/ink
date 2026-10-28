import csv,json,hashlib,os,re
from pathlib import Path
R=Path('/home/workspace/reader')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def csvmap(p,k): return {r[k]:r for r in csv.DictReader(open(p,encoding='utf-8',newline=''))}
def jlmap(p,k): return {r[k]:r for r in map(json.loads,Path(p).read_text().splitlines()) if r}
chg=json.loads((R/'work/research/batch35-integration/changes.json').read_text())
newF=set(chg['new_flows']); newE=set(chg['new_evidence'])
bf=csvmap(R/'work/research/batch35-integration/before/docs/research/interaction-library/coverage.csv','flow_id')
af=csvmap(R/'docs/research/interaction-library/coverage.csv','flow_id')
be=jlmap(R/'work/research/batch35-integration/before/docs/research/interaction-library/evidence.jsonl','evidence_id')
ae=jlmap(R/'docs/research/interaction-library/evidence.jsonl','evidence_id')
changedF={k for k in bf.keys()&af if bf[k]!=af[k]}; changedE={k for k in be.keys()&ae if be[k]!=ae[k]}
res={}
res['flow_added_exact']=sorted(set(af)-set(bf))==sorted(newF)
res['evidence_added_exact']=sorted(set(ae)-set(be))==sorted(newE)
res['old_flows_changed']=sorted(changedF)
res['old_evidence_changed']=sorted(changedE)
res['expected_old_changes']=changedF=={'F-M015-26'} and changedE=={'E-RWCD-17'}
res['new_flows_D2_false']={k:(af[k]['depth'],af[k]['hands_on']) for k in sorted(newF)}
res['new_flows_D2_false_ok']=all(af[k]['depth']=='D2' and af[k]['hands_on']=='false' for k in newF)
loc={k:ae[k].get('locator') for k in sorted(newE)}
res['new_evidence_locators']=loc
res['new_evidence_locators_ok']=all(isinstance(v,str) and v.strip() for v in loc.values())
res['requirements_sha']=sha(R/'docs/requirements.md')
res['requirements_ok']=res['requirements_sha']=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
# Verify every new E artifact/raw/text and nested artifact hashes.
checks=[]
for eid in sorted(newE):
 r=ae[eid]
 for pathkey,hashkey,prefix in [('archived_artifact','sha256','docs/research/interaction-library'),('raw_work_path','raw_sha256',''),('text_work_path','text_sha256','')]:
  if pathkey in r and r[pathkey]:
   p=R/prefix/r[pathkey] if prefix else R/r[pathkey]
   checks.append((eid,pathkey,str(p.relative_to(R)),p.exists(),sha(p) if p.exists() else None,r.get(hashkey),(p.exists() and sha(p)==r.get(hashkey))))
 for rel,expected in r.get('artifact_hashes',{}).items():
  p=R/'docs/research/interaction-library'/rel
  checks.append((eid,'artifact_hashes',str(p.relative_to(R)),p.exists(),sha(p) if p.exists() else None,expected,(p.exists() and sha(p)==expected)))
res['file_hash_checks']=checks
res['file_hash_checks_ok']=all(x[-1] for x in checks)
# Inventory counts and uniqueness
invs={
 'new_readwise_14':'docs/research/interaction-library/batches/readwise-ai-workflows-inventory.csv',
 'new_xiaoyuan_12':'docs/research/interaction-library/batches/xiaoyuan-historical-learning-inventory.csv',
 'old_readwise_281':'docs/research/interaction-library/batches/readwise-controls-deepening-inventory.csv',
 'old_xiaoyuan_33':'docs/research/interaction-library/batches/xiaoyuan-hardware-controls-deepening-inventory.csv'}
res['inventories']={}
for name,rel in invs.items():
 rows=list(csv.DictReader(open(R/rel,encoding='utf-8',newline='')))
 key=next(iter(rows[0]))
 res['inventories'][name]={'path':rel,'rows':len(rows),'unique_ids':len({x[key] for x in rows})==len(rows),'sha256':sha(R/rel)}
# Formal README references resolve and expected text exists
p=R/'docs/research/interaction-library/inventories/README.md'; txt=p.read_text()
refs=[]
for label,target in re.findall(r'\[([^]]+)\]\(([^)]+)\)',txt):
 if 'Readwise 14项' in label or '小猿历史12域' in label:
  q=(p.parent/target).resolve(); refs.append((label,target,q.exists(),str(q.relative_to(R)) if q.exists() else str(q)))
res['new_inventory_refs']=refs
res['new_inventory_refs_ok']=len(refs)==2 and all(x[2] for x in refs)
print(json.dumps(res,ensure_ascii=False,indent=2))
