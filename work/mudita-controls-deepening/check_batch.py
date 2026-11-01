from pathlib import Path
from collections import Counter
from urllib.parse import urlparse
import json,csv,hashlib,re
R=Path('/home/workspace/reader');W=R/'work/mudita-controls-deepening';L=R/'docs/research/interaction-library';A=L/'artifacts/mudita-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
E=[json.loads(x) for x in (L/'batches/mudita-controls-deepening-evidence.jsonl').read_text().splitlines()]
F=list(csv.DictReader((L/'batches/mudita-controls-deepening-coverage.csv').open()))
U=json.loads((W/'old-coverage-updates.json').read_text());I=list(csv.DictReader((A/'function-inventory.csv').open()))
assert len(E)==22 and len(F)==3 and len(U)==7 and len(I)==111
csv_newlines=[]
for rel,count in [('batches/mudita-controls-deepening-coverage.csv',4),('artifacts/mudita-controls-deepening/function-inventory.csv',112)]:
 b=(L/rel).read_bytes()
 assert b.count(b'\n')==count and b.count(b'\r\n')==0 and b.count(b'\\r\\n')==0
 csv_newlines.append(dict(file=rel,physical_LF=count,physical_CRLF=0,literal_backslash_r_backslash_n=0))
assert [x['evidence_id'] for x in E]==[f'E-MDCD-{i:02}' for i in range(1,23)]
assert [x['flow_id'] for x in F]==[f'F-M012-{i:02}' for i in range(8,11)]
oldE={x['evidence_id']:x for x in map(json.loads,(L/'batches/reader-evidence.jsonl').read_text().splitlines()) if x['product_id']=='M-012'}
ids=set(oldE)|{x['evidence_id'] for x in E}
for e in E:
 assert e['product_id']=='M-012' and e['hands_on'] is False
 assert urlparse(e['url']).scheme in ('https','http')
 assert (R/e['raw_work_path']).is_file() and sha(R/e['raw_work_path'])==e['raw_sha256']
 if e['archived_artifact']:
  assert not e['archived_artifact'].startswith(('work/','/'))
  assert sha(L/e['archived_artifact'])==e['sha256']
 for extra in e.get('additional_raw_sources',[]):assert sha(R/extra['raw_work_path'])==extra['raw_sha256']
assert {e['evidence_id'] for e in E if e['visual_observed']}=={'E-MDCD-01','E-MDCD-12'}
# Check prefix collisions against other authoritative source batches, excluding master duplicates after a root merge.
for p in (L/'batches').glob('*evidence.jsonl'):
 if p.name=='mudita-controls-deepening-evidence.jsonl':continue
 assert not re.search(r'"evidence_id"\s*:\s*"E-MDCD-',p.read_text()),p
old={x['flow_id']:x for x in csv.DictReader((L/'batches/reader-coverage.csv').open()) if x['product_id']=='M-012'}
old_states={}
for u in U:
 n=u['replacement_row']; b=old[u['flow_id']]
 assert n['flow_id']==u['flow_id']
 original=dict(n)
 for key,change in u['changes'].items():
  assert n[key]==change['after'];original[key]=change['before']
 assert {k for k in original if original[k]!=n[k]}==set(u['changes'])
 assert b==original or b==n, (u['flow_id'],'current old row matches neither saved before nor proposed after')
 old_states[u['flow_id']]='before' if b==original else 'after_already_applied'
for f in [*F,*[u['replacement_row'] for u in U]]:
 assert f['depth']=='D2' and f['hands_on']=='false'
 assert set(f['evidence_ids'].split(';'))<=ids
 assert f['visual_status'] in ('not_observed','static_reference_only')
 assert (L/f['case_path']).exists()
assert Counter(i['scope'] for i in I)=={'manual_toc':15,'help_article':81,'release_section':15}
for i in I:
 assert urlparse(i['url']).scheme in ('https','http')
 assert set(i['evidence_ids'].split(';'))<=ids
 assert not i['flow_ids'] or set(i['flow_ids'].split(';'))<={f'F-M012-{n:02}' for n in range(1,11)}
case=L/'cases/M-012-mudita.md';txt=case.read_text()
assert set(re.findall(r'F-M012-\d+',txt))=={f'F-M012-{i:02}' for i in range(1,11)}
assert set(re.findall(r'E-(?:MUD|MDCD)-\d+',txt))<=ids
for target in re.findall(r'\]\(([^)]+)\)',txt):
 if not urlparse(target).scheme: assert (case.parent/target).resolve().is_file(),target
manifest=json.loads((W/'source-manifest.json').read_text())
for m in manifest:assert m['status']==200 and sha(R/m['relative_raw_path'])==m['sha256']
result=dict(status='pass',new_evidence=len(E),new_flows=len(F),old_updates=len(U),inventory_rows=len(I),inventory_reading_status=dict(Counter(i['reading_status'] for i in I)),raw_files_hashed=len(manifest),official_frames_observed=2,hands_on=False,old_rows_state=old_states,csv_newlines=csv_newlines)
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
