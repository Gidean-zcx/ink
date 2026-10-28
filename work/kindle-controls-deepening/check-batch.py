import csv,hashlib,json,re
from pathlib import Path
from urllib.parse import urlparse
ROOT=Path(__file__).resolve().parents[2];WORK=ROOT/'work/kindle-controls-deepening';LIB=ROOT/'docs/research/interaction-library'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((WORK/'baseline.json').read_text())
for p,d in baseline.items():
 if '/cases/' not in p:assert sha(ROOT/p)==d,('old batch changed',p)
ep=LIB/'batches/kindle-controls-deepening-evidence.jsonl';cp=LIB/'batches/kindle-controls-deepening-coverage.csv';ip=LIB/'batches/kindle-controls-deepening-inventory.csv';casep=LIB/'cases/M-001-kindle.md';up=WORK/'existing-coverage-updates.csv'
ev=[json.loads(x) for x in ep.read_text().splitlines()];cv=list(csv.DictReader(cp.open()));iv=list(csv.DictReader(ip.open()));case=casep.read_text();changes=list(csv.DictReader(up.open()))
assert len(ev)==14 and len(cv)==1 and len(iv)==58 and len(changes)==34
newids={e['evidence_id'] for e in ev};assert len(newids)==14
for p in (LIB/'batches').glob('*evidence.jsonl'):
 if p==ep:continue
 for line in p.read_text().splitlines():
  if line.strip():assert json.loads(line)['evidence_id'] not in newids,('collision',p)
allids=set(newids)
for line in (LIB/'batches/reader-evidence.jsonl').read_text().splitlines():allids.add(json.loads(line)['evidence_id'])
for e in ev:
 assert e['product_id']=='M-001' and e['checked_on']=='2026-10-27' and not e['hands_on'] and not e['visual_observed']
 assert e['archived_artifact'] is None and e['sha256'] is None and e['http_status']==200
 assert sha(ROOT/e['raw_work_path'])==e['raw_sha256']
assert cv[0]['flow_id']=='F-M001-09' and cv[0]['depth']=='D2' and cv[0]['visual_status']=='not_observed' and cv[0]['hands_on']=='false'
assert set(cv[0]['evidence_ids'].split(';'))<=allids
assert set(re.findall(r'^\| (F-M001-\d+) ',case,re.M))=={'F-M001-'+str(n).zfill(2) for n in range(1,10)}
assert set(re.findall(r'E-(?:KDL|KDCD)-\d+',case))<=allids
for link in re.findall(r'\]\(([^)]+)\)',case):
 if not link.startswith(('https://','http://')):assert (casep.parent/link).resolve().exists(),link
manual=(WORK/'02-scribe-2024.html').read_text()
for r in iv:
 fragment=urlparse(r['source_url']).fragment
 assert re.search(r'(?:id|name)=[\"\']'+re.escape(fragment)+r'[\"\']',manual),fragment
 if r['flow_ids']:assert all(f in case for f in r['flow_ids'].split(';'))
assert sum(r['entry_kind']=='parent_group' for r in iv)==9
assert sum(r['entry_kind']=='child_topic' for r in iv)==47
assert sum(r['entry_kind']=='body_subtopic' for r in iv)==2
old={r['flow_id']:r for r in csv.DictReader((LIB/'batches/reader-coverage.csv').open())}
for r in changes:assert old[r['flow_id']][r['field']]==r['before'] and r['before']!=r['after']
assert len({r['flow_id'] for r in changes})==8
assert '2025+' in next(r['after'] for r in changes if r['flow_id']=='F-M001-07' and r['field']=='task')
for p in [ep,cp,ip,casep,up]:
 b=p.read_bytes();assert b'\r' not in b and b'\\r' not in b and b'\\n' not in b and b.endswith(b'\n'),p
fetch=json.loads((WORK/'fetch-results.json').read_text());assert len(fetch)==14 and all(r['status']==200 for r in fetch)
assert 'Only for Kindle Scribe models released in 2025 or later' in (WORK/'10-share.txt').read_text()
assert 'Welcome to your new Kindle Scribe (1st generation, 2024 release)' in (WORK/'02-scribe-2024.txt').read_text()
index=(WORK/'01-guide-index.html').read_text()
assert 'Kindle Scribe - 2024 Release' in index and 'kindle_scribe_2024/v1/en-US/html/kug.html' in index
result={'status':'PASS','new_evidence':14,'new_flows':1,'case_flows':9,'case_evidence':23,'inventory':{'parent_groups':9,'child_topics':47,'body_subtopics':2},'old_flow_changes':8,'old_field_changes':34,'old_batches_unchanged':True,'http_200':14,'hands_on':False,'visual_observed':False,'files':{str(p.relative_to(ROOT)):sha(p) for p in [casep,ep,cp,ip,up]}}
(WORK/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
