from pathlib import Path
import json,csv,re,hashlib,urllib.parse
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/ak-controls-deepening'
epath=lib/'batches/ak-controls-deepening-evidence.jsonl';cpath=lib/'batches/ak-controls-deepening-coverage.csv';case=lib/'cases/M-065-astell-kern.md'
e=[json.loads(x) for x in epath.read_text().splitlines()];c=list(csv.DictReader(cpath.open()));u=json.loads((w/'old-coverage-updates.json').read_text())
assert len(e)==11 and len(c)==4 and len(u)==8
newids={x['evidence_id'] for x in e};assert len(newids)==11
master={json.loads(x)['evidence_id'] for x in (lib/'evidence.jsonl').read_text().splitlines()};allids=master|newids
keys={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
for x in e:
 assert set(x)==keys and x['evidence_id'].startswith('E-AKCD-') and x['product_id']=='M-065'
 assert x['checked_on']=='2026-10-26' and x['hands_on'] is False
 assert urllib.parse.urlsplit(x['url']).scheme in ['http','https']
 if x['visual_observed']:
  p=lib/x['archived_artifact'];assert p.is_file()
  assert hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
 else: assert x['archived_artifact'] is None
assert sum(x['visual_observed'] for x in e)==5
for x in c+[x['replacement_row'] for x in u]:
 assert x['product_id']=='M-065' and x['hands_on']=='false'
 assert all(y in allids for y in x['evidence_ids'].split(';'))
 assert (lib/x['case_path']).exists()
 assert x['visual_status'] in ['not_observed','static_reference_only']
for x in u:
 assert x['flow_id']==x['replacement_row']['flow_id']
 assert x['changes']
assert {x['flow_id'] for x in c}=={f'F-M065-{i:02d}' for i in range(13,17)}
assert not ({x['flow_id'] for x in c}&{x['flow_id'] for x in u})
s=case.read_text();ids=re.findall(r'\| (F-M065-\d+) \|',s)
assert len(ids)==16 and len(set(ids))==16 and set(ids)=={f'F-M065-{i:02d}' for i in range(1,17)}
for eid in re.findall(r'E-(?:AK|AKCD)-\d+',s): assert eid in allids,eid
for p in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',s): assert Path(p).exists(),p
assert not any(line.endswith(' ') for line in s.splitlines())
for p in (lib/'batches').glob('*evidence.jsonl'):
 if p==epath: continue
 for line in p.read_text().splitlines(): assert json.loads(line)['evidence_id'] not in newids,(p,line)
assert hashlib.sha256((w/'SR35_instruction_book_ENG.pdf').read_bytes()).hexdigest()=='2fc0dd11a7b6babfcc11028e2f1223c16320287f47adb18bb72ae10d97155182'
print('PASS: 11 new E; 4 new D2 F; 8 old-row suggestions; F01–12 preserved; 5 actually-viewed artifacts/SHA256; IDs, links, local paths; no hands-on/continuous claims.')
for x in u: print(x['flow_id'],','.join(x['changes']))
