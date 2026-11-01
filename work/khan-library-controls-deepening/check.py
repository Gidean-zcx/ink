from pathlib import Path
import json,csv,re,hashlib,sys
R=Path('/home/workspace/reader');W=R/'work/khan-library-controls-deepening';L=R/'docs/research/interaction-library';B=L/'batches';P='khan-library-controls-deepening';checks=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def ec(p):return {x['evidence_id']:x for l in p.read_text().splitlines() if l.strip() for x in [json.loads(l)]}
def fc(p):return {x['flow_id']:x for x in csv.DictReader(p.open())}
def ck(ok,s):
 checks.append((bool(ok),s))
 if not ok:print('FAIL',s)
ef={e['evidence_id']:e for e in load(W/'evidence-before.json')};ff={f['flow_id']:f for f in load(W/'coverage-before.json')};base=load(W/'baseline.json');ne=ec(B/(P+'-evidence.jsonl'));nf=fc(B/(P+'-coverage.csv'));me=ec(L/'evidence.jsonl');mf=fc(L/'coverage.csv')
ck(sha(W/'case-before.md')==base['case_sha256'],'case baseline hash');ck(not base['visual_case_dependencies'],'no case visual dependency')
for k,v in ef.items():ck(me[k]==v,'master old E '+k)
for k,v in ff.items():ck(mf[k]==v,'master old F '+k)
oldbatchE={};oldbatchF={}
for p in B.glob('*-evidence.jsonl'):
 if p.name!=P+'-evidence.jsonl':oldbatchE.update(ec(p))
for p in B.glob('*-coverage.csv'):
 if p.name!=P+'-coverage.csv':oldbatchF.update(fc(p))
for k,v in ef.items():ck(oldbatchE[k]==v,'old batch E '+k)
for k,v in ff.items():ck(oldbatchF[k]==v,'old batch F '+k)
for e in ef.values():
 if e.get('raw_work_path') and e.get('raw_sha256'):ck(sha(R/e['raw_work_path'])==e['raw_sha256'],'old raw '+e['evidence_id'])
 if e.get('visual_audit_path'):ck(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'old visual audit '+e['evidence_id'])
for name,key in [('existing-coverage-updates.json','flow_id'),('existing-evidence-field-updates.json','evidence_id'),('existing-evidence-additional-captures.json','evidence_id')]:
 seen=set()
 for p in load(W/name):
  ident=(p[key],p['field']);ck(ident not in seen,'unique proposal '+str(ident));seen.add(ident)
  row=ff[p[key]] if key=='flow_id' else ef[p[key]];ck(p['before']==row.get(p['field'],[] if p['field']=='additional_captures' else None),'before '+str(ident));ck(p['before']!=p['after'],'changed proposal '+str(ident))
  owner=(fc if key=='flow_id' else ec)(R/p['source_batch']);ck(owner[p[key]]==row,'owner baseline '+str(ident))
  if p['field']=='additional_captures':
   ck(p['after'][:-1]==p['before'],'append only');c=p['after'][-1];ck(c['capture_url']==row['url'],'same URL reuse');ck(sha(R/c['raw_work_path'])==c['raw_sha256'],'old capture raw');ck(sha(L/c['archived_artifact'])==c['sha256'],'old capture note')
ck(len(ne)==11,'11 new E');ck(len(nf)==2,'2 new F');ck(not(set(ne)&set(me)),'new E unique');ck(not(set(nf)&set(mf)),'new F unique')
for eid,e in ne.items():
 ck(e['supersedes'] is None and e['hands_on'] is False,'scope '+eid);ck(e['url'] not in {x['url'] for x in me.values()},'new URL '+eid)
 ck(sha(R/e['raw_work_path'])==e['raw_sha256'],'new raw '+eid);ck(not e['archived_artifact'].startswith('work/'),'relative artifact '+eid);ck(sha(L/e['archived_artifact'])==e['sha256'],'formal hash '+eid)
 if e.get('visual_observed'):
  ck(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'new visual audit '+eid)
  for p,h in e['artifact_hashes'].items():ck(sha(L/p)==h,'new image '+p)
 for c in e.get('additional_captures',[]):ck(sha(R/c['raw_work_path'])==c['raw_sha256'],'directory raw '+c['capture_url'])
for fid,f in nf.items():ck(f['depth']=='D2' and f['hands_on']=='false','new flow doc-only '+fid);ck(set(f['evidence_ids'].split(';'))<=set(ne),'new flow E '+fid)
inv=list(csv.DictReader((B/(P+'-inventory.csv')).open()));dc=load(W/'directory-counts.json');ck(len(inv)==130 and dc['article_entries']==112 and dc['control_rows']==18,'directory counts');ck(len({x['inventory_id'] for x in inv})==len(inv),'inventory IDs unique')
for row in inv:
 ck(set(filter(None,row['evidence_ids'].split(';')))<=set(ef)|set(ne),'inventory E '+row['inventory_id']);ck(set(filter(None,row['flow_ids'].split(';')))<=set(ff)|set(nf),'inventory F '+row['inventory_id'])
ck(sum(x['node_type']=='help_article_index' for x in inv)==112,'112 index entries');ck(any(x['body_status']=='index_only' for x in inv),'unread title distinction')
for p in [B/(P+'-coverage.csv'),B/(P+'-inventory.csv')]:ck(b'\r' not in p.read_bytes() and b'\\r\\n' not in p.read_bytes(),'normal LF '+p.name)
ck((B/(P+'-coverage.csv')).read_text().splitlines()[0]==(L/'coverage.csv').read_text().splitlines()[0],'coverage header')
for name in ['mute.png','look-at-my-map.png']:
 meta=load(W/(name+'.capture.json'));ck(meta['status']==200 and sha(W/name)==meta['raw_sha256'],'image raw '+name);ck(sha(L/'artifacts'/P/name)==meta['raw_sha256'],'image archive '+name)
for p in [L/'cases/M-030-khan-kids.md',L/'artifacts'/P/'source-notes.md']:
 s=p.read_text();ck('待root合并' not in s,'neutral prose '+p.name)
 for link in re.findall(r'\]\(([^)]+)\)',s):
  if not link.startswith(('https://','http://')):ck((p.parent/link).exists(),'local link '+link)
case=(L/'cases/M-030-khan-kids.md').read_text();ck(set(re.findall(r'F-M030-\d+',case))==set(ff)|set(nf),'13 case task IDs')
ck('两种措辞并列' in case and '重播上句' in case and '历史' in case,'scope conflicts retained')
if (W/'manifest.json').exists():
 for x in load(W/'manifest.json')['files']:ck(sha(R/x['path'])==x['sha256'],'frozen '+x['path'])
result={'status':'PASS' if all(x[0] for x in checks) else 'FAIL','assertions':len(checks),'failures':[x[1] for x in checks if not x[0]],'new_E':len(ne),'new_F':len(nf),'old_F_fields':len(load(W/'existing-coverage-updates.json')),'old_E_capture_fields':len(load(W/'existing-evidence-additional-captures.json')),'old_E_metadata_fields':len(load(W/'existing-evidence-field-updates.json')),'inventory_rows':len(inv),'before_dependent':'do not rerun after root merges old rows'}
print(json.dumps(result,ensure_ascii=False,indent=2));sys.exit(0 if result['status']=='PASS' else 1)
