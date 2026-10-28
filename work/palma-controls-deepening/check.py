from pathlib import Path
import csv,json,hashlib,re
W=Path('work/palma-controls-deepening');L=Path('docs/research/interaction-library')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
master=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];ed={x['evidence_id']:x for x in master}
cov={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
ep=L/'batches/palma-controls-deepening-evidence.jsonl';es=[json.loads(x) for x in ep.read_text().splitlines()];assert len(es)==4
external=[]
for p in [L/'evidence.jsonl',*list((L/'batches').glob('*evidence.jsonl'))]:
 if p==ep:continue
 external.extend(json.loads(x) for x in p.read_text().splitlines() if x.strip())
assert not {e['evidence_id'] for e in es}&{e['evidence_id'] for e in external}
urls={e['url'] for e in external}
for e in es:
 assert e['supersedes'] is None and e['hands_on'] is False
 if e['url'] in urls:
  assert e['evidence_id']=='E-PLCD-04' and e['derived_from']=='E-PALMA-15' and e['url']==ed['E-PALMA-15']['url']
 assert e['url'].startswith('https://')
 assert not e['archived_artifact'].startswith('work/')
 assert sha(L/e['archived_artifact'])==e['sha256']
 for pk,hk in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:
  if pk in e:assert sha(e[pk])==e[hk]
 if 'visual_audit_path' in e:assert sha(L/e['visual_audit_path'])==e['visual_audit_sha256']
 for p in e.get('observed_artifacts',[]):assert sha(L/p['path'])==p['sha256']
 assert e['visual_observed']==(e['evidence_id'] in ['E-PLCD-03','E-PLCD-04'])
updates=json.loads((W/'old-coverage-updates.json').read_text());assert len(updates)==8
assert sum(len(x['changed_fields']) for x in updates)==31
for u in updates:
 assert cov[u['flow_id']]==u['before'],'old F baseline changed: '+u['flow_id']
 assert u['after']['depth']==u['before']['depth'] and u['after']['hands_on']=='false'
 assert set(u['changed_fields'])=={k for k in u['before'] if u['before'][k]!=u['after'][k]}
 for k,v in u['changed_fields'].items():assert v=={'before':u['before'][k],'after':u['after'][k]}
 assert set(u['after']['evidence_ids'].split(';')) <= (set(ed)|{e['evidence_id'] for e in es})
 assert u['after']['visual_status'] in ['not_observed','static_reference_only']
adds=json.loads((W/'old-evidence-additional-captures.json').read_text());assert len(adds)==14
for u in adds:
 e=ed[u['evidence_id']];assert e['url']==u['url'] and e.get('additional_captures',[])==u['before_additional_captures']
 for c in u['append_additional_captures']:
  for pk,hk in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:
   if pk in c:assert sha(c[pk])==c[hk]
  assert sha(L/c['archived_artifact'])==c['sha256']
for kind,n in [('coverage',0),('inventory',137)]:
 p=L/f'batches/palma-controls-deepening-{kind}.csv';b=p.read_bytes();rs=list(csv.DictReader(p.open()))
 assert len(rs)==n and b.count(b'\n')==n+1 and b'\r' not in b and b'\\r\\n' not in b
 assert all(None not in r for r in rs)
 print(kind,'records',len(rs),'physical_newlines',b.count(b'\n'))
case=L/'cases/M-010-palma.md';s=case.read_text();assert set(re.findall(r'F-M010-\d+',s))=={f'F-M010-{i:02}' for i in range(1,11)}
for target in re.findall(r'\]\(([^)]+)\)',s):
 if not target.startswith(('https://','http://')):assert (case.parent/target).exists(),target
assert sha(W/'case-before.md')==json.loads((W/'baseline.json').read_text())['case_sha256']
for x in json.loads((W/'formal-manifest.json').read_text()):assert sha(x['path'])==x['sha256'],x['path']
meta=json.loads((W/'raw/manual-metadata.json').read_text());assert meta['pdf_pages']==103 and meta['observed_pdf_pages']==[1,86,95,97]
assert len(json.loads((W/'raw/manual-outline.json').read_text()))==73
print('PASS:4 E(2 generic text+2 static derivatives),0 new F,8 old F/31 fields,14 old E captures,137 inventory; hashes/references/before/CSV checked.')
