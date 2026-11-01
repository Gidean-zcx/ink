import csv,json
from pathlib import Path
root=Path('/home/workspace/reader');batch=root/'docs/research/interaction-library/batches'
updates={}
for x in json.loads((root/'work/pocketbook-controls-deepening/old-coverage-updates.json').read_text()):
 updates[x['flow_id']]={k:(v['old'],v['new']) for k,v in x['changes'].items()}
for x in json.loads((root/'work/fenbi-controls-deepening/existing-coverage-field-changes.json').read_text()):
 updates[x['flow_id']]={k:(v['before'],v['after']) for k,v in x['field_updates'].items()}
found=set(); staged=[]
for p in batch.glob('*-coverage.csv'):
 with p.open() as f: reader=csv.DictReader(f);fields=reader.fieldnames;rows=list(reader)
 changed=False
 for r in rows:
  if r['flow_id'] not in updates:continue
  found.add(r['flow_id'])
  for k,(old,new) in updates[r['flow_id']].items():
   assert r[k]==old,(p,r['flow_id'],k,r[k],old)
   r[k]=new
  changed=True
 if changed:staged.append((p,fields,rows))
assert found==set(updates),(found,set(updates))
for p,fields,rows in staged:
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
print(json.dumps({'updated_flows':len(found),'batches_changed':[p.name for p,_,_ in staged]}))
