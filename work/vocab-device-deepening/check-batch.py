from pathlib import Path
import csv,hashlib,json,re

root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';work=root/'work/vocab-device-deepening';batch=lib/'batches'
ev=[json.loads(s) for s in (batch/'vocab-device-deepening-evidence.jsonl').read_text().splitlines()]
flows=list(csv.DictReader((batch/'vocab-device-deepening-coverage.csv').open()))
updates=list(csv.DictReader((work/'existing-coverage-updates.csv').open()))
allids=set()
for p in batch.glob('*-evidence.jsonl'):
 for s in p.read_text().splitlines():
  if s.strip():allids.add(json.loads(s)['evidence_id'])
required={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
assert len(ev)==6 and len({r['evidence_id'] for r in ev})==6
assert len(flows)==1 and {r['flow_id'] for r in flows}=={'F-M023-06'}
assert len(updates)==6
for r in ev:
 assert required <= r.keys()
 assert r['product_id'] in {'M-022','M-023'} and r['hands_on'] is False and r['visual_observed'] is False
 assert r['url'].startswith('https://') and r['checked_on']=='2026-10-26'
for r in flows+updates:
 assert all(e in allids for e in r['evidence_ids'].split(';'))
 assert r['flow_id'] in (lib/r['case_path']).read_text()
 assert r['hands_on']=='false' and r['depth'] in {'D1','D2'} and r['visual_status']=='not_observed'
old={r['flow_id'] for r in csv.DictReader((batch/'vocab-coverage.csv').open())}
assert not old.intersection(r['flow_id'] for r in flows)
for p,digest in json.loads((work/'baseline.json').read_text()).items():
 if '/batches/' in p:assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
for name in ['M-022-baicizhan-device.md','M-023-maimemo-device.md']:
 p=lib/'cases'/name;t=p.read_text()
 ids=re.findall(r'^\| (F-M\d+-\d+) ',t,re.M);assert len(ids)==len(set(ids))
 for eid in re.findall(r'E-(?:BCDVC|MMDVC)-\d+',t):assert eid in allids,eid
 for link in re.findall(r'\]\(([^)]+)\)',t):
  if link.startswith('https://'):continue
  q=Path(link) if link.startswith('/') else p.parent/link
  assert q.exists(),q
 assert not any(s.rstrip()!=s for s in t.splitlines()),p
result=dict(status='passed',new_evidence=6,new_flows=1,case_flows={'M-022':3,'M-023':6},old_rows_for_root=6,new_visuals=0,old_vocab_batches_unchanged=True,hands_on=False,master_written=False,product_code_changed=False)
(work/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
