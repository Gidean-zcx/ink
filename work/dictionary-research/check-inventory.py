from pathlib import Path
import csv, hashlib, json, re

r=Path('docs/research/interaction-library');b=r/'batches'
p=b/'dictionary-inventory-evidence.jsonl'
new=[json.loads(x) for x in p.read_text().splitlines()]
base=set(json.loads((b/'dictionary-evidence.jsonl').read_text().splitlines()[0]))
assert all(set(x)==base for x in new)
newids={x['evidence_id'] for x in new};assert len(newids)==7
others=[json.loads(x) for f in b.glob('*evidence.jsonl') if f!=p for x in f.read_text().splitlines() if x.strip()]
assert not newids&{x['evidence_id'] for x in others}
ids=newids|{x['evidence_id'] for x in others}
fids={x['flow_id'] for f in b.glob('*coverage.csv') for x in csv.DictReader(f.open())}
baseline=json.loads(Path('work/dictionary-research/inventory-baseline.json').read_text())
for name,prior in baseline['flow_ids'].items():
    f=r/'cases'/name;s=f.read_text()
    assert sorted(set(re.findall(r'F-M\d+-\d+',s)))==prior
    assert set(re.findall(r'F-M\d+-\d+',s))<=fids
    assert set(re.findall(r'E-(?:CASIO|YDP|EUD)-\d+',s))<=ids
    assert '功能目录与覆盖' in s and s.endswith('\n')
    assert not any(line.rstrip()!=line for line in s.splitlines())
    for href in re.findall(r'\]\(([^)]+)\)',s):
        assert href.startswith('https://') or (href.startswith('/home/workspace/reader/') and Path(href).is_file()),href
    print(name,'flows unchanged',len(prior),'changed',hashlib.sha256(s.encode()).hexdigest()!=baseline['sha256'][name])
assert all(x['checked_on']=='2026-10-26' and x['hands_on'] is False and x['visual_observed'] is False for x in new)
print('PASS 3 cases; 7 evidence; no new F; schema, crossrefs, local links, whitespace checked')
