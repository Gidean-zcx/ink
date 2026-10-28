import concurrent.futures
import hashlib
import json
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

out = Path('/home/workspace/reader/work/learning-control-deepening')
out.mkdir(parents=True, exist_ok=True)
base = 'https://api.xunfeixxj.com/assistant/'

class Text(HTMLParser):
    def __init__(self):
        super().__init__(); self.parts=[]
    def handle_data(self, data):
        if data.strip(): self.parts.append(data.strip())

def fetch(job):
    name, endpoint, payload = job
    request = urllib.request.Request(base + endpoint,
        data=json.dumps(payload).encode(), method='POST',
        headers={'Content-Type':'application/json', 'User-Agent':'Mozilla/5.0'})
    result={'name':name, 'url':base+endpoint, 'method':'POST', 'payload':payload}
    try:
        with urllib.request.urlopen(request,timeout=25) as response:
            raw=response.read(); result['http_status']=response.status
        path=out/(name+'.json'); path.write_bytes(raw)
        result.update(raw_work_path=str(path.relative_to(out.parents[1])),
                      raw_sha256=hashlib.sha256(raw).hexdigest(), bytes=len(raw))
        data=json.loads(raw); result['application_status']=data.get('status')
        if 'guideContent' in data.get('data',{}):
            parser=Text(); parser.feed(data['data']['guideContent'])
            (out/(name+'.txt')).write_text('\n'.join(parser.parts)+'\n')
            result['title']=data['data'].get('guideTitle')
    except Exception as error:
        result['error']=str(error)
    return result

# Exact public read-only requests used by the official help frontend.
jobs=[('guide-list','guide/getGuideList',{})]
jobs += [('guide-'+str(i),'guide/getGuideDetail',{'guideId':i})
         for i in [13,15,16,17,18,19,90,91,92,94]]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    results=list(pool.map(fetch,jobs))
(out/'fetch-log.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for result in results:
    print(json.dumps({k:result[k] for k in ['name','http_status','application_status','title','error'] if k in result},ensure_ascii=False))
