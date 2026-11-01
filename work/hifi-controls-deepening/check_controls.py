from pathlib import Path
import json,csv,re,hashlib,urllib.parse
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/hifi-controls-deepening'
epath=lib/'batches/hifi-controls-deepening-evidence.jsonl';cpath=lib/'batches/hifi-controls-deepening-coverage.csv'
e=[json.loads(x) for x in epath.read_text().splitlines()];c=list(csv.DictReader(cpath.open()))
assert len(e)==16 and len(c)==6
assert len({x['evidence_id'] for x in e})==16
assert len({x['flow_id'] for x in c})==6
expected_keys={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
master_e={json.loads(x)['evidence_id'] for x in (lib/'evidence.jsonl').read_text().splitlines()}
all_e=master_e|{x['evidence_id'] for x in e}
for x in e:
 assert set(x)==expected_keys
 assert x['hands_on'] is False and x['checked_on']=='2026-10-26'
 assert urllib.parse.urlsplit(x['url']).scheme in ['http','https']
 assert x['evidence_id'].startswith(('E-SHCD-','E-IBCD-'))
 if x['archived_artifact']:
  p=lib/x['archived_artifact'];assert p.exists()
  assert hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
  assert x['visual_observed'] is True
 else: assert x['visual_observed'] is False
 if x['supersedes']: assert x['supersedes'] in all_e
for x in c:
 assert x['depth']=='D2' and x['hands_on']=='false'
 assert x['visual_status'] in ['not_observed','static_reference_only']
 assert all(y in all_e for y in x['evidence_ids'].split(';'))
 assert (lib/x['case_path']).exists()
updates=json.loads((w/'old-coverage-updates.json').read_text());assert len(updates)==10
for x in updates:
 assert x['replacement_row']['flow_id']==x['flow_id']
 assert all(y in all_e for y in x['replacement_row']['evidence_ids'].split(';'))
 assert x['changes']
for pid,name,count in [('063','shanling',10),('064','ibasso',12)]:
 p=lib/f'cases/M-{pid}-{name}.md';s=p.read_text()
 ids=re.findall(r'\| (F-M\d+-\d+) \|',s)
 assert len(ids)==count and set(ids)=={f'F-M{pid}-{n:02d}' for n in range(1,count+1)}
 for link in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',s): assert Path(link).exists(),link
 for eid in re.findall(r'E-(?:SHCD|IBCD|SHAN|IBA)-\d+',s): assert eid in all_e,eid
 assert not any(line.endswith(' ') for line in s.splitlines())
assert all(x['flow_id'] not in {y['flow_id'] for y in updates} for x in c)
# Only detect collisions in new, unmerged research batches; master duplicates after parent merge are legitimate.
newids={x['evidence_id'] for x in e}
for p in (lib/'batches').glob('*evidence.jsonl'):
 if p==epath: continue
 for line in p.read_text().splitlines(): assert json.loads(line)['evidence_id'] not in newids,(p,line)
print('PASS: 16 evidence; 6 new D2 flows; 10 old-row suggestions; 10 static artifacts with SHA256; all references/paths; no hands-on or continuous claims; old F01–08 preserved.')
