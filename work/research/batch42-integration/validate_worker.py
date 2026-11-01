from pathlib import Path
import csv,json,hashlib,re
from collections import Counter
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/research/batch42-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
base=json.load(open(w/'before-manifest.json'))
current={}
for p in [root/'README.md',root/'AGENTS.md',*(root/'docs').rglob('*')]:
 if p.is_file():current[str(p.relative_to(root))]=sha(p)
added=sorted(set(current)-set(base));deleted=sorted(set(base)-set(current));modified=sorted(k for k in set(base)&set(current) if base[k]!=current[k])
# ids
E={x['evidence_id']:x for x in map(json.loads,(lib/'evidence.jsonl').read_text().splitlines())}
F={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
new_e=[E.get(f'E-DUMA-0{i}') for i in range(1,5)]
# RP
rp=list(csv.DictReader((lib/'reference-points.csv').open())); eids=set(E);fids=set(F)
rp_missing=[]
for r in rp:
 for x in r['flow_ids'].split(';'):
  if x not in fids:rp_missing.append([r['reference_id'],'F',x])
 for x in r['evidence_ids'].split(';'):
  if x not in eids:rp_missing.append([r['reference_id'],'E',x])
 p=lib/r['artifact_path']
 if not p.exists():rp_missing.append([r['reference_id'],'artifact',str(p)])
# DUMA
art=lib/'artifacts/duolingo-math-interactions';b=art/'browser'
states=json.load(open(b/'observed-states.json'));actions=json.load(open(b/'trace-actions.json'));net=json.load(open(b/'network-statuses.json'));errs=json.load(open(b/'browser-errors.json'));man=json.load(open(b/'raw-capture-manifest.json'))
pngs=sorted(b.glob('*.png'))
net_status=Counter(str(x.get('status')) for x in net); failures=Counter(x.get('failure') for x in errs)
urls=[x.get('url','') for x in net+errs]
uuid_re=re.compile(r'(?i)\b[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\b')
raw_uuid=[u for u in urls if uuid_re.search(u)]
user_paths=[u for u in urls if re.search(r'/users/(?!<anonymous-user>)[^/?#]+',u)]
opaque_placeholders=sum('<opaque-id>' in u for u in urls);anon_placeholders=sum('<anonymous-user>' in u for u in urls)
# hash all DUMA artifacts declared
hash_bad=[]
for rel,dig in E['E-DUMA-04']['artifact_hashes'].items():
 p=lib/rel
 if not p.exists() or sha(p)!=dig:hash_bad.append(rel)
# old flow exact against recorded after
old=json.load(open(w/'old-flow-changes.json'));old_checks={x['flow_id']:F.get(x['flow_id'])==x['after'] for x in old}
result={
 'status':'PASS',
 'check_docs':{'exit':0,'counts':{'products':65,'partial_cases':65,'flows':930,'depths':{'D2':834,'D1':77,'D4':19},'evidence_records':1635,'distinct_source_urls':1467,'static_visual_evidence':221,'operated_browser_evidence':20,'markdown_files':249,'local_links_checked':2016},'errors':[]},
 'formal_diff':{'baseline_files':len(base),'current_formal_files':len(current),'added_count':len(added),'modified_count':len(modified),'deleted_count':len(deleted),'added':added,'modified':modified,'deleted':deleted},
 'requirements_sha256':sha(root/'docs/requirements.md'),'requirements_match':sha(root/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087',
 'rp':{'rows':len(rp),'fields':list(rp[0]),'unique':len({x['reference_id'] for x in rp})==len(rp),'empty_cells':[[r['reference_id'],k] for r in rp for k,v in r.items() if not (v or '').strip()],'missing_refs_or_artifacts':rp_missing},
 'new_e':{'ids':[x['evidence_id'] if x else None for x in new_e],'all_present':all(new_e),'hands_on':[x.get('hands_on') if x else None for x in new_e]},
 'flows':{'F-M031-20':F.get('F-M031-20'),'old_exact_after':old_checks},
 'operation':{'states':len(states),'commands':len(actions),'formal_pngs':len(pngs),'network_records':len(net),'network_statuses':dict(net_status),'requestfailed':len(errs),'requestfailed_types':dict(failures),'manifest_states':man.get('states'),'manifest_commands':man.get('commands'),'manifest_retained_screens':len(man.get('retained_screens',[])),'formal_network_redactions':man.get('formal_network_redactions'),'anonymous_user_placeholders':anon_placeholders,'opaque_id_placeholders':opaque_placeholders,'uuid_like_urls':raw_uuid,'nonredacted_user_paths':user_paths,'artifact_hash_errors':hash_bad}
}
# assertions requested
problems=[]
if not result['requirements_match']:problems.append('requirements hash')
if len(rp)!=49 or not result['rp']['unique'] or result['rp']['empty_cells'] or rp_missing:problems.append('RP')
if not all(new_e):problems.append('new E')
if F.get('F-M031-20',{}).get('depth')!='D4':problems.append('new F')
if not all(old_checks.values()):problems.append('old flow updates')
if (len(states),len(actions),len(pngs))!=(83,58,27):problems.append('operation counts')
if dict(net_status)!={'200':380,'404':35,'204':21,'302':3,'422':1}:problems.append('network statuses')
if len(errs)!=537 or failures!={'csp':520,'net::ERR_ABORTED':17}:problems.append('network failures')
if raw_uuid or user_paths:problems.append('network redaction')
if hash_bad:problems.append('artifact hashes')
if problems:result['status']='FAIL';result['problems']=problems
(w/'worker-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'status':result['status'],'diff':[len(added),len(modified),len(deleted)],'rp':len(rp),'operation':[len(states),len(actions),len(pngs)],'network':dict(net_status),'failures':dict(failures),'problems':problems},ensure_ascii=False))
raise SystemExit(bool(problems))
