from pathlib import Path
import csv,io,json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-video-controls-deepening'
assert not (W/'applied-reviewed-updates.json').exists(),'one-time merge'
def read(p):return json.loads((R/p).read_text())
fc=[];ec=[]
for path in ['work/iflytek-video-controls-deepening/existing-coverage-updates.json','work/pocketbook-app-controls-deepening/old-coverage-updates.json']:
 for x in read(path):
  for k,v in x['after'].items():
   if x['before'].get(k)!=v:fc.append(dict(id=x['flow_id'],field=k,before=x['before'].get(k),after=v))
for x in read('work/supernote-notes-controls-deepening/existing-coverage-updates.json'):
 for k,v in x['changes'].items():fc.append(dict(id=x['flow_id'],field=k,**v))
for path in ['work/iflytek-video-controls-deepening/existing-evidence-additional-captures.json','work/supernote-notes-controls-deepening/existing-evidence-additional-captures.json']:
 for x in read(path):ec.append(dict(id=x['evidence_id'],field='additional_captures',before=x['before'],after=x['after']))
for x in read('work/pocketbook-app-controls-deepening/old-evidence-additional-captures.json'):ec.append(dict(id=x['evidence_id'],field='additional_captures',before=x['before_additional_captures'],after=x['before_additional_captures']+x['append_additional_captures']))
assert read('work/pocketbook-app-controls-deepening/old-evidence-field-updates.json')==[]
cs={p:list(csv.DictReader(p.open())) for p in (L/'batches').glob('*-coverage.csv')};es={p:list(map(json.loads,filter(None,p.read_text().splitlines()))) for p in (L/'batches').glob('*-evidence.jsonl')}
fi={x['flow_id']:(p,x) for p,rs in cs.items() for x in rs};ei={x['evidence_id']:(p,x) for p,rs in es.items() for x in rs}
assert len(fi)==sum(map(len,cs.values())) and len(ei)==sum(map(len,es.values()))
changed=set()
for props,idx in [(fc,fi),(ec,ei)]:
 assert len(props)==len({(x['id'],x['field']) for x in props})
 for x in props:
  p,row=idx[x['id']];a=row.get(x['field'])
  if x['field']=='additional_captures' and a is None and x['before']==[]:a=[]
  assert a==x['before'],(x['id'],x['field'],a,x['before'])
  row[x['field']]=x['after'];changed.add(p)
sha=lambda b:hashlib.sha256(b).hexdigest()
backup=W/'reviewed-before';backup.mkdir(exist_ok=False);files=[];prepared={}
for p in changed:
 if p.suffix=='.csv':
  rows=cs[p];s=io.StringIO(newline='');w=csv.DictWriter(s,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows);b=s.getvalue().encode()
 else:b=''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es[p]).encode()
 old=p.read_bytes();bp=backup/p.name;bp.write_bytes(old);prepared[p]=b
 files.append(dict(path=str(p.relative_to(R)),backup=str(bp.relative_to(R)),before_sha256=sha(old),after_sha256=sha(b)))
for p,b in prepared.items():p.write_bytes(b)
report=dict(flow_changes=fc,evidence_changes=ec,files=files,old_tasks=len({x['id'] for x in fc}),old_task_fields=len(fc),old_evidence_captures=len(ec))
(W/'applied-reviewed-updates.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k.startswith('old_')}))
