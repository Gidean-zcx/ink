from pathlib import Path
import json,csv,hashlib,re,sys
R=Path(__file__).resolve().parents[2];W=R/'work/fenbi-practice-controls-deepening';L=R/'docs/research/interaction-library';checks=[]
def ck(ok,label):
 checks.append((bool(ok),label))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def jl(p):return [json.loads(x) for x in Path(p).read_text().splitlines() if x.strip()]
b=json.loads((W/'baseline.json').read_text());all_e=jl(L/'evidence.jsonl');all_f=list(csv.DictReader((L/'coverage.csv').open()));old_e={x['evidence_id']:x for x in all_e if x['product_id']=='M-032'};old_f={x['flow_id']:x for x in all_f if x['product_id']=='M-032'}
ck(sha(W/'case-before.md')==b['case_sha256'],'case baseline hash')
for x in b['evidence']:ck(old_e.get(x['evidence_id'])==x,'master old E unchanged '+x['evidence_id'])
for x in b['coverage']:ck(old_f.get(x['flow_id'])==x,'master old F unchanged '+x['flow_id'])
ep=L/'batches/fenbi-practice-controls-deepening-evidence.jsonl';es=jl(ep);ids={x['evidence_id'] for x in all_e};newids={x['evidence_id'] for x in es}
ck(len(es)==3 and len(newids)==3,'3 unique new E');ck(not ids&newids,'new E absent from master')
for e in es:
 eid=e['evidence_id'];ck(e['supersedes'] is None,eid+' supersedes:null');ck(e['hands_on'] is False,eid+' no operation')
 for field,hf,base in [('raw_work_path','raw_sha256',R),('archived_artifact','sha256',L),('visual_audit_path','visual_audit_sha256',L)]:
  if field in e:
   p=base/e[field];ck(p.is_file(),eid+' '+field+' exists');ck(p.is_file() and sha(p)==e[hf],eid+' '+hf)
   if field!='raw_work_path':ck(not e[field].startswith(('work/','/')),eid+' formal relative path')
 for a in e.get('additional_artifacts',[]):ck(sha(L/a['path'])==a['sha256'],eid+' additional artifact hash')
 for p in (L/'batches').glob('*evidence.jsonl'):
  if p!=ep:ck(eid not in {x['evidence_id'] for x in jl(p)},eid+' no collision '+p.name)
ups=json.loads((W/'existing-coverage-updates.json').read_text());ck(len(ups)==2,'2 old F patches');ck(sum(len(x['changes']) for x in ups)==11,'11 old F fields')
for u in ups:
 row=next(x for x in csv.DictReader((R/u['source_batch']).open()) if x['flow_id']==u['flow_id']);new=dict(row)
 for field,ch in u['changes'].items():ck(row[field]==ch['before'],u['flow_id']+' source before '+field);new[field]=ch['after']
 ck(new==u['replacement_row'],u['flow_id']+' complete replacement');ck(new['depth']==old_f[u['flow_id']]['depth'],u['flow_id']+' depth unchanged');ck(new['hands_on']=='false',u['flow_id']+' no operation')
 for ei in new['evidence_ids'].split(';'):ck(ei in ids|newids,u['flow_id']+' E '+ei)
for u in json.loads((W/'existing-evidence-additional-captures.json').read_text()):
 e=next(x for x in jl(R/u['source_batch']) if x['evidence_id']==u['evidence_id']);f=u['field'];ck((f in e)==u['before_exists'],u['evidence_id']+' before presence');ck(e.get(f,[])==u['before'],u['evidence_id']+' before capture');ck(u['after']==u['before']+u['append'],u['evidence_id']+' append only')
 for c in u['append']:ck(sha(R/c['raw_work_path'])==c['raw_sha256'],'local reread original hash');ck(sha(L/c['archived_artifact'])==c['sha256'],'local reread formal hash')
cp=L/'batches/fenbi-practice-controls-deepening-coverage.csv';ck(list(csv.DictReader(cp.open()))==[],'0 new F header only');ck(next(csv.reader(cp.open()))==list(b['coverage'][0]),'standard coverage header')
ip=L/'batches/fenbi-practice-controls-deepening-inventory.csv';ir=list(csv.DictReader(ip.open()));ck(len(ir)==21,'21 inventory rows')
for row in ir:
 for eid in row['evidence_ids'].split(';'):ck(eid in ids|newids,row['item_id']+' E exists')
 for fid in row['flow_ids'].split(';'):
  if fid:ck(fid in old_f,row['item_id']+' F exists')
for p in (cp,ip):ck(b'\\r\\n' not in p.read_bytes(),p.name+' no literal escapes');ck(b'\r' not in p.read_bytes(),p.name+' LF')
case=L/'cases/M-032-fenbi.md';s=case.read_text();ck(set(re.findall(r'F-M032-\d{2}',s))==set(old_f),'case retains exactly10 F IDs')
for lab,url in re.findall(r'\[([^]]+)\]\(([^)]+)\)',s):
 if not url.startswith(('https://','http://','#')):ck((case.parent/url.split('#')[0]).is_file(),'case link '+url)
ck('待root合并' not in s and '主表未改' not in s,'case neutral state')
for e in b['evidence']:
 if e.get('visual_audit_path'):ck('cases/' not in e['visual_audit_path'],'old audit independent of case');ck(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'old audit hash preserved')
candidates=json.loads((W/'candidate-register.json').read_text());ck(len(candidates)==8,'8 candidates no expansion');imgs=json.loads((W/'fetch-images-results.json').read_text());ck(len(imgs)==2 and [x['status'] for x in imgs]==[200,403],'image route stopped after403');ck(sum(e['visual_observed'] for e in es)==1,'1 visual source only')
if '--manifest' in sys.argv:
 m=json.loads((W/'manifest.json').read_text())
 for x in m['files']:ck(sha(R/x['path'])==x['sha256'],'manifest '+x['path'])
failed=[x[1] for x in checks if not x[0]]
print(json.dumps(dict(status='PASS' if not failed else 'FAIL',passed=sum(x[0] for x in checks),total=len(checks),failures=failed,state_changed=False),ensure_ascii=False,indent=2))
sys.exit(1 if failed else 0)
