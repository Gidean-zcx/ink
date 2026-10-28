"""Read-only pre-merge verification; never run after the parent merges this batch."""
import csv, hashlib, json, re
from pathlib import Path
from collections import Counter
ROOT=Path('/home/workspace/reader');W=ROOT/'work/mudita-app-controls-deepening';L=ROOT/'docs/research/interaction-library'
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines() if x.strip()]
F=list(csv.DictReader((L/'coverage.csv').open())); em={e['evidence_id']:e for e in E};fm={f['flow_id']:f for f in F}
newE=[json.loads(x) for x in (L/'batches/mudita-app-controls-deepening-evidence.jsonl').read_text().splitlines()]
cp=L/'batches/mudita-app-controls-deepening-coverage.csv';ip=L/'batches/mudita-app-controls-deepening-inventory.csv'
newF=list(csv.DictReader(cp.open()));inv=list(csv.DictReader(ip.open()));oldinv=list(csv.DictReader((L/'artifacts/mudita-controls-deepening/function-inventory.csv').open()))
assert len(newE)==18 and len(newF)==6 and len(inv)==111
assert [x['evidence_id'] for x in newE]==[f'E-MACD-{i:02d}' for i in range(1,19)]
assert [x['flow_id'] for x in newF]==[f'F-M012-{i:02d}' for i in range(11,17)]
assert not set(x['evidence_id'] for x in newE)&em.keys(), 'not a pre-merge state'
assert not set(x['flow_id'] for x in newF)&fm.keys(), 'not a pre-merge state'
for e in newE:
 assert e['supersedes'] is None and e['hands_on'] is False and e['visual_observed'] is False
 assert e['firmware_version'] is None and e['app_build'] is None
 assert e['source_updated_on']=='2026-10-03'
 assert not any(x.get('url')==e['url'] for x in E), 'old URL must reuse old E'
 assert e['url'].startswith('https://support.mudita.com/en/support/solutions/articles/')
 assert e['archived_artifact'].startswith('artifacts/') and not Path(e['archived_artifact']).is_absolute()
 assert sha(L/e['archived_artifact'])==e['sha256']
 assert sha(ROOT/e['raw_work_path'])==e['raw_sha256']
 assert sha(ROOT/e['raw_text_path'])==e['raw_text_sha256']
for p in L.rglob('*.jsonl'):
 if p.name=='mudita-app-controls-deepening-evidence.jsonl':continue
 assert not re.search(r'E-MACD-\d+',p.read_text()), f'new E prefix collision {p}'
for p in [cp,ip]:
 b=p.read_bytes();assert b'\r' not in b and b'\\r\\n' not in b
 assert b.endswith(b'\n')
for f in newF:
 assert f['depth']=='D2' and f['visual_status']=='not_observed' and f['hands_on']=='false'
 assert set(f)==set(F[0])
allids=em.keys()|{e['evidence_id'] for e in newE}
allflows=fm.keys()|{f['flow_id'] for f in newF}
for r in newF+inv:
 assert all(x in allids for x in r['evidence_ids'].split(';') if x)
 if 'flow_ids' in r:assert all(x in allflows for x in r['flow_ids'].split(';') if x)
updates=j(W/'old-coverage-updates.json');assert len(updates)==6
for u in updates:
 assert fm[u['flow_id']]==u['before'], f"old F changed: {u['flow_id']}"
 assert u['changed_fields']=={k:{'before':u['before'][k],'after':v} for k,v in u['after'].items() if v!=u['before'][k]}
 assert set(u['changed_fields'])=={'evidence_ids','known_branches','gaps'}
 assert all(x in allids for x in u['after']['evidence_ids'].split(';'))
extra=j(W/'old-evidence-additional-captures.json');assert len(extra)==2
for u in extra:
 e=em[u['evidence_id']];assert e['url']==u['expected_url']
 assert e.get('additional_captures',[])==u['before_additional_captures']
 for c in u['append_additional_captures']:
  assert sha(ROOT/c['raw_work_path'])==c['raw_sha256']
  assert sha(L/c['archived_artifact'])==c['sha256']
  assert c['hands_on'] is False and c['visual_observed'] is False
assert j(W/'old-evidence-field-updates.json')==[]
new_read={c['inventory_id'] for c in j(W/'captures.json')}
for a,b in zip(oldinv,inv):
 assert all(a[k]==b[k] for k in ['inventory_id','scope','domain','title','url'])
 assert b['reading_status']==('body_read' if b['inventory_id'] in new_read else a['reading_status'])
assert Counter(x['reading_status'] for x in inv)=={'text_read':30,'body_read':34,'index_only':46,'prior_body_read':1}
baseline=j(W/'baseline.json');assert sha(W/'case-before.md')==baseline['case_sha256'];assert baseline['reverse_refs']==[]
case=L/'cases/M-012-mudita.md';s=case.read_text()
assert all(f'F-M012-{n:02d}' in s for n in range(1,17))
for file in [case,L/'artifacts/mudita-app-controls-deepening/source-notes.md']:
 for url in re.findall(r'\]\(([^)]+)\)',file.read_text()):
  if url.startswith(('https://','http://','#')):continue
  assert (file.parent/url.split('#')[0]).exists(),(file,url)
 for line in file.read_text().splitlines():assert line.rstrip()==line,'trailing whitespace'
for name in ['formal-manifest.json','raw-manifest.json']:
 for r in j(W/name):assert sha(ROOT/r['path'])==r['sha256'],r['path']
result={'status':'PASS','mode':'pre_merge_read_only','new_e':18,'new_f':6,'old_f':6,'old_f_fields':18,'old_e_local_rereads':2,'inventory_rows':111,'new_body_read_rows':18,'remaining_index_only':46,'new_static_observations':0,'formal_files':len(j(W/'formal-manifest.json')),'raw_files':len(j(W/'raw-manifest.json')),'case_sha256':sha(case),'formal_manifest_sha256':sha(W/'formal-manifest.json'),'state_changed_by_check':False}
print(json.dumps(result,ensure_ascii=False,indent=2))
