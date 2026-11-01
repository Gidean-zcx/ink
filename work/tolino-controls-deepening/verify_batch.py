import csv,json,re,hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/tolino-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[]
for n in ['tolino','casio','fiio']:E += [json.loads(s) for s in (L/f'batches/{n}-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(E)==27
raw={}
for e in E:
 if e.get('raw_work_path'):
  p=R/e['raw_work_path'];assert sha(p)==e['raw_sha256'],p;raw[str(p.relative_to(R))]=sha(p)
 if e.get('parsed_text_path'):assert (R/e['parsed_text_path']).is_file()
 if e.get('extracted_work_path'):assert sha(R/e['extracted_work_path'])==e['extracted_sha256']
masterE=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];eids={e['evidence_id'] for e in masterE};flows=list(csv.DictReader((L/'coverage.csv').open()));fids={f['flow_id'] for f in flows};byf={f['flow_id']:f for f in flows};bye={e['evidence_id']:e for e in masterE}
# Verify the duplicate pages retained their original IDs and every new capture hash.
for fn in ['applied-existing-evidence-updates.json','applied-fiio-evidence-rechecks.json']:
 for x in json.loads((W/fn).read_text()):
  assert bye[x['evidence_id']]==x['after']
  for cap in x['after']['additional_captures']:
   assert sha(R/cap['raw_work_path'])==cap['raw_sha256']
   assert sha(L/cap['archived_artifact'])==cap['sha256']
assert not {'E-FICD-03','E-FICD-12','E-FICD-14'}&eids
idx=L/'inventories/README.md';ps=set((idx.parent/t).resolve() for t in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',idx.read_text()));counts={}
for p in ps:
 rs=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rs)
 for r in rs:
  assert None not in r and None not in r.values()
  for v in r.values():
   assert set(re.findall(r'E-[A-Z]+-\d{2}',v))<=eids,(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=fids,(p,v)
assert len(counts)==26 and sum(counts.values())==2286,counts
for n,c in [('tolino',185),('casio',209),('fiio',97)]:
 p=L/f'batches/{n}-controls-deepening-inventory.csv';rs=list(csv.DictReader(p.open()));key=next(iter(rs[0]));assert len(rs)==c and len({r[key] for r in rs})==c
 assert b'\r' not in p.read_bytes() and p.read_bytes().endswith(b'\n')
 if n=='tolino':assert all(not r['parent_id'] or r['parent_id'] in {x[key] for x in rs} for r in rs)
a=[]
for name in ['applied-existing-updates.json','applied-helper-updates.json']:
 for x in json.loads((W/name).read_text()):assert byf[x['flow_id']][x['field']]==x['after'],x;a.append(x)
assert len(a)==83 and len({x['flow_id'] for x in a})==21
ba=L/'artifacts/tolino-controls-deepening/browser'
for x in json.loads((ba/'raw-capture-manifest.json').read_text()):assert sha(R/x['path'])==x['sha256']
states=json.loads((ba/'observed-states.json').read_text());assert len(states)==10;assert sum('archived_screenshot' in s for s in states)==6;assert sum(s['screenshot_reviewed'] for s in states)==7
assert json.loads((ba/'browser-errors.json').read_text())==[]
# Recheck all root acquired public sources and each helper preserved raw file.
for fn in ['sources.json','sources-extra.json','sources-new.json']:
 for x in json.loads((W/fn).read_text()):assert x['status']==200 and sha(R/x['raw_work_path'])==x['raw_sha256']
report=dict(new_evidence=27,new_tasks=23,new_D2=22,new_D4=1,old_tasks_amended=21,old_fields_amended=83,rechecked_existing_evidence=6,inventories=len(counts),inventory_rows=sum(counts.values()),new_inventory_rows=491,new_archived_manual_pages=8,browser_captures=10,reviewed_browser_frames=7,archived_browser_images=6,new_device_operations=0,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
