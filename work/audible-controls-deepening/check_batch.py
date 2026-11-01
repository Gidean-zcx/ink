from pathlib import Path
from collections import Counter
from urllib.parse import urlparse
import json,csv,re,hashlib
R=Path('/home/workspace/reader');W=R/'work/audible-controls-deepening';L=R/'docs/research/interaction-library'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[json.loads(x) for x in (L/'batches/audible-controls-deepening-evidence.jsonl').read_text().splitlines()]
F=list(csv.DictReader((L/'batches/audible-controls-deepening-coverage.csv').open(newline='')))
I=list(csv.DictReader((L/'batches/audible-controls-deepening-inventory.csv').open(newline='')))
U=json.loads((W/'old-coverage-updates.json').read_text());B=json.loads((W/'old-coverage-before.json').read_text())
assert (len(E),len(F),len(I),len(U))==(23,3,53,11)
assert [e['evidence_id'] for e in E]==[f'E-AUCD-{i:02}' for i in range(1,24)]
assert [f['flow_id'] for f in F]==[f'F-M039-{i:02}' for i in range(12,15)]
for p in (L/'batches').glob('*evidence.jsonl'):
 if p.name=='audible-controls-deepening-evidence.jsonl':continue
 assert not re.search(r'"evidence_id"\s*:\s*"E-AUCD-',p.read_text()),p
oldE={x['evidence_id'] for x in map(json.loads,(L/'batches/audio-evidence.jsonl').read_text().splitlines()) if x['product_id']=='M-039'}
ids=oldE|{x['evidence_id'] for x in E}
for e in E:
 assert e['product_id']=='M-039' and e['hands_on'] is False and e['visual_observed'] is False
 assert e['archived_artifact'] is None and e['sha256'] is None
 assert urlparse(e['url']).netloc=='help.audible.com'
 assert sha(R/e['raw_work_path'])==e['raw_sha256']
 assert sha(R/e['extracted_work_path'])==e['extracted_sha256']
 assert e['capture_state']=='web_tool_text_snapshot_not_raw_html'
 assert e['url'] in (R/e['extracted_work_path']).read_text()
current={x['flow_id']:x for x in csv.DictReader((L/'batches/audio-coverage.csv').open(newline='')) if x['product_id']=='M-039'}
states={}
for b,u in zip(B,U):
 n=u['replacement_row'];assert u['flow_id']==b['flow_id']==n['flow_id']
 for k,d in u['changes'].items():assert b[k]==d['before'] and n[k]==d['after']
 assert {k for k in b if b[k]!=n[k]}==set(u['changes'])
 assert current[b['flow_id']] in (b,n)
 states[b['flow_id']]='before' if current[b['flow_id']]==b else 'after_already_applied'
for f in [*F,*[u['replacement_row'] for u in U]]:
 assert f['depth']=='D2' and f['hands_on']=='false' and f['visual_status']=='not_observed'
 assert set(f['evidence_ids'].split(';'))<=ids
for i in I:
 assert urlparse(i['source_url']).netloc=='help.audible.com'
 assert not i['article_url'] or urlparse(i['article_url']).netloc=='help.audible.com'
 assert set(i['evidence_ids'].split(';'))<=ids
 assert not i['flow_ids'] or set(i['flow_ids'].split(';'))<={f'F-M039-{i:02}' for i in range(1,15)}
case=L/'cases/M-039-audible.md';t=case.read_text()
assert set(re.findall(r'F-M039-\d+',t))=={f'F-M039-{i:02}' for i in range(1,15)}
assert set(re.findall(r'E-(?:AUD|AUCD)-\d+',t))<=ids
for link in re.findall(r'\]\(([^)]+)\)',t):
 if not urlparse(link).scheme:assert (case.parent/link).resolve().is_file(),link
newline=[]
for fn,count in [('audible-controls-deepening-coverage.csv',4),('audible-controls-deepening-inventory.csv',54)]:
 p=L/'batches'/fn;b=p.read_bytes();assert (b.count(b'\n'),b.count(b'\r\n'),b.count(b'\\r\\n'))==(count,0,0)
 newline.append({'file':fn,'physical_LF':count,'physical_CRLF':0,'literal_backslash_r_backslash_n':0})
M=json.loads((W/'source-manifest.json').read_text())
for x in M:assert sha(R/x['path'])==x['sha256']
assert len({sha(p) for p in W.glob('*.html')})==1
res=dict(status='pass',new_evidence=23,new_flows=3,old_updates=11,case_flows=14,inventory_rows=53,inventory_status=dict(Counter(i['coverage_status'] for i in I)),hashed_work_files=len(M),raw_http_files_are_shells=True,visual_observed=False,hands_on=False,old_rows_state=states,csv_newlines=newline)
(W/'check-results.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n');print(json.dumps(res,ensure_ascii=False))
