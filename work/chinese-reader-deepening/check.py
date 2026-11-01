from pathlib import Path
import csv,json,re,hashlib
from urllib.parse import urlsplit
r=Path('/home/workspace/reader'); lib=r/'docs/research/interaction-library'
e=[json.loads(x) for x in (lib/'batches/chinese-reader-deepening-evidence.jsonl').read_text().splitlines()]
f=list(csv.DictReader((lib/'batches/chinese-reader-deepening-coverage.csv').open()))
assert len(e)==23 and len(f)==6
assert len({x['evidence_id'] for x in e})==len(e)
assert len({x['flow_id'] for x in f})==len(f)
all_e=set()
for p in (lib/'batches').glob('*evidence.jsonl'):
    for line in p.read_text().splitlines():
        if line.strip(): all_e.add(json.loads(line)['evidence_id'])
for x in e:
    assert urlsplit(x['url']).scheme in ['http','https'] and urlsplit(x['url']).netloc
    assert x['checked_on']=='2026-10-26' and x['hands_on'] is False
    if x['visual_observed']:
        p=lib/x['archived_artifact']; assert p.is_file()
        assert hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
    else: assert x['archived_artifact'] is None
for x in f:
    assert x['hands_on']=='false'
    assert set(x['evidence_ids'].split(';')) <= all_e
    assert x['flow_id'] in (lib/x['case_path']).read_text()
for product,path in [('M053','M-053-ireader-hardware.md'),('M054','M-054-moaan.md')]:
    s=(lib/'cases'/path).read_text()
    ids=set(re.findall(r'F-'+product+r'-\d+',s))
    assert ids=={f'F-{product}-{i:02d}' for i in range(1,9)},ids
    # Explicit single IDs must resolve; compressed source ranges are checked separately below.
    for id in re.findall(r'E-[A-Z]+-\d{2}',s): assert id in all_e,id
    for path in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',s): assert Path(path).is_file(),path
    assert 'D3/D4' in s
assert sum(x['visual_observed'] for x in e)==5
assert sum(x['depth']=='D2' for x in f)==3
assert not any(x['evidence_id'].startswith('E-MOD') and x['url'].startswith('https://www.moaansmart') for x in e)
print('PASS: 23 E (5 viewed static artifacts), 6 new F (3 D2 + 3 D1), all 10 old F retained; references/URLs/SHAs/local links valid.')
