from pathlib import Path
import json,csv,io,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-planning-reading-deepening'
def load(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert not (W/'merge-result.json').exists(),'one-time merge'
cov={p:list(csv.DictReader(io.StringIO(p.read_text()))) for p in (L/'batches').glob('*-coverage.csv')}
ev={p:[json.loads(s) for s in p.read_text().splitlines() if s.strip()] for p in (L/'batches').glob('*-evidence.jsonl')}
changed=set();audit=[]
for name in ['boox-reading-controls-deepening/old-coverage-updates.json','ihuman-learning-controls-deepening/existing-coverage-updates.json']:
 for u in load(R/'work'/name):
  matches=[(p,i,r) for p,rs in cov.items() for i,r in enumerate(rs) if r['flow_id']==u['flow_id']];assert len(matches)==1
  p,i,r=matches[0]
  if 'before' in u:assert r==u['before'];after=u['after']
  else:
   for k,v in u['changes'].items():assert r[k]==v['before'],(u['flow_id'],k)
   after=u['replacement_row']
  audit.append(dict(kind='flow',id=u['flow_id'],path=str(p.relative_to(R)),before=r,after=after));cov[p][i]=after;changed.add(p)
for name in ['boox-reading-controls-deepening/old-evidence-additional-captures.json','ihuman-learning-controls-deepening/existing-evidence-additional-captures.json','iflytek-planning-reading-deepening/existing-evidence-additional-captures.json']:
 for u in load(R/'work'/name):
  matches=[(p,i,r) for p,rs in ev.items() for i,r in enumerate(rs) if r['evidence_id']==u['evidence_id']];assert len(matches)==1
  p,i,r=matches[0];before=u.get('before',u.get('before_additional_captures',[]));assert r.get('additional_captures',[])==before,u['evidence_id']
  if 'expected_url' in u:assert r['url']==u['expected_url']
  after=u.get('after',before+u.get('append_additional_captures',[]))
  audit.append(dict(kind='evidence_additional_capture',id=u['evidence_id'],path=str(p.relative_to(R)),before=before,after=after));r['additional_captures']=after;changed.add(p)
for p in changed:
 if p.suffix=='.csv':
  with p.open('w',newline='') as h:
   w=csv.DictWriter(h,fieldnames=list(cov[p][0]),lineterminator='\n');w.writeheader();w.writerows(cov[p])
 else:p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev[p]))
out=dict(old_flows=4,old_flow_fields=sum(sum(x['before'].get(k)!=v for k,v in x['after'].items()) for x in audit if x['kind']=='flow'),old_evidence_captures=4,changed_files={str(p.relative_to(R)):sha(p) for p in changed},audit=audit)
(W/'merge-result.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print({k:v for k,v in out.items() if k!='audit'})
