from pathlib import Path
import json,csv,re,hashlib
root=Path('docs/research/interaction-library');w=Path('work/fenbi-controls-deepening')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
case=root/'cases/M-032-fenbi.md';s=case.read_text()
efile=root/'batches/fenbi-controls-deepening-evidence.jsonl';cfile=root/'batches/fenbi-controls-deepening-coverage.csv'
ev=[json.loads(l) for l in efile.read_text().splitlines()];ids={e['evidence_id'] for e in ev}
assert len(ev)==len(ids)==12
assert ids=={f'E-FBCD-{n:02d}' for n in range(1,13)}
required={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
for r in ev:
 assert required<=r.keys() and r['product_id']=='M-032' and r['hands_on'] is False
 assert r['evidence_id'] in s
 if r['archived_artifact']:assert sha(root/r['archived_artifact'])==r['sha256']
 if r.get('visual_audit_path'):assert sha(root/r['visual_audit_path'])==r['visual_audit_sha256']
 for a in r.get('additional_artifacts',[]):assert sha(root/a['path'])==a['sha256']
 if r['visual_observed']:assert r['evidence_id']=='E-FBCD-07'
with cfile.open() as f:new=list(csv.DictReader(f))
assert [r['flow_id'] for r in new]==['F-M032-09','F-M032-10']
for r in new:
 assert r['depth']=='D2' and r['hands_on']=='false' and r['flow_id'] in s
 assert set(r['evidence_ids'].split(';'))<=ids
 assert (root/r['case_path']).exists()
allids=set()
for f in root.rglob('*evidence.jsonl'):
 if f==efile:continue
 for line in f.read_text().splitlines():
  try:r=json.loads(line)
  except Exception:continue
  if r.get('evidence_id') in ids:raise AssertionError(f'new E duplicate outside own batch: {f}')
  allids.add(r.get('evidence_id'))
for eid in re.findall(r'E-(?:FENB|FBCD)-\d+',s):assert eid in allids|ids,eid
baseline=json.loads((w/'baseline.json').read_text())
for name,old in baseline.items():
 if name==str(case):continue
 assert sha(Path(name))==old,f'old batch changed: {name}'
with (w/'existing-coverage-updates.csv').open() as f:updates=list(csv.DictReader(f))
assert len(updates)==6
assert {r['flow_id'] for r in updates if r['depth']=='D1'}=={'F-M032-04','F-M032-05','F-M032-06','F-M032-08'}
for f in [cfile,w/'existing-coverage-updates.csv']:
 b=f.read_bytes();assert b'\r' not in b and b'\\r' not in b and b'\\n' not in b,f
for target in re.findall(r'\]\(([^)]+)\)',s):
 if '://' not in target:assert (case.parent/target).resolve().exists(),target
files=[case,efile,cfile,*sorted((root/'artifacts/fenbi-controls-deepening').iterdir()),w/'existing-coverage-updates.csv',w/'existing-coverage-field-changes.json']
result={'status':'PASS','new_evidence':len(ev),'new_flows':len(new),'old_row_proposals':len(updates),'static_images':2,'hands_on':False,'old_batch_hashes':'unchanged','files':[{'path':str(f),'sha256':sha(f),'bytes':f.stat().st_size} for f in files]}
(w/'check-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
