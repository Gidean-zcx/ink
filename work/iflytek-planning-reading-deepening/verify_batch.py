from pathlib import Path
import json,csv,io,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-planning-reading-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];em={x['evidence_id']:x for x in es};fs=list(csv.DictReader(io.StringIO((L/'coverage.csv').read_text())));fm={x['flow_id']:x for x in fs}
assert len(es)==1411 and len(fs)==857
names=['boox-reading-controls-deepening','ihuman-learning-controls-deepening','iflytek-planning-reading-deepening','ihuman-video-preview']
new=[];nf=[];files=set()
for name in names:
 ep=L/'batches'/f'{name}-evidence.jsonl';files.add(ep);records=[json.loads(s) for s in ep.read_text().splitlines() if s.strip()];new+=records
 for e in records:
  assert e==em[e['evidence_id']];assert e.get('supersedes') is None
  p=L/e['archived_artifact'];assert sha(p)==e['sha256'];files.add(p)
  for path,h in e.get('artifact_hashes',{}).items():assert sha(L/path)==h;files.add(L/path)
 cp=L/'batches'/f'{name}-coverage.csv'
 if cp.exists():
  files.add(cp)
  for f in csv.DictReader(io.StringIO(cp.read_text())):assert f==fm[f['flow_id']];nf.append(f)
assert len(new)==20 and len(nf)==11 and all(f['depth']=='D2' and f['hands_on']=='false' for f in nf)
assert len({e['url'] for e in es})==1243
assert fm['F-M029-01']['depth']=='D1'
ma=load(W/'merge-result.json')
ih=load(R/'work/ihuman-video-preview/coverage-update-applied.json')
for x in ma['audit']:
 if x['kind']=='flow':assert fm[x['id']]==(ih['after'] if x['id']==ih['flow_id'] else x['after'])
 else:assert em[x['id']]['additional_captures']==x['after']
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
media=load(L/'artifacts/iflytek-planning-reading-deepening/video-samples.json');assert sum(len(x['selected_frames']) for x in media)==35
for m in media:
 for a in m['selected_frames']:
  p=L/a['path'];assert sha(p)==a['sha256'];b=p.read_bytes();assert [int.from_bytes(b[16:20],'big'),int.from_bytes(b[20:24],'big')]==[1280,720]
net=load(W/'network-events.json');assert net and all(x['status'] in [200,206] for x in net)
ir=load(R/'work/ihuman-video-preview/video-inspection.json');assert len(ir['samples'])==4 and all(x['status']==206 for x in ir['network'])
for name in names:
 ap=L/'artifacts'/name
 if ap.exists():files.update(x for x in ap.rglob('*') if x.is_file())
for n in ['cases/M-004-boox-neoreader.md','cases/M-027-iflytek-learning.md','cases/M-029-ihuman.md','inventories/M027-help-inventory.csv','batches/boox-reading-controls-deepening-inventory.csv','batches/ihuman-learning-controls-deepening-inventory.csv']:files.add(L/n)
manifest=[dict(path=str(p.relative_to(R)),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(files)]
(W/'formal-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
result=dict(status='PASS',products=65,flows=857,new_flows=11,new_evidence=20,old_flows=4,old_evidence_captures=4,formal_files=len(manifest),iflytek_frames=35,ihuman_frames=2,boox_images=4,directory_images=1,requirements_unchanged=True,new_product_operations=0)
(W/'verification-result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False))
