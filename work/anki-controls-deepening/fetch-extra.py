from pathlib import Path
import requests,json,hashlib,concurrent.futures
from html.parser import HTMLParser
class Extract(HTMLParser):
 def __init__(self): super().__init__(); self.parts=[]; self.skip=0
 def handle_starttag(self,tag,attrs):
  if tag in ["script","style"]: self.skip+=1
 def handle_endtag(self,tag):
  if tag in ["script","style"]: self.skip=max(0,self.skip-1)
 def handle_data(self,data):
  if not self.skip and data.strip(): self.parts.append(data.strip())
out=Path('work/anki-controls-deepening')
urls={'release-23.12.html':'https://github.com/ankitects/anki/releases/tag/23.12','scheduler-2021.md':'https://docs.ankiweb.net/faqs/the-2021-scheduler.md','issue-4228.html':'https://github.com/ankitects/anki/issues/4228'}
def fetch(item):
 n,u=item; d={'name':n,'url':u,'checked_on':'2026-10-27'}
 try:
  r=requests.get(u,timeout=(5,18));d.update(status=r.status_code,final_url=r.url,content_type=r.headers.get('content-type'))
  if r.status_code==200:
   (out/n).write_bytes(r.content);d.update(raw_work_path=str(out/n),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
   if n.endswith('.html'):
    s=Extract();s.feed(r.text)
    (out/n.replace('.html','.txt')).write_text('\n'.join(s.parts)+'\n')
  else:d['stopped_without_retry']=True
 except Exception as e:d['error']=str(e)
 return d
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as p: res=list(p.map(fetch,urls.items()))
(out/'fetch-extra-log.json').write_text(json.dumps(res,indent=2)+'\n')
for x in res:print(x['name'],x.get('status',x.get('error')),x.get('raw_sha256'))
