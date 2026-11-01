"""Resolve commit/tree identity, then fetch only documentation. No document execution."""
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
import hashlib,json,datetime
W=Path(__file__).resolve().parent
assert not (W/'canonical-tree.json').exists(), 'No automatic retry.'
log=json.loads((W/'fetch-log.json').read_text());identity=json.loads((W/'identity.json').read_text());old=json.loads((W/'tree.json').read_text())
def get(url,path):
 row={'url':url,'method':'anonymous_GET_once','retrieved_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'saved_path':str(path.relative_to(Path.cwd()))}
 try:
  with urlopen(Request(url,headers={'User-Agent':'reader-research-public-document-capture','Accept':'application/vnd.github+json' if 'api.github.com/' in url else 'text/plain'}),timeout=30) as r:
   row.update(status=r.status,final_url=r.geturl());data=r.read()
  assert row['status']==200
  path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);row.update(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data));log.append(row)
  (W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n');return data,row
 except (HTTPError,URLError) as e:
  row.update(status=getattr(e,'code',None),error=str(e),stopped=True);log.append(row);(W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n');raise SystemExit('Stopped without retry: '+str(e))
url='https://api.github.com/repos/readwiseio/readwise-skills/git/trees/'+identity['commit_tree']+'?recursive=1'
data,_=get(url,W/'canonical-tree.json');canonical=json.loads(data)
assert canonical['sha']==identity['commit_tree'] and canonical.get('truncated') is False
normalize=lambda x:{(r['path'],r['type'],r['sha']) for r in x['tree']}
identity.update(canonical_tree_sha=canonical['sha'],existing_tree_entries_match=normalize(old)==normalize(canonical),existing_tree_header_equals_commit=old['sha']==identity['commit'],identity_note='Existing tree response header equals commit, canonical root tree fetched separately; compare path/type/blob entries instead of equating headers.')
(W/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
assert identity['existing_tree_entries_match'], 'File-tree entries differ; stop for review'
selected=[x for x in canonical['tree'] if x['path']=='README.md' or (x['path'].startswith('skills/') and x['path'].endswith('/SKILL.md'))]
assert len(selected)==12 and all(x['type']=='blob' for x in selected)
for item in selected:
 data,row=get('https://raw.githubusercontent.com/readwiseio/readwise-skills/'+identity['commit']+'/'+item['path'],W/'raw'/item['path'])
 blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest();row.update(commit=identity['commit'],repo_path=item['path'],expected_git_blob=item['sha'],computed_git_blob=blob,blob_matches=blob==item['sha'])
 (W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n');assert row['blob_matches'],item['path']
print(json.dumps({'commit':identity['commit'],'tree':canonical['sha'],'entry_sets_match':True,'readme':1,'skills':11,'new_requests':len(log),'all_http_200':all(r['status']==200 for r in log),'all_12_blobs_match':True}))
