import csv,hashlib,json,re,sys
from pathlib import Path
R=Path('/home/workspace/reader');W=R/'work/xiaoyuan-hardware-controls-deepening';L=R/'docs/research/interaction-library';B=L/'batches';P='xiaoyuan-hardware-controls-deepening';checks=[]
def ck(ok,label):
 checks.append({'check':label,'pass':bool(ok)})
 if not ok:print('FAIL',label)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def ec(p):return {x['evidence_id']:x for l in p.read_text().splitlines() if l.strip() for x in [json.loads(l)]}
def fc(p):return {x['flow_id']:x for x in csv.DictReader(p.open())}
base=load(W/'baseline.json');ef={x['evidence_id']:x for x in load(W/'evidence-before.json')};ff={x['flow_id']:x for x in load(W/'coverage-before.json')}
ck(sha(W/'case-before.md')==base['case_sha256'],'case baseline hash')
ck(not base['visual_case_dependencies'],'no old visual audit depends on case')
for label,now,before in [('old batch E',ec(B/'learning-hardware-evidence.jsonl'),ef),('master E',ec(L/'evidence.jsonl'),ef),('old batch F',fc(B/'learning-hardware-coverage.csv'),ff),('master F',fc(L/'coverage.csv'),ff)]:
 for k,v in before.items():ck(now.get(k)==v,label+' unchanged '+k)
for e in ef.values():
 if e.get('raw_work_path'):ck(sha(R/e['raw_work_path'])==e['raw_sha256'],'old raw '+e['evidence_id'])
 if e.get('archived_artifact'):ck(sha(L/e['archived_artifact'])==e['sha256'],'old archive '+e['evidence_id'])
for name,key,data in [('existing-coverage-updates.json','flow_id',ff),('existing-evidence-additional-captures.json','evidence_id',ef),('existing-evidence-field-updates.json','evidence_id',ef)]:
 proposals=load(W/name);seen=set()
 for p in proposals:
  ident=(p[key],p['field']);ck(ident not in seen,name+' unique '+str(ident));seen.add(ident)
  d=data[p[key]];b=d.get(p['field'],[] if p['field']=='additional_captures' else None)
  ck(p['before']==b,name+' before '+str(ident));ck(p['before']!=p['after'],name+' nonempty '+str(ident))
  if p['field']=='additional_captures':
   ck(p['after'][:-1]==p['before'],'append only '+p[key]);c=p['after'][-1]
   ck(c['capture_url']==d['url'],'reuse URL '+p[key]);ck(sha(R/c['raw_work_path'])==c['raw_sha256'],'capture raw hash '+p[key])
   ck(not c['archived_artifact'].startswith('work/'),'formal relative path '+p[key]);ck(sha(L/c['archived_artifact'])==c['sha256'],'source note hash '+p[key])
   ck(c['hands_on'] is False and c['visual_observed'] is False,'no new observation '+p[key])
ck(len(load(W/'existing-coverage-updates.json'))==16,'16 old F fields')
ck(len(load(W/'existing-evidence-additional-captures.json'))==3,'3 appended captures')
ck(len(load(W/'existing-evidence-field-updates.json'))==3,'3 old E metadata fields')
ck((B/(P+'-evidence.jsonl')).read_text()=='','zero new E')
cv=list(csv.DictReader((B/(P+'-coverage.csv')).open()));ck(cv==[],'zero new F')
ck((B/(P+'-coverage.csv')).read_text().splitlines()[0]==(L/'coverage.csv').read_text().splitlines()[0],'standard coverage header')
inv=list(csv.DictReader((B/(P+'-inventory.csv')).open()));ck(len(inv)==33,'33 directory rows')
for x in inv:
 ck(x['product_id']=='M-056' and x['visual_status']=='not_observed','inventory current scope '+x['inventory_id'])
 ck(set(filter(None,x['evidence_ids'].split(';')))<=set(ef),'inventory E mapping '+x['inventory_id'])
 ck(set(filter(None,x['flow_ids'].split(';')))<=set(ff),'inventory F mapping '+x['inventory_id'])
for f in [B/(P+'-coverage.csv'),B/(P+'-inventory.csv')]:ck(b'\r' not in f.read_bytes() and b'\\r\\n' not in f.read_bytes(),'CSV normal LF '+f.name)
html=(W/'parent-store.html').read_text();vers=load(W/'store-version-index.json');ck(len(vers)==25 and vers[0]['primarySubtitle']=='1.25.1','25 versions/1.25.1')
ck('datetime="2026-09-25"' in html and 'iOS 13.5' in html,'date attribute and compatibility in raw')
meta=load(W/'parent-store.html.capture.json');ck(meta['status']==200 and sha(R/meta['raw_work_path'])==meta['raw_sha256'],'fresh capture metadata')
d=load(W/'discovery.json');ck(len(d['queries'])==20 and d['usable_new_operation_evidence']==0,'bounded query record')
ck(d['new_images_observed']==0 and d['videos_watched_or_downloaded']==0,'no new media observations')
for f in [L/'cases/M-056-xiaoyuan-hardware.md',L/'artifacts'/P/'source-notes.md']:
 s=f.read_text();ck('待root' not in s and '主表未改' not in s,'neutral formal prose '+f.name)
 for target in re.findall(r'\]\(([^)]+)\)',s):
  if not re.match(r'https?://',target):ck((f.parent/target).exists(),'local link '+target)
ck('5 个 D1' in (L/'cases/M-056-xiaoyuan-hardware.md').read_text(),'depth unchanged case')
if (W/'manifest.json').exists():
 for row in load(W/'manifest.json')['files']:ck(sha(R/row['path'])==row['sha256'],'frozen '+row['path'])
result={'status':'PASS' if all(x['pass'] for x in checks) else 'FAIL','assertions':len(checks),'failed':[x for x in checks if not x['pass']],'new_E':0,'new_F':0,'old_F_fields':16,'additional_captures':3,'old_E_metadata_fields':3,'inventory_rows':33,'state':'read-only check; before-dependent: do not rerun after root merge'}
print(json.dumps(result,ensure_ascii=False,indent=2));sys.exit(0 if result['status']=='PASS' else 1)
