from pathlib import Path
import json,csv,hashlib,re,collections
lib=Path('docs/research/interaction-library');b=Path('work/maimemo-controls-deepening');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ev=[json.loads(x) for x in (lib/'batches/maimemo-controls-deepening-evidence.jsonl').read_text().splitlines()];ids={e['evidence_id'] for e in ev};assert len(ids)==13
prior=set()
for p in (lib/'batches').glob('*-evidence.jsonl'):
 if p.name=='maimemo-controls-deepening-evidence.jsonl':continue
 for line in p.read_text().splitlines():
  if line:prior.add(json.loads(line)['evidence_id'])
assert not ids&prior
for e in ev:
 assert sha(Path(e['raw_work_path']))==e['raw_sha256']
 assert sha(lib/e['archived_artifact'])==e['sha256']
 assert not e['archived_artifact'].startswith('work/') and not e['hands_on'] and not e['visual_observed']
 if e.get('parsed_text_path'):assert Path(e['parsed_text_path']).exists()
 if e.get('discovery_raw_work_path'):assert sha(Path(e['discovery_raw_work_path']))==e['discovery_raw_sha256']
paths=[lib/'batches/maimemo-controls-deepening-coverage.csv',lib/'batches/maimemo-controls-deepening-inventory.csv',b/'existing-coverage-updates.csv']
for p in paths:
 raw=p.read_bytes();assert b'\r' not in raw and b'\\r\\n' not in raw and raw.endswith(b'\n')
 rows=list(csv.DictReader(p.open()));assert rows
 for r in rows:
  assert None not in r
  for eid in r.get('evidence_ids','').split(';'):
   if eid:assert eid in ids|prior,(p,eid)
coverage=list(csv.DictReader(paths[0].open()));assert [r['flow_id'] for r in coverage]==['F-M017-08','F-M017-09']
inv=list(csv.DictReader(paths[1].open()));counts=collections.Counter(x['kind'] for x in inv);assert dict(counts)=={'help_node':30,'support_index':7,'api_operation':24}
h=[x for x in inv if x['kind']=='help_node'];assert sum(x['coverage']=='parent_index' for x in h)==4;assert sum(x['coverage']=='excluded_other_product' for x in h)==2
baseline=json.loads((b/'baseline.json').read_text());unchanged=[]
for name,old in baseline.items():
 if '/batches/' in name:assert sha(Path(name))==old;unchanged.append(name)
case=lib/'cases/M-017-maimemo.md';text=case.read_text();flowids=set(re.findall(r'F-M017-\d{2}',text));assert flowids=={f'F-M017-{i:02}' for i in range(1,10)}
for target in re.findall(r'\]\(([^)]+)\)',text):
 if target.startswith(('https://','http://')):continue
 assert (case.parent/target.split('#')[0]).resolve().exists(),target
report={'result':'PASS','new_E':13,'new_F':2,'case_unique_F':9,'inventory':dict(counts),'old_field_proposals':len(list(csv.DictReader(paths[2].open()))),'unchanged_old_batches':unchanged,'all_raw_and_artifact_hashes_match':True,'csv_LF':True,'no_static_or_hands_on_claim':True}
(b/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False,indent=2))
manifest=[]
for p in [case,*(lib/'batches').glob('maimemo-controls-deepening-*'),*(lib/'artifacts/maimemo-controls-deepening').glob('*'),*b.glob('*.html'),b/'16-api_bundle.yaml',b/'existing-coverage-updates.csv']:
 if p.is_file():manifest.append({'path':str(p),'sha256':sha(p)})
(b/'hash-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
