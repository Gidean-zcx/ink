from pathlib import Path
from collections import Counter
import csv,json,hashlib,re
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/research/batch43-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
base=json.load(open(w/'before-manifest.json'))
cur={}
for p in [root/'README.md',root/'AGENTS.md',*(root/'docs').rglob('*')]:
 if p.is_file():cur[str(p.relative_to(root))]=sha(p)
added=sorted(set(cur)-set(base));deleted=sorted(set(base)-set(cur));modified=sorted(k for k in set(base)&set(cur) if base[k]!=cur[k])
E={x['evidence_id']:x for x in map(json.loads,(lib/'evidence.jsonl').read_text().splitlines())}
F={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
# before old records
be={x['evidence_id']:x for x in map(json.loads,(w/'before/docs/research/interaction-library/evidence.jsonl').read_text().splitlines())}
bf={x['flow_id']:x for x in csv.DictReader((w/'before/docs/research/interaction-library/coverage.csv').open())}
# RP
rp=list(csv.DictReader((lib/'reference-points.csv').open()));problems=[];rpm=[]
for r in rp:
 for x in r['flow_ids'].split(';'):
  if x not in F:rpm.append([r['reference_id'],'F',x])
 for x in r['evidence_ids'].split(';'):
  if x not in E:rpm.append([r['reference_id'],'E',x])
 p=lib/r['artifact_path']
 if not p.exists():rpm.append([r['reference_id'],'artifact',str(p)])
# operation
art=lib/'artifacts/time-sample-reader';b=art/'browser'
states=json.load(open(b/'observed-states.json'));actions=json.load(open(b/'trace-actions.json'));net=json.load(open(b/'network-statuses.json'));errs=json.load(open(b/'browser-errors.json'));man=json.load(open(b/'raw-capture-manifest.json'))
pngs=sorted(b.glob('*.png')); statuses=Counter(str(x.get('status')) for x in net)
hashbad=[]
for rel,d in E['E-TKSM-01']['artifact_hashes'].items():
 p=lib/rel
 if not p.exists() or sha(p)!=d:hashbad.append(rel)
# E-TFK/F old changes, expected characteristics
old_e_changed=be['E-TFK-04']!=E['E-TFK-04'];old_f_changed=bf['F-M047-02']!=F['F-M047-02']
# anonymization scan
urls=[x.get('url','') for x in net+errs]
uuid=re.compile(r'(?i)\b[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\b')
# ipt token is public source URL, allow; inspect other uuid
uuid_urls=[u for u in urls if uuid.search(u) and 'reader.emagazines.com/issue/time-for-kids-k-1/may-8-2026' not in u and 'cdn.cookielaw.org/consent/' not in u]
result={'status':'PASS','formal_diff':{'baseline_files':len(base),'current_formal_files':len(cur),'added_count':len(added),'modified_count':len(modified),'deleted_count':len(deleted),'added':added,'modified':modified,'deleted':deleted},'requirements_sha256':sha(root/'docs/requirements.md'),'requirements_match':sha(root/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087','rp':{'rows':len(rp),'fields':list(rp[0]),'unique':len({r['reference_id'] for r in rp})==len(rp),'empty':[[r['reference_id'],k] for r in rp for k,v in r.items() if not(v or '').strip()],'missing':rpm},'new':{'E-TKSM-01':E.get('E-TKSM-01'),'F-M047-06':F.get('F-M047-06')},'old_updates':{'E-TFK-04_changed':old_e_changed,'E-TFK-04':E.get('E-TFK-04'),'F-M047-02_changed':old_f_changed,'F-M047-02':F.get('F-M047-02')},'audit':{'states':len(states),'commands':len(actions),'pngs':len(pngs),'network_records':len(net),'network_statuses':dict(statuses),'browser_errors':len(errs),'manifest_states':man.get('states'),'manifest_commands':man.get('commands'),'manifest_retained_screens':len(man.get('selected_visual_captures',[])),'artifact_hash_errors':hashbad,'unexpected_uuid_urls':uuid_urls}}
if len(rp)!=56 or not result['rp']['unique'] or result['rp']['empty'] or rpm:problems.append('RP')
if not E.get('E-TKSM-01') or not F.get('F-M047-06'):problems.append('new ids')
if not old_e_changed or not old_f_changed:problems.append('old updates')
if (len(states),len(actions),len(pngs))!=(23,25,15):problems.append('audit counts')
if len(net)!=75 or dict(statuses)!={'200':32,'204':43}:problems.append('network counts')
if hashbad:problems.append('hashes')
if not result['requirements_match']:problems.append('requirements')
if uuid_urls:problems.append('unexpected uuid')
if problems:result['status']='FAIL';result['problems']=problems
(w/'worker-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'status':result['status'],'diff':[len(added),len(modified),len(deleted)],'rp':len(rp),'audit':[len(states),len(actions),len(pngs)],'network':dict(statuses),'problems':problems},ensure_ascii=False))
raise SystemExit(bool(problems))
