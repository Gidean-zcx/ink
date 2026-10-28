from pathlib import Path
import json,hashlib,requests,concurrent.futures
root=Path('/home/workspace/reader'); out=root/'work/anki-controls-deepening'
urls={
'llms.txt':'https://docs.ankiweb.net/llms.txt',
'misc.md':'https://docs.ankiweb.net/manual/misc.md',
'errors.md':'https://docs.ankiweb.net/manual/templates/errors.md',
'files.md':'https://docs.ankiweb.net/manual/files.md',
'syncing.md':'https://docs.ankiweb.net/manual/syncing.md',
'backups.md':'https://docs.ankiweb.net/manual/backups.md',
'profiles.md':'https://docs.ankiweb.net/manual/profiles.md',
'media.md':'https://docs.ankiweb.net/manual/media.md',
'deck-options.md':'https://docs.ankiweb.net/manual/deck-options.md',
'changes-2.1.40-49.md':'https://docs.ankiweb.net/releases/changes/changes/2.1.40-49.md',
'changes-23.12-plus.md':'https://docs.ankiweb.net/releases/changes/changes/github.md',
'missing-media.md':'https://docs.ankiweb.net/faqs/sounds-or-images-have-gone-missing.md',
'sync-profiles.md':'https://docs.ankiweb.net/faqs/synchronizing-multiple-profiles.md'}
def fetch(item):
 name,url=item; result={'name':name,'url':url,'checked_on':'2026-10-27'}
 try:
  r=requests.get(url,timeout=(5,18)); result.update(status=r.status_code,final_url=r.url,content_type=r.headers.get('content-type'))
  if r.status_code==200:
   (out/name).write_bytes(r.content); result.update(raw_work_path=str((out/name).relative_to(root)),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
  else: result['stopped_without_retry']=True
 except Exception as e: result['error']=str(e)
 return result
old={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'docs/research/interaction-library/batches').glob('*') if p.is_file() and ('anki-deepening' in p.name or p.name in ['initial-evidence.jsonl','initial-coverage.csv'])}
(out/'old-batch-hashes.json').write_text(json.dumps(old,indent=2)+'\n')
(root/'work/anki-controls-deepening/case-before.md').write_bytes((root/'docs/research/interaction-library/cases/M-020-anki.md').read_bytes())
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool: results=list(pool.map(fetch,urls.items()))
(out/'fetch-log.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results: print(r['name'],r.get('status',r.get('error')),r.get('bytes',0),r.get('raw_sha256',''))
