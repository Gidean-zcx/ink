from pathlib import Path
import csv, json, hashlib, re

ROOT=Path('/home/workspace/reader'); LIB=ROOT/'docs/research/interaction-library'; W=ROOT/'work/bbdc-controls-deepening'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
checks=[]
def check(condition,label):
 checks.append({'check':label,'pass':bool(condition)})
 if not condition:print('FAIL',label)
base=json.loads((W/'baseline.json').read_text())
evidence={}; flows={}
for p in (LIB/'batches').glob('*-evidence.jsonl'):
 for line in p.read_text().splitlines():
  e=json.loads(line);evidence[e['evidence_id']]=e
for p in (LIB/'batches').glob('*-coverage.csv'):
 for row in csv.DictReader(p.open()):flows[row['flow_id']]=row
for path,expected in base['old_batch_hashes'].items():check(sha(ROOT/path)==expected,'old batch unchanged '+path)
check(sha(W/'case-before.md')==base['case_sha256'],'case baseline preserved')
check(not base['prefix_collisions'],'new prefix unused in whole library')
for e in evidence.values():
 for k in ['visual_audit_path','operation_audit_path']:
  check(e.get(k)!='cases/M-018-bbdc.md','no reverse case audit dependency '+e['evidence_id']+' '+k)
for u in json.loads((W/'existing-coverage-updates.json').read_text()):
 check(flows[u['flow_id']][u['field']]==u['before'],'old flow before '+u['flow_id']+' '+u['field'])
 check(u['field'] not in ['depth','hands_on','visual_status'],'no depth/observation inflation '+u['flow_id']+' '+u['field'])
for u in json.loads((W/'additional-captures-updates.json').read_text()):
 check(evidence[u['evidence_id']].get('additional_captures')==u['before'],'old capture before '+u['evidence_id'])
 for c in u['after']:
  rp=ROOT/c['raw_work_path'];ap=LIB/c['archived_artifact']
  check(rp.is_file() and sha(rp)==c['raw_sha256'],'raw hash '+str(rp.relative_to(ROOT)))
  check(not c['archived_artifact'].startswith('work/'),'formal archive path '+u['evidence_id'])
  check(ap.is_file() and sha(ap)==c['sha256'],'archive hash '+u['evidence_id'])
  check(c['hands_on'] is False and c['visual_observed'] is False,'no product/image actions '+u['evidence_id'])
inv=list(csv.DictReader((LIB/'batches/bbdc-controls-deepening-inventory.csv').open()))
check(len(inv)==34,'34 distinct inventory entries')
check(len({x['item_id'] for x in inv})==34,'inventory unique IDs')
for row in inv:
 check(bool(row['gaps'] and row['next_evidence']),'inventory gap '+row['item_id'])
 for eid in filter(None,row['evidence_ids'].split(';')):check(eid in evidence,'inventory evidence '+row['item_id']+' '+eid)
 for fid in filter(None,row['flow_ids'].split(';')):check(fid in flows and flows[fid]['product_id']=='M-018','inventory flow '+row['item_id']+' '+fid)
 if row['raw_work_path']:check((ROOT/row['raw_work_path']).is_file(),'inventory raw '+row['item_id'])
check((LIB/'batches/bbdc-controls-deepening-evidence.jsonl').read_text()=='','zero new E')
newc=LIB/'batches/bbdc-controls-deepening-coverage.csv'
check(not list(csv.DictReader(newc.open())),'zero new F')
check(newc.read_text().strip().split(',')==list(next(iter(flows.values()))),'coverage standard header')
case=LIB/'cases/M-018-bbdc.md';text=case.read_text()
check(len(set(re.findall(r'F-M018-\d+',text)))==5,'case still five F')
check('尝试修复' in text and '100 词/组' in text,'attempt fix and group boundary present')
check('976 字节脚本壳' in text and '搜索索引' in text,'policy acquisition distinction')
for ref in re.findall(r'\]\(([^)]+)\)',text):
 if not re.match(r'https?://',ref):check((case.parent/ref).exists(),'case local link '+ref)
versions=json.loads((W/'appstore-version-records.json').read_text())
check(len(versions)==25,'25 raw version records, not task count')
check(versions[0]['primarySubtitle']=='5.11.6' and 'Sep 18 2026' in versions[0]['secondarySubtitle'],'embedded release date')
check('尝试修复' in versions[0]['text'],'raw attempted correction fix')
check((W/'privacy.html').stat().st_size==976,'privacy shell byte count')
check('账号设置' not in (W/'privacy.html').read_text(),'policy body absent from raw HTML')
search=json.loads((W/'privacy-web-search-response.json').read_text())
for phrase in ['2026 年 08 月 28 日','2026 年 09 月 04 日','账号设置','账号信息','老用户','我的学习','我的数据']:check(phrase in search,'indexed policy text '+phrase)
formal=[case,*sorted((LIB/'batches').glob('bbdc-controls-deepening-*')),*sorted((LIB/'artifacts/bbdc-controls-deepening').glob('*'))]
for p in formal+[W/'existing-coverage-updates.csv']:
 data=p.read_bytes();check(b'\r' not in data,'LF only '+p.name)
 check(b'\\r\\n' not in data,'no literal CRLF '+p.name)
 check(all(line.rstrip()==line for line in p.read_text().splitlines()),'no trailing whitespace '+p.name)
result={'passed':sum(x['pass'] for x in checks),'total':len(checks),'failures':[x for x in checks if not x['pass']],'formal_hashes':{str(p.relative_to(ROOT)):sha(p) for p in formal},'checks':checks}
(W/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(f"{result['passed']}/{result['total']} PASS")
raise SystemExit(bool(result['failures']))
