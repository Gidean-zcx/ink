from pathlib import Path
import csv,json,re,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/toutiao-controls-deepening';A=L/'artifacts/toutiao-controls-deepening';B=L/'batches'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=lambda p:list(csv.DictReader(p.open()))
records=lambda p:[json.loads(s) for s in p.read_text().splitlines()]
b=json.loads((W/'baseline.json').read_text());count=0
def ok(c,n):
 global count
 count+=1
 if not c:raise AssertionError(n)
es=records(B/'toutiao-controls-deepening-evidence.jsonl');fs=rows(B/'toutiao-controls-deepening-coverage.csv');inv=rows(B/'toutiao-controls-deepening-inventory.csv');idx=rows(A/'creator-help-index.csv');u=json.loads((W/'existing-coverage-updates.json').read_text());a=json.loads((W/'existing-evidence-additional-captures.json').read_text())
oldEs={r['evidence_id']:r for r in b['evidence']};oldFs={r['flow_id']:r for r in b['flows']};allEs=set(oldEs)|{r['evidence_id'] for r in es};allFs=set(oldFs)|{r['flow_id'] for r in fs}
ok((len(es),len(fs),len(inv),len(idx),len(u),len(a))==(3,1,22,73,18,2),'counts')
ok(not b['prefix_collisions'] and not b['reverse_dependencies'],'baseline prefix and case audit')
ok(sha(W/'case-before.md')==b['case_sha256'],'baseline case')
for p,h in b['old_batch_hashes'].items():ok(sha(R/p)==h,'old batch unchanged '+p)
other=[]
for p in B.glob('*-evidence.jsonl'):
 if p.name!='toutiao-controls-deepening-evidence.jsonl':other+=records(p)
otherIds={r['evidence_id'] for r in other};otherUrls={r['url'] for r in other}
for r in es:
 ok(r['evidence_id'] not in otherIds,'new E unique')
 ok(r['url'] not in otherUrls,'new URL unique')
 ok(r['product_id']=='M-052' and r['supersedes'] is None,'new E scope')
 ok(r['hands_on'] is False and r['visual_observed'] is False,'no operations or visuals')
 ok(r['archived_artifact'].startswith('artifacts/toutiao-controls-deepening/'),'formal library relative')
 ok(sha(L/r['archived_artifact'])==r['sha256'],'formal hash')
 ok(sha(R/r['raw_work_path'])==r['raw_sha256'],'raw hash')
for r in fs:
 ok(r['flow_id']=='F-M052-07' and r['depth']=='D2' and r['visual_status']=='not_observed' and r['hands_on']=='false','new F policy-only')
 ok(set(r['evidence_ids'].split(';'))<=allEs,'new F references')
for r in inv:
 ok(r['product_id']=='M-052' and set(r['evidence_ids'].split(';'))<=allEs,'inventory E')
 if r['flow_ids']:ok(set(r['flow_ids'].split(';'))<=allFs,'inventory F')
ok(len({r['source_id'] for r in idx})==73,'creator IDs unique')
ok(sum(r['read_scope']=='one_faq_body_read' for r in idx)==1,'one creator body only')
ok([r['source_id'] for r in idx if r['read_scope']=='one_faq_body_read']==['282'],'only selected body')
ok(all(r['scope']=='creator_help_not_reader_menu' for r in idx),'creator scope not reader')
for x in u:
 r={r['flow_id']:r for r in rows(R/x['source_batch'])}[x['flow_id']]
 ok(r[x['field']]==x['before'],'old F before '+x['flow_id']+' '+x['field'])
 ok(x['after']!=x['before'],'F changed field')
 if x['field']=='evidence_ids':ok(set(x['after'].split(';'))<=allEs,'F after E reference')
for x in a:
 r={r['evidence_id']:r for r in records(R/x['source_batch'])}[x['evidence_id']]
 ok(r.get(x['field'],[])==x['before'],'old E before')
 for c in x['after']:
  ok(sha(R/c['raw_work_path'])==c['raw_sha256'],'old E raw hash')
  ok(sha(L/c['archived_artifact'])==c['sha256'],'old E note hash')
case=L/'cases/M-052-toutiao.md';s=case.read_text()
ok(set(re.findall(r'F-M052-\d{2}',s))==allFs,'case7 F')
ok(set(re.findall(r'E-[A-Z]+-\d{2}',s))<=allEs,'case E refs')
for p in [case,A/'source-notes.md']:
 t=p.read_text();ok(t.endswith('\n') and '\r' not in t,'markdown LF')
 ok(all(l==l.rstrip() for l in t.splitlines()),'trailing spaces')
 for link in re.findall(r'\[[^\]]*\]\(([^\n)]+)\)',t):
  if not link.startswith(('http:','https:','#')):ok((p.parent/link).exists(),'local link '+link)
for p in [*B.glob('toutiao-controls-deepening-*'),A/'creator-help-index.csv',W/'existing-coverage-updates.json',W/'existing-evidence-additional-captures.json']:
 t=p.read_bytes();ok(t.endswith(b'\n') and b'\r' not in t,'structured LF '+p.name)
for c in json.loads((W/'captures.json').read_text()):
 ok(c['status']==200 and not c.get('error'),'GET status '+c['name'])
 ok(sha(R/c['raw_work_path'])==c['raw_sha256'],'GET SHA '+c['name'])
ok('您需要允许该网站执行 JavaScript' in (W/'homepage.txt').read_text(),'homepage shell recorded')
body=(W/'basic-service.txt').read_text();ok('更新日期：2024年5月6日' in body and '生效日期：2024年5月13日' in body,'historical dates')
ok('永久清除历史阅读兴趣' in body and '重新记录你的历史兴趣' in body,'permanent label not stopping new learning')
ok('操作、使用的行为信息' in (W/'personal-management.txt').read_text(),'undated guide broader wording')
ok('2025-02-21 13:21:53' in (W/'creator-help.txt').read_text(),'creator source date')
store=(W/'appstore.html').read_text();ok(re.search(r'18\.8\.0</span>\s*<time datetime="2026-10-22"',store) is not None,'iOS store exact date')
print(json.dumps({'status':'PASS','checks':count,'new_E':3,'new_F':1,'reader_inventory':22,'creator_index':73,'creator_body_read':1,'old_F_fields':18,'old_E_captures':2,'images_observed':0,'old_batch_unchanged':True,'hands_on':False},ensure_ascii=False))
