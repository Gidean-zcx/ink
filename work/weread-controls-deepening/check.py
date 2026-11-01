from pathlib import Path
import csv,json,re,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/weread-controls-deepening';A=L/'artifacts/weread-controls-deepening';B=L/'batches'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
b=json.loads((W/'baseline.json').read_text());checks=[]
def ok(cond,name):
 checks.append(name)
 if not cond:raise AssertionError(name)
def rows(p):return list(csv.DictReader(p.open()))
def records(p):return [json.loads(s) for s in p.read_text().splitlines()]
es=records(B/'weread-controls-deepening-evidence.jsonl');fs=rows(B/'weread-controls-deepening-coverage.csv');inv=rows(B/'weread-controls-deepening-inventory.csv');u=json.loads((W/'existing-coverage-updates.json').read_text());a=json.loads((W/'existing-evidence-additional-captures.json').read_text())
oldEs={x['evidence_id']:x for x in b['evidence']};oldFs={x['flow_id']:x for x in b['flows']};allE=set(oldEs)|{e['evidence_id'] for e in es};allF=set(oldFs)|{f['flow_id'] for f in fs}
ok(len(es)==4 and len(fs)==2 and len(inv)==32,'bounded counts 4E 2F 32inventory')
ok(len(u)==23 and len(a)==7,'23 oldF field suggestions and 7 captures')
ok(sha(W/'case-before.md')==b['case_sha256'],'case baseline intact')
ok(not b['reverse_dependencies'],'no inherited case audit dependency')
for p,h in b['old_batch_hashes'].items():ok(sha(R/p)==h,'old batch unchanged '+p)
for e in es:
 ok(e['evidence_id'] not in oldEs and e['product_id']=='M-013','new E unique '+e['evidence_id'])
 ok(e['hands_on'] is False,'not executed '+e['evidence_id'])
 ok(e['archived_artifact'].startswith('artifacts/weread-controls-deepening/'),'formal path '+e['evidence_id'])
 ok(sha(L/e['archived_artifact'])==e['sha256'],'formal SHA '+e['evidence_id'])
 ok(sha(R/e['raw_work_path'])==e['raw_sha256'],'raw SHA '+e['evidence_id'])
 for rel,h in e.get('artifact_hashes',{}).items():ok(sha(L/rel)==h,'multi asset '+rel)
 for v in e.get('visual_artifacts',[]):
  ok(sha(L/v['archived_artifact'])==v['sha256']==sha(R/v['raw_work_path']),'visual raw/formal match '+v['archived_artifact'])
 if e.get('visual_audit_path'):
  ok(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'visual audit SHA')
  ok(e['evidence_id'] in (L/e['visual_audit_path']).read_text(),'visual audit E link')
 for p in e.get('provenance_captures',[]):ok(sha(R/p['raw_work_path'])==p['raw_sha256'],'provenance SHA '+p['raw_work_path'])
other=[]
for path in B.glob('*-evidence.jsonl'):
 if path.name!='weread-controls-deepening-evidence.jsonl':other+=records(path)
other_ids={r['evidence_id'] for r in other};other_urls={r['url'] for r in other}
for e in es:
 ok(e['evidence_id'] not in other_ids,'global ID unique '+e['evidence_id'])
 ok(e['url'] not in other_urls,'new URL unique '+e['evidence_id'])
for f in fs:
 ok(f['flow_id'] not in oldFs and f['depth']=='D2' and f['hands_on']=='false','new F scope '+f['flow_id'])
 ok(set(f['evidence_ids'].split(';'))<=allE,'newF known E '+f['flow_id'])
 ok(f['visual_status']=='not_observed','Web no visual observation')
for x in u:
 current={r['flow_id']:r for r in rows(R/x['source_batch'])}[x['flow_id']]
 ok(current[x['field']]==x['before'],'before matches '+x['flow_id']+' '+x['field'])
 ok(x['after']!=x['before'],'actual field change')
 if x['field']=='evidence_ids':ok(set(x['after'].split(';'))<=allE,'old F proposed E exist')
for x in a:
 current={r['evidence_id']:r for r in records(R/x['source_batch'])}[x['evidence_id']]
 ok(current.get(x['field'],[])==x['before'],'old E before '+x['evidence_id'])
 for c in x['after']:
  ok(sha(R/c['raw_work_path'])==c['raw_sha256'],'old E raw SHA '+x['evidence_id'])
  ok(sha(L/c['archived_artifact'])==c['sha256'],'old E note SHA '+x['evidence_id'])
  if c.get('auxiliary_original'):
   t=c['auxiliary_original'];ok(sha(R/t['raw_work_path'])==t['raw_sha256'],'README raw SHA')
for r in inv:
 ok(r['product_id']=='M-013','inventory product')
 ok(set(r['evidence_ids'].split(';'))<=allE,'inventory E '+r['inventory_id'])
 if r['flow_ids']:ok(set(r['flow_ids'].split(';'))<=allF,'inventory F '+r['inventory_id'])
case=L/'cases/M-013-weread.md';body=case.read_text()
ok(set(re.findall(r'F-M013-\d{2}',body))==allF,'case all9 F and no invented F')
ok(set(re.findall(r'E-[A-Z]+-\d{2}',body))<=allE,'case E references')
for p in [case,A/'source-notes.md',A/'visual-audit.md']:
 s=p.read_text();ok(s.endswith('\n') and '\r' not in s,'Markdown LF '+p.name)
 ok(all(x==x.rstrip() for x in s.splitlines()),'no trailing space '+p.name)
 for target in re.findall(r'\[[^\]]*\]\(([^\n)]+)\)',s):
  if target.startswith(('http:','https:','#')):continue
  ok((p.parent/target).exists(),'local link '+target)
for p in [*B.glob('weread-controls-deepening-*'),W/'existing-coverage-updates.json',W/'existing-evidence-additional-captures.json']:
 data=p.read_bytes();ok(data.endswith(b'\n') and b'\r' not in data,'structured LF '+p.name)
for c in json.loads((W/'captures.json').read_text()):
 ok(c['status']==200 and not c.get('error'),'capture success '+c['name'])
 ok(sha(R/c['raw_work_path'])==c['raw_sha256'],'capture hash '+c['name'])
raw=(W/'web-entry.js').read_text();hp=(W/'homepage.html').read_text();faq=json.loads((W/'web-help-literals.json').read_text())
ok('C_fW_JUQ.js' in hp and '帮助与反馈' in hp,'homepage provenance')
ok(len(faq)==4 and all(x['question'] in raw for x in faq),'four actual help questions')
for x in faq:ok(x['question'] in raw[max(0,x['character_offset']-80):x['character_offset']+120],'FAQ exact offset '+str(x['character_offset']))
article=(W/'ai-rag.txt').read_text();ok('2024-07-01' in article and '原始发表：2024-06-28' in article,'both article dates retained')
ok('腾讯云大数据官方账号' in (W/'author.txt').read_text(),'author profile self-description')
for version,date in [('10.2.3','2026-10-26'),('10.2.2','2026-09-17'),('10.2.0','2026-06-30'),('10.1.0','2026-03-27'),('9.3.0','2025-04-29')]:
 ok(re.search(re.escape(version)+r'</span>\s*<time datetime="'+date+r'"',(W/'appstore.html').read_text()) is not None,'HTML version date '+version)
print(json.dumps({'status':'PASS','checks':len(checks),'new_E':len(es),'new_F':len(fs),'inventory':len(inv),'old_F_fields':len(u),'additional_captures':len(a),'static_images':3,'old_batch_unchanged':True,'hands_on':False},ensure_ascii=False))
