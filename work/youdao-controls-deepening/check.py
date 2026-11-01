from pathlib import Path
import csv,json,re,hashlib
R=Path('.');L=R/'docs/research/interaction-library';W=R/'work/youdao-controls-deepening';A=L/'artifacts/youdao-controls-deepening';checks=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def ok(v,s):
 if not v:raise AssertionError(s)
 checks.append(s)
case=L/'cases/M-025-youdao-pen.md';efile=L/'batches/youdao-controls-deepening-evidence.jsonl';cfile=L/'batches/youdao-controls-deepening-coverage.csv';ifile=L/'batches/youdao-controls-deepening-inventory.csv';dfile=L/'batches/youdao-controls-deepening-discovery.csv'
ok(efile.read_bytes()==b'','deliberate zero new E');ok(list(csv.DictReader(cfile.open()))==[],'header-only zero new F');ok(json.loads((W/'existing-coverage-updates.json').read_text())==[],'zero old F changes')
oldE={};oldF={}
for p in (L/'batches').glob('*evidence.jsonl'):
 if p==efile:continue
 for ln in p.read_text().splitlines():
  if ln.strip():x=json.loads(ln);oldE[x['evidence_id']]=x
for p in (L/'batches').glob('*coverage.csv'):
 if p==cfile:continue
 for x in csv.DictReader(p.open()):
  if x['product_id']=='M-025':oldF[x['flow_id']]=x
ok(len(oldF)==5 and {x['depth'] for x in oldF.values()}=={'D1','D2'},'five old flows remain');ok(not any(k.startswith('E-YDCT') for k in oldE),'candidate prefix unassigned')
inv=list(csv.DictReader(ifile.open()));disc=list(csv.DictReader(dfile.open()));ok(len(inv)==20,'20 coverage inventory rows');ok(len(disc)==8,'8 separate discovery rows')
for r in inv:
 ok(set(filter(None,r['evidence_ids'].split(';')))<=set(oldE),'inventory E resolve');ok(set(filter(None,r['flow_ids'].split(';')))<=set(oldF),'inventory F resolve')
 if r['raw_work_path']:ok((R/r['raw_work_path']).is_file(),'inventory local file exists')
adds=json.loads((W/'additional-captures-updates.json').read_text());ok(len(adds)==4,'4 old evidence records');newcaps=[]
for a in adds:
 ok(oldE[a['evidence_id']].get('additional_captures')==a['before'],'old before exact '+a['evidence_id'])
 new=a['after'][len(a['before'] or []):];newcaps+=new
 for cap in new:
  ok(cap['network_refetched'] is False and cap['visual_observed'] is False and cap['hands_on'] is False,'local reread no new observation');ok(sha(R/cap['raw_work_path'])==cap['raw_sha256'],'raw hash');ok(not cap['archived_artifact'].startswith('work/'),'artifact relative to library');ok(sha(L/cap['archived_artifact'])==cap['sha256'],'note hash')
ok(len(newcaps)==5,'5 raw captures')
for p,h in json.loads((W/'old-batch-hashes.json').read_text()).items():ok(sha(R/p)==h,'old batch unchanged '+p)
ok(set(re.findall(r'F-M025-\d+',case.read_text()))==set(oldF),'no extra task in case')
files=[case,efile,cfile,ifile,dfile,A/'source-notes.md',W/'existing-coverage-updates.csv']
for p in files:
 b=p.read_bytes();ok(b'\r' not in b and b'\\r\\n' not in b,'normal LF '+p.name);ok(all(l.rstrip()==l for l in p.read_text().splitlines()),'no trailing space '+p.name)
 if p.suffix=='.md':
  for link in re.findall(r'\]\(([^)]+)\)',p.read_text()):
   if not link.startswith(('http:','https:','#')):ok((p.parent/link.split('#')[0]).exists(),'local link '+link)
manifest={cap['raw_work_path']:cap['raw_sha256'] for cap in newcaps}
for p in [*files,W/'additional-captures-updates.json',W/'existing-coverage-updates.json']:manifest[str(p)]=sha(p)
(W/'material-hashes.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
queries=['有道 智能硬件 官方 服务支持 词典笔 帮助中心','有道词典笔2.0 官方 操作教程 收藏 复习 设置','site.youdao.com "词典笔" "帮助中心"','site.youdao.com "词典笔" "用户手册"','site.youdao.com "词典笔" "售后服务" "教程"','site.youdao.com "2.0" "词典笔" "视频" "使用"','site.ydstatic.com "dictPen2.0" "help"']
(W/'search-queries.json').write_text(json.dumps(queries,ensure_ascii=False,indent=2)+'\n')
r=dict(status='PASS',checks=len(checks),new_evidence=0,new_flows=0,old_flow_changes=0,old_evidence_records=4,additional_local_captures=5,inventory_rows=20,discovery_rows=8,new_visual_observations=0,old_batches_unchanged=True)
(W/'check-result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False))
