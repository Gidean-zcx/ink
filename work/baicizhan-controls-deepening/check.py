"""Read-only pre-merge check for this batch; do not use after old rows are merged."""
import csv
import hashlib
import json
from pathlib import Path
import re

R=Path('/home/workspace/reader')
W=R/'work/baicizhan-controls-deepening'
L=R/'docs/research/interaction-library'
P='baicizhan-controls-deepening'
checks=0
def ok(x,label):
    global checks
    assert x,label
    checks+=1
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def readjson(p):return json.loads(p.read_text())
def csvrows(p):
    with p.open() as f:return list(csv.DictReader(f))
oldf=readjson(W/'coverage-before.json');olde=readjson(W/'evidence-before.json')
fs={r['flow_id']:r for r in oldf};es={r['evidence_id']:r for r in olde}
fc=readjson(W/'existing-coverage-updates.json');ec=readjson(W/'existing-evidence-additional-captures.json')
for c in fc:
    rows=csvrows(R/c['source_batch']);row=next(r for r in rows if r['flow_id']==c['flow_id'])
    ok(row[c['field']]==c['before'],'old F before '+c['flow_id']+' '+c['field'])
    ok(fs[c['flow_id']][c['field']]==c['before'],'F baseline '+c['flow_id'])
    ok(c['before']!=c['after'],'F no-op')
for c in ec:
    rows=[json.loads(x) for x in (R/c['source_batch']).read_text().splitlines()]
    row=next(r for r in rows if r['evidence_id']==c['evidence_id'])
    ok(row.get(c['field'],[])==c['before'],'old E before '+c['evidence_id'])
    ok(c['after'][:-1]==c['before'],'append only')
    cap=c['after'][-1]
    ok(cap['capture_url']==row['url'],'same URL reused')
    ok(sha(R/cap['raw_work_path'])==cap['raw_sha256'],'capture raw hash')
    ok(sha(L/cap['archived_artifact'])==cap['sha256'],'capture notes hash')
    ok(not cap['hands_on'] and not cap['visual_observed'],'capture observations')
baseline=readjson(W/'baseline.json')
ok(sha(W/'case-before.md')==baseline['case_sha256'],'case baseline hash')
ok(baseline['visual_case_dependencies']==[],'no legacy case-dependent visual audit')
for p in readjson(W/'build-result.json')['old_sources']:
    ok(sha(R/p)==baseline['old_batches'][p],'old batch unchanged '+p)
for e in olde:
    if e.get('visual_audit_path'):
        ok(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'old visual audit '+e['evidence_id'])
new_e=[json.loads(x) for x in (L/f'batches/{P}-evidence.jsonl').read_text().splitlines()]
new_f=csvrows(L/f'batches/{P}-coverage.csv')
inventory=csvrows(L/f'batches/{P}-inventory.csv')
ok(len(new_e)==2,'new E count');ok(len(new_f)==2,'new F count');ok(len(inventory)==55,'inventory rows')
ok(sum(r['node_type']=='category' for r in inventory)==5,'five category parents')
ok(sum(r['node_type']=='question' for r in inventory)==50,'fifty unique questions')
ok(sum(r['hot_duplicate']=='true' for r in inventory)==8,'hot duplicate count')
ok(sum(r['node_type']=='question' and r['scope'].startswith('爱阅读') for r in inventory)==11,'reading excluded')
known_e=set(es)|{e['evidence_id'] for e in new_e}
known_f=set(fs)|{f['flow_id'] for f in new_f}
for e in new_e:
    ok(e['supersedes'] is None,'supersedes null')
    ok(e['product_id']=='M-016' and e['hands_on'] is False,'new E scope')
    ok(e['archived_artifact'].startswith('artifacts/') and not e['archived_artifact'].startswith('/'),'archive relative')
    ok(sha(L/e['archived_artifact'])==e['sha256'],'archive hash')
    ok(sha(R/e['raw_work_path'])==e['raw_sha256'],'raw hash')
    if e.get('visual_audit_path'):ok(sha(L/e['visual_audit_path'])==e['visual_audit_sha256'],'new visual audit')
    for c in e.get('provenance',[]):ok(sha(R/c['raw_work_path'])==c['raw_sha256'],'provenance hash')
for f in new_f:
    ok(f['depth']=='D2' and f['hands_on']=='false' and f['visual_status']=='not_observed','new F depth/visual')
    ok(set(f['evidence_ids'].split(';'))<=known_e,'new F evidence')
    ok((L/f['case_path']).is_file(),'new F case')
for r in inventory:
    ok(set(r['evidence_ids'].split(';'))<=known_e,'inventory E')
    ok(set(r['flow_ids'].split(';'))-{''}<=known_f,'inventory F')
    ok(r['visual_status'] in ['static_reference_only','not_observed'],'inventory visual enum')
for c in fc:
    if c['field']=='evidence_ids':ok(set(c['after'].split(';'))<=known_e,'updated F references')
ok(not any(c['field']=='depth' for c in fc),'old F depth unchanged')
ok(fs['F-M016-01']['depth']=='D1','main learning remains D1')
for p in (L/'batches').glob('*-evidence.jsonl'):
    if p.name==P+'-evidence.jsonl':continue
    for line in p.read_text().splitlines():
        e=json.loads(line)
        ok(e['evidence_id'] not in {'E-BCCD-01','E-BCCD-02'},'new E collision')
        if e['product_id']=='M-016':ok(e['url'] not in {r['url'] for r in new_e},'new URL not existing')
for p in (L/'batches').glob('*-coverage.csv'):
    if p.name==P+'-coverage.csv':continue
    for f in csvrows(p):
        if f['product_id']=='M-016':ok(f['flow_id'] not in {'F-M016-07','F-M016-08'},'new F collision')
case=L/'cases/M-016-baicizhan.md';text=case.read_text()
for target in re.findall(r'\]\(([^)]+)\)',text):
    if target.startswith('https://'):continue
    ok((case.parent/target).resolve().exists(),'case link '+target)
ok('待root' not in text and '待 root' not in text,'neutral case')
for p in list(W.glob('*.json'))+list(W.glob('*.py'))+list((L/'batches').glob(P+'-*')):
    ok(b'\r' not in p.read_bytes(),'LF only '+p.name)
    if p.suffix=='.csv':ok(b'\\r\\n' not in p.read_bytes(),'no literal CRLF')
raw=readjson(W/'qa-bczApp.json')['data']
actual={q['id'] for c in raw['question_categories'] for q in c['questions']}
ok({int(r['question_id']) for r in inventory if r['node_type']=='question'}==actual,'exact QA directory')
manifest=readjson(W/'manifest.json')
for entry in manifest:
    ok(sha(R/entry['path'])==entry['sha256'],'frozen manifest '+entry['path'])
print(json.dumps(dict(result='PASS',checks=checks,manifest_files=len(manifest),new_evidence=2,new_flows=2,old_flow_fields=len(fc),old_evidence_captures=len(ec),state_changed=False),ensure_ascii=False))
