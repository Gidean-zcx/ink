import csv,json,hashlib
from pathlib import Path
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'; work=root/'work/shortcuts-controls-deepening'
audit=work/'applied-existing-updates.json'; assert not audit.exists(),'Already applied; do not rerun'
updates=[]
for rel in ['shortcuts-controls-deepening/existing-coverage-updates.csv','baicizhan-device-controls-deepening/existing-coverage-updates.csv']:
 updates+=list(csv.DictReader((root/'work'/rel).open()))
for group in json.loads((root/'work/mudita-controls-deepening/old-coverage-updates.json').read_text()):
 for field,v in group['changes'].items(): updates.append(dict(flow_id=group['flow_id'],field=field,**v,reason='已由根代理复核重要原文及静态图'))
files={p:list(csv.DictReader(p.open())) for p in (lib/'batches').glob('*-coverage.csv')}
locations={}
for p,rows in files.items():
 for row in rows:
  locations.setdefault(row['flow_id'],[]).append((p,row))
changes={}
for u in updates:
 hits=locations[u['flow_id']]; assert len(hits)==1,(u['flow_id'],len(hits))
 p,row=hits[0]; assert row[u['field']]==u['before'],(u['flow_id'],u['field'],row[u['field']],u['before'])
 changes[p]=True
before={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in changes}
for u in updates:
 _,row=locations[u['flow_id']][0];row[u['field']]=u['after']
for p in changes:
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(files[p][0]),lineterminator='\n');w.writeheader();w.writerows(files[p])
audit.write_text(json.dumps(dict(before=before,updates=updates,after={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in changes}),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(files=len(changes),old_flows=len({x['flow_id'] for x in updates}),field_updates=len(updates)),ensure_ascii=False))
