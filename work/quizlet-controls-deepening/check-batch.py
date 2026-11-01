from pathlib import Path
import csv,json,re,hashlib
b=Path('work/quizlet-controls-deepening');lib=Path('docs/research/interaction-library');bn='quizlet-controls-deepening';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
case=lib/'cases/M-021-quizlet.md';ep=lib/'batches'/f'{bn}-evidence.jsonl';cp=lib/'batches'/f'{bn}-coverage.csv';ip=lib/'batches'/f'{bn}-inventory.csv';up=b/'existing-coverage-updates.csv'
e=[json.loads(x) for x in ep.read_text().splitlines() if x];f=list(csv.DictReader(cp.open()));inv=list(csv.DictReader(ip.open()));u=list(csv.DictReader(up.open()))
assert [x['evidence_id'] for x in e]==[f'E-QZCD-{i:02}' for i in range(1,18)]
assert [x['flow_id'] for x in f]==[f'F-M021-{i:02}' for i in range(11,14)]
assert all(x['depth']=='D2' and x['hands_on']=='false' and x['visual_status']=='not_observed' for x in f)
assert len(inv)==68 and sum(x['entry_kind']=='parent_group' for x in inv)==4
assert sum(bool(x['flow_ids']) for x in inv)==24
assert len(u)==16 and {x['flow_id'] for x in u}=={'F-M021-02','F-M021-04','F-M021-07','F-M021-08'}
old={x['flow_id']:x for x in json.loads((b/'existing-coverage-before.json').read_text())}
for x in u:assert x['before']==old[x['flow_id']][x['field']]
assert re.findall(r'^\| (F-M021-\d+)',case.read_text(),re.M)==[f'F-M021-{i:02}' for i in range(1,14)]
all_e=set();new_count={x['evidence_id']:0 for x in e}
for p in (lib/'batches').glob('*-evidence.jsonl'):
 for line in p.read_text().splitlines():
  if not line.strip():continue
  eid=json.loads(line)['evidence_id'];all_e.add(eid)
  if eid in new_count:new_count[eid]+=1
assert all(n==1 for n in new_count.values()),new_count
for row in f+inv:
 assert all(x in all_e for x in row['evidence_ids'].split(';') if x)
for row in u:
 if row['field']=='evidence_ids':assert all(x in all_e for x in row['after'].split(';'))
for x in e:
 assert not x['hands_on'] and not x['visual_observed'] and x['archived_artifact'] is None
 assert sha(Path(x['raw_work_path']))==x['raw_sha256'];assert Path(x['parsed_text_path']).exists()
 assert x['url'] in Path(x['raw_work_path']).read_text(),x['url']
 assert 'no_source_HTML' in x['capture_state']
assert json.loads((b/'fetch-probe.json').read_text())['status']==403
assert not list(b.glob('*.html'))
for label,target in re.findall(r'\[([^\]]+)\]\(([^)]+)\)',case.read_text()):
 if target.startswith(('https://','http://')):continue
 p=Path(target) if target.startswith('/') else case.parent/target
 assert p.exists(),(label,target)
files=[case,ep,cp,ip,up,b/'research-log.md']
for p in files:
 raw=p.read_bytes();raw.decode('utf-8');assert raw.endswith(b'\n') and b'\r' not in raw and b'\\r\\n' not in raw
 assert all(not ln.endswith((' ','\t')) for ln in raw.decode().splitlines()),p
base=json.loads((b/'baseline.json').read_text());protected={p:sha(Path(p))==h for p,h in base.items() if p!=str(case)};assert all(protected.values())
# Verify exact source phrases used for the highest-impact distinctions, without executing product UI.
checks={'articles-360030642972':"your progress isn't saved",'articles-360030171011':"can't be recovered",'articles-360048806511':'remove your Learn mode progress','articles-360030990531':'each question correctly twice','articles-360030645752':'spell each word correctly twice','articles-360048803491':'Progress is not tracked on game modes'}
for key,needle in checks.items():assert needle in (b/'parsed'/f'{key}.txt').read_text(),(key,needle)
r={'status':'PASS','new_E':17,'new_F':3,'case_total_F':13,'case_total_E':30,'depth':'D2 document reconstruction only','inventory_parents':4,'inventory_articles':64,'inventory_partial_topics':24,'inventory_directory_only_topics':40,'old_fields':16,'old_batches_unchanged':protected,'raw_capture':'5 web-tool response text files; no source HTML; direct request 403 stopped','raw_hashes':{str(p):sha(p) for p in sorted(b.glob('quizlet-response-*.txt'))},'output_hashes':{str(p):sha(p) for p in files},'checks':'JSONL/CSV; E collision; old before values; stable F IDs; evidence references; UTF-8/LF; local links; source phrase checks; hashes; no visual/hands-on claims'}
(b/'validation.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False,indent=2))
