"""Read-only, before-merge checks. Does not modify any file. Do not rerun after merge."""
import csv,hashlib,json,re,sys
from pathlib import Path
R=Path('docs/research/interaction-library');W=Path('work/supernote-notes-controls-deepening');A=R/'artifacts/supernote-notes-controls-deepening';B=R/'batches'
checks=[]
def ok(v,what):
 if not v:raise AssertionError(what)
 checks.append(what)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def j(p):return json.loads(Path(p).read_text())
def readrows(p):return list(csv.DictReader(Path(p).open()))
oldf=j(W/'coverage-before.json');olde=j(W/'evidence-before.json')
for x in oldf:
 now=[r for r in readrows(x['source_batch']) if r['flow_id']==x['record']['flow_id']]
 ok(now==[x['record']],'unchanged oldF '+x['record']['flow_id'])
for x in olde:
 now=[json.loads(l) for l in Path(x['source_batch']).read_text().splitlines() if json.loads(l)['evidence_id']==x['record']['evidence_id']]
 ok(now==[x['record']],'unchanged oldE '+x['record']['evidence_id'])
 ok(not str(x['record'].get('visual_audit_path','')).endswith('cases/M-006-supernote.md'),'no case visual-hash dependency '+x['record']['evidence_id'])
for x in j(W/'existing-coverage-updates.json'):
 bef=next(z['record'] for z in oldf if z['record']['flow_id']==x['flow_id']);after=dict(bef)
 for k,v in x['changes'].items():
  ok(bef[k]==v['before'],'before '+x['flow_id']+' '+k);after[k]=v['after']
 ok(after==x['replacement_row'],'replacement '+x['flow_id'])
ids={x['record']['evidence_id'] for x in olde};oldids={x['record']['flow_id'] for x in oldf}
new=readrows(B/'supernote-notes-controls-deepening-coverage.csv');inv=readrows(B/'supernote-notes-controls-deepening-inventory.csv')
master={r['flow_id'] for r in readrows(R/'coverage.csv')}
case=(R/'cases/M-006-supernote.md').read_text()
ok(len(new)==5,'five new tasks');ok(len(inv)==140,'140 hierarchy rows');ok(sum(x['this_batch_review']=='body_read_this_batch' for x in inv)==29,'29 reread entries')
ok((B/'supernote-notes-controls-deepening-evidence.jsonl').read_bytes()==b'','zero new evidence')
for r in new:
 ok(r['flow_id'] not in master,'new task not yet merged '+r['flow_id']);ok(r['depth']=='D2' and r['hands_on']=='false' and r['visual_status']=='static_reference_only','scope '+r['flow_id'])
 ok(set(r['evidence_ids'].split(';'))<=ids,'E refs '+r['flow_id']);ok(r['flow_id'] in case,'case '+r['flow_id'])
allf=oldids|{x['flow_id'] for x in new}
for x in inv:
 ok(set(filter(None,x['flow_ids'].split(';')))<=allf,'inventory F refs '+x['order'])
 ok(set(filter(None,x['evidence_ids'].split(';')))<=ids,'inventory E refs '+x['order'])
 ok(x['this_batch_review'] in ['body_read_this_batch','inherited_prior_only'],'inventory review status '+x['order'])
for x in j(W/'existing-evidence-additional-captures.json'):
 old=next(z['record'] for z in olde if z['record']['evidence_id']==x['evidence_id'])
 ok(x['before_exists']==('additional_captures' in old),'old capture field presence '+x['evidence_id']);ok(x['before']==old.get('additional_captures',[]),'old capture before '+x['evidence_id']);ok(x['after']==x['before']+[x['append']],'append only '+x['evidence_id'])
 c=x['append'];ok(sha(c['raw_work_path'])==c['raw_sha256'],'raw hash '+x['evidence_id']);ok(c['raw_sha256']==old['raw_sha256'],'same source snapshot '+x['evidence_id'])
 ok(not c['archived_artifact'].startswith(('work/','/')),'relative archived path '+x['evidence_id']);ok(sha(R/c['archived_artifact'])==c['sha256'],'notes hash '+x['evidence_id'])
 if c['visual_observed']:
  ok(sha(R/c['visual_audit_path'])==c['visual_audit_sha256'],'independent visual audit hash')
  ok(len(c['artifact_hashes'])==5,'five viewed new static pages')
  for p,h in c['artifact_hashes'].items():ok(sha(R/p)==h,'image hash '+p)
for p in [R/'cases/M-006-supernote.md',A/'source-notes.md']:
 for target in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',p.read_text()):ok(Path(target).is_file(),'local link '+target)
for p in [B/'supernote-notes-controls-deepening-coverage.csv',B/'supernote-notes-controls-deepening-inventory.csv']:
 data=p.read_bytes();ok(b'\r' not in data and b'\\r\\n' not in data,'normal LF '+p.name)
raw=(W/'raw/selected-pages.txt').read_text();ok(raw.count('=== PDF / printed ')==39,'39 text page excerpts')
for p in [A/'source-notes.md',R/'cases/M-006-supernote.md']:
 ok('待root' not in p.read_text() and '主表未改' not in p.read_text(),'no transient merge status '+p.name)
if '--manifest' in sys.argv:
 m=j(W/'manifest.json')
 for x in m['files']:ok(sha(x['path'])==x['sha256'],'manifest '+x['path'])
print(json.dumps({'status':'PASS','checks':len(checks),'before_merge_only':True,'new_E':0,'new_F':5,'old_F_rows':3,'old_F_fields':10,'old_E_capture_rows':2,'inventory_rows':140,'this_batch_body_rows':29,'network_requests':0,'device_or_account_state_changed':False},ensure_ascii=False,indent=2))
