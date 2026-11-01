import csv,json,hashlib,re
from pathlib import Path
R=Path('.');L=R/'docs/research/interaction-library';W=R/'work/ibasso-controls-deepening';A=L/'artifacts/ibasso-controls-deepening';checks=[]
def ok(x,n):
 if not x:raise AssertionError(n)
 checks.append(n)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
newE=L/'batches/ibasso-controls-deepening-evidence.jsonl';newC=L/'batches/ibasso-controls-deepening-coverage.csv';newI=L/'batches/ibasso-controls-deepening-inventory.csv'
es=[json.loads(x) for x in newE.read_text().splitlines()];cs=list(csv.DictReader(newC.open()));inv=list(csv.DictReader(newI.open()));oldE={};oldF={}
for p in (L/'batches').glob('*evidence.jsonl'):
 if p==newE:continue
 for line in p.read_text().splitlines():
  if line.strip():x=json.loads(line);oldE[x['evidence_id']]=x
for p in (L/'batches').glob('*coverage.csv'):
 if p==newC:continue
 for x in csv.DictReader(p.open()):oldF[x['flow_id']]=x
ok(len(es)==1 and es[0]['evidence_id']=='E-IBDC-01','1 new E');ok('E-IBDC-01' not in oldE,'new E no collision');ok(len(cs)==3,'3 new F')
for x in cs:
 ok(x['flow_id'] not in oldF,'new F no collision');ok(set(x['evidence_ids'].split(';'))<=set(oldE),'new F reuses old manual');ok(x['depth']=='D2' and x['hands_on']=='false' and x['visual_status']=='static_reference_only','new F documented not operated')
ok({r['flow_id'] for r in cs}=={f'F-M064-{n}' for n in [13,14,15]},'F13-15 IDs')
for x in es:
 ok(sha(R/x['raw_work_path'])==x['raw_sha256'],'new raw hash');ok(not x['archived_artifact'].startswith('work/'),'formal path');ok(sha(L/x['archived_artifact'])==x['sha256'],'note sha')
 for cap in x.get('provenance_captures',[]):ok(sha(R/cap['raw_work_path'])==cap['raw_sha256'],'provenance hash')
 for cap in x.get('additional_captures',[]):ok(cap['raw_work_path'] is None and cap['raw_sha256'] is None,'unobtained raw not invented')
updates=json.loads((W/'existing-coverage-updates.json').read_text());ok(len(updates)==14,'14 old fields');ok({u['flow_id'] for u in updates}=={'F-M064-01','F-M064-06','F-M064-09','F-M064-10'},'4 old F')
for u in updates:ok(oldF[u['flow_id']][u['field']]==u['before'],'old before exact '+u['flow_id']+'/'+u['field']);ok(u['before']!=u['after'],'not noop')
adds=json.loads((W/'additional-captures-updates.json').read_text());ok(len(adds)==3,'3 old E captures')
for u in adds:
 ok(oldE[u['evidence_id']].get('additional_captures')==u['before'],'old E before exact')
 for cap in u['after'][len(u['before'] or []):]:
  ok(sha(R/cap['raw_work_path'])==cap['raw_sha256'],'old capture raw hash');ok(sha(L/cap['archived_artifact'])==cap['sha256'],'old capture note hash')
  for p,h in cap.get('artifact_hashes',{}).items():ok(sha(L/p)==h,'render hash')
for p,h in json.loads((W/'old-batch-hashes.json').read_text()).items():ok(sha(R/p)==h,'old batch unchanged '+p)
allE=set(oldE)|{'E-IBDC-01'};allF=set(oldF)|{x['flow_id'] for x in cs}
ok(len(inv)==129 and len({x['item_id'] for x in inv})==129,'129 unique inventory rows')
for x in inv:ok(set(filter(None,x['evidence_ids'].split(';')))<=allE,'inventory evidence refs');ok(set(filter(None,x['flow_ids'].split(';')))<=allF,'inventory flow refs')
from collections import Counter
counts=dict(Counter(x['kind'] for x in inv));ok(counts==json.loads((W/'inventory-counts.json').read_text()),'separate category counts')
case=L/'cases/M-064-ibasso.md';ct=case.read_text();ok({f'F-M064-{i:02}' for i in range(1,16)}<=set(re.findall(r'F-M064-\d+',ct)),'15 cases flow ids')
files=[case,newE,newC,newI,A/'source-notes.md',W/'existing-coverage-updates.csv']
for p in files:
 b=p.read_bytes();ok(b'\r' not in b and b'\\r\\n' not in b,'normal LF '+p.name);ok(all(x.rstrip()==x for x in p.read_text().splitlines()),'no trailing spaces '+p.name)
 if p.suffix=='.md':
  for dest in re.findall(r'\]\(([^)]+)\)',p.read_text()):
   if not dest.startswith(('http:','https:','#')):ok((p.parent/dest.split('#')[0]).exists(),'file link '+dest)
manifest=json.loads((W/'render-manifest.json').read_text());ok(len(manifest)==6,'6 observed pages')
for x in manifest:ok(x['observed'] is True,'actual view recorded');ok(sha(R/x['raw_work_path'])==x['raw_sha256'],'work render hash');ok(sha(L/x['archived_artifact'])==x['sha256'],'formal render hash')
records={}
for x in json.loads((W/'fetch-log.json').read_text()):
 if x.get('raw_work_path'):ok(sha(R/x['raw_work_path'])==x['raw_sha256'],'http raw hash');records[x['raw_work_path']]=x['raw_sha256']
m=json.loads((W/'manual-metadata.json').read_text());ok(sha(R/m['raw_work_path'])==m['raw_sha256'],'PDF unchanged');records[m['raw_work_path']]=m['raw_sha256']
for p in [*files,*A.glob('*.png'),W/'existing-coverage-updates.json',W/'additional-captures-updates.json']:records[str(p)]=sha(p)
(W/'material-hashes.json').write_text(json.dumps(records,indent=2)+'\n')
r=dict(status='PASS',checks=len(checks),new_evidence=1,new_flows=3,old_flow_fields=14,old_flow_count=4,old_evidence_captures=3,inventory_counts=counts,actually_viewed_printed_pages=[17,18,20,21,34,42],old_batches_unchanged=True)
(W/'check-result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False))
