import csv,json,re,hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/remarkable-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[]
for n in ['remarkable','sony','eudic']:E += [json.loads(s) for s in (L/f'batches/{n}-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(E)==36
raw={}
for e in E:
 for x in [e]+e.get('additional_raw_sources',[]):
  p=R/x['raw_work_path'];assert sha(p)==x['raw_sha256'],p;raw[str(p.relative_to(R))]=sha(p)
 if e.get('parsed_text_path'):assert (R/e['parsed_text_path']).is_file()
 if e.get('extracted_work_path'):assert sha(R/e['extracted_work_path'])==e['extracted_sha256']
for x in json.loads((L/'artifacts/remarkable-controls-deepening/rechecked-existing-evidence.json').read_text()):assert sha(R/x['raw_work_path'])==x['raw_sha256']
for x in json.loads((R/'work/sony-controls-deepening/source-manifest.json').read_text()):assert sha(R/x['path'])==x['sha256']
eu=0
for x in json.loads((R/'work/eudic-controls-deepening/hash-manifest.json').read_text()):
 if x['path'].startswith('work/'):assert sha(R/x['path'])==x['sha256'];eu+=1
masterE=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];eids={e['evidence_id'] for e in masterE};flows=list(csv.DictReader((L/'coverage.csv').open()));fids={f['flow_id'] for f in flows};byf={f['flow_id']:f for f in flows}
idx=L/'inventories/README.md';ps=set((idx.parent/t).resolve() for t in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',idx.read_text()));counts={}
for p in ps:
 rs=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rs)
 for r in rs:
  assert None not in r and None not in r.values()
  for v in r.values():
   assert set(re.findall(r'E-[A-Z]+-\d{2}',v))<=eids,(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=fids,(p,v)
assert len(counts)==23 and sum(counts.values())==1795,counts
for n,count in [('remarkable',125),('sony',82),('eudic',34)]:
 p=L/f'batches/{n}-controls-deepening-inventory.csv';rs=list(csv.DictReader(p.open()));key=next(iter(rs[0]));assert len(rs)==count and len({r[key] for r in rs})==count
 assert b'\r' not in p.read_bytes() and p.read_bytes().endswith(b'\n')
 if n=='remarkable':assert all(not r['parent_id'] or r['parent_id'] in {x[key] for x in rs} for r in rs)
a=[]
for name in ['applied-existing-updates.json','applied-helper-updates.json','post-review-flow-updates.json']:
 for x in json.loads((W/name).read_text()):assert byf[x['flow_id']][x['field']]==x['after'],x;a.append(x)
# Repeat captures remain same IDs and are also present in master.
for e in masterE:
 for x in e.get('additional_captures',[]):
  if x.get('raw_work_path','').startswith('work/remarkable-controls-deepening/'):
   assert sha(R/x['raw_work_path'])==x['raw_sha256']
report=dict(new_evidence=len(E),new_tasks=16,old_tasks_amended=20,old_fields_amended=76,post_review_new_task_title=1,distinct_new_raw_files=len(raw),rechecked_root_sources=5,sony_html_sources=18,eudic_work_manifest_files=eu,inventories=len(counts),inventory_rows=sum(counts.values()),new_inventory_rows=241,new_static_pages=3,new_product_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
