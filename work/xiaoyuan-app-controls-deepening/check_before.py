from pathlib import Path
import json,csv,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/xiaoyuan-app-controls-deepening'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
es={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};fs={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
for x in j(W/'existing-coverage-updates.json'):assert fs[x['flow_id']]==x['before']
for x in j(W/'existing-evidence-additional-captures.json'):assert es[x['evidence_id']].get('additional_captures',[])==x['before']
for x in j(W/'captures.json'):
 assert x['status']==200 and sha(R/x['raw_work_path'])==x['raw_sha256']
 assert sha(R/x['text_work_path'])==x['text_sha256']
for x in j(W/'observed-artifacts.json'):assert sha(L/x['artifact'])==x['sha256'] and sha(R/x['raw_work_path'])==x['sha256']
newes=list(map(json.loads,(L/'batches/xiaoyuan-app-controls-deepening-evidence.jsonl').read_text().splitlines()));newfs=list(csv.DictReader((L/'batches/xiaoyuan-app-controls-deepening-coverage.csv').open()))
for x in newes:
 assert x['evidence_id'] not in es and x['derived_from'] in es and x['supersedes'] is None
 assert not x['hands_on'] and x['visual_observed'] and sha(L/x['archived_artifact'])==x['sha256']
for x in newfs:
 assert x['flow_id'] not in fs and x['hands_on']=='false' and x['depth']=='D2'
 assert set(x['evidence_ids'].split(';'))<=(set(es)|{z['evidence_id'] for z in newes})
assert len(newes)==13 and len(newfs)==6
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
report=dict(status='PASS',new_E=13,new_F=6,old_F=6,old_E_captures=9,images=38,inventory_rows=len(list(csv.DictReader((L/'batches/xiaoyuan-app-controls-deepening-inventory.csv').open()))),root_review='35 new images and 3 existing; M007 12 selected formal pages and version text; M056 policy/version evidence checked')
(W/'before-check-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
