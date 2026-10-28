from pathlib import Path
import json,csv,re,collections
r=Path('docs/research/interaction-library');b=r/'batches'
e=[json.loads(x) for x in (b/'chinese-apps-evidence.jsonl').read_text().splitlines()]
c=list(csv.DictReader((b/'chinese-apps-coverage.csv').open()))
base=set(json.loads((b/'dictionary-evidence.jsonl').read_text().splitlines()[0]))
assert all(set(x)==base for x in e)
ids={x['evidence_id'] for x in e};assert len(ids)==len(e)
fids={x['flow_id'] for x in c};assert len(fids)==len(c)
assert all(x['checked_on']=='2026-10-26' and x['hands_on'] is False and x['visual_observed'] is False for x in e)
for x in c:
 assert x['depth'] in ['D1','D2'] and x['hands_on']=='false'
 assert set(x['evidence_ids'].split(';'))<=ids
 p=r/x['case_path'];assert p.exists()
 assert x['flow_id'] in p.read_text()
for p in {r/x['case_path'] for x in c}:
 text=p.read_text()
 assert set(re.findall(r'E-(?:WRD|IRD|FENB|JKB)-\d+',text))<=ids
 assert set(re.findall(r'F-M\d+-\d+',text))<=fids
 for href in re.findall(r'\]\(([^)]+)\)',text):
  assert href.startswith('https://'),(p,href)
 print(p.name,len(text),'chars')
print('PASS evidence=',len(e),'flows=',len(c),'depths=',dict(collections.Counter(x['depth'] for x in c)))
