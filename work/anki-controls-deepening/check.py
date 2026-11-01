from pathlib import Path
import json,csv,hashlib,re,collections
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'; work=root/'work/anki-controls-deepening'; batch=lib/'batches'
sh=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
errs=[]; checked=[]
def ck(test,msg):
 if not test:errs.append(msg)
 checked.append(msg)
owned=[lib/'cases/M-020-anki.md',batch/'anki-controls-deepening-evidence.jsonl',batch/'anki-controls-deepening-coverage.csv',batch/'anki-controls-deepening-inventory.csv',lib/'artifacts/anki-controls-deepening/source-notes.md']
for p in owned:
 b=p.read_bytes();ck(b'\r' not in b,'LF:'+str(p.relative_to(root)));ck(b'\\r\\n' not in b,'no literal CRLF:'+p.name)
evs=[json.loads(x) for x in owned[1].read_text().splitlines()]; ids={r['evidence_id'] for r in evs}
ck(len(evs)==len(ids)==7,'7 unique new E');ck(all(not r['hands_on'] and not r['visual_observed'] for r in evs),'all new E text only')
oldE={}; oldURLs={};rows={};oldF=set()
for p in batch.glob('*evidence.jsonl'):
 if p==owned[1]:continue
 for line in p.read_text().splitlines():
  if not line:continue
  r=json.loads(line);oldE[r['evidence_id']]=r;oldURLs[r.get('url','').removesuffix('.md').rstrip('/')]=r['evidence_id']
ck(not ids.intersection(oldE),'no old E collision')
for r in evs:
 ck(r['url'].removesuffix('.md').rstrip('/') not in oldURLs,'new URL:'+r['evidence_id'])
 ck((lib/r['archived_artifact']).exists() and sh(lib/r['archived_artifact'])==r['sha256'],'artifact hash:'+r['evidence_id'])
 ck((root/r['raw_work_path']).exists() and sh(root/r['raw_work_path'])==r['raw_sha256'],'raw hash:'+r['evidence_id'])
 ck(not r['archived_artifact'].startswith('work/'),'formal artifact path:'+r['evidence_id'])
for p in batch.glob('*coverage.csv'):
 if p==owned[2]:continue
 for r in csv.DictReader(p.open(newline='')):
  oldF.add(r['flow_id'])
  if r.get('product_id')=='M-020':rows[r['flow_id']]=r
ck(len(list(csv.DictReader(owned[2].open())))==0,'zero new F rows')
sug=json.loads((work/'existing-coverage-updates.json').read_text()); ck(len(sug)==36,'36 old field suggestions')
for s in sug:
 ck(rows[s['flow_id']][s['field']]==s['before'],'before match:'+s['flow_id']+'/'+s['field'])
 if s['field']=='evidence_ids':ck(set(s['after'].split(';'))<=ids|oldE.keys(),'suggested references:'+s['flow_id'])
for a in json.loads((work/'additional-captures-updates.json').read_text()):
 ck(a['before']==oldE[a['evidence_id']].get('additional_captures'),'capture before:'+a['evidence_id'])
 for c in a['after']:
  if c.get('checked_on')=='2026-10-27':ck(sh(root/c['raw_work_path'])==c['raw_sha256'] and sh(lib/c['archived_artifact'])==c['sha256'],'capture hash:'+a['evidence_id'])
inv=list(csv.DictReader(owned[3].open())); ck(len(inv)==60,'60 inventory rows');ck(len({x['item_id'] for x in inv})==60,'unique inventory ids')
ck(sum(x['kind']=='manual_page' for x in inv)==55,'55 Manual entries including parent-child pages')
ck(sum(x['kind']=='manual_page' and x['status']=='directory_only' for x in inv)==30,'30 directory-only Manual entries')
for r in inv:
 ck(set(filter(None,r['evidence_ids'].split(';')))<=ids|oldE.keys(),'inventory E:'+r['item_id'])
 ck(set(filter(None,r['flow_ids'].split(';')))<=oldF,'inventory F:'+r['item_id'])
 if r['raw_work_path']:ck(sh(root/r['raw_work_path'])==r['raw_sha256'],'inventory raw:'+r['item_id'])
text=owned[0].read_text();ck(set(re.findall(r'F-M020-\d{2}',text))==set(rows),'22 stable case flow IDs')
for eid in set(re.findall(r'E-(?:ANCD|ANK|AND)-\d{2}',text)):ck(eid in ids|oldE.keys(),'case E:'+eid)
for p in [owned[0],owned[-1]]:
 for link in re.findall(r'\]\(([^)]+)\)',p.read_text()):
  if not link.startswith(('http:','https:','#')):ck((p.parent/link.split('#')[0]).exists(),'relative link:'+link)
for p,h in json.loads((work/'old-batch-hashes.json').read_text()).items():ck(sh(root/p)==h,'old batch unchanged:'+p)
manifest={str(p.relative_to(root)):sh(p) for p in owned};(work/'formal-file-hashes.json').write_text(json.dumps(manifest,indent=2)+'\n')
report={'result':'PASS' if not errs else 'FAIL','checks':len(checked),'errors':errs,'new_evidence':len(evs),'new_flows':0,'old_flows_suggested':len({x['flow_id'] for x in sug}),'old_fields_suggested':len(sug),'manual_pages':55,'manual_body_read_partial':25,'manual_directory_only':30,'support_pages':5,'visual_observed':False,'hands_on':False}
(work/'check-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n'); print(json.dumps(report,ensure_ascii=False,indent=2));raise SystemExit(bool(errs))
