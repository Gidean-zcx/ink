import csv,hashlib,json,re
from pathlib import Path
from urllib.parse import urlparse
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/notebooklm-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((WORK/'baseline.json').read_text())
for p,sha in baseline.items():
 if '/cases/' not in p:assert digest(ROOT/p)==sha,('old batch changed',p)
ep=LIB/'batches/notebooklm-controls-deepening-evidence.jsonl'
cp=LIB/'batches/notebooklm-controls-deepening-coverage.csv'
ip=LIB/'batches/notebooklm-controls-deepening-inventory.csv'
casep=LIB/'cases/M-041-notebooklm.md'
case=casep.read_text()
ev=[json.loads(x) for x in ep.read_text().splitlines()]
cv=list(csv.DictReader(cp.open()))
iv=list(csv.DictReader(ip.open()))
assert len(ev)==13 and len(cv)==6 and len(iv)==19
newids={x['evidence_id'] for x in ev};assert len(newids)==13
for p in (LIB/'batches').glob('*evidence.jsonl'):
 if p==ep:continue
 for line in p.read_text().splitlines():
  if line.strip():assert json.loads(line)['evidence_id'] not in newids,('collision',p)
allids=set(newids)
for line in (LIB/'batches/ai-evidence.jsonl').read_text().splitlines():allids.add(json.loads(line)['evidence_id'])
for e in ev:
 assert e['product_id']=='M-041' and not e['hands_on'] and not e['visual_observed']
 assert e['archived_artifact'] is None and e['sha256'] is None
 assert digest(ROOT/e['raw_work_path'])==e['raw_sha256']
 assert e['http_status']==200 and urlparse(e['url']).scheme=='https'
assert [r['flow_id'] for r in cv]==['F-M041-'+str(n).zfill(2) for n in range(8,14)]
for row in cv:
 assert row['depth']=='D2' and row['visual_status']=='not_observed' and row['hands_on']=='false'
 assert row['flow_id'] in case and (LIB/row['case_path']).exists()
 assert set(row['evidence_ids'].split(';'))<=allids
assert set(re.findall(r'^\| (F-M041-\d+) ',case,re.M))=={'F-M041-'+str(n).zfill(2) for n in range(1,14)}
assert set(re.findall(r'E-(?:NBL|NBCD)-\d+',case))<=allids
for link in re.findall(r'\]\(([^)]+)\)',case):
 if not link.startswith(('https://','http://')):assert (casep.parent/link).resolve().exists(),link
old={r['flow_id']:r for r in csv.DictReader((LIB/'batches/ai-coverage.csv').open())}
up=WORK/'existing-coverage-updates.csv';changes=list(csv.DictReader(up.open()))
assert len(changes)==18 and len({r['flow_id'] for r in changes})==6
for r in changes:assert old[r['flow_id']][r['field']]==r['before'] and r['before']!=r['after']
for p in [ep,cp,ip,casep,up]:
 data=p.read_bytes();assert b'\r' not in data and b'\\r' not in data and b'\\n' not in data,p
 assert data.endswith(b'\n')
fetch=json.loads((WORK/'fetch-results.json').read_text())
assert len(fetch)==14 and [r['status'] for r in fetch].count(200)==13
assert fetch[-1]['status']==429 and fetch[-1]['stopped']
for p in [WORK/'probe-create.html',WORK/'04-create.html']:
 a=re.search(r'<article\b.*?</article>',p.read_text(),re.S).group()
 assert 'id="nb_copy"' in a and 'All Studio content &amp; sources are copied.' in a
 assert 'Chat history &amp; notes are not copied.' in a
result={'status':'PASS','new_evidence':13,'new_flows':6,'case_flows':13,'case_evidence':20,'inventory_rows':19,'old_flow_changes':6,'old_field_changes':18,'old_batches_unchanged':True,'visual_observed':False,'hands_on':False,'fetch_statuses':{'200':13,'429_stopped':1},'files':{str(p.relative_to(ROOT)):digest(p) for p in [casep,ep,cp,ip,up]}}
(WORK/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
