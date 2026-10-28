import csv,hashlib,json,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch47-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
f=list(csv.DictReader((L/'coverage.csv').open()));e=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];rp=list(csv.DictReader((L/'reference-points.csv').open()))
fm={x['flow_id']:x for x in f};em={x['evidence_id']:x for x in e}
assert len(f)==962 and len(e)==1695 and len(rp)==98
assert len({r['reference_id'] for r in rp})==len(rp)
for r in rp:
 assert len(r)==15 and all(r.values()),r['reference_id']
 assert all(x in fm and fm[x]['product_id']==r['product_id'] for x in r['flow_ids'].split(';')),r['reference_id']
 assert all(x in em for x in r['evidence_ids'].split(';')),r['reference_id']
 if r['product_id']=='M-043':assert all(em[x]['product_id']=='M-043' for x in r['evidence_ids'].split(';')),r['reference_id']
 assert (L/r['artifact_path']).exists(),r['reference_id']
case=L/'cases/M-043-shortcuts.md'
assert set(re.findall(r'F-M043-\d+',case.read_text()))=={f'F-M043-{i:02d}' for i in range(1,19)}
assert all(r['depth']=='D2' and r['hands_on']=='false' and r['visual_status']=='not_observed' for r in f if r['product_id']=='M-043')
assert len([r for r in e if r['product_id']=='M-043'])==60
audit=json.loads((L/'artifacts/shortcuts-data-flow/fetch-audit.json').read_text())
assert len(audit['sources'])==28
for s in audit['sources']:
 assert s['body_id'] in s['url'] and s['status']==200
 for path,h in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256')]:assert sha(R/s[path])==s[h]
 html=(R/s['raw_work_path']).read_text()
 assert s['body_id'] in html and any(v['text']=='iOS 27' and '/10.0/ios/27' in v['url'] for v in s['selected_versions'])
 record=em[s['evidence_id']];assert record['visual_observed'] is False and record['hands_on'] is False
 assert sha(L/record['archived_artifact'])==record['sha256']
 for path,h in record['artifact_hashes'].items():assert sha(L/path)==h
inv=list(csv.DictReader((L/'batches/shortcuts-controls-deepening-inventory.csv').open()))
assert len(inv)==114 and sum(r['entry_type']=='group' for r in inv)==19
assert sum(r['entry_type']=='topic' and r['review_status']=='body_read_documented_not_operated' for r in inv)==49
assert sum(r['entry_type']=='topic' and r['review_status']=='directory_only' for r in inv)==46
owners={n:[] for n in [f'F-M043-{i:02d}' for i in range(1,19)]}
for p in (L/'batches').glob('*-coverage.csv'):
 for row in csv.DictReader(p.open()):
  if row['flow_id'] in owners:owners[row['flow_id']].append(str(p.relative_to(R)))
assert all(len(x)==1 for x in owners.values())
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));idx=(R/'docs/briefs/README.md').read_text()
assert len(briefs)==47 and all(p.name in idx for p in briefs)
old=json.loads((W/'before-manifest.json').read_text());paths=[R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))];now={str(p.relative_to(R)):sha(p) for p in paths if p.is_file()}
new=sorted(set(now)-set(old));modified=sorted(k for k in set(now)&set(old) if now[k]!=old[k]);deleted=sorted(set(old)-set(now));assert not deleted
for rel in new+modified:
 if rel.endswith('.md'):
  txt=(R/rel).read_text();assert not any(line.rstrip()!=line for line in txt.splitlines()),rel
result=dict(status='passed',source_bodies_and_hashes=28,M043_flows=18,M043_evidence=60,M043_reference_points=10,total_reference_points=98,all_briefs_indexed=47,flow_owners=owners,new_formal_files=new,modified_formal_files=modified,deleted_formal_files=deleted,requirements_unchanged=True,scope='Document and evidence consistency; no product behavior verified')
(W/'root-final-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
(W/'final-manifest.json').write_text(json.dumps(now,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['status','source_bodies_and_hashes','M043_flows','M043_evidence','total_reference_points','requirements_unchanged']}))
print('formal files:',len(new),'new,',len(modified),'modified,',len(deleted),'deleted')
