"""Read-only pre-merge document check. No network, builds, or product actions."""
from pathlib import Path
import json,csv,hashlib,io,re
ROOT=Path(__file__).resolve().parents[2];W=ROOT/'work/viwoods-controls-deepening';L=ROOT/'docs/research/interaction-library';B=L/'batches';A=L/'artifacts/viwoods-controls-deepening'
def j(p):return json.loads(p.read_text())
def jl(p):return [json.loads(x) for x in p.read_text().splitlines() if x]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p,own=False):
 b=p.read_bytes()
 if own:assert b'\r' not in b and b'\\r\\n' not in b
 r=list(csv.DictReader(io.StringIO(b.decode())));assert all(None not in x for x in r);return r
def paths(x):
 for pk,hk,base in [('raw_work_path','raw_sha256',ROOT),('extracted_work_path','extracted_sha256',ROOT),('archived_artifact','sha256',L),('visual_audit_path','visual_audit_sha256',L)]:
  if x.get(pk):
   assert not Path(x[pk]).is_absolute()
   if base==L:assert x[pk].startswith('artifacts/') and not x[pk].startswith('work/')
   assert sha(base/x[pk])==x[hk],(pk,x[pk])
baseline=j(W/'baseline.json');assert sha(W/'case-before.md')==baseline['case_sha256'];assert baseline['reverse_dependencies']==[]
for p in ['formal-manifest.json','raw-manifest.json']:
 for x in j(W/p):assert sha(ROOT/x['path'])==x['sha256'],x['path']
oe={x['evidence_id']:x for x in jl(L/'evidence.jsonl')};of={x['flow_id']:x for x in rows(L/'coverage.csv')}
ne=jl(B/'viwoods-controls-deepening-evidence.jsonl');nf=rows(B/'viwoods-controls-deepening-coverage.csv',True);iv=rows(B/'viwoods-controls-deepening-inventory.csv',True)
assert len(ne)==14 and len(nf)==3 and len(iv)==578
assert {x['evidence_id'] for x in ne}=={f'E-VWCD-{i:02}' for i in range(1,15)}
assert {x['flow_id'] for x in nf}=={f'F-M007-{i:02}' for i in range(18,21)}
assert not ({x['evidence_id'] for x in ne}&oe.keys()),'Already merged or collision'
assert not ({x['flow_id'] for x in nf}&of.keys()),'Already merged or collision'
for x in ne:
 assert x['supersedes'] is None and x['hands_on'] is False and x['product_id']=='M-007';paths(x)
 if x['visual_observed']:assert x['derived_from']=='E-VICD-01' and x['url']==oe['E-VICD-01']['url']
 else:assert x['url'] not in {z.get('url') for z in oe.values()}
assert sum(x['visual_observed'] for x in ne)==13
eids=set(oe)|{x['evidence_id'] for x in ne};fids=set(of)|{x['flow_id'] for x in nf}
for x in nf:
 assert x['depth']=='D2' and x['hands_on']=='false' and x['visual_status']=='static_manual_pages_only'
 assert set(x['evidence_ids'].split(';'))<=eids
 assert (L/x['case_path']).is_file()
ids={x['inventory_id'] for x in iv};assert len(ids)==578
for x in iv:
 assert not x['parent_id'] or x['parent_id'] in ids
 assert set(filter(None,x['evidence_ids'].split(';')))<=eids
 assert set(filter(None,x['flow_ids'].split(';')))<=fids
obs=j(A/'observed-pages.json');assert [x['file_page'] for x in obs]==list(range(2,70))
for x in obs:
 assert x['printed_page']==x['file_page']-1 and x['hands_on'] is False and x['continuous_sequence'] is False
 assert sha(ROOT/x['raw_work_path'])==x['raw_sha256']
 if x['archived_artifact']:assert sha(L/x['archived_artifact'])==x['raw_sha256']
up=j(W/'old-coverage-updates.json');assert len(up)==10 and sum(len(x['changed_fields']) for x in up)==51
for x in up:
 assert of[x['flow_id']]==x['before'],x['flow_id']
 assert x['changed_fields']=={k:{'before':x['before'][k],'after':v} for k,v in x['after'].items() if x['before'][k]!=v}
 assert x['after']['depth']==x['before']['depth'] and x['after']['hands_on']=='false'
 assert set(x['after']['evidence_ids'].split(';'))<=eids
caps=j(W/'old-evidence-additional-captures.json');assert len(caps)==4
for x in caps:
 assert oe[x['evidence_id']]['url']==x['expected_url']
 assert oe[x['evidence_id']].get('additional_captures',[])==x['before_additional_captures']
 for cap in x['append_additional_captures']:paths(cap);assert cap['hands_on'] is False
assert j(W/'old-evidence-field-updates.json')==[]
pdfcap=j(W/'pdf-capture.json');assert pdfcap['status']==200 and pdfcap['same_as_local_old'] is True
assert pdfcap['raw_sha256']==oe['E-VICD-01']['raw_sha256']
case=L/'cases/M-007-viwoods.md';s=case.read_text();assert set(re.findall(r'F-M007-\d{2}',s))=={f'F-M007-{i:02}' for i in range(1,21)}
for target in re.findall(r'\]\(([^)]+)\)',s):
 if not target.startswith(('https://','http://')):assert (case.parent/target).resolve().exists(),target
assert not any(x in s for x in ['待root合并','等待合并','主表未改'])
print(json.dumps({'status':'PASS','mode':'pre_merge_read_only','new_e':14,'new_f':3,'old_f':10,'old_f_fields':51,'old_e_additional_captures':4,'inventory_rows':578,'actually_viewed_static_pages':68,'new_archived_pages':13,'formal_files':len(j(W/'formal-manifest.json')),'case_sha256':sha(case),'manifest_sha256':sha(W/'formal-manifest.json'),'state_changed_by_check':False},ensure_ascii=False,indent=2))
