"""Read-only before-merge checks. Do not run after root merges old-field updates."""
import csv,json,hashlib,re,sys
from pathlib import Path
R=Path('docs/research/interaction-library');W=Path('work/ihuman-learning-controls-deepening');B=R/'batches';A=R/'artifacts/ihuman-learning-controls-deepening'
def readj(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(Path(p).open()))
checks=[]
def ok(c,n):
 if not c:raise AssertionError(n)
 checks.append(n)
oldf=readj(W/'coverage-before.json');olde=readj(W/'evidence-before.json')
for x in oldf:
 matches=[r for r in rows(x['source_batch']) if r['flow_id']==x['record']['flow_id']]
 ok(matches==[x['record']],'old F unchanged '+x['record']['flow_id'])
for x in olde:
 matches=[json.loads(l) for l in Path(x['source_batch']).read_text().splitlines() if json.loads(l)['evidence_id']==x['record']['evidence_id']]
 ok(matches==[x['record']],'old E unchanged '+x['record']['evidence_id'])
 if x['record'].get('visual_audit_path'):
  ok(sha(R/x['record']['visual_audit_path'])==x['record']['visual_audit_sha256'],'prior visual hash '+x['record']['evidence_id'])
  ok(x['record']['visual_audit_path']!='cases/M-029-ihuman.md','no old case hash dependency')
for x in readj(W/'existing-coverage-updates.json'):
 b=next(y['record'] for y in oldf if y['record']['flow_id']==x['flow_id']);a=dict(b)
 for f,v in x['changes'].items():ok(b[f]==v['before'],'before '+x['flow_id']+'/'+f);a[f]=v['after']
 ok(a==x['replacement_row'],'complete replacement '+x['flow_id']);ok(a['depth']==b['depth'],'no depth upgrade '+x['flow_id']);ok(a['visual_status']==b['visual_status'],'no visual upgrade '+x['flow_id'])
new=[json.loads(l) for l in (B/'ihuman-learning-controls-deepening-evidence.jsonl').read_text().splitlines()]
oldids={x['record']['evidence_id'] for x in olde};allids=oldids|{x['evidence_id'] for x in new};flowids={x['record']['flow_id'] for x in oldf}
ok(len(new)==2,'two new E');ok(len(rows(B/'ihuman-learning-controls-deepening-coverage.csv'))==0,'zero new F')
masterids={json.loads(l)['evidence_id'] for l in (R/'evidence.jsonl').read_text().splitlines()}
for x in new:
 ok(x['evidence_id'] not in masterids,'new ID not merged '+x['evidence_id']);ok(x['supersedes'] is None,'supersedes null '+x['evidence_id']);ok(not x['hands_on'] and not x['visual_observed'],'text only '+x['evidence_id']);ok(sha(x['raw_work_path'])==x['raw_sha256'],'reading record hash '+x['evidence_id']);ok('not_origin_html' in x['raw_kind'],'raw-kind honesty '+x['evidence_id']);ok(sha(R/x['archived_artifact'])==x['sha256'],'notes hash '+x['evidence_id']);ok(not x['archived_artifact'].startswith(('work/','/')),'relative formal path '+x['evidence_id'])
for x in readj(W/'existing-evidence-additional-captures.json'):
 b=next(y['record'] for y in olde if y['record']['evidence_id']==x['evidence_id']);ok(x['before']==b.get('additional_captures',[]) and x['before_exists']==('additional_captures' in b),'capture before');ok(x['after']==x['before']+[x['append']],'append only');c=x['append'];ok(sha(c['raw_work_path'])==c['raw_sha256']==b['raw_sha256'],'unchanged original HTML');ok(sha(R/c['archived_artifact'])==c['sha256'],'capture notes hash')
inv=rows(B/'ihuman-learning-controls-deepening-inventory.csv');ok(len(inv)==36,'36 sourced inventory entries')
for x in inv:
 ok(set(x['evidence_ids'].split(';'))<=allids,'inventory E '+x['item_id']);ok(set(filter(None,x['flow_ids'].split(';')))<=flowids,'inventory F '+x['item_id']);ok(x['new_visual_observation']=='false','no new visuals '+x['item_id'])
for x in readj(W/'existing-coverage-updates.json'):ok(set(x['replacement_row']['evidence_ids'].split(';'))<=allids,'updated F Erefs')
explore=readj(W/'exploration-log.json');ok(explore['candidates_checked']==5 and len(explore['candidates'])<=6,'bounded candidate count')
ok('TencentCaptcha' in (W/'raw/smzdm-2020.html').read_text(),'captcha not mistaken for article')
ok(not list(W.rglob('*.mp4')) and not list(A.glob('*.png')) and not list(A.glob('*.jpg')),'no saved media or new images')
for p in [B/'ihuman-learning-controls-deepening-coverage.csv',B/'ihuman-learning-controls-deepening-inventory.csv']:
 data=p.read_bytes();ok(b'\r' not in data and b'\\r\\n' not in data,'LF '+p.name)
for p in [R/'cases/M-029-ihuman.md',A/'source-notes.md']:
 for target in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',p.read_text()):ok(Path(target).is_file(),'local link '+target)
 ok('待root' not in p.read_text(),'no temporary handoff in case/notes')
if '--manifest' in sys.argv:
 for x in readj(W/'manifest.json')['files']:ok(sha(x['path'])==x['sha256'],'manifest '+x['path'])
print(json.dumps({'status':'PASS','checks':len(checks),'new_E':2,'new_F':0,'old_F_rows':2,'old_F_fields':8,'old_E_capture_rows':1,'inventory_rows':36,'new_visual_observations':0,'before_merge_only':True,'external_state_changed':False},ensure_ascii=False,indent=2))
