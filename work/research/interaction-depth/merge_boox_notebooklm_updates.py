import csv,json
from pathlib import Path
root=Path('/home/workspace/reader');batch=root/'docs/research/interaction-library/batches'
records={p:list(csv.DictReader(p.open())) for p in batch.glob('*-coverage.csv')}
changed=[]
for u in json.loads((root/'work/boox-controls-deepening/old-coverage-updates.json').read_text()):
 hits=[(p,r) for p,rows in records.items() for r in rows if r['flow_id']==u['flow_id']]
 assert len(hits)==1,(u['flow_id'],len(hits))
 p,row=hits[0];before=dict(row)
 for k,v in u['changes'].items():assert row[k]==v['before'],(u['flow_id'],k)
 row.update({k:v['after'] for k,v in u['changes'].items()})
 assert row==u['replacement_row']
 changed.append(dict(batch=p.name,before=before,after=dict(row)))
updates=list(csv.DictReader((root/'work/notebooklm-controls-deepening/existing-coverage-updates.csv').open()))
for fid in dict.fromkeys(u['flow_id'] for u in updates):
 hits=[(p,r) for p,rows in records.items() for r in rows if r['flow_id']==fid];assert len(hits)==1
 p,row=hits[0];before=dict(row)
 for u in (x for x in updates if x['flow_id']==fid):
  assert row[u['field']]==u['before'],(fid,u['field']);row[u['field']]=u['after']
 changed.append(dict(batch=p.name,before=before,after=dict(row)))
assert len(changed)==13
for name in {x['batch'] for x in changed}:
 p=batch/name;rows=records[p]
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
(root/'work/research/interaction-depth/applied-boox-notebooklm-updates.json').write_text(json.dumps(changed,ensure_ascii=False,indent=2)+'\n')
print('Applied 7 BOOX + 6 NotebookLM existing task updates with before-value assertions.')
