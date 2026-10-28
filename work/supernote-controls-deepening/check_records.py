from pathlib import Path
from urllib.parse import urlparse
import csv,json,re,hashlib

ROOT=Path('/home/workspace/reader');BASE=ROOT/'docs/research/interaction-library';WORK=ROOT/'work/supernote-controls-deepening'
ep=BASE/'batches/supernote-controls-deepening-evidence.jsonl';cp=BASE/'batches/supernote-controls-deepening-coverage.csv';case=BASE/'cases/M-006-supernote.md'
e=[json.loads(x) for x in ep.read_text().splitlines()];c=list(csv.DictReader(cp.open()));s=case.read_text()
required={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
assert len(e)==11 and all(set(r)==required for r in e)
assert len(c)==4 and {r['flow_id'] for r in c}=={f'F-M006-{i:02}' for i in range(7,11)}
all_e={json.loads(x)['evidence_id'] for x in (BASE/'evidence.jsonl').read_text().splitlines()}
all_e.update(r['evidence_id'] for r in e)
batch_counts={r['evidence_id']:0 for r in e}
for p in (BASE/'batches').glob('*evidence.jsonl'):
 for line in p.read_text().splitlines():
  key=json.loads(line)['evidence_id']
  if key in batch_counts:batch_counts[key]+=1
assert all(v==1 for v in batch_counts.values()),batch_counts
for r in e:
 assert r['product_id']=='M-006' and r['hands_on'] is False and r['checked_on']=='2026-10-26'
 assert urlparse(r['url']).scheme in ('http','https') and urlparse(r['url']).netloc
 assert r['sha256']==hashlib.sha256((ROOT/r['archived_artifact']).read_bytes()).hexdigest()
for r in c:
 assert r['depth']=='D2' and r['hands_on']=='false'
 assert set(r['evidence_ids'].split(';'))<=all_e
 assert r['case_path']=='cases/M-006-supernote.md'
assert set(re.findall(r'^### (F-M006-\d+)',s,re.M))=={f'F-M006-{i:02}' for i in range(1,11)}
for path in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',s):assert Path(path).exists(),path
for key in re.findall(r'E-SNCD-\d+',s):assert key in all_e,key
updates=json.loads((WORK/'old-coverage-updates.json').read_text())
assert len(updates)==6 and {r['flow_id'] for r in updates}=={f'F-M006-{i:02}' for i in range(1,7)}
for u in updates:
 assert u['changes'] and u['replacement_row']['hands_on']=='false'
 for k,v in u['changes'].items():assert u['replacement_row'][k]==v['after'] and v['before']!=v['after']
 assert set(u['replacement_row']['evidence_ids'].split(';'))<=all_e
inv=list(csv.DictReader((BASE/'artifacts/supernote-controls-deepening/functional-inventory.csv').open()))
assert len(inv)==140 and [int(r['order']) for r in inv]==list(range(1,141))
assert all(1<=int(r['pdf_bookmark_page'])<=182 for r in inv)
for r in inv:
 assert set(r['evidence_ids'].split(';'))<=all_e
 if r['flow_ids']:assert set(r['flow_ids'].split(';'))<={f'F-M006-{i:02}' for i in range(1,11)}
m=json.loads((WORK/'manifest.json').read_text())
assert len(m['assets'])==4
for a in m['assets']:
 assert a['visual_observed'] is True and a['hands_on'] is False
 assert hashlib.sha256((ROOT/a['path']).read_bytes()).hexdigest()==a['sha256']
assert hashlib.sha256((ROOT/m['manual']['path']).read_bytes()).hexdigest()==m['manual']['sha256']
report={'new_evidence':len(e),'new_flows':len(c),'old_flow_update_suggestions':len(updates),'toc_rows':len(inv),'viewed_static_pages':len(m['assets']),'duplicate_new_prefix_in_batches':False,'hands_on':False,'status':'PASS','scope':'documentation/schema/references/local hashes; no live product test or exhaustive remote link revalidation'}
(WORK/'check-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
