"""One-time anonymous retrieval of three help pages and six pinned source files."""
import hashlib
import json
import urllib.error
import urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
WORK=Path(__file__).resolve().parent
OLD=ROOT/'work/antennapod-controls-deepening'
COMMIT='175e5ef8fe3e125a5014d70ef0c57cf85661c71b'
assert not (WORK/'fetch-log.json').exists(), 'Do not retry this capture'
tree=json.loads((OLD/'tree.json').read_text())['tree']
names=['QueueFragment.java','EpisodeMultiSelectActionHandler.java','ItemFilterDialog.java',
       'FeedItemFilterQuery.java','DownloadLogAdapter.java','DownloadLogDetailsDialog.java']
targets=[{'name':n+'.html','url':'https://antennapod.org/documentation/'+path,'kind':'help'} for n,path in [
    ('multiple-queues','queue/multiple-queues'),
    ('subscription-groups','subscriptions/subscription-groups'),
    ('refreshing-podcasts','automation/refreshing-podcasts')]]
for name in names:
    matches=[x for x in tree if x['path'].endswith('/'+name)];assert len(matches)==1
    row=matches[0]
    targets.append({'name':name,'url':f'https://raw.githubusercontent.com/AntennaPod/AntennaPod/{COMMIT}/'+row['path'],
                    'source_url':f'https://github.com/AntennaPod/AntennaPod/blob/{COMMIT}/'+row['path'],
                    'kind':'pinned_source','repo_path':row['path'],'expected_git_blob':row['sha']})
results=[]
for target in targets:
    row=target|{'checked_on':'2026-10-27','method':'anonymous_ordinary_GET_once'}
    try:
        with urllib.request.urlopen(target['url'],timeout=30) as response:
            raw=response.read();row.update(status=response.status,final_url=response.geturl())
        if row['kind']=='pinned_source':
            gitsha=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()
            assert gitsha==row['expected_git_blob'],row['name']
            row['verified_git_blob']=gitsha
        path=WORK/'raw'/row['name'];path.write_bytes(raw)
        row.update(raw_work_path=str(path.relative_to(ROOT)),raw_sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw))
    except (urllib.error.HTTPError,urllib.error.URLError) as error:
        row.update(status=getattr(error,'code',None),error=str(error),retry=False)
    results.append(row)
    print(json.dumps({k:v for k,v in row.items() if k in ['name','status','raw_sha256','verified_git_blob','error']},ensure_ascii=False))
    if row.get('status') in (401,403,412,429):break
(WORK/'fetch-log.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
# Reuse one already archived source without another GET.
match=[x for x in tree if x['path'].endswith('/SubscriptionFragment.java')][0]
path=OLD/'src'/match['path'];raw=path.read_bytes()
gitsha=hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest();assert gitsha==match['sha']
reuse={'name':'SubscriptionFragment.java','repo_path':match['path'],'raw_work_path':str(path.relative_to(ROOT)),
       'raw_sha256':hashlib.sha256(raw).hexdigest(),'verified_git_blob':gitsha,
       'url':f'https://github.com/AntennaPod/AntennaPod/blob/{COMMIT}/'+match['path'],
       'access_mode':'existing_local_source_reread','checked_on':'2026-10-27','derived_from':'E-APCD-13'}
(WORK/'local-source.json').write_text(json.dumps(reuse,ensure_ascii=False,indent=2)+'\n')
print('reused',reuse['name'],reuse['raw_sha256'])
