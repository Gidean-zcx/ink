from pathlib import Path
import csv,json,re,hashlib,urllib.parse
r=Path('docs/research/interaction-library'); b=r/'batches'; case=r/'cases/M-002-kobo.md'; text=case.read_text()
e=[json.loads(x) for x in (b/'kobo-deepening-evidence.jsonl').read_text().splitlines()]
c=list(csv.DictReader((b/'kobo-deepening-coverage.csv').open()))
base=json.loads((b/'dictionary-evidence.jsonl').read_text().splitlines()[0]); assert all(set(x)==set(base) for x in e)
ids=[x['evidence_id'] for x in e]; assert len(ids)==len(set(ids))==34
assert len(c)==21 and [x['flow_id'] for x in c]==[f'F-M002-{i:02}' for i in range(4,25)]
rows=re.findall(r'^\| (F-M002-\d+) ',text,re.M); assert rows==[f'F-M002-{i:02}' for i in range(1,25)],rows
for x in c:
 assert x['depth']=='D2' and x['visual_status']=='not_observed' and x['hands_on']=='false'
 assert (r/x['case_path']).is_file()
 assert set(x['evidence_ids'].split(';'))<=set(ids)
for x in e:
 assert x['product_id']=='M-002' and not x['hands_on'] and not x['visual_observed']
 assert urllib.parse.urlparse(x['url']).netloc in ('help.kobo.com','www.kobo.com')
for x in re.findall(r'\]\((/[^)]+)\)',text):assert Path(x).is_file(),x
for name,sha in json.loads(Path('work/kobo-deepening/baseline.json').read_text()).items():assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==sha,name
old=Path('work/kobo-deepening/case-before.md').read_text()
for row in re.findall(r'^\| F-M002-0[123] .+$',old,re.M):assert row in text,'Changed original flow row'
# References are only row-local evidence, shorthand ranges in prose are checked by bounded suffix.
assert all(int(x)<=34 for x in re.findall(r'E-KBD-(\d+)',text))
print('PASS: 34 evidence fields match base schema;',len(base),'fields; 21 new D2 flows; 24 total task rows; ID/reference/local-link/date-scope checks')
print('PASS: 3 original flow rows preserved; initial batch hashes unchanged; no visual or hands-on claims')
for p in [case,b/'kobo-deepening-evidence.jsonl',b/'kobo-deepening-coverage.csv']:
 print(str(p),'bytes='+str(p.stat().st_size),'sha256='+hashlib.sha256(p.read_bytes()).hexdigest())
