from pathlib import Path
import csv,json,hashlib,re,collections
W=Path('work/ak-directory-deepening');L=Path('docs/research/interaction-library');B=L/'batches'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
E=[json.loads(l) for l in (B/'ak-directory-deepening-evidence.jsonl').read_text().splitlines()]
F=list(csv.DictReader(open(B/'ak-directory-deepening-coverage.csv')))
I=list(csv.DictReader(open(B/'ak-directory-deepening-inventory.csv')))
ME={e['evidence_id']:e for e in (json.loads(l) for l in (L/'evidence.jsonl').read_text().splitlines())}
MF={e['flow_id']:e for e in csv.DictReader(open(L/'coverage.csv'))}
assert len(E)==3 and len(F)==2 and len(I)==202
assert len({e['evidence_id'] for e in E})==3
assert len({i['inventory_id'] for i in I})==202
assert all(e['evidence_id'] not in ME for e in E),'new E already merged or collision'
assert all(f['flow_id'] not in MF for f in F),'new F already merged or collision'
validE=set(ME)|{e['evidence_id'] for e in E};validF=set(MF)|{f['flow_id'] for f in F}
for e in E:
 assert e['hands_on'] is False
 assert re.match(r'^https?://',e['url'])
 assert sha(e['raw_work_path'])==e['raw_sha256']
 assert not Path(e['archived_artifact']).is_absolute() and not e['archived_artifact'].startswith('work/')
 assert sha(L/e['archived_artifact'])==e['sha256']
 for p,h in e.get('artifact_hashes',{}).items():assert sha(L/p)==h
 if e['visual_observed']:assert e['access_mode']=='local_pdf_page_render_viewed' and e['derived_from'] in validE
 else:assert sha(e['extracted_work_path'])==e['extracted_sha256']
 # URL may recur only for explicitly derived image evidence.
 dup=[x['evidence_id'] for x in ME.values() if x.get('url')==e['url']]
 assert not dup or e.get('derived_from') in dup,(e['evidence_id'],dup)
for f in F:
 assert f['depth']=='D2' and f['hands_on']=='false'
 assert f['visual_status']=='static_reference_only'
 assert set(f['evidence_ids'].split(';'))<=validE
for i in I:
 assert set(filter(None,i['evidence_ids'].split(';')))<=validE
 assert set(filter(None,i['flow_ids'].split(';')))<=validF
assert collections.Counter(i['domain'] for i in I)=={'印刷目录':36,'设置标题':30,'设置明列子项':23,'编号界面项':54,'操作菜单明列项':26,'型号支持FAQ':24,'型号支持下载索引':9}
changes=json.loads((W/'old-coverage-updates.json').read_text());assert len(changes)==4
for r in changes:
 assert MF[r['flow_id']]==r['before'],('old F before mismatch',r['flow_id'])
 diff={k:{'before':r['before'][k],'after':v} for k,v in r['after'].items() if v!=r['before'][k]}
 assert diff==r['changed_fields']
 assert r['after']['hands_on']=='false'
 assert set(r['after']['evidence_ids'].split(';'))<=validE
assert sum(len(r['changed_fields']) for r in changes)==15
adds=json.loads((W/'old-evidence-additional-captures.json').read_text());assert len(adds)==5
for r in adds:
 assert ME[r['evidence_id']].get('additional_captures',[])==r['before'],('old E before mismatch',r['evidence_id'])
 assert r['after']==r['before']+[r['append']]
 a=r['append'];assert sha(a['raw_work_path'])==a['raw_sha256']
 assert sha(a['extracted_work_path'])==a['extracted_sha256']
 assert sha(L/a['archived_artifact'])==a['sha256']
for p in [B/'ak-directory-deepening-coverage.csv',B/'ak-directory-deepening-inventory.csv']:
 b=p.read_bytes();assert b'\\r\\n' not in b and b'\r' not in b
 count=len(list(csv.DictReader(b.decode().splitlines())));assert b.count(b'\n')==count+1
 print(p.name,'actual_LF=',b.count(b'\n'),'literal_backslash_rn=',b.count(b'\\r\\n'),'DictReader_rows=',count)
case=L/'cases/M-065-astell-kern.md';s=case.read_text()
ids=re.findall(r'^\| (F-M065-\d+) \|',s,re.M);assert len(ids)==18 and len(set(ids))==18
for p in [case,L/'artifacts/ak-directory-deepening/source-notes.md']:
 for target in re.findall(r'\]\((/[^)]+)\)',p.read_text()):assert Path(target).exists(),target
for x in json.loads((W/'formal-manifest.json').read_text()):assert sha(x['path'])==x['sha256'],x['path']
for c in json.loads((W/'captures.json').read_text()):assert c['http_status']==200 and sha(c['raw_work_path'])==c['raw_sha256']
print('PASS: 3 E / 2 F / 202 inventory; 4 old F / 15 fields; 5 old E captures; 5 viewed static pages; hashes/links/schema/scope and pre-merge before assertions')
