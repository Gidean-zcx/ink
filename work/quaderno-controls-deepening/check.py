from pathlib import Path
import json,csv,hashlib,re,collections,difflib
w=Path('work/quaderno-controls-deepening');lib=Path('docs/research/interaction-library');prefix='quaderno-controls-deepening';errs=[];out=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ck(ok,s):
 if not ok:errs.append(s)
e=[json.loads(x) for x in (lib/f'batches/{prefix}-evidence.jsonl').read_text().splitlines()];master=[json.loads(x) for x in (lib/'evidence.jsonl').read_text().splitlines()];mastermap={x['evidence_id']:x for x in master};newmap={x['evidence_id']:x for x in e};ck(not(set(newmap)&set(mastermap)),'new evidence collides with master')
for x in e:
 ck(x['hands_on'] is False,'hands_on '+x['evidence_id']);ck(x['raw_sha256']==sha(x['raw_work_path']),'rawhash '+x['evidence_id']);ck(x['sha256']==sha(lib/x['archived_artifact']),'archivehash '+x['evidence_id']);ck(not x['archived_artifact'].startswith('work/'),'work as archive')
 if 'extracted_work_path' in x:ck(x['extracted_sha256']==sha(x['extracted_work_path']),'extracthash '+x['evidence_id'])
 dup=[m['evidence_id'] for m in master if m['url']==x['url']]
 ck(not dup or x.get('derived_from') in dup,'duplicate non-derived URL '+x['evidence_id'])
 for p,h in x.get('artifact_hashes',{}).items():ck(sha(lib/p)==h,'renderhash '+p)
for kind,count in [('coverage',8),('inventory',275)]:
 p=lib/f'batches/{prefix}-{kind}.csv';b=p.read_bytes();rows=list(csv.DictReader(p.open()));ck(len(rows)==count,kind+' row count');ck(b'\\r\\n' not in b and b'\r' not in b,kind+' literal/bad newlines');ck(b.count(b'\n')==count+1,kind+' physical newline count');out.append({'file':str(p),'rows':len(rows),'physical_LF':b.count(b'\n'),'literal_backslash_rn':b.count(b'\\r\\n')})
 for x in rows:
  for eid in x.get('evidence_ids','').split(';'):
   if eid:ck(eid in mastermap or eid in newmap,'unknown E '+eid)
  if kind=='coverage':ck(x['depth']=='D2' and x['hands_on']=='false','coverage level/state')
cv={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())};u=json.loads((w/'old-coverage-updates.json').read_text())
for x in u:
 ck(cv[x['flow_id']]==x['before'],'oldF before differs '+x['flow_id']);ck(set(x['before'])==set(x['after']),'oldF schema')
 ck(x['changed_fields']=={k:{'before':x['before'][k],'after':x['after'][k]} for k in x['before'] if x['before'][k]!=x['after'][k]},'changed field diff')
for x in json.loads((w/'old-evidence-additional-captures.json').read_text()):
 ck(mastermap[x['evidence_id']].get('additional_captures',[])==x['before'],'oldE capture before '+x['evidence_id']);ck(x['after']==x['before']+[x['append']],'capture not append');ck(sha(x['append']['raw_work_path'])==x['append']['raw_sha256'],'capture rawhash')
for x in json.loads((w/'old-evidence-date-corrections.json').read_text()):
 for k,d in x['changed_fields'].items():ck(mastermap[x['evidence_id']][k]==d['before'],'date before '+x['evidence_id']+k)
case=lib/'cases/M-060-quaderno.md';s=case.read_text();flowids=set(re.findall(r'F-M060-\d{2}',s));ck(flowids=={f'F-M060-{i:02d}' for i in range(1,17)},'case flow IDs')
for target in re.findall(r'\]\((/[^)]+)\)',s):ck(Path(target).exists(),'missing local link '+target)
for eid in set(re.findall(r'E-(?:QUA|QCD)-\d{2}',s)):ck(eid in mastermap or eid in newmap,'case E missing '+eid)
before=(w/'case-before.md').read_text();(w/'case-change.diff').write_text(''.join(difflib.unified_diff(before.splitlines(True),s.splitlines(True),fromfile='before/M-060-quaderno.md',tofile='after/M-060-quaderno.md')))
files=[case,*sorted((lib/'batches').glob(prefix+'-*')),*sorted((lib/'artifacts'/prefix).glob('*'))]
manifest=[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in files];(w/'formal-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
result={'status':'PASS' if not errs else 'FAIL','new_E':len(e),'new_F':8,'total_case_F':len(flowids),'old_F_rows':len(u),'old_F_fields':sum(len(x['changed_fields']) for x in u),'csv_checks':out,'errors':errs,'state_changed':'only assigned case/new batch/artifacts and work; no device/account/product changes'}
(w/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2));raise SystemExit(bool(errs))
