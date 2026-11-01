from pathlib import Path
import csv,hashlib,json,re
R=Path('.');W=R/'work/hanvon-controls-deepening';L=R/'docs/research/interaction-library'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
E=[json.loads(x) for x in (L/'batches/hanvon-controls-deepening-evidence.jsonl').read_text().splitlines()]
masterE=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
masterF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
assert len(E)==2 and len({e['evidence_id'] for e in E})==2
assert all(e['supersedes'] is None and e['hands_on'] is False and e['visual_observed'] is False for e in E)
newids={e['evidence_id'] for e in E}; newurls={e['url'] for e in E}
existing=[]
for p in [L/'evidence.jsonl',*list((L/'batches').glob('*evidence.jsonl'))]:
 if p.name=='hanvon-controls-deepening-evidence.jsonl':continue
 existing.extend(json.loads(x) for x in p.read_text().splitlines() if x.strip())
assert not newids&{e['evidence_id'] for e in existing},'ID collision before merge'
assert not newurls&{e['url'] for e in existing},'URL collision before merge'
for e in E:
 assert e['url'].startswith('https://')
 for pk,hk in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:assert sha(e[pk])==e[hk]
 assert not e['archived_artifact'].startswith('work/')
 assert sha(L/e['archived_artifact'])==e['sha256']
old=json.loads((W/'old-coverage-updates.json').read_text())
assert len(old)==4
for u in old:
 assert masterF[u['flow_id']]==u['before'],f"before mismatch {u['flow_id']}"
 assert set(u['changed_fields'])=={k for k in u['before'] if u['before'][k]!=u['after'][k]}
 assert u['after']['depth']=='D1' and u['after']['hands_on']=='false'
 assert set(u['after']['evidence_ids'].split(';')) <= ({x['evidence_id'] for x in masterE}|newids)
for u in json.loads((W/'old-evidence-additional-captures.json').read_text()):
 e=next(x for x in masterE if x['evidence_id']==u['evidence_id'])
 assert u['url']==e['url'] and e.get('additional_captures',[])==u['before_additional_captures']
 for c in u['append_additional_captures']:
  for pk,hk in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:assert sha(c[pk])==c[hk]
  assert sha(L/c['archived_artifact'])==c['sha256']
for name,n in [('coverage',0),('inventory',53)]:
 p=L/f'batches/hanvon-controls-deepening-{name}.csv';data=p.read_bytes();rows=list(csv.DictReader(p.open()))
 assert len(rows)==n and data.count(b'\n')==n+1 and b'\\r\\n' not in data and b'\r' not in data
 assert all(None not in row for row in rows)
 print(name,'rows',len(rows),'physical_newlines',data.count(b'\n'),'tail',repr(data[-50:]))
case=L/'cases/M-008-hanvon.md';text=case.read_text()
assert set(re.findall(r'F-M008-\d+',text))=={f'F-M008-{i:02}' for i in range(1,7)}
for target in re.findall(r'\]\(([^)]+)\)',text):
 if not target.startswith(('https://','http://')):assert (case.parent/target).exists(),target
assert sha(W/'case-before.md')==json.loads((W/'baseline.json').read_text())['case_sha256']
for f in json.loads((W/'formal-manifest.json').read_text()):assert sha(f['path'])==f['sha256'],f['path']
print('PASS: 2 new E; 0 new F; 4 old F / 16 fields; 2 old E local rereads; raw/formal hashes, before assertions, URLs and CSV checked.')
