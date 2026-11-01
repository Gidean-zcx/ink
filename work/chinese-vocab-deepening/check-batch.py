from pathlib import Path
import csv,hashlib,json,re

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
work=root/'work/chinese-vocab-deepening'
base=json.loads((work/'baseline.json').read_text())
batch=lib/'batches'
ep=batch/'chinese-vocab-deepening-evidence.jsonl'
cp=batch/'chinese-vocab-deepening-coverage.csv'
ev=[json.loads(s) for s in ep.read_text().splitlines()]
rows=list(csv.DictReader(cp.open()))
updates=list(csv.DictReader((work/'existing-coverage-updates.csv').open()))
allids=set()
for p in batch.glob('*-evidence.jsonl'):
 for line in p.read_text().splitlines():
  if line.strip():allids.add(json.loads(line)['evidence_id'])
required={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
assert len(ev)==14 and len({r['evidence_id'] for r in ev})==14
assert len(rows)==2 and {r['flow_id'] for r in rows}=={'F-M016-05','F-M016-06'}
assert len(updates)==5
for r in ev:
 assert required <= r.keys()
 assert r['product_id'] in {'M-016','M-018'} and r['hands_on'] is False
 assert r['checked_on']=='2026-10-26' and r['url'].startswith('https://')
 if r['archived_artifact']:
  p=lib/r['archived_artifact'];assert p.is_file()
  assert hashlib.sha256(p.read_bytes()).hexdigest()==r['sha256']
  assert r['visual_observed'] and r['capture_state']=='selected_static_image'
 if r.get('visual_audit_path'):
  assert hashlib.sha256((lib/r['visual_audit_path']).read_bytes()).hexdigest()==r['visual_audit_sha256']
for r in rows+updates:
 assert all(e in allids for e in r['evidence_ids'].split(';'))
 assert r['hands_on']=='false' and r['depth'] in {'D1','D2'}
 assert r['visual_status'] in {'not_observed','static_reference_only'}
 assert r['flow_id'] in (lib/r['case_path']).read_text()
assert len([r for r in ev if r['visual_observed']])==5
for name,digest in base.items():
 if '/batches/' in name:assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest, name+' changed'
cases=[lib/'cases/M-016-baicizhan.md',lib/'cases/M-018-bbdc.md']
oldids=set()
for r in csv.DictReader((batch/'vocab-coverage.csv').open()):oldids.add(r['flow_id'])
assert not oldids.intersection(r['flow_id'] for r in rows)
for p in cases:
 text=p.read_text()
 for eid in re.findall(r'E-(?:BCD|BBD)-\d+',text):assert eid in allids,eid
 flow_defs=re.findall(r'^\| (F-M\d+-\d+) ',text,re.M)
 assert len(flow_defs)==len(set(flow_defs))
 for target in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',text):assert Path(target).exists(),target
for p in cases+[ep,cp,lib/'artifacts/chinese-vocab-deepening/static-observation-log.md']:
 assert not any(s.rstrip()!=s for s in p.read_text().splitlines()),p
result=dict(status='passed',new_evidence=14,new_flows=2,case_flows={'M-016':6,'M-018':5},static_images=5,existing_coverage_updates_for_root=5,old_vocab_batches_unchanged=True,hands_on=False,product_code_changed=False,master_written=False)
(work/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False))
