from pathlib import Path
from urllib.parse import urlparse
import csv,json,re,hashlib
R=Path('/home/workspace/reader');B=R/'docs/research/interaction-library';W=R/'work/boox-controls-deepening'
case=B/'cases/M-004-boox-neoreader.md';text=case.read_text()
ev=[json.loads(x) for x in (B/'batches/boox-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(ev)==14 and len({x['evidence_id'] for x in ev})==14
ids={f'E-BXCD-{n:02}' for n in range(1,15)};assert {x['evidence_id'] for x in ev}==ids
counts={k:0 for k in ids}
for p in (B/'batches').glob('*evidence.jsonl'):
 for line in p.read_text().splitlines():
  k=json.loads(line)['evidence_id']
  if k in counts:counts[k]+=1
assert set(counts.values())=={1},counts
for e in ev:
 assert not Path(e['archived_artifact']).is_absolute() and not e['archived_artifact'].startswith('work/')
 assert (B/e['archived_artifact']).is_file()
 assert e['sha256']==hashlib.sha256((B/e['archived_artifact']).read_bytes()).hexdigest()
 assert e['raw_work_path'].startswith('work/') and (R/e['raw_work_path']).is_file()
 assert e['raw_sha256']==hashlib.sha256((R/e['raw_work_path']).read_bytes()).hexdigest()
 assert e['hands_on'] is False and e['product_id']=='M-004'
 assert urlparse(e['url']).scheme in ('https','http') and urlparse(e['url']).netloc
assert sum(e['visual_observed'] for e in ev)==4
reader=csv.DictReader((B/'batches/boox-controls-deepening-coverage.csv').open());assert len(reader.fieldnames)==11 and list(reader)==[]
assert set(re.findall(r'^\| (F-M004-\d+) \|',text,re.M))=={f'F-M004-{n:02}' for n in range(1,15)}
updates=json.loads((W/'old-coverage-updates.json').read_text());assert len(updates)==7
assert {u['flow_id'] for u in updates}=={'F-M004-02','F-M004-03','F-M004-06','F-M004-07','F-M004-08','F-M004-09','F-M004-14'}
known={json.loads(x)['evidence_id'] for x in (B/'evidence.jsonl').read_text().splitlines()}|ids
for u in updates:
 assert u['replacement_row']['hands_on']=='false' and u['replacement_row']['depth']=='D2'
 assert set(u['replacement_row']['evidence_ids'].split(';'))<=known
 for k,v in u['changes'].items():assert v['before']!=v['after'] and u['replacement_row'][k]==v['after']
inv=list(csv.DictReader((B/'artifacts/boox-controls-deepening/functional-inventory.csv').open()))
assert len(inv)==59 and len({r['article_id'] for r in inv})==59 and len({r['section'] for r in inv})==14
for row in inv:assert set(row['evidence_ids'].split(';'))<=known
for path in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',text):assert Path(path).is_file(),path
m=json.loads((W/'manifest.json').read_text())
for f in m['raw_files']:assert hashlib.sha256((R/f['path']).read_bytes()).hexdigest()==f['sha256']
for f in m['formal_files']:assert hashlib.sha256((B/f['path']).read_bytes()).hexdigest()==f['sha256']
assert not re.search(r'turn\d+(?:view|search)\d+',text)
report=dict(status='PASS',new_evidence=14,new_flows=0,old_flow_updates=7,directory_articles=59,directory_sections=14,static_images=4,hands_on=False,scope='schema/IDs/local paths/hashes; no product operation or exhaustive remote-link revalidation')
(W/'check-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
