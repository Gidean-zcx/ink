from pathlib import Path
import json,csv,re,hashlib
r=Path('docs/research/interaction-library');w=Path('work/ihuman-controls-deepening');case=r/'cases/M-029-ihuman.md'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
efile=r/'batches/ihuman-controls-deepening-evidence.jsonl';cfile=r/'batches/ihuman-controls-deepening-coverage.csv'
ev=[json.loads(x) for x in efile.read_text().splitlines()];ids={x['evidence_id'] for x in ev}
assert ids=={f'E-IHCD-{n:02d}' for n in range(1,6)} and len(ev)==5
s=case.read_text()
for e in ev:
 assert e['product_id']=='M-029' and e['hands_on'] is False and e['evidence_id'] in s
 if e.get('archived_artifact'):assert sha(r/e['archived_artifact'])==e['sha256']
 if e.get('visual_audit_path'):assert sha(r/e['visual_audit_path'])==e['visual_audit_sha256']
 for a in e.get('additional_artifacts',[]):assert sha(r/a['path'])==a['sha256']
assert {x['evidence_id'] for x in ev if x['visual_observed']}=={'E-IHCD-02','E-IHCD-03'}
allids=set()
for f in r.rglob('*evidence.jsonl'):
 if f==efile:continue
 for line in f.read_text().splitlines():
  x=json.loads(line);eid=x['evidence_id'];assert eid not in ids,f'new E conflict {f}';allids.add(eid)
for eid in re.findall(r'E-(?:IHUM|IHD|IHCD)-\d+',s):assert eid in ids|allids,eid
with cfile.open() as f:flows=list(csv.DictReader(f))
assert len(flows)==1 and flows[0]['flow_id']=='F-M029-05' and flows[0]['depth']=='D1' and flows[0]['visual_status']=='not_observed'
assert set(re.findall(r'F-M029-\d+',s))=={f'F-M029-{n:02d}' for n in range(1,6)}
with (w/'existing-coverage-updates.csv').open() as f:updates=list(csv.DictReader(f))
assert len(updates)==4
for f in [cfile,w/'existing-coverage-updates.csv']:
 data=f.read_bytes();assert b'\r' not in data and b'\\r' not in data and b'\\n' not in data,f
for name,old in json.loads((w/'baseline.json').read_text()).items():
 if name!=str(case):assert sha(Path(name))==old,f'old batch changed:{name}'
for target in re.findall(r'\]\(([^)]+)\)',s):
 if '://' not in target:
  p=Path(target) if target.startswith('/') else case.parent/target
  assert p.exists(),target
assert '当前版本3.0.1' in (r/'artifacts/ihuman-controls-deepening/static-observation-log.md').read_text()
files=[case,efile,cfile,*sorted((r/'artifacts/ihuman-controls-deepening').iterdir()),w/'existing-coverage-updates.csv',w/'existing-coverage-field-changes.json']
result={'status':'PASS','new_evidence':5,'new_flows':1,'new_flow_depth':'D1','old_row_proposals':4,'static_images':3,'old_batch_hashes':'unchanged','hands_on':False,'failed_image_requests':4,'files':[{'path':str(f),'bytes':f.stat().st_size,'sha256':sha(f)} for f in files]}
(w/'check-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
