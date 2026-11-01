from pathlib import Path
import json,csv,hashlib,re

W=Path('work/meebook-controls-deepening');L=Path('docs/research/interaction-library')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((W/'baseline.json').read_text());assert sha(W/'case-before.md')==baseline['case_sha256']
e=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines() if x.strip()];by_e={x['evidence_id']:x for x in e}
flows={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
updates=json.loads((W/'old-coverage-updates.json').read_text())
for u in updates:
 assert flows[u['flow_id']]==u['before'],u['flow_id']+' before changed; root must reconcile'
 assert u['changed_fields']=={k:{'before':u['before'][k],'after':v} for k,v in u['after'].items() if u['before'][k]!=v}
 assert u['after']['hands_on']=='false'
for u in json.loads((W/'old-evidence-additional-captures.json').read_text()):
 assert by_e[u['evidence_id']].get('additional_captures',[])==u['before_additional_captures']
 for c in u['append_additional_captures']:
  assert sha(L/c['archived_artifact'])==c['sha256']
  assert c['hands_on'] is False and c['visual_observed'] is False
  for field,hf in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256'),('failed_response_work_path','failed_response_sha256')]:
   if c.get(field):assert sha(Path(c[field]))==c[hf]
  if c['http_status']==403:assert c['raw_work_path'] is None
for c in [json.loads(x) for x in (W/'captures.jsonl').read_text().splitlines()]:assert sha(Path(c['raw_work_path']))==c['raw_sha256']
counts={}
for name,expected in [('coverage',0),('inventory',49)]:
 p=L/f'batches/meebook-controls-deepening-{name}.csv';b=p.read_bytes();rows=list(csv.DictReader(p.open()))
 assert len(rows)==expected and b'\\r\\n' not in b and b'\r' not in b
 assert b.count(b'\n')==expected+1
 counts[name]={'rows':len(rows),'physical_LF':b.count(b'\n'),'literal_backslash_r_n':b.count(b'\\r\\n')}
 if name=='inventory':
  assert len({r['inventory_id'] for r in rows})==len(rows)
  for r in rows:
   for eid in r['evidence_ids'].split(';'):
    if eid:assert eid in by_e
   for fid in r['flow_ids'].split(';'):
    if fid:assert fid in flows
assert (L/'batches/meebook-controls-deepening-evidence.jsonl').read_bytes()==b''
case=L/'cases/M-057-meebook.md';s=case.read_text()
assert set(re.findall(r'F-M057-\d+',s))=={u['flow_id'] for u in updates}
for target in re.findall(r'\]\(([^)]+)\)',s):
 if not target.startswith(('http:','https:')):assert (case.parent/target).exists(),target
refs=[];scanned=0
for p in L.rglob('*'):
 if p.suffix not in ['.json','.jsonl']:continue
 try:o=json.loads(p.read_text()) if p.suffix=='.json' else [json.loads(x) for x in p.read_text().splitlines() if x.strip()]
 except Exception:continue
 scanned+=1
 def walk(o,loc=''):
  if isinstance(o,dict):
   for k,v in o.items():walk(v,loc+'/'+k)
  elif isinstance(o,list):
   for k,v in enumerate(o):walk(v,loc+'/'+str(k))
  elif isinstance(o,str) and ('cases/M-057-meebook.md' in o or baseline['case_sha256'] in o):refs.append({'file':str(p),'location':loc,'value':o})
 walk(o)
assert not refs,'New reverse dependency requires root review'
print(json.dumps({'status':'PASS','new_E':0,'new_F':0,'old_F':len(updates),'changed_fields':sum(len(x['changed_fields']) for x in updates),'old_E_captures':4,'csv':counts,'reverse_json_files_scanned':scanned,'reverse_refs':refs,'case_before_sha256':baseline['case_sha256'],'case_after_sha256':sha(case),'hands_on':False,'new_visuals':0},ensure_ascii=False,indent=2))
