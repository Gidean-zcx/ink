import csv, hashlib, json, re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/dragonbox-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[json.loads(s) for s in (L/'batches/dragonbox-controls-deepening-evidence.jsonl').read_text().splitlines()]
F=list(csv.DictReader((L/'batches/dragonbox-controls-deepening-coverage.csv').open()))
I=list(csv.DictReader((L/'batches/dragonbox-controls-deepening-inventory.csv').open()))
oldE={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
oldF={f['flow_id']:f for f in csv.DictReader((L/'coverage.csv').open())}
assert len(E)==5 and len(F)==2 and len(I)==27
assert not set(oldE)&{e['evidence_id'] for e in E}
assert not set(oldF)&{f['flow_id'] for f in F}
urls={e['url'] for e in oldE.values()}
for e in E:
 assert not e['hands_on']
 assert e['url'] not in urls or e.get('derived_from') in oldE
 assert sha(R/e['raw_work_path'])==e['raw_sha256']
 assert sha(L/e['archived_artifact'])==e['sha256']
 for a in e.get('additional_artifacts',[]):assert sha(L/a['path'])==a['sha256']
 for a in e.get('observed_work_images',[]):assert sha(R/a['path'])==a['sha256']
 if e.get('visual_audit_path'):assert sha(L/e['visual_audit_path'])==e['visual_audit_sha256']
ids=set(oldE)|{e['evidence_id'] for e in E};fids=set(oldF)|{f['flow_id'] for f in F}
for row in F+I:
 assert None not in row and None not in row.values()
 assert set(row['evidence_ids'].split(';'))-ids<=set([''])
 if 'flow_ids' in row:assert set(row['flow_ids'].split(';'))-fids<=set([''])
for name in ['coverage','inventory']:
 raw=(L/f'batches/dragonbox-controls-deepening-{name}.csv').read_bytes()
 assert b'\r' not in raw and raw.endswith(b'\n')
updates=json.loads((W/'old-coverage-updates.json').read_text())
for u in updates:
 assert oldF[u['flow_id']]==u['before']
 assert u['changed_fields']=={k:dict(before=u['before'][k],after=u['after'][k]) for k in u['before'] if u['before'][k]!=u['after'][k]}
caps=json.loads((W/'old-evidence-additional-captures.json').read_text())
for u in caps:
 assert oldE[u['evidence_id']]==u['before']
 for c in u['after']['additional_captures']:
  assert sha(R/c['raw_work_path'])==c['raw_sha256']
  assert sha(L/c['archived_artifact'])==c['sha256']
assert len(set(re.findall(r'F-M050-\d{2}',(L/'cases/M-050-dragonbox.md').read_text())))==8
report=dict(result='PASS',new_E=5,new_F=2,new_D1=1,new_D2=1,inventory=27,old_F=5,old_fields=sum(len(u['changed_fields']) for u in updates),old_E=1,archived_images=2,viewed_pages=3,hands_on=False)
(W/'check-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
