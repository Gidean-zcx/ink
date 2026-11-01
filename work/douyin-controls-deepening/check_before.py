from pathlib import Path
import csv,json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/douyin-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
base=json.loads((W/'baseline.json').read_text());assert sha(R/'docs/requirements.md')==base['requirements_sha256'];assert sha(W/'case-before.md')==base['case_sha256']
currentF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())};currentE={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
for x in json.loads((W/'existing-coverage-updates.json').read_text()):assert currentF[x['flow_id']]==x['before'];assert x['after']['depth']==x['before']['depth']
for x in json.loads((W/'additional-captures-updates.json').read_text()):assert currentE[x['evidence_id']].get('additional_captures',[])==x['before']
es=[json.loads(s) for s in (L/'batches/douyin-controls-deepening-evidence.jsonl').read_text().splitlines()];fs=list(csv.DictReader((L/'batches/douyin-controls-deepening-coverage.csv').open()));inv=list(csv.DictReader((L/'batches/douyin-controls-deepening-inventory.csv').open()))
for e in es:
 assert e['evidence_id'] not in currentE and e['supersedes'] is None and e['hands_on'] is False and e['visual_observed'] is True
 assert sha(L/e['archived_artifact'])==e['sha256'];assert sha(R/e['raw_work_path'])==e['raw_sha256'];assert sha(R/e['raw_image_work_path'])==e['raw_image_sha256']
 for a in e['observed_artifacts']:assert sha(L/a['artifact'])==a['sha256']==sha(R/a['raw_work_path'])
for f in fs:assert f['flow_id'] not in currentF and f['depth']=='D2' and f['hands_on']=='false'
assert len(es)==4 and len(fs)==5 and len(inv)==105
assert len({a['artifact'] for e in es for a in e['observed_artifacts']})==20
E=set(currentE)|{e['evidence_id'] for e in es};F=set(currentF)|{f['flow_id'] for f in fs}
for row in fs+inv:
 for key,target in [('evidence_ids',E),('flow_ids',F)]:
  if key in row:assert set(filter(None,row[key].split(';')))<=target
print(json.dumps(dict(result='PASS',newE=4,newF=5,inventory_rows=105,formal_images=20,oldF=6,requirements_unchanged=True)))
