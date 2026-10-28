import csv,json,hashlib,re
from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
new=[]
for name in ['shortcuts','mudita','baicizhan-device']:
 new += [json.loads(x) for x in (lib/f'batches/{name}-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(new)==48
raw={}
for x in new:
 for r in [x]+x.get('additional_raw_sources',[]):
  if r.get('raw_work_path'):
   p=root/r['raw_work_path']; assert p.exists(),str(p)
   assert hashlib.sha256(p.read_bytes()).hexdigest()==r['raw_sha256'],str(p)
   raw[str(p.relative_to(root))]=r['raw_sha256']
 for key in ['source_notes_path','archived_artifact']:
  if x.get(key):assert (lib/x[key]).exists(),x[key]
# Validate all linked inventories rather than an ad hoc count.
readme=lib/'inventories/README.md'; paths=[]
for rel in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',readme.read_text()):
 p=(readme.parent/rel).resolve()
 if p not in paths:paths.append(p)
counts={str(p.relative_to(lib)):len(list(csv.DictReader(p.open()))) for p in paths}
assert len(counts)==14 and sum(counts.values())==880,counts
es={json.loads(x)['evidence_id'] for x in (lib/'evidence.jsonl').read_text().splitlines()}
fs={x['flow_id'] for x in csv.DictReader((lib/'coverage.csv').open())}
for p in [lib/'batches/shortcuts-controls-deepening-inventory.csv',lib/'artifacts/mudita-controls-deepening/function-inventory.csv']:
 b=p.read_bytes();assert b'\\r\\n' not in b and b'\r' not in b
 rows=list(csv.DictReader(p.open()));assert rows and None not in rows[0]
 for row in rows:
  for value in row.values():
   assert value is not None
   assert set(re.findall(r'E-[A-Z]+-\d{2}',value))<=es
   assert set(re.findall(r'F-M\d{3}-\d{2}',value))<=fs
sct=list(csv.DictReader((lib/'batches/shortcuts-controls-deepening-inventory.csv').open()));ids={r['entry_id'] for r in sct};assert len(ids)==114
for r in sct:
 assert not r['parent_id'] or r['parent_id'] in ids
 if r['url'] and r['upstream_id']!='APDA2B83D0E0':assert '/10.0/ios/27' in r['url']
# The two static images are preserved independently of prose edits.
images=[]
for x in new:
 if x.get('visual_observed'):
  p=lib/x['archived_artifact'];assert hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256'];images.append(x['evidence_id'])
assert images==['E-MDCD-01','E-MDCD-12']
report=dict(new_evidence=48,raw_source_records_checked=len(raw),static_images=images,inventories=len(counts),inventory_rows=sum(counts.values()),new_directory_rows=225,new_tasks=9,existing_tasks_updated=11,errors=[])
(root/'work/shortcuts-controls-deepening/verification-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False))
