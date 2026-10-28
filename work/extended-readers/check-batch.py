from pathlib import Path
import csv, collections, hashlib, json, re

r=Path('docs/research/interaction-library');b=r/'batches'
ep=b/'extended-readers-evidence.jsonl';cp=b/'extended-readers-coverage.csv'
e=[json.loads(x) for x in ep.read_text().splitlines()]
c=list(csv.DictReader(cp.open()))
base=set(json.loads((b/'dictionary-evidence.jsonl').read_text().splitlines()[0]))
assert all(set(x)==base for x in e)
expected={'flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on'}
assert all(set(x)==expected for x in c)
ids={x['evidence_id'] for x in e};fids={x['flow_id'] for x in c}
assert len(ids)==len(e) and len(fids)==len(c)
assert all(x['checked_on']=='2026-10-26' and x['hands_on'] is False for x in e)
assert all(x['depth'] in ['D1','D2'] and x['hands_on']=='false' for x in c)
for x in e:
    assert x['url'].startswith('https://')
    if x['archived_artifact']:
        p=r/x['archived_artifact']
        assert p.is_file() and x['sha256']==hashlib.sha256(p.read_bytes()).hexdigest()
        assert x['visual_observed'] is True
for x in c:
    assert set(x['evidence_ids'].split(';'))<=ids
    p=r/x['case_path'];assert p.is_file()
    assert x['flow_id'] in p.read_text()
paths=sorted({r/x['case_path'] for x in c})
for p in paths:
    text=p.read_text()
    assert set(re.findall(r'E-(?:IHW|MOA|HIS|MEE|TOL)-\d+',text))<=ids
    assert set(re.findall(r'F-M\d+-\d+',text))<=fids
    for href in re.findall(r'\]\(([^)]+)\)',text):
        assert href.startswith('https://') or (href.startswith('/home/workspace/reader/') and Path(href).is_file()),(p,href)
    print(p.name,len(text),'chars')
for p in paths+[ep,cp]:
    data=p.read_text()
    assert data.endswith('\n')
    assert not any(line.rstrip()!=line for line in data.splitlines()),p
print('PASS evidence=',len(e),'flows=',len(c),'depths=',dict(collections.Counter(x['depth'] for x in c)))
print('PASS artifacts=',sum(bool(x['archived_artifact']) for x in e),'hashes checked; 0 hands-on; 0 D3/D4')
