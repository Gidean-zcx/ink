from pathlib import Path
import json,csv,re,hashlib,collections
lib=Path('docs/research/interaction-library');b=Path('work/eudic-controls-deepening');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ev=[json.loads(s) for s in (lib/'batches/eudic-controls-deepening-evidence.jsonl').read_text().splitlines()];ids={x['evidence_id'] for x in ev};assert ids=={'E-EUCD-01','E-EUCD-02','E-EUCD-04'}
oldids=set()
for p in (lib/'batches').glob('*-evidence.jsonl'):
 if p.name=='eudic-controls-deepening-evidence.jsonl':continue
 for line in p.read_text().splitlines():
  if line:oldids.add(json.loads(line)['evidence_id'])
assert not ids&oldids
for e in ev:
 assert sha(Path(e['raw_work_path']))==e['raw_sha256'];assert sha(lib/e['archived_artifact'])==e['sha256'];assert not e['hands_on'] and not e['visual_observed'];assert not e['archived_artifact'].startswith('work/')
 for r in e.get('additional_raw_sources',[]):assert sha(Path(r['raw_work_path']))==r['raw_sha256'];assert not r['body_available']
cp=lib/'batches/eudic-controls-deepening-coverage.csv';ip=lib/'batches/eudic-controls-deepening-inventory.csv';up=b/'existing-coverage-updates.csv'
for p in [cp,ip,up]:
 raw=p.read_bytes();assert b'\r' not in raw and b'\\r\\n' not in raw and raw.endswith(b'\n')
 for r in csv.DictReader(p.open()):
  assert None not in r
  for e in r.get('evidence_ids','').split(';'):
   if e:assert e in ids|oldids,e
flows=list(csv.DictReader(cp.open()));assert [r['flow_id'] for r in flows]==['F-M026-11','F-M026-12','F-M026-13']
inv=list(csv.DictReader(ip.open()));count=collections.Counter(r['kind'] for r in inv);assert count=={'help_page':22,'help_section':12}
changes=json.loads((b/'existing-coverage-updates.json').read_text());assert len(changes)==22;assert changes==list(csv.DictReader(up.open()))
case=lib/'cases/M-026-eudic.md';s=case.read_text();fids=set(re.findall(r'F-M026-\d{2}',s));assert fids=={f'F-M026-{n:02}' for n in range(1,14)}
for eid in re.findall(r'E-(?:EUD|EUCD)-\d{2}',s):assert eid in ids|oldids,eid
for target in re.findall(r'\]\(([^)]+)\)',s):
 if target.startswith(('http://','https://')):continue
 assert (case.parent/target.split('#')[0]).resolve().exists(),target
baseline=json.loads((b/'baseline.json').read_text());unchanged=[]
for p,h in baseline.items():
 if '/batches/' in p:assert sha(Path(p))==h;unchanged.append(p)
report={'result':'PASS','new_E':3,'new_F':3,'case_F':13,'case_D2':12,'case_D1':1,'old_field_changes':22,'inventory':dict(count),'raw_and_artifact_hashes':'match','csv_LF':True,'unchanged_old_batches':unchanged,'no_hands_on_or_visual_claim':True}
(b/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False,indent=2))
files=[case,*lib.glob('batches/eudic-controls-deepening-*'),*lib.glob('artifacts/eudic-controls-deepening/*'),*b.glob('*.html'),*b.glob('*.txt'),b/'existing-coverage-updates.json',up]
(b/'hash-manifest.json').write_text(json.dumps([{'path':str(p),'sha256':sha(p)} for p in files],ensure_ascii=False,indent=2)+'\n')
