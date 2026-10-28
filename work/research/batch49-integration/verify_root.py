import csv,hashlib,json,re
from collections import Counter
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch49-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
f=rows(L/'coverage.csv');e=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];rp=rows(L/'reference-points.csv')
fm={r['flow_id']:r for r in f};em={r['evidence_id']:r for r in e}
assert len(f)==964 and len(e)==1710 and len(rp)==114
assert Counter(r['depth'] for r in f)=={'D2':866,'D1':78,'D4':20}
assert len({r['url'] for r in e})==1539
assert len({r['reference_id'] for r in rp})==114
for r in rp:
 assert len(r)==15 and all(r.values()),r['reference_id']
 assert all(k in fm and fm[k]['product_id']==r['product_id'] for k in r['flow_ids'].split(';'))
 assert all(k in em for k in r['evidence_ids'].split(';'))
 if r['reference_id'].startswith('RP-M040-'):assert all(em[k]['product_id']=='M-040' for k in r['evidence_ids'].split(';'))
 assert (L/r['artifact_path']).exists()
assert sum(r['product_id']=='M-040' for r in rp)==8
case=(L/'cases/M-040-antennapod.md').read_text()
assert set(re.findall(r'F-M040-\d+',case))=={f'F-M040-{i:02d}' for i in range(1,15)}
assert all(x['depth']=='D2' and x['hands_on']=='false' and x['visual_status']=='not_observed' for x in f if x['product_id']=='M-040')
assert len([r for r in e if r['product_id']=='M-040'])==59
owners={f'F-M040-{i:02d}':[] for i in range(1,15)}
for p in (L/'batches').glob('*-coverage.csv'):
 for r in rows(p):
  if r['flow_id'] in owners:owners[r['flow_id']].append(str(p.relative_to(R)))
assert all(len(v)==1 for v in owners.values())
for x in json.loads((W/'old-flow-updates.json').read_text()):assert fm[x['before']['flow_id']]==x['after'] and x['before']['depth']==x['after']['depth']
for x in json.loads((W/'old-evidence-updates.json').read_text()):assert em[x['before']['evidence_id']]==x['after']

a=json.loads((L/'artifacts/antennapod-state-controls/source-audit.json').read_text())
tree=json.loads((R/a['source_index_path']).read_text());tm={x['path']:x for x in tree['tree'] if x['type']=='blob'}
assert len(a['new_sources'])==14 and len(a['reused_sources'])==7
def blob(p):
 b=p.read_bytes();return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
for x in a['new_sources']:
 p=R/x['path'];assert x['status']==200 and p.stat().st_size==x['bytes'] and sha(p)==x['sha256']
 assert blob(p)==x['git_blob_sha']==tm[x['repo_path']]['sha']
 assert a['commit'] in x['source_url']
 r=em[x['evidence_id']];assert r['hands_on'] is False and r['visual_observed'] is False and r['raw_sha256']==x['sha256']
 assert sha(L/r['archived_artifact'])==r['sha256']
 for rel,h in r['artifact_hashes'].items():assert sha(L/rel)==h
old_count=0
for eid,fs in a['reused_sources'].items():
 for x in fs:
  p=R/x['raw_work_path'];assert sha(p)==x['raw_sha256']
  candidates=[t for t in tm if t.endswith('/'+p.name)]
  assert any(tm[t]['sha']==blob(p) for t in candidates),(eid,p)
  old_count+=1
 cap=em[eid]['additional_captures'][-1];assert cap['source_files']==fs and sha(L/cap['archived_artifact'])==cap['sha256']
inv=rows(L/'batches/antennapod-queue-controls-deepening-inventory.csv');assert len(inv)==162
assert len({r['inventory_id'] for r in inv})==162
for r in inv:
 assert all(k in fm for k in r['flow_ids'].split(';') if k)
 assert all(k in em for k in r['evidence_ids'].split(';') if k)
assert {f'C-{i:02d}' for i in range(1,32)} <= {r['inventory_id'] for r in inv}
iv=json.loads((W/'inventory-updates.json').read_text());assert len(iv['updated_ids'])==12 and len(iv['new_ids'])==4
for ident in ['C-17','C-18','C-19','C-28','C-29']:assert next(x for x in inv if x['inventory_id']==ident)['flow_ids']=='F-M040-14'

reqsha=sha(R/'docs/requirements.md');assert reqsha=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));idx=(R/'docs/briefs/README.md').read_text();assert len(briefs)==49 and all(x.name in idx for x in briefs)
cleanup=json.loads((W/'cache-cleanup.json').read_text());assert cleanup['bytes']==26318084 and cleanup['retained_profile_unchanged']
profile=R/'work/research/batch38-week/desktop-profile';assert not (R/cleanup['target']).exists()
assert all(sha(profile/k)==v for k,v in cleanup['retained_profile_hashes'].items())
pdfcheck=json.loads((W/'dragonbox-pdf-check.json').read_text());assert pdfcheck['page_count']==9 and sha(R/pdfcheck['path'])==pdfcheck['sha256']
old=json.loads((W/'before-manifest.json').read_text());paths=[R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))]
now={str(p.relative_to(R)):sha(p) for p in paths if p.is_file()}
new=sorted(set(now)-set(old));modified=sorted(k for k in set(now)&set(old) if now[k]!=old[k]);deleted=sorted(set(old)-set(now));assert not deleted
assert 'docs/requirements.md' not in modified and 'AGENTS.md' not in modified
for rel in new+modified:
 if rel.endswith('.md'):assert all(line.rstrip()==line for line in (R/rel).read_text().splitlines())
result=dict(status='passed',total_flows=964,total_evidence=1710,total_reference_points=114,new_source_git_blobs_verified=14,reused_source_files_verified=old_count,M040_flows=14,M040_evidence=59,M040_reference_points=8,inventory_rows=162,briefs_indexed=49,requirements_sha256=reqsha,requirements_unchanged=True,cache_only_cleanup_profile_other_files_unchanged=True,DragonBox_pdf_page_count_verified=9,flow_owners=owners,new_formal_files=new,modified_formal_files=modified,deleted_formal_files=deleted,scope='Source identity, references, data consistency and narrow storage cleanup; no APK/device execution or learning result verified')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','total_flows','total_evidence','total_reference_points','new_source_git_blobs_verified','reused_source_files_verified','requirements_unchanged']},ensure_ascii=False))
print('Formal file changes:',len(new),'new',len(modified),'modified',len(deleted),'deleted')
