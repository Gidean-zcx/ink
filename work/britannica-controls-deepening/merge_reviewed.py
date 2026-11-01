from pathlib import Path
import csv,io,json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/britannica-controls-deepening'
audit=W/'applied-reviewed-updates.json';assert not audit.exists(),'one-time merge already applied'
def read(p):return json.loads((R/p).read_text())
def sha(b):return hashlib.sha256(b).hexdigest()
fc=[];ec=[]
for src in ['work/britannica-controls-deepening/existing-coverage-updates.json','work/meebook-controls-deepening/old-coverage-updates.json']:
 for x in read(src):
  for k,v in x['after'].items():
   if x['before'][k]!=v:fc.append(dict(id=x['flow_id'],field=k,before=x['before'][k],after=v))
for x in read('work/weread-controls-deepening/existing-coverage-updates.json'):fc.append(dict(id=x['flow_id'],**{k:x[k] for k in ['field','before','after']}))
for src in ['work/weread-controls-deepening/existing-evidence-additional-captures.json']:
 for x in read(src):ec.append(dict(id=x['evidence_id'],field='additional_captures',before=x['before'],after=x['after']))
for x in read('work/meebook-controls-deepening/old-evidence-additional-captures.json'):
 ec.append(dict(id=x['evidence_id'],field='additional_captures',before=x['before_additional_captures'],after=x['before_additional_captures']+x['append_additional_captures']))
cs={p:list(csv.DictReader(p.open())) for p in (L/'batches').glob('*-coverage.csv')}
js={p:[json.loads(s) for s in p.read_text().splitlines() if s] for p in (L/'batches').glob('*-evidence.jsonl')}
fi={x['flow_id']:(p,x) for p,rows in cs.items() for x in rows};ei={x['evidence_id']:(p,x) for p,rows in js.items() for x in rows}
assert sum(map(len,cs.values()))==len(fi) and sum(map(len,js.values()))==len(ei)
for x in read('work/britannica-controls-deepening/additional-captures-updates.json'):
 prev=ei[x['evidence_id']][1].get('additional_captures');ec.append(dict(id=x['evidence_id'],field='additional_captures',before=prev,after=(prev or [])+[x['capture']]))
changed=set()
for changes,idx in [(fc,fi),(ec,ei)]:
 assert len(changes)==len({(x['id'],x['field']) for x in changes})
 for x in changes:
  p,row=idx[x['id']];a=row.get(x['field'])
  if x['field']=='additional_captures' and x['before']==[] and a is None:a=[]
  assert a==x['before'],(x['id'],x['field'],a,x['before'])
  row[x['field']]=x['after'];changed.add(p)
prepared={}
for p in changed:
 if p.suffix=='.csv':
  rows=cs[p];s=io.StringIO(newline='');w=csv.DictWriter(s,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows);b=s.getvalue().encode()
 else:b=''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in js[p]).encode()
 prepared[p]=b
backup=W/'reviewed-before';backup.mkdir(exist_ok=False);files=[]
for p,b in prepared.items():
 old=p.read_bytes();bp=backup/p.name;bp.write_bytes(old);files.append(dict(path=str(p.relative_to(R)),backup=str(bp.relative_to(R)),before_sha256=sha(old),after_sha256=sha(b)))
for p,b in prepared.items():p.write_bytes(b)
report=dict(flow_changes=fc,evidence_changes=ec,files=files,old_tasks=len({x['id'] for x in fc}),old_task_fields=len(fc),old_evidence_captures=len(ec))
audit.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k.startswith('old_')}))
