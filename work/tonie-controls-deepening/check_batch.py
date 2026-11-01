import csv, hashlib, json, pathlib, re
ROOT=pathlib.Path('/home/workspace/reader');L=ROOT/'docs/research/interaction-library';W=ROOT/'work/tonie-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
e=[json.loads(x) for x in (L/'batches/tonie-controls-deepening-evidence.jsonl').read_text().splitlines()]
assert len(e)==24 and len({x['evidence_id'] for x in e})==24
master_e={json.loads(x)['evidence_id'] for x in (L/'evidence.jsonl').read_text().splitlines()}
ids=master_e|{x['evidence_id'] for x in e};flowids={f'F-M038-{i:02}' for i in range(1,19)}
for x in e:
    assert not x['hands_on'] and not x['visual_observed'] and x['archived_artifact'] is None
    assert re.match(r'^https?://',x['url'])
    for p,h in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:assert sha(ROOT/x[p])==x[h],x['evidence_id']
report={'evidence':len(e),'csv':{},'hands_on':False,'visual_observed':False}
for name,count in [('coverage',7),('inventory',150)]:
    p=L/f'batches/tonie-controls-deepening-{name}.csv';b=p.read_bytes();rows=list(csv.DictReader(p.open()))
    assert len(rows)==count
    assert b.count(b'\n')==count+1 and b'\r' not in b and b'\\r\\n' not in b
    for row in rows:
        assert None not in row and all(v is not None for v in row.values())
        for x in row['evidence_ids'].split(';'):assert x in ids,x
        for x in row.get('flow_ids',row.get('flow_id','')).split(';'):
            if x:assert x in flowids,x
    report['csv'][name]={'rows':len(rows),'physical_lf':b.count(b'\n'),'literal_backslash_rn':b.count(b'\\r\\n'),'sha256':sha(p)}
case=(L/'cases/M-038-tonie.md').read_text();found=re.findall(r'^\| (F-M038-\d+) \|',case,re.M)
assert len(found)==18 and set(found)==flowids
assert '\n\n| F-M038-12' not in case
for x in re.findall(r'E-(?:TONI|TNCD)-\d+',case):assert x in ids,x
for url in re.findall(r'\]\(([^)]+)\)',case):
    if not url.startswith(('http://','https://','#')):assert (L/'cases'/url).resolve().exists(),url
old=json.loads((W/'old-coverage-updates.json').read_text());current={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open())};states={}
for x in old:
    assert x['before']['flow_id']==x['after']['flow_id']==x['flow_id']
    assert set(x['changes'])=={k for k in x['before'] if x['before'][k]!=x['after'][k]}
    for k,v in x['changes'].items():assert v==dict(before=x['before'][k],after=x['after'][k])
    actual=current[x['flow_id']];states[x['flow_id']]='before' if actual==x['before'] else 'after' if actual==x['after'] else 'changed_by_other'
    assert states[x['flow_id']]!='changed_by_other',(x['flow_id'],actual)
report['old_rows']=states
report['case_flows']=len(found)
manifest=[]
for p in sorted(list(W.glob('tn-pages*.json'))+[W/'web-discovery.json']+list((W/'extracted').glob('*.txt'))):
    manifest.append(dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p)))
(W/'source-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
report['source_files']=len(manifest)
(W/'check-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False,indent=2))
