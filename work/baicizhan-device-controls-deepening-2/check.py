"""Read-only checks for this batch BEFORE root merges old-row proposals."""
import csv
import hashlib
import json
from pathlib import Path
import re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
P='baicizhan-device-controls-deepening-2';W=R/'work'/P
count=0
def ok(x,label):
 global count
 assert x,label
 count+=1
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def rows(p):
 with p.open() as f:return list(csv.DictReader(f))
b=load(W/'baseline.json');fs=load(W/'coverage-before.json');es=load(W/'evidence-before.json')
fc=load(W/'existing-coverage-updates.json');ec=load(W/'existing-evidence-additional-captures.json')
ok(sha(W/'case-before.md')==b['case_sha256'],'case baseline')
ok(b['visual_case_dependencies']==[],'no case-dependent old audit')
for p in load(W/'build-result.json')['old_source_batches']:ok(sha(R/p)==b['old_batches'][p],'old batch hash '+p)
for c in fc:
 r=next(r for r in rows(R/c['source_batch']) if r['flow_id']==c['flow_id'])
 ok(r[c['field']]==c['before'],'F before '+c['flow_id']+' '+c['field'])
 ok(c['before']!=c['after'],'F changed value')
 ok(c['field'] in ['evidence_ids','gaps','checked_on'],'no depth/visual modification')
for c in ec:
 r=next(json.loads(s) for s in (R/c['source_batch']).read_text().splitlines() if json.loads(s)['evidence_id']==c['evidence_id'])
 ok(r.get('additional_captures',[])==c['before'],'E before')
 ok(c['after'][:-1]==c['before'],'append-only capture')
 a=c['after'][-1]
 ok(a['capture_url']==r['url'],'same URL')
 ok(sha(R/a['raw_work_path'])==a['raw_sha256'],'capture raw hash')
 ok(sha(L/a['archived_artifact'])==a['sha256'],'capture notes hash')
 ok(a['access_mode']=='local_reread_of_existing_public_artifact','no fresh GET claimed')
 ok(not a['hands_on'] and not a['visual_observed'],'capture no observation')
for e in es:
 if e.get('archived_artifact'):ok(sha(L/e['archived_artifact'])==e['sha256'],'old image unchanged')
ne=[json.loads(s) for s in (L/'batches'/f'{P}-evidence.jsonl').read_text().splitlines()]
nf=rows(L/'batches'/f'{P}-coverage.csv');inv=rows(L/'batches'/f'{P}-inventory.csv')
ok(len(ne)==1,'one E');ok(len(nf)==0,'zero new F');ok(len(inv)==45,'45 inventory rows')
ok(sum(r['node_type']=='existing_target_or_historical_item' for r in inv)==17,'17 old target/historical entries')
ok(sum(r['node_type']=='excluded_category' for r in inv)==5,'5 excluded parents')
ok(sum(r['node_type']=='excluded_question' for r in inv)==23,'23 excluded questions')
e=ne[0]
ok(e['evidence_id']=='E-BDCQ-01' and e['supersedes'] is None,'E id/supersedes')
ok(e['scope_role']=='model_applicability_boundary_only','E limited to applicability')
ok(not e['hands_on'] and not e['visual_observed'],'new E no visual/hands-on')
ok(e['archived_artifact'].startswith('artifacts/'),'library relative archive')
ok(sha(L/e['archived_artifact'])==e['sha256'],'formal notes hash')
ok(sha(R/e['raw_work_path'])==e['raw_sha256'],'new JSON hash')
for pr in e['provenance']:ok(sha(R/pr['raw_work_path'])==pr['raw_sha256'],'routing provenance hash')
for p in (L/'batches').glob('*-evidence.jsonl'):
 if p.name==P+'-evidence.jsonl':continue
 for line in p.read_text().splitlines():
  r=json.loads(line)
  ok(r['evidence_id']!='E-BDCQ-01','no E collision')
  ok(r['url']!=e['url'],'no repeated primary URL')
allowed_e={r['evidence_id'] for r in es}|{'E-BDCQ-01'};allowed_f={r['flow_id'] for r in fs}
for r in inv:
 ok(set(r['evidence_ids'].split(';'))<=allowed_e,'inventory E')
 ok(set(r['flow_ids'].split(';'))-{''}<=allowed_f,'inventory F')
 ok(r['visual_status']=='not_observed','this batch no visual observation')
 if r['node_type'].startswith('excluded'):
  ok(r['flow_ids']=='','excluded help not mapped into Mini/Max operations')
  ok(r['applicability']=='excluded_model_scope','excluded scope')
for r in fs:ok(r['depth']=='D1' and r['hands_on']=='false','old tasks stay D1')
d=load(W/'qa-dcj2.json')['data']
ok(len(d['question_categories'])==5 and len(d['hot_questions'])==0,'QA shape')
ids={str(q['id']) for c in d['question_categories'] for q in c['questions']}
ok({r['inventory_id'].split('Q')[-1] for r in inv if r['node_type']=='excluded_question'}==ids,'all 23 Q indexed')
ok(all(not any(len(a)==2 and a[0]=='4' for a in q['answer']) for c in d['question_categories'] for q in c['questions']),'QA text only')
case=L/'cases/M-022-baicizhan-device.md'
ok(case.read_bytes()==(W/'case-after.md').read_bytes(),'case after snapshot')
for path in re.findall(r'\]\(([^)]+)\)',case.read_text()):
 if path.startswith('https://'):continue
 ok((case.parent/path).resolve().is_file(),'case local link')
ok('待root' not in case.read_text(),'neutral case status')
for p in list(W.glob('*.json'))+list(W.glob('*.py'))+list((L/'batches').glob(P+'-*')):
 ok(b'\r' not in p.read_bytes(),'LF '+p.name)
 if p.suffix=='.csv':ok(b'\\r\\n' not in p.read_bytes(),'no literal CRLF')
manifest=load(W/'manifest.json')
for r in manifest:ok(sha(R/r['path'])==r['sha256'],'manifest '+r['path'])
print(json.dumps(dict(result='PASS',checks=count,new_evidence=1,new_flows=0,old_flow_fields=len(fc),old_evidence_captures=len(ec),inventory_rows=45,manifest_files=len(manifest),state_changed=False),ensure_ascii=False))
