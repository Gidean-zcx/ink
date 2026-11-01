import csv,hashlib,json,re
from pathlib import Path
root=Path('.');lib=root/'docs/research/interaction-library';w=root/'work/huawei-controls-deepening';checks=[]
def ok(value,label):
    if not value: raise AssertionError(label)
    checks.append(label)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
efile=lib/'batches/huawei-controls-deepening-evidence.jsonl';cfile=lib/'batches/huawei-controls-deepening-coverage.csv';ifile=lib/'batches/huawei-controls-deepening-inventory.csv'
e=[json.loads(x) for x in efile.read_text().splitlines()];c=list(csv.DictReader(cfile.open()));inv=list(csv.DictReader(ifile.open()))
oldE=[json.loads(x) for x in (lib/'batches/extension-manuals-evidence.jsonl').read_text().splitlines()];oldEd={x['evidence_id']:x for x in oldE}
oldC={x['flow_id']:x for x in csv.DictReader((lib/'batches/extension-manuals-coverage.csv').open())}
ids={x['evidence_id'] for x in e};allids=ids|set(oldEd)
ok(len(e)==len(ids)==13,'13 unique E');ok({x['flow_id'] for x in c}=={f'F-M061-{i:02d}' for i in range(11,18)},'7 new F11-17');ok(len(inv)==25,'25 functional-domain inventory entries')
for p in (lib/'batches').glob('*evidence.jsonl'):
    if p==efile:continue
    for line in p.read_text().splitlines():
        if line.strip():ok(json.loads(line).get('evidence_id') not in ids,f'no ID collision {p.name}')
for x in e:
    ok(x['hands_on'] is False,'not hands on '+x['evidence_id'])
    p=lib/x['archived_artifact'];ok(not x['archived_artifact'].startswith('work/'),'formal artifact path');ok(p.exists() and sha(p)==x['sha256'],'artifact sha '+x['evidence_id'])
    p=root/x['raw_work_path'];ok(p.exists() and sha(p)==x['raw_sha256'],'raw sha '+x['evidence_id'])
    for cap in x.get('additional_captures',[]):ok(sha(root/cap['raw_work_path'])==cap['raw_sha256'],'additional raw sha')
    for p,h in x.get('artifact_hashes',{}).items():ok(sha(lib/p)==h,'image sha '+p)
    if x['visual_observed']:ok(x['evidence_id'] in {'E-HWCD-03','E-HWCD-07'},'only actually viewed evidence');ok((lib/x['visual_audit_path']).exists(),'visual audit exists')
for row in c:
    ok(row['depth']=='D2' and row['hands_on']=='false','documented D2 '+row['flow_id'])
    ok(set(row['evidence_ids'].split(';'))<=allids,'F evidence resolves')
    ok((lib/row['case_path']).exists(),'case resolves')
for row in inv:
    ok(row['kind']=='functional_domain','not full official toc')
    ok(set(filter(None,row['evidence_ids'].split(';')))<=allids,'inventory E refs')
    ok(set(filter(None,row['flow_ids'].split(';')))<=set(oldC)|{x['flow_id'] for x in c},'inventory F refs')
updates=json.loads((w/'existing-coverage-updates.json').read_text());ok(len(updates)==15,'15 old-field changes');ok(len({x['flow_id'] for x in updates})==4,'4 old F')
for u in updates:ok(oldC[u['flow_id']][u['field']]==u['before'],'old before exact '+u['flow_id']+'/'+u['field']);ok(u['before']!=u['after'],'change not no-op')
adds=json.loads((w/'additional-captures-updates.json').read_text());ok(len(adds)==7,'7 old captures')
for u in adds:
    ok(oldEd[u['evidence_id']].get('additional_captures')==u['before'],'old capture before exact')
    for cap in u['after']:
        ok(cap['network_refetched'] is False,'old local reread not network');ok(sha(root/cap['raw_work_path'])==cap['raw_sha256'],'old raw sha');ok(sha(lib/cap['archived_artifact'])==cap['sha256'],'old note sha')
for p,h in json.loads((w/'old-batch-hashes.json').read_text()).items():ok(sha(root/p)==h,'old batch unchanged '+p)
case=lib/'cases/M-061-huawei-paper.md';text=case.read_text();ok({f'F-M061-{i:02d}' for i in range(1,18)}<=set(re.findall(r'F-M061-\d+',text)),'all 17 F in case')
for p in [case,efile,cfile,ifile,lib/'artifacts/huawei-controls-deepening/source-notes.md',w/'existing-coverage-updates.csv']:
    b=p.read_bytes();ok(b'\r' not in b and b'\\r\\n' not in b,'normal LF '+p.name)
    if p.suffix=='.md':
        for dest in re.findall(r'\]\(([^)]+)\)',p.read_text()):
            if not dest.startswith(('http:','https:','#')):ok((p.parent/dest.split('#')[0]).exists(),'relative link '+dest)
# Material hashes for all acquired raw captures and the formal files, without claiming a derived JS as original.
records={}
for fname in ['fetch-log.json','fetch-extra-log.json','fetch-regional-log.json','image-fetch-log.json']:
    data=json.loads((w/fname).read_text())
    if isinstance(data,dict):data=data.get('captures',data.get('results',[]))
    for row in data:
        p=row.get('raw_work_path');h=row.get('raw_sha256')
        if p and h:ok(sha(root/p)==h,'fetch raw hash '+p);records[p]=h
for x in e:
    records[x['raw_work_path']]=x['raw_sha256']
    for cap in x.get('additional_captures',[]):records[cap['raw_work_path']]=cap['raw_sha256']
for p in [case,efile,cfile,ifile,*sorted((lib/'artifacts/huawei-controls-deepening').glob('*')),w/'existing-coverage-updates.json',w/'additional-captures-updates.json']:
    if p.is_file():records[str(p)]=sha(p)
(w/'material-hashes.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
result={'status':'PASS','checks':len(checks),'new_evidence':len(e),'new_flows':len(c),'inventory_rows':len(inv),'old_flow_ids':sorted({u['flow_id'] for u in updates}),'old_field_changes':len(updates),'old_evidence_captures':len(adds),'actually_viewed_images':5,'old_batches_unchanged':True}
(w/'check-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
