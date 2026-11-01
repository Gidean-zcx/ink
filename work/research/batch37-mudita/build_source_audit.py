import csv, hashlib, json, re, sys
from pathlib import Path
from urllib.parse import urlparse
ROOT=Path('/home/workspace/reader')
W=ROOT/'work/research/batch37-mudita'
INV=ROOT/'docs/research/interaction-library/batches/mudita-app-controls-deepening-inventory.csv'
EVID=ROOT/'docs/research/interaction-library/evidence.jsonl'
LOG=W/'fetch-log.jsonl'
OUT=W/'source-audit.json'
if OUT.exists(): raise SystemExit('refusing overwrite of existing source-audit.json')
rows=[r for r in csv.DictReader(INV.open()) if r['reading_status']=='index_only']
logs=[json.loads(x) for x in LOG.read_text().splitlines() if x.strip()]
evidence=[json.loads(x) for x in EVID.read_text().splitlines() if x.strip()]
if len(rows)!=46 or len(logs)!=46: raise SystemExit(f'invariant failed rows={len(rows)} logs={len(logs)}')
log_by_id={r['inventory_id']:r for r in logs}
if len(log_by_id)!=46: raise SystemExit('duplicate inventory_id in fetch log')
items=[]; conflicts=[]; unknown=[]
for row in rows:
    iid=row['inventory_id']; rec=log_by_id.get(iid)
    if rec is None: raise SystemExit(f'missing fetch record: {iid}')
    hp=ROOT/rec['html_path']; tp=ROOT/rec['text_path']
    if not hp.is_file() or not tp.is_file(): raise SystemExit(f'missing artifact: {iid}')
    hsha=hashlib.sha256(hp.read_bytes()).hexdigest(); tb=tp.read_bytes(); tsha=hashlib.sha256(tb).hexdigest()
    text=tb.decode('utf-8')
    m=re.search(r'^Modified on:.*$',text,re.M)
    modified=m.group(0) if m else None
    article_m=re.search(r'/articles/(\d+)',row['url']); article_id=article_m.group(1) if article_m else None
    exact=[]; id_matches=[]
    for e in evidence:
        eurl=e.get('url','')
        if eurl==row['url']: exact.append(e.get('evidence_id'))
        elif article_id and re.search(r'/articles/'+re.escape(article_id)+r'(?:-|$|[/?#])',eurl): id_matches.append(e.get('evidence_id'))
    exact=[x for x in exact if x]; id_matches=[x for x in id_matches if x]
    readable=bool(rec.get('body_readable')) and len(text.strip())>=20 and modified is not None
    hashes_match=(hsha==rec.get('html_sha256') and tsha==rec.get('text_sha256'))
    if not hashes_match: conflicts.append({'inventory_id':iid,'kind':'hash_mismatch','logged_html':rec.get('html_sha256'),'actual_html':hsha,'logged_text':rec.get('text_sha256'),'actual_text':tsha})
    if not readable: conflicts.append({'inventory_id':iid,'kind':'readability_or_modified_marker_failure','logged_body_readable':rec.get('body_readable'),'text_chars':len(text),'modified_found':modified is not None})
    if not article_id: unknown.append({'inventory_id':iid,'kind':'article_id_missing'})
    items.append({
      'inventory_id':iid,'title':row['title'],'url':row['url'],'article_id':article_id,
      'modified_on_raw':modified,'text_path':rec['text_path'],'html_path':rec['html_path'],
      'html_sha256':hsha,'text_sha256':tsha,'hashes_match':hashes_match,'body_readable':readable,
      'existing_evidence_ids':exact+id_matches,'existing_evidence_exact_url_ids':exact,
      'existing_evidence_article_id_ids':id_matches
    })
summary={'checked_on':'2026-10-27','inventory_count':len(rows),'fetch_log_count':len(logs),'item_count':len(items),
         'hashes_match_count':sum(x['hashes_match'] for x in items),'readable_count':sum(x['body_readable'] for x in items),
         'exact_url_duplicate_count':sum(bool(x['existing_evidence_exact_url_ids']) for x in items),
         'article_id_variant_duplicate_count':sum(bool(x['existing_evidence_article_id_ids']) for x in items),
         'conflict_count':len(conflicts),'unknown_count':len(unknown)}
OUT.write_text(json.dumps({'summary':summary,'conflicts':conflicts,'unknowns':unknown,'items':items},ensure_ascii=False,indent=2)+'\n')
print(json.dumps(summary,ensure_ascii=False))
if conflicts: sys.exit(1)
