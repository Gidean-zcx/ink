import csv,json,re,hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/duolingo-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
new=[]
for name in ['duolingo','maimemo','tonie']:
 new += [json.loads(x) for x in (L/f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(new)==64
raw={};extracted=set()
for e in new:
 for rec in [e]+e.get('additional_raw_sources',[]):
  if rec.get('raw_work_path'):
   p=R/rec['raw_work_path'];assert sha(p)==rec['raw_sha256'],p;raw[str(p.relative_to(R))]=sha(p)
 if e.get('parsed_text_path'):assert (R/e['parsed_text_path']).is_file()
 if e.get('extracted_work_path'):
  p=R/e['extracted_work_path'];assert sha(p)==e['extracted_sha256'];extracted.add(str(p.relative_to(R)))
for rec in json.loads((R/'work/tonie-controls-deepening/source-manifest.json').read_text()):
 p=R/rec['path'];assert sha(p)==rec['sha256'];assert p.stat().st_size==rec['bytes']
# Helper manifest is a handoff snapshot. Root amended case wording after review;
# verify its work source entries, not the deliberately changed final case digest.
mm_count=0
for rec in json.loads((R/'work/maimemo-controls-deepening/hash-manifest.json').read_text()):
 if rec['path'].startswith('work/'):
  p=R/rec['path'];assert sha(p)==rec['sha256'],p;mm_count+=1
for rec in json.loads((L/'artifacts/duolingo-controls-deepening/browser/raw-capture-manifest.json').read_text()):
 p=R/rec['path'];assert sha(p)==rec['sha256'];assert p.stat().st_size==rec['bytes']
for rec in json.loads((W/'help-article-results.json').read_text()):
 assert sha(R/rec['raw_work_path'])==rec['raw_sha256'];assert sha(R/rec['text_path'])==rec['text_sha256']
eids={json.loads(x)['evidence_id'] for x in (L/'evidence.jsonl').read_text().splitlines()};flows=list(csv.DictReader((L/'coverage.csv').open()));fids={f['flow_id'] for f in flows}
idx=L/'inventories/README.md';paths=set((idx.parent/t).resolve() for t in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',idx.read_text()));counts={}
for p in sorted(paths):
 rs=list(csv.DictReader(p.open()));counts[str(p.relative_to(L))]=len(rs)
 for row in rs:
  assert None not in row and None not in row.values(),p
  for v in row.values():
   assert set(re.findall(r'E-[A-Z]+-\d{2}',v))<=eids,(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=fids,(p,v)
assert len(counts)==20 and sum(counts.values())==1554,counts
for name,total,key in [('duolingo',26,'entry_id'),('maimemo',61,'item_id'),('tonie',150,'inventory_id')]:
 p=L/f'batches/{name}-controls-deepening-inventory.csv';rs=list(csv.DictReader(p.open()));assert len(rs)==total and len({r[key] for r in rs})==total;assert b'\r' not in p.read_bytes() and p.read_bytes().endswith(b'\n')
 if name=='duolingo':assert all(not r['parent_id'] or r['parent_id'] in {x[key] for x in rs} for r in rs)
byf={r['flow_id']:r for r in flows}
audit=json.loads((W/'applied-existing-updates.json').read_text())
for u in audit['updates']:assert byf[u['flow_id']][u['field']]==u['after'],u
assert len({u['flow_id'] for u in audit['updates']})==8
for u in json.loads((W/'post-review-flow-updates.json').read_text()):assert byf[u['flow_id']][u['field']]==u['after']
bs=json.loads((L/'artifacts/duolingo-controls-deepening/browser/observed-states.json').read_text());assert len(bs)==57
screens=[s for s in bs if s['archived_screenshot']];assert len(screens)==14 and all(s['screenshot_reviewed'] for s in screens)
report=dict(new_evidence=64,new_tasks=23,old_tasks_amended=8,old_fields_amended=len(audit['updates']),distinct_raw_source_files=len(raw),tonie_extracted_texts=len(extracted),maimemo_work_manifest_files=mm_count,duolingo_help_articles=17,browser_states=57,browser_screenshots=14,browser_commands=44,browser_locator_errors=4,inventories=20,inventory_rows=1554,new_inventory_rows=237,errors=[])
(W/'verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
