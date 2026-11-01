from pathlib import Path
import csv,json,re,hashlib,collections
W=Path('work/kobo-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def aid(u):
 m=re.search(r'/articles/(\d+)',u);return m.group(1) if m else u
E=[json.loads(x) for x in (B/'kobo-controls-deepening-evidence.jsonl').read_text().splitlines()]
F=list(csv.DictReader(open(B/'kobo-controls-deepening-coverage.csv')));I=list(csv.DictReader(open(B/'kobo-controls-deepening-inventory.csv')))
ME={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};MF={f['flow_id']:f for f in csv.DictReader(open(L/'coverage.csv'))}
assert len(E)==12 and len(F)==2 and len(I)==141
assert len({i['inventory_id'] for i in I})==141
assert all(e['evidence_id'] not in ME for e in E),'new evidence already merged or collision'
assert all(f['flow_id'] not in MF for f in F),'new flow already merged or collision'
ve=set(ME)|{e['evidence_id'] for e in E};vf=set(MF)|{f['flow_id'] for f in F}
for e in E:
 assert not e['hands_on'] and re.match(r'^https?://',e['url'])
 assert sha(e['raw_work_path'])==e['raw_sha256']
 assert not Path(e['archived_artifact']).is_absolute() and not e['archived_artifact'].startswith('work/')
 assert sha(L/e['archived_artifact'])==e['sha256']
 matches=[x['evidence_id'] for x in ME.values() if aid(x.get('url',''))==aid(e['url'])]
 assert not matches or e.get('derived_from') in matches
 if e['visual_observed']:assert e['access_mode']=='official_static_image_downloaded_and_viewed' and e['derived_from'] in ve
 else:assert sha(e['extracted_work_path'])==e['extracted_sha256']
assert sum(e['visual_observed'] for e in E)==2
for f in F:
 assert f['hands_on']=='false' and f['depth']=='D2' and f['visual_status']=='not_observed'
 assert set(f['evidence_ids'].split(';'))<=ve
for r in I:
 assert set(filter(None,r['evidence_ids'].split(';')))<=ve
 assert set(filter(None,r['flow_ids'].split(';')))<=vf
 assert r['coverage_status'] in ('body_read_this_batch','prior_evidence_not_reread','index_only','other_model_accessory_out_of_scope','body_read_partial','static_control_only')
assert collections.Counter(i['domain'] for i in I)=={'在线指南链接':74,'Reading栏目链接':28,'阅读菜单控件':14,'静态设置项':5,'暖光设置项':4,'音频文档章节':11,'补充正文':5}
u=json.loads((W/'old-coverage-updates.json').read_text());assert len(u)==4 and sum(len(x['changed_fields']) for x in u)==18
for x in u:
 assert MF[x['flow_id']]==x['before'],('old F before mismatch',x['flow_id'])
 assert {k:{'before':x['before'][k],'after':v} for k,v in x['after'].items() if v!=x['before'][k]}==x['changed_fields']
 assert x['after']['hands_on']=='false' and set(x['after']['evidence_ids'].split(';'))<=ve
a=json.loads((W/'old-evidence-additional-captures.json').read_text());assert len(a)==4
for x in a:
 assert ME[x['evidence_id']].get('additional_captures',[])==x['before'],('old E before mismatch',x['evidence_id'])
 assert x['after']==x['before']+[x['append']]
 p=x['append'];assert sha(p['raw_work_path'])==p['raw_sha256'] and sha(p['extracted_work_path'])==p['extracted_sha256'] and sha(L/p['archived_artifact'])==p['sha256']
for p in [B/'kobo-controls-deepening-coverage.csv',B/'kobo-controls-deepening-inventory.csv']:
 b=p.read_bytes();assert b'\r' not in b and b'\\r\\n' not in b
 n=len(list(csv.DictReader(b.decode().splitlines())));assert b.count(b'\n')==n+1
 print(p.name,'LF',b.count(b'\n'),'DictReader',n,'literal_backslash_rn',b.count(b'\\r\\n'))
case=L/'cases/M-002-kobo.md';s=case.read_text();ids=re.findall(r'^\| (F-M002-\d+) ',s,re.M);assert len(ids)==26 and len(set(ids))==26
for p in [case,L/'artifacts/kobo-controls-deepening/source-notes.md']:
 for target in re.findall(r'\]\((/[^)]+)\)',p.read_text()):assert Path(target).exists(),target
for x in json.loads((W/'formal-manifest.json').read_text()):assert sha(x['path'])==x['sha256'],x['path']
for c in json.loads((W/'captures.json').read_text()):
 assert c['http_status']==200 and c['text_chars']>0
 assert sha(c['raw_work_path'])==c['raw_sha256'] and sha(c['extracted_work_path'])==c['extracted_sha256']
for c in json.loads((W/'image-captures.json').read_text()):assert c['http_status']==200 and sha(c['raw_work_path'])==c['raw_sha256']
assert json.loads((W/'reverse-dependencies.json').read_text())==[]
# Detect newly introduced audit dependencies, not coverage's intended case_path column.
for e in ME.values():
 for k,v in e.items():
  if isinstance(v,str) and ('artifact' in k or 'audit' in k):assert 'M-002-kobo.md' not in v,(e['evidence_id'],k)
print('PASS 12E/2F/141 inventory; 4 old F/18 fields; 4 old E captures; 2 static images; all hashes, links, scope and pre-merge before assertions')
