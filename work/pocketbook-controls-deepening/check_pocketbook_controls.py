from pathlib import Path
import json,csv,re,hashlib,urllib.parse
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/pocketbook-controls-deepening'
epath=lib/'batches/pocketbook-controls-deepening-evidence.jsonl';cpath=lib/'batches/pocketbook-controls-deepening-coverage.csv'
e=[json.loads(x) for x in epath.read_text().splitlines()];c=list(csv.DictReader(cpath.open()));u=json.loads((w/'old-coverage-updates.json').read_text());inv=list(csv.DictReader((lib/'artifacts/pocketbook-controls-deepening/functional-inventory.csv').open()))
assert len(e)==7 and len(c)==3 and len(u)==6 and len(inv)==65
newids={x['evidence_id'] for x in e};assert len(newids)==7
master={json.loads(x)['evidence_id'] for x in (lib/'evidence.jsonl').read_text().splitlines()};allids=master|newids
keys={'evidence_id','product_id','title','url','checked_on','source_type','access_mode','version_scope','app_build','firmware_version','source_updated_on','locator','note','hands_on','visual_observed','archived_artifact','sha256','capture_state','supersedes'}
for x in e:
 assert set(x)==keys and x['evidence_id'].startswith('E-PBCD-') and x['product_id']=='M-003'
 assert x['checked_on']=='2026-10-26' and x['hands_on'] is False
 assert urllib.parse.urlsplit(x['url']).scheme in ['http','https']
 if x['visual_observed']:
  p=lib/x['archived_artifact'];assert p.is_file()
  assert hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
 else: assert x['archived_artifact'] is None
assert sum(x['visual_observed'] for x in e)==5
for x in c+[x['replacement_row'] for x in u]:
 assert x['product_id']=='M-003' and x['hands_on']=='false' and x['depth']=='D2'
 assert all(y in allids for y in x['evidence_ids'].split(';'))
 assert (lib/x['case_path']).exists()
 assert x['visual_status'] in ['not_observed','static_reference_only']
for x in u: assert x['flow_id']==x['replacement_row']['flow_id'] and x['changes']
assert {x['flow_id'] for x in c}=={f'F-M003-{i:02d}' for i in range(7,10)}
assert {x['flow_id'] for x in u}=={f'F-M003-{i:02d}' for i in range(1,7)}
s=(lib/'cases/M-003-pocketbook.md').read_text();ids=re.findall(r'\| (F-M003-\d+) \|',s)
assert len(ids)==9 and len(set(ids))==9 and set(ids)=={f'F-M003-{i:02d}' for i in range(1,10)}
for eid in re.findall(r'E-(?:PBK|PBCD)-\d+',s): assert eid in allids,eid
for p in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',s): assert Path(p).exists(),p
assert not any(line.endswith(' ') for line in s.splitlines())
for p in (lib/'batches').glob('*evidence.jsonl'):
 if p==epath: continue
 for line in p.read_text().splitlines(): assert json.loads(line)['evidence_id'] not in newids,(p,line)
manual=json.loads((w/'pb743k3-text.json').read_text());toc='\n'.join(x['text'] for x in manual[1:4]);toclines=[x for x in toc.splitlines() if re.search(r'(?:\. ){3,}',x) and re.search(r'\d+$',x)]
assert len(toclines)==len(inv)==65
assert [int(re.search(r'(\d+)$',x)[1]) for x in toclines]==[int(x['printed_page']) for x in inv]
assert len({(x['manual_item'],x['printed_page']) for x in inv})==65
for x in inv:
 assert x['printed_page']==x['pdf_page']
 for f in x['flow_ids'].split(';'):
  if f: assert f in ids,f
 assert x['evidence_ids'] in allids
assert hashlib.sha256((root/'work/research/interaction-depth/pocketbook-6.10.2854.pdf').read_bytes()).hexdigest()=='87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101'
assert hashlib.sha256((w/'RN_PB743K3_6.10.2854_EN.pdf').read_bytes()).hexdigest()=='051c457ece78b237ff83a1c190d5e36a721260c6d10e21d7f7be862a35bb6201'
print('PASS: 7 E; 3 new D2 F; 6 old-row suggestions; all 65 TOC rows/pages; 5 viewed artifact SHA256; references/paths; no hands-on/continuous claims.')
for x in u: print(x['flow_id'],','.join(x['changes']))
