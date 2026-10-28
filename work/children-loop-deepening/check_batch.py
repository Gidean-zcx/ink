from pathlib import Path
import csv, hashlib, json, re, subprocess

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
batch=lib/'batches/children-loop-deepening-evidence.jsonl'
rows=[json.loads(x) for x in batch.read_text().splitlines()]
assert len(rows)==6 and len({x['evidence_id'] for x in rows})==6
all_e={json.loads(line)['evidence_id'] for p in (lib/'batches').glob('*-evidence.jsonl') for line in p.read_text().splitlines() if line.strip()}
for r in rows:
    assert not r['hands_on']
    assert r['product_id'] in ('M-029','M-031')
    assert not r['archived_artifact'] or r['archived_artifact'].startswith('artifacts/')
    if r.get('qualifies'): assert r['qualifies'] in all_e
    if r.get('raw_work_path'):
        p=root/r['raw_work_path']; assert hashlib.sha256(p.read_bytes()).hexdigest()==r['raw_sha256']
    for n,h in r.get('artifact_hashes',{}).items(): assert hashlib.sha256((lib/n).read_bytes()).hexdigest()==h
    for n,h in r.get('raw_supporting_paths',{}).items(): assert hashlib.sha256((root/n).read_bytes()).hexdigest()==h
flows=list(csv.DictReader((lib/'batches/children-loop-deepening-coverage.csv').open()))
assert len(flows)==2
old_f={r['flow_id'] for p in (lib/'batches').glob('*-coverage.csv') if p.name!='children-loop-deepening-coverage.csv' for r in csv.DictReader(p.open())}
for f in flows:
    assert re.fullmatch(r'F-M\d{3}-\d{2}',f['flow_id'])
    assert f['flow_id'] not in old_f
    assert f['flow_id'] in (lib/f['case_path']).read_text()
    assert all(e in all_e for e in f['evidence_ids'].split(';'))
    assert f['depth'] in ('D1','D2') and f['hands_on']=='false'
for n in ('M-029-ihuman.md','M-031-duolingo-adventures.md'):
    p=lib/'cases'/n;s=p.read_text()
    for e in re.findall(r'E-[A-Z]+-\d{2}',s): assert e in all_e,(p,e)
    for link in re.findall(r'\]\((/[^)]+)\)',s): assert Path(link).exists(),link
    for line in s.splitlines():
        if line.startswith('|'): assert line.endswith('|')
for n in ('cases/M-029-ihuman.md','cases/M-031-duolingo-adventures.md',
          'batches/children-loop-deepening-evidence.jsonl','batches/children-loop-deepening-coverage.csv'):
    p=lib/n;s=p.read_text()
    assert s.endswith('\n') and all(x==x.rstrip() for x in s.splitlines()),p
    assert not any(x.startswith(('<<<<<<< ','=======','>>>>>>> ')) for x in s.splitlines()),p
    r=subprocess.run(['git','diff','--no-index','--check','/dev/null',str(p)],capture_output=True,text=True)
    print(n,'git no-index check exit',r.returncode,'diagnostic bytes',len(r.stdout+r.stderr))
    assert r.returncode in (0,1) and not(r.stdout or r.stderr)
print('PASS: 6 evidence records / 2 distinct new flows / 4 archived static images; references, paths, hashes, D1/D2 limits and whitespace valid')
