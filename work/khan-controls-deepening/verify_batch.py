import csv,json,hashlib,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
new=[]
for name in ['khan','kindle','antennapod']:
 new += [json.loads(x) for x in (L/f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines()]
for e in new:
 assert sha(R/e['raw_work_path'])==e['raw_sha256'],e['evidence_id']
for item in json.loads((R/'work/antennapod-controls-deepening/source-manifest.json').read_text()):assert sha(R/item['raw_work_path'])==item['raw_sha256']
for item in json.loads((R/'work/kindle-controls-deepening/fetch-results.json').read_text()):assert sha(R/item['raw_work_path'])==item['raw_sha256'] and item['status']==200
for dr in ['browser','browser-clean']:
 m=json.loads((R/'work/khan-controls-deepening'/dr/'samples.json').read_text());assert len(m['samples'])==8
 for item in m['samples']:assert sha(R/item['file'])==item['sha256']
idx=L/'inventories/README.md';paths=[]
for s in re.findall(r'\]\(([^)]+\.csv)\)',idx.read_text()):
 p=(idx.parent/s).resolve()
 if p not in paths:paths.append(p)
E={e['evidence_id'] for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};F={r['flow_id'] for r in csv.DictReader((L/'coverage.csv').open())}
counts={}
for p in paths:
 rs=list(csv.DictReader(p.open()));assert rs
 for r in rs:
  assert None not in r,(p,'column mismatch')
  for v in r.values():
   assert v is not None
   assert set(re.findall(r'E-[A-Z]+-\d{2}',v))<=E,(p,v)
   assert set(re.findall(r'F-M\d{3}-\d{2}',v))<=F,(p,v)
 counts[str(p.relative_to(L))]=len(rs)
report={'new_evidence_raw_hashes':len(new),'antenna_manifest_raw_hashes':50,'kindle_raw_hashes':14,'khan_sample_hashes':16,'inventories':len(paths),'inventory_rows':sum(counts.values()),'inventory_counts':counts,'errors':[]}
(R/'work/khan-controls-deepening/final-extra-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False,indent=2))
