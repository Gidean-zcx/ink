from pathlib import Path
import csv,json,hashlib,re
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/research/batch44-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
base=json.load(open(w/'before-manifest.json'));cur={}
for p in [root/'README.md',root/'AGENTS.md',*(root/'docs').rglob('*')]:
 if p.is_file():cur[str(p.relative_to(root))]=sha(p)
added=sorted(set(cur)-set(base));deleted=sorted(set(base)-set(cur));modified=sorted(k for k in set(base)&set(cur) if base[k]!=cur[k])
E={x['evidence_id']:x for x in map(json.loads,(lib/'evidence.jsonl').read_text().splitlines())};F={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
# RP current/before
rp=list(csv.DictReader((lib/'reference-points.csv').open())); expected_new={f'RP-M004-{i:02d}' for i in range(1,11)}|{f'RP-M010-{i:02d}' for i in range(1,3)}; newrp=[r for r in rp if r['reference_id'] in expected_new]
rpm=[]
for r in rp:
 for x in r['flow_ids'].split(';'):
  if x not in F:rpm.append([r['reference_id'],'F',x])
 for x in r['evidence_ids'].split(';'):
  if x not in E:rpm.append([r['reference_id'],'E',x])
 p=lib/r['artifact_path']
 if not p.exists():rpm.append([r['reference_id'],'artifact',str(p)])
# batch
bev=[json.loads(x) for x in (lib/'batches/boox-system-content-deepening-evidence.jsonl').read_text().splitlines() if x.strip()]
bfl=list(csv.DictReader((lib/'batches/boox-system-content-deepening-coverage.csv').open()))
sc=json.load(open(lib/'artifacts/boox-system-content-deepening/source-captures.json'));sources=sc['sources']
articles=[str(x['article_id']) for x in sources];img_hashes=[]
for x in bev:
 for p,d in x.get('artifact_hashes',{}).items():
  if Path(p).suffix.lower() in {'.png','.jpg','.jpeg','.webp'}:img_hashes.append((x['evidence_id'],p,d))
# unique image paths, verify
uniq={p:(eid,d) for eid,p,d in img_hashes};bad=[]
for p,(eid,d) in uniq.items():
 q=lib/p
 if not q.exists() or sha(q)!=d:bad.append([eid,p])
# brief links
brieftext=(root/'docs/briefs/README.md').read_text();brief_links=[x for x in re.findall(r'\[[^\]]+\]\(([^)]+\.md)\)',brieftext) if re.search(r'/0(?:0[1-9]|[1-3][0-9]|4[0-4])-',x)]
rootread=(root/'README.md').read_text()
problems=[]
if len(rp)!=68 or len(newrp)!=12 or len({r['reference_id'] for r in rp})!=68 or any(not(v or '').strip() for r in rp for v in r.values()) or rpm:problems.append('RP')
if len(bev)!=25 or len(bfl)!=7 or any(r['depth']!='D2' or r['hands_on']!='false' for r in bfl) or any(x['hands_on'] is not False for x in bev):problems.append('batch E/F')
if len(sources)!=31 or len(set(articles))!=31:problems.append('source article count')
if len(uniq)!=7 or bad:problems.append('image hashes')
if len(brief_links)!=44 or len(set(brief_links))!=44 or '044-system-controls-and-evidence-scope.md' not in rootread or '043-issue-reading-and-resume.md' in rootread:problems.append('brief indexes')
req=sha(root/'docs/requirements.md')
if req!='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087':problems.append('requirements')
out={'status':'PASS' if not problems else 'FAIL','formal_diff':{'baseline_files':len(base),'current_files':len(cur),'added_count':len(added),'modified_count':len(modified),'deleted_count':len(deleted),'added':added,'modified':modified,'deleted':deleted},'requirements_sha256':req,'rp':{'rows':len(rp),'new_rows':len(newrp),'new_ids':[r['reference_id'] for r in newrp],'unique':len({r['reference_id'] for r in rp})==len(rp),'empty_cells':[[r['reference_id'],k] for r in rp for k,v in r.items() if not(v or '').strip()],'missing':rpm},'boox_batch':{'evidence':len(bev),'flows':len(bfl),'depths':dict(__import__('collections').Counter(r['depth'] for r in bfl)),'flow_hands_on':dict(__import__('collections').Counter(r['hands_on'] for r in bfl)),'evidence_hands_on':dict(__import__('collections').Counter(str(x['hands_on']).lower() for x in bev)),'source_articles':len(sources),'unique_article_ids':len(set(articles)),'unique_image_artifacts':len(uniq),'image_hash_errors':bad},'briefs':{'brief_links':len(brief_links),'unique_links':len(set(brief_links)),'root_has_044': '044-system-controls-and-evidence-scope.md' in rootread,'root_has_043_direct': '043-issue-reading-and-resume.md' in rootread,'brief_index_has_043':'043-issue-reading-and-resume.md' in brieftext},'problems':problems}
(w/'worker-validation.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'status':out['status'],'diff':[len(added),len(modified),len(deleted)],'rp':[len(rp),len(newrp)],'boox':[len(bev),len(bfl),len(sources),len(uniq)],'briefs':len(brief_links),'problems':problems},ensure_ascii=False));raise SystemExit(bool(problems))
