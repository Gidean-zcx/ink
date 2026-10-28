from pathlib import Path
import json,csv,re,hashlib,collections
W=Path('work/casio-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
ep=B/'casio-controls-deepening-evidence.jsonl';cp=B/'casio-controls-deepening-coverage.csv';ip=B/'casio-controls-deepening-inventory.csv';case=L/'cases/M-024-casio.md';notes=L/'artifacts/casio-controls-deepening/source-notes.md'
e=[json.loads(x) for x in ep.read_text().splitlines()];c=list(csv.DictReader(cp.open()));inv=list(csv.DictReader(ip.open()));s=case.read_text();base=json.loads((W/'baseline.json').read_text())
assert len(e)==8 and len(c)==7 and len(inv)==209
assert len({r['evidence_id'] for r in e})==8
assert {r['flow_id'] for r in c}=={f'F-M024-{n:02}' for n in range(10,17)}
assert len(set(re.findall(r'F-M024-\d{2}',s)))==16
olde={r['evidence_id'] for r in (json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines())}
newids={r['evidence_id'] for r in e};assert not newids&olde
for r in e:
 assert r['hands_on'] is False
 assert r['raw_sha256']==sha(r['raw_work_path'])
 assert not r['archived_artifact'].startswith('work/')
 assert r['sha256']==sha(L/r['archived_artifact'])
 for p,h in r['artifact_hashes'].items():assert sha(L/p)==h
 assert (bool(r['observed_pages'])==r['visual_observed'])
for r in c+inv:
 assert set(r['evidence_ids'].split(';'))<=olde|newids
 for fid in r.get('flow_ids',r.get('flow_id','')).split(';'):
  assert not fid or fid in s
for r in c:assert r['depth']=='D2' and r['hands_on']=='false' and r['visual_status']=='static_manual_pages_only'
assert collections.Counter(r['kind'] for r in inv)=={'pdf_outline':113,'model_content_index':96}
assert len({r['item_id'] for r in inv})==209
allids={r['item_id'] for r in inv}
for r in inv:assert not r['parent_id'] or r['parent_id'] in allids
for p in [cp,ip,W/'existing-coverage-updates.csv']:
 raw=p.read_bytes();assert b'\r' not in raw and b'\\r\\n' not in raw and raw.endswith(b'\n')
updates=json.loads((W/'existing-coverage-updates.json').read_text());ucsv=list(csv.DictReader((W/'existing-coverage-updates.csv').open()));assert updates==ucsv and len(updates)==21
old={r['flow_id']:r for r in json.loads((W/'existing-coverage-before.json').read_text())}
for u in updates:assert old[u['flow_id']][u['field']]==u['before'];assert u['field']!='depth'
unchanged=[]
for path,h in base.items():
 if path.startswith('batches/'):
  assert sha(L/path)==h;unchanged.append(path)
for p in [case,notes]:
 for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
  if target.startswith(('http:','https:','#')):continue
  target=target.split('#')[0]
  q=Path(target) if target.startswith('/') else p.parent/target
  assert q.exists(),(p,target)
manifest={p.as_posix():sha(p) for p in [case,ep,cp,ip,notes,W/'E-W_series_full_CN-MA2307-A.pdf',W/'pdf-outline.json',W/'selected-pages.txt',W/'existing-coverage-updates.json',W/'existing-coverage-updates.csv']+sorted((L/'artifacts/casio-controls-deepening').glob('*.png'))+sorted(W.glob('page-*.png'))}
(W/'hash-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
result=dict(result='PASS',new_E=len(e),new_F=len(c),case_F=16,all_case_depth='D2',inventory={'pdf_outline':113,'model_content_index':96,'total':209},observed_static_pages=[34,87,116,117,121,123,156,161,171,197],archived_static_pages=[87,117,121],old_field_changes=len(updates),old_tasks=sorted({r['flow_id'] for r in updates}),old_depth_changed=False,csv_LF=True,raw_and_artifact_hashes='match',unchanged_old_batches=unchanged,network_requests=0,hands_on=False)
(W/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
