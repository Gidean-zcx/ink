from pathlib import Path
import csv,json,re,hashlib,difflib,collections
w=Path('work/shanling-controls-deepening');l=Path('docs/research/interaction-library');pre='shanling-controls-deepening';errors=[]
def ck(b,msg):
 if not b:errors.append(msg)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
me=[json.loads(x) for x in (l/'evidence.jsonl').read_text().splitlines()];em={x['evidence_id']:x for x in me};new=[json.loads(x) for x in (l/f'batches/{pre}-evidence.jsonl').read_text().splitlines()];allids=set(em)|{x['evidence_id'] for x in new};ck(not set(em)&{x['evidence_id'] for x in new},'evidence collision')
for x in new:
 ck(x['hands_on'] is False,'hands_on');ck(sha(x['raw_work_path'])==x['raw_sha256'],'rawhash '+x['evidence_id']);ck(sha(l/x['archived_artifact'])==x['sha256'],'archivehash '+x['evidence_id']);ck(not x['archived_artifact'].startswith('work/'),'workarchive')
 if x.get('extracted_work_path'):ck(sha(x['extracted_work_path'])==x['extracted_sha256'],'extracted hash')
 for p,h in x.get('artifact_hashes',{}).items():ck(sha(l/p)==h,'visual hash '+p)
 dup=[z['evidence_id'] for z in me+new if z['url']==x['url'] and z['evidence_id']!=x['evidence_id']]
 ck(not dup or x.get('derived_from') in dup or any(z.get('derived_from')==x['evidence_id'] for z in new if z['evidence_id'] in dup),'duplicate nonderived URL')
rows=[x for x in csv.DictReader((l/'coverage.csv').open()) if x['product_id']=='M-063'];cm={x['flow_id']:x for x in rows};updates=json.loads((w/'old-coverage-updates.json').read_text())
for u in updates:
 ck(cm[u['flow_id']]==u['before'],'before oldF '+u['flow_id']);ck(u['changed_fields']=={k:{'before':u['before'][k],'after':u['after'][k]} for k in u['before'] if u['before'][k]!=u['after'][k]},'diff oldF')
 for e in u['after']['evidence_ids'].split(';'):ck(e in allids,'ref '+e)
for u in json.loads((w/'old-evidence-additional-captures.json').read_text()):
 ck(em[u['evidence_id']].get('additional_captures',[])==u['before'],'before oldE '+u['evidence_id']);ck(u['after']==u['before']+[u['append']],'capture append');ck(sha(u['append']['raw_work_path'])==u['append']['raw_sha256'],'capture rawhash')
csvchecks=[]
for kind,n in [('coverage',0),('inventory',114)]:
 p=l/f'batches/{pre}-{kind}.csv';b=p.read_bytes();rs=list(csv.DictReader(p.open()));ck(len(rs)==n,'count '+kind);ck(b.count(b'\n')==n+1 and b'\r' not in b and b'\\r\\n' not in b,'newline '+kind);csvchecks.append({'kind':kind,'rows':len(rs),'physical_LF':b.count(b'\n'),'literal_backslash_rn':b.count(b'\\r\\n')})
 for r in rs:
  for e in r.get('evidence_ids','').split(';'):ck(e in allids,'inventory E '+e)
  for f in r.get('flow_ids','').split(';'):
   if f:ck(f in cm,'inventory F '+f)
case=l/'cases/M-063-shanling.md';s=case.read_text();fs=set(re.findall(r'F-M063-\d{2}',s));ck(fs==set(cm),'case F count');ck(cm['F-M063-07']['depth']=='D1','format D1 retained')
for p in re.findall(r'\]\((/[^)]+)\)',s):ck(Path(p).exists(),'missing link '+p)
for e in set(re.findall(r'E-(?:SHAN|SHCD|SLCD)-\d{2}',s)):ck(e in allids,'case E '+e)
old=(w/'case-before.md').read_text();(w/'case-change.diff').write_text(''.join(difflib.unified_diff(old.splitlines(True),s.splitlines(True),fromfile='before/M-063-shanling.md',tofile='after/M-063-shanling.md')))
files=[case,*sorted((l/'batches').glob(pre+'-*')),*sorted((l/'artifacts'/pre).glob('*'))];manifest=[{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)} for p in files];(w/'formal-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
result=dict(status='FAIL' if errors else 'PASS',new_E=len(new),new_F=0,total_case_F=len(fs),old_F_rows=len(updates),old_F_fields=sum(len(u['changed_fields']) for u in updates),csv_checks=csvchecks,errors=errors);(w/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2));raise SystemExit(bool(errors))
