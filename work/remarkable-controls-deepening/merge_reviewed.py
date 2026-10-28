import csv,json,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/remarkable-controls-deepening'
assert not (W/'applied-helper-updates.json').exists(),'already merged'
def out(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
changes=json.loads((R/'work/eudic-controls-deepening/existing-coverage-updates.json').read_text())
sony=json.loads((R/'work/sony-controls-deepening/old-coverage-updates.json').read_text())
for x in sony:
 for k,v in x['changed_fields'].items():changes.append(dict(flow_id=x['flow_id'],field=k,**v))
files={p:list(csv.DictReader(p.open())) for p in B.glob('*-coverage.csv')}
# Validate all before values before any write.
for x in changes:
 matches=[(p,r) for p,rs in files.items() for r in rs if r['flow_id']==x['flow_id']]
 assert len(matches)==1,(x['flow_id'],len(matches))
 p,r=matches[0];assert r[x['field']]==x['before'],(x,r);x['file']=str(p.relative_to(R))
for x in changes:
 for r in files[R/x['file']]:
  if r['flow_id']==x['flow_id']:r[x['field']]=x['after']
for p,rs in files.items():
 if any(x['file']==str(p.relative_to(R)) for x in changes):out(p,rs)
(W/'applied-helper-updates.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
# Canonical evidence enum and clearer attribution in root case.
for p in B.glob('*-coverage.csv'):
 rs=list(csv.DictReader(p.open()));dirty=False
 for r in rs:
  if r['flow_id']=='F-M005-01':assert r['visual_status']=='observed_static';r['visual_status']='static_manual_pages_only';dirty=True
 if dirty:out(p,rs)
a=json.loads((W/'applied-existing-updates.json').read_text())
for x in a:
 if x['flow_id']=='F-M005-01' and x['field']=='visual_status':x['after']='static_manual_pages_only'
(W/'applied-existing-updates.json').write_text(json.dumps(a,ensure_ascii=False,indent=2)+'\n')
p=L/'cases/M-005-remarkable.md';s=p.read_text().replace('前者保留手写原页，后者移除被选墨迹；不是两种相同的转换入口。','整页转换保留原稿；转字专页称选区移除墨迹，实际粘贴机制仍有上述文档冲突。');p.write_text(s)
p=B/'remarkable-controls-deepening-inventory.csv';rs=list(csv.DictReader(p.open()));codes={}
for r in rs:
 if r['entry_type'].startswith('manual_'):
  m=re.match(r'\d+(?:\.\d+)*',r['title'])
  if m:codes[m.group()]=r['entry_id']
for r in rs:
 if r['parent_id']:r['parent_id']=codes[r['parent_id']]
out(p,rs)
print(json.dumps(dict(helper_flows=len(set(x['flow_id'] for x in changes)),helper_fields=len(changes),root_fields=len(a)),ensure_ascii=False))
