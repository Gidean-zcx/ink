import pathlib,json,csv,re,hashlib,collections
R=pathlib.Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/antennapod-controls-deepening'
E=[json.loads(x) for x in (L/'batches/antennapod-controls-deepening-evidence.jsonl').read_text().splitlines()];C=list(csv.DictReader((L/'batches/antennapod-controls-deepening-coverage.csv').open()));U=json.load(open(W/'old-coverage-updates.json'));case=(L/'cases/M-040-antennapod.md').read_text();I=list(csv.DictReader((L/'artifacts/antennapod-controls-deepening/functional-inventory.csv').open()))
assert len(E)==18 and len(C)==0 and len(U)==9 and len(I)==131
ids={e['evidence_id'] for e in E};assert ids=={f'E-APCD-{i:02}' for i in range(1,19)}
allids=set()
for p in (L/'batches').glob('*evidence.jsonl'):
 for line in p.read_text().splitlines():
  if line.strip():allids.add(json.loads(line)['evidence_id'])
for e in E:
 assert e['hands_on'] is False and e['visual_observed'] is False
 assert re.match('https?://',e['url'])
 a=pathlib.Path(e['archived_artifact']);assert not a.is_absolute() and 'work' not in a.parts and '..' not in a.parts
 p=L/a;assert p.is_file();assert hashlib.sha256(p.read_bytes()).hexdigest()==e['sha256']
 p=R/e['raw_work_path'];assert p.is_file();assert hashlib.sha256(p.read_bytes()).hexdigest()==e['raw_sha256']
 assert e['evidence_id'] in case
flows=re.findall(r'^\| (F-M040-\d+) \|',case,re.M);assert len(flows)==12 and len(set(flows))==12
old={r['flow_id']:r for r in csv.DictReader((L/'batches/audio-coverage.csv').open())}
for u in U:
 assert u['flow_id'] in flows
 for k,v in u['changes'].items():assert old[u['flow_id']][k]==v['before'] and u['replacement_row'][k]==v['after']
 assert u['replacement_row']['hands_on']=='false' and u['replacement_row']['depth']=='D2' and u['replacement_row']['visual_status']=='not_observed'
 assert set(u['replacement_row']['evidence_ids'].split(';'))<=allids
for e in re.findall(r'(?<![A-Z0-9-])E-[A-Z]+-\d+',case):assert e in allids,e
for link in re.findall(r'\]\(([^)]+)\)',case):
 if not link.startswith(('http:','https:')):assert ((L/'cases')/link).resolve().exists(),link
manifest=json.load(open(W/'source-manifest.json'))
for x in manifest:assert hashlib.sha256((R/x['raw_work_path']).read_bytes()).hexdigest()==x['raw_sha256']
counts=collections.Counter()
for p in (L/'batches').glob('*evidence.jsonl'):
 for line in p.read_text().splitlines():
  if line.strip():
   eid=json.loads(line)['evidence_id']
   if eid.startswith('E-APCD-'):counts[eid]+=1
assert all(counts[x]==1 for x in ids)
result={'passed':True,'new_E':18,'new_F':0,'preserved_case_F':12,'old_F_updates':[u['flow_id'] for u in U],'inventory_help':47,'inventory_settings':84,'raw_sources_hash_checked':len(manifest),'hands_on':False,'visual_observed':False}
(W/'check-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
