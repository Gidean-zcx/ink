import csv,json,hashlib,pathlib,collections
root=pathlib.Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
before=root/'work/research/batch39-integration/before/docs/research/interaction-library'

def csvrows(p): return list(csv.DictReader(open(p,encoding='utf-8',newline='')))
def jsonl(p): return [json.loads(x) for x in open(p,encoding='utf-8') if x.strip()]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
oldF=csvrows(before/'coverage.csv'); curF=csvrows(lib/'coverage.csv')
oldE=jsonl(before/'evidence.jsonl'); curE=jsonl(lib/'evidence.jsonl')
of={x['flow_id']:x for x in oldF}; cf={x['flow_id']:x for x in curF}
oe={x['evidence_id']:x for x in oldE}; ce={x['evidence_id']:x for x in curE}
newf=sorted(set(cf)-set(of)); newe=sorted(set(ce)-set(oe))
assert set(of)<=set(cf), sorted(set(of)-set(cf))
assert set(oe)<=set(ce), sorted(set(oe)-set(ce))
assert newf==['F-M007-21','F-M007-22','F-M007-23'],newf
assert all(cf[x]['depth']=='D2' for x in newf)
expected={'F-M007-11':'D1','F-M007-12':'D1','F-M063-07':'D1','F-M065-09':'D1'}
assert {x:cf[x]['depth'] for x in expected}==expected
rp=csvrows(lib/'reference-points.csv')
assert len(rp)==16
rids=[x['reference_id'] for x in rp]; assert len(rids)==len(set(rids))
missing=[]
for r in rp:
 fs=[x for x in r['flow_ids'].split(';') if x]; es=[x for x in r['evidence_ids'].split(';') if x]
 for x in fs:
  if x not in cf: missing.append((r['reference_id'],'F',x))
 for x in es:
  if x not in ce: missing.append((r['reference_id'],'E',x))
 a=lib/r['artifact_path']
 if not a.is_file(): missing.append((r['reference_id'],'artifact',str(a)))
assert not missing,missing
m7=[r for r in rp if r['product_id']=='M-007']; m48=[r for r in rp if r['product_id']=='M-048']
assert m7 and all(r['evidence_basis']=='manual_diagram_static' for r in m7)
assert m48 and all(r['evidence_basis']=='automated_public_browser' for r in m48)
assert all(any(cf[f]['depth']=='D4' for f in r['flow_ids'].split(';')) for r in m48)
inv=csvrows(lib/'batches/viwoods-chinese-workflows-inventory.csv')
assert len(inv)==75
pages=[int(x['source_file_page']) for x in inv]
assert len(pages)==len(set(pages))==75 and sorted(pages)==list(range(1,76))
obs=json.load(open(lib/'artifacts/viwoods-chinese-workflows/observed-pages.json',encoding='utf-8'))
assert obs['text_read_file_pages']==list(range(1,76))
assert len(obs['visual_observed_file_pages'])==29 and len(set(obs['visual_observed_file_pages']))==29
assert len(obs['archived_file_pages'])==19 and len(set(obs['archived_file_pages']))==19
invobs=sorted(int(x['source_file_page']) for x in inv if x['visual_status']=='static_page_observed')
invarch=sorted(int(x['source_file_page']) for x in inv if x['artifact_path'])
assert invobs==sorted(obs['visual_observed_file_pages']),(invobs,obs['visual_observed_file_pages'])
assert invarch==sorted(obs['archived_file_pages']),(invarch,obs['archived_file_pages'])
assert len(obs['observations'])==29
assert sorted(x['file_page'] for x in obs['observations'])==sorted(obs['visual_observed_file_pages'])
assert sorted(x['file_page'] for x in obs['observations'] if x.get('archived_artifact'))==sorted(obs['archived_file_pages'])
hash_results=[]
for eid in newe:
 e=ce[eid]
 ah=e.get('artifact_hashes') or {}
 assert ah, eid
 for rel,want in ah.items():
  p=lib/rel
  got=sha(p) if p.is_file() else None
  hash_results.append((eid,rel,want,got))
  assert got==want,(eid,rel,want,got)
 # main archived artifact hash, including source-notes
 ap=e.get('archived_artifact')
 if ap and e.get('sha256'):
  assert sha(lib/ap)==e['sha256'],(eid,ap)
req=root/'docs/requirements.md'; breq=root/'work/research/batch39-integration/before/docs/requirements.md'
reqsha=sha(req); breqsha=sha(breq)
assert reqsha==breqsha=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
out=root/'work/research/batch39-integration/worker-validation.md'
lines=['# Batch 39 worker validation','', '- Result: PASS',f'- Before/current flows: {len(oldF)} / {len(curF)}; old IDs retained: {len(set(of)&set(cf))}; new: {", ".join(newf)} (all D2).',f'- Before/current evidence: {len(oldE)} / {len(curE)}; old IDs retained: {len(set(oe)&set(ce))}; new: {", ".join(newe)}.',f'- Required D1: '+', '.join(f'{k}={cf[k]["depth"]}' for k in expected),f'- Reference points: {len(rp)} unique IDs; all F/E/artifacts exist; M007={len(m7)} manual_diagram_static; M048={len(m48)} automated_public_browser, each row links at least one D4 flow.',f'- Chinese manual inventory: {len(inv)} unique pages (1–75); observed={len(invobs)}, archived={len(invarch)}; both match observed-pages.json.',f'- New E artifact hashes checked: {len(hash_results)} entries, including source-notes.md; all match.',f'- requirements.md SHA-256 unchanged: `{reqsha}`.','', '## Scope', '', 'Local snapshot/current-table comparison only. No source browsing, product operation, or formal-document edits. The only state change is this validation report.']
out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('\n'.join(lines))
