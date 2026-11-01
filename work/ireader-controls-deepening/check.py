from pathlib import Path
import json, csv, hashlib, re

R=Path('/home/workspace/reader');W=R/'work/ireader-controls-deepening';L=R/'docs/research/interaction-library';B=L/'batches'
checks=[]
def ck(condition,message):
    assert condition,message
    checks.append(message)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((W/'baseline.json').read_text())
ck(not baseline['visual_case_dependencies'],'no prior case visual audit dependency')
ck(sha(W/'case-before.md')==baseline['old_batch_hashes']['docs/research/interaction-library/cases/M-014-ireader.md'],'case baseline hash')
case=L/'cases/M-014-ireader.md'
ck(case.read_bytes()==(W/'case-after.md').read_bytes(),'case equals proposed after')
ck(set(re.findall(r'F-M014-\d{2}',case.read_text()))=={f'F-M014-{n:02}' for n in range(1,7)},'six existing tasks only')
mastere=[json.loads(line) for line in (L/'evidence.jsonl').read_text().splitlines()]
ck(not any(d['evidence_id'].startswith('E-IRCD') for d in mastere),'proposed new prefix unoccupied; no new E used')
ck((B/'ireader-controls-deepening-evidence.jsonl').read_bytes()==b'','zero new evidence')
newcov=list(csv.DictReader((B/'ireader-controls-deepening-coverage.csv').open(newline='')))
ck(not newcov,'zero new flow rows')
inventory=list(csv.DictReader((B/'ireader-controls-deepening-inventory.csv').open(newline='')))
ck(len(inventory)==45,'inventory 45 rows')
for k,n in [('mobile_domain',17),('faq_question',18),('faq_subbranch',4),('version_note',6)]:ck(sum(x['node_type']==k for x in inventory)==n,k)
eids={d['evidence_id'] for d in mastere}; fids={f'F-M014-{n:02}' for n in range(1,7)}
for row in inventory:
    ck(row['product_id']=='M-014','inventory product '+row['inventory_id'])
    ck(set(row['evidence_ids'].split(';'))<=eids,'inventory E '+row['inventory_id'])
    ck(set(filter(None,row['flow_ids'].split(';')))<=fids,'inventory F '+row['inventory_id'])
allchanges=[]
for name in ['existing-coverage-updates.json','existing-evidence-additional-captures.json','existing-evidence-field-updates.json']:
    changes=json.loads((W/name).read_text());allchanges+=changes
    for x in changes:
        p=R/x['source_batch']
        ck(sha(p)==baseline['old_batch_hashes'][x['source_batch']],'old batch unchanged '+x['source_batch'])
        if p.suffix=='.csv':
            old=next(row for row in csv.DictReader(p.open(newline='')) if row['flow_id']==x['flow_id'])
        else:old=next(d for d in map(json.loads,p.read_text().splitlines()) if d['evidence_id']==x['evidence_id'])
        ck(old.get(x['field'],[])==x['before'],'exact before '+str(x.get('flow_id',x.get('evidence_id')))+' '+x['field'])
        ck(x['after']!=x['before'],'nonempty change')
        if x['field']=='additional_captures':
            for capture in x['after'][len(x['before']):]:
                ck(sha(R/capture['raw_work_path'])==capture['raw_sha256'],'raw hash '+capture['raw_work_path'])
                ck(not capture['archived_artifact'].startswith(('work/','/')),'archive relative path')
                ck(sha(L/capture['archived_artifact'])==capture['sha256'],'archive hash')
                ck(capture['hands_on'] is False and capture['visual_observed'] is False,'no observation claim')
ck(len(json.loads((W/'existing-coverage-updates.json').read_text()))==18,'18 flow field updates')
ck(len(json.loads((W/'existing-evidence-additional-captures.json').read_text()))==3,'3 additional captures')
ck(all(x['field']!='depth' for x in allchanges),'no depth upgrades')
for p in [case,B/'ireader-controls-deepening-coverage.csv',B/'ireader-controls-deepening-evidence.jsonl',B/'ireader-controls-deepening-inventory.csv',L/'artifacts/ireader-controls-deepening/source-notes.md']:
    ck(b'\r' not in p.read_bytes(),'LF '+p.name)
    ck(b'\\r\\n' not in p.read_bytes(),'no literal CRLF escapes '+p.name)
for target in re.findall(r'\]\(([^)]+)\)',case.read_text()):
    if not target.startswith('http'):ck((case.parent/target).resolve().exists(),'case relative link '+target)
versions=json.loads((W/'app-store-version-index.json').read_text())
ck(versions[0]['version']=='8.6.2' and 'Oct 22 2026' in versions[0]['date_raw'],'direct snapshot current version/date')
ck('标点' in next(v['note'] for v in versions if v['version']=='8.5.1'),'8.5.1 punctuation')
ck('外部音乐' in next(v['note'] for v in versions if v['version']=='8.5.2'),'8.5.2 music')
ck(json.loads((W/'captures.json').read_text())[0]['status']==200,'one GET HTTP200')
print(f'{len(checks)} PASS; 0 new E/F; 45 inventory; 18 old F fields; 3 E captures; 1 E locator correction')
