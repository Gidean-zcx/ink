"""Anonymous public document capture only. Does not execute captured content."""
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
import hashlib, json, datetime
W=Path(__file__).resolve().parent
assert not (W/'fetch-log.json').exists(), 'Capture already exists; no automatic retry.'
original=json.loads((W/'tree.json').read_text())
assert original.get('truncated') is False
log=[]
def get(url,path):
    row={'url':url,'method':'anonymous_GET_once','retrieved_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'saved_path':str(path.relative_to(Path.cwd()))}
    try:
        with urlopen(Request(url,headers={'User-Agent':'reader-research-public-document-capture','Accept':'application/vnd.github+json' if 'api.github.com/' in url else 'text/plain'}),timeout=30) as response:
            row.update(status=response.status,final_url=response.geturl());data=response.read()
        assert row['status']==200
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        row.update(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data));log.append(row)
        (W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n')
        return data,row
    except (HTTPError,URLError) as e:
        row.update(status=getattr(e,'code',None),error=str(e),stopped=True);log.append(row)
        (W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n')
        raise SystemExit('Stopped without retry: '+str(e))
data,meta=get('https://api.github.com/repos/readwiseio/readwise-skills/commits/master',W/'commits-master.json')
commit=json.loads(data);sha=commit['sha'];tree=commit['commit']['tree']['sha']
identity={'repository':'readwiseio/readwise-skills','ref':'master','commit':sha,'commit_tree':tree,'existing_tree':original['sha'],'tree_matches':tree==original['sha'],'existing_tree_sha256':hashlib.sha256((W/'tree.json').read_bytes()).hexdigest(),'commit_date':commit['commit']['committer']['date'],'commit_api_capture':meta}
(W/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
assert identity['tree_matches'], 'master changed relative to parent tree; stop for review'
selected=[x for x in original['tree'] if x['path']=='README.md' or (x['path'].startswith('skills/') and x['path'].endswith('/SKILL.md'))]
assert len(selected)==12 and all(x['type']=='blob' for x in selected)
for item in selected:
    url='https://raw.githubusercontent.com/readwiseio/readwise-skills/'+sha+'/'+item['path']
    data,row=get(url,W/'raw'/item['path'])
    blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    row.update(commit=sha,repo_path=item['path'],expected_git_blob=item['sha'],computed_git_blob=blob,blob_matches=blob==item['sha'])
    (W/'fetch-log.json').write_text(json.dumps(log,indent=2)+'\n')
    assert row['blob_matches'], item['path']
print(json.dumps({'commit':sha,'tree':tree,'tree_matches':True,'readme':1,'skills':11,'requests':len(log),'statuses':[x['status'] for x in log],'all_blob_matches':True}))
