import csv,json,hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/tolino-controls-deepening'
assert not (W/'applied-helper-updates.json').exists(),'already applied'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jd(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def wr(p,rs):
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
changes=json.loads((R/'work/casio-controls-deepening/existing-coverage-updates.json').read_text())
fiio=json.loads((R/'work/fiio-controls-deepening/old-coverage-updates.json').read_text())
for x in fiio:
 for k,v in x['changed_fields'].items():changes.append(dict(flow_id=x['flow_id'],field=k,**v))
files={p:list(csv.DictReader(p.open())) for p in B.glob('*-coverage.csv')}
for x in changes:
 matches=[(p,r) for p,rs in files.items() for r in rs if r['flow_id']==x['flow_id']];assert len(matches)==1
 p,r=matches[0];assert r[x['field']]==x['before'],x;x['file']=str(p.relative_to(R))
# Rechecking the same page preserves its stable evidence ID. Static manual observation stays a derived E.
remap={'E-FICD-03':'E-FIIO-03','E-FICD-12':'E-FIIO-12','E-FICD-14':'E-FIIO-10'}
def sub(s):
 for a,b in remap.items():s=s.replace(a,b)
 return s
for p in [L/'cases/M-034-fiio.md',L/'artifacts/fiio-controls-deepening/source-notes.md',B/'fiio-controls-deepening-inventory.csv']:
 s=sub(p.read_text());p.write_text(s)
for p in [L/'cases/M-034-fiio.md',L/'artifacts/fiio-controls-deepening/source-notes.md']:
 s=p.read_text()+'\n根复核：工作模式、装配范围、Audio菜单复抓沿用E-FIIO-03/12/10并追加本轮原件；新E保留12条，不把三次复查计为新来源。E-FICD-15是E-FIIO-01同一PDF的静态补证。\n';p.write_text(s)
notes=L/'artifacts/fiio-controls-deepening/source-notes.md'
p=B/'fiio-controls-deepening-evidence.jsonl';es=[json.loads(l) for l in p.read_text().splitlines()]
base=B/'audio-evidence.jsonl';old=[json.loads(l) for l in base.read_text().splitlines()];by={e['evidence_id']:e for e in old};ec=[]
for e in es:
 if e['archived_artifact']=='artifacts/fiio-controls-deepening/source-notes.md':e['sha256']=sha(notes)
 if e['evidence_id'] in remap:
  original=by[remap[e['evidence_id']]];before=json.loads(json.dumps(original));cap={k:v for k,v in e.items() if k not in ['evidence_id','product_id','supersedes']};original.setdefault('additional_captures',[]).append(cap);ec.append(dict(evidence_id=original['evidence_id'],before=before,after=original))
 if e['evidence_id']=='E-FICD-15':e['derived_from']='E-FIIO-01'
es=[e for e in es if e['evidence_id'] not in remap]
p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es));base.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in old));jd(W/'applied-fiio-evidence-rechecks.json',ec)
for p,rs in files.items():
 if p.name=='fiio-controls-deepening-coverage.csv':
  for r in rs:r['evidence_ids']=';'.join(dict.fromkeys(sub(r['evidence_ids']).split(';')))
for x in changes:
 if x['field']=='evidence_ids':x['after']=';'.join(dict.fromkeys(sub(x['after']).split(';')))
 for r in files[R/x['file']]:
  if r['flow_id']==x['flow_id']:r[x['field']]=x['after']
for p,rs in files.items():
 if p.name=='fiio-controls-deepening-coverage.csv' or any(x['file']==str(p.relative_to(R)) for x in changes):wr(p,rs)
jd(W/'applied-helper-updates.json',changes)
p=L/'cases/M-024-casio.md';s=p.read_text().replace('旧F02/04/05/08/09沿用编号，本批仅提交逐字段修订建议由root合并，未改旧批或主台账。','旧F02/04/05/08/09沿用编号；root核原手册并已将21项字段修订合回旧批和主索引。');p.write_text(s)
print(json.dumps(dict(helper_old_flows=len({x['flow_id'] for x in changes}),helper_fields=len(changes),fiio_new_evidence=len(es),rechecks=len(ec)),ensure_ascii=False))
