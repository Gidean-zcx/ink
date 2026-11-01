import csv,json
from pathlib import Path
R=Path('/home/workspace/reader');B=R/'docs/research/interaction-library/batches'
changes={}
for d in json.loads((R/'work/antennapod-controls-deepening/old-coverage-updates.json').read_text()):changes[d['flow_id']]=d['changes']
for d in csv.DictReader((R/'work/kindle-controls-deepening/existing-coverage-updates.csv').open()):changes.setdefault(d['flow_id'],{})[d['field']]={'before':d['before'],'after':d['after']}
files={};seen={};audit=[]
for p in B.glob('*-coverage.csv'):
 rows=list(csv.DictReader(p.open()));files[p]=rows
 for r in rows:
  if r['flow_id'] not in changes:continue
  assert r['flow_id'] not in seen
  for k,v in changes[r['flow_id']].items():assert r[k]==v['before'],(r['flow_id'],k,r[k],v['before'])
  seen[r['flow_id']]=p
assert set(seen)==set(changes)
for p,rows in files.items():
 touched=False
 for r in rows:
  if r['flow_id'] not in changes:continue
  old=r.copy()
  for k,v in changes[r['flow_id']].items():r[k]=v['after']
  if r['flow_id']=='F-M001-06':r['known_branches']+='；已作为附件发送给Alexa的副本另行保留，不随Kindle永久删除自动清除。'
  audit.append({'batch':str(p.relative_to(R)),'before':old,'after':r.copy()});touched=True
 if touched:
  with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(R/'work/research/interaction-depth/applied-kindle-antennapod-updates.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
print('APPLIED',len(audit),'old flows; before values verified; do not rerun')
