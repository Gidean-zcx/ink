from pathlib import Path
from urllib.request import Request, urlopen
from urllib.parse import quote
from urllib.error import HTTPError, URLError
from html.parser import HTMLParser
import json,hashlib,sys
W=Path('/home/workspace/reader/work/baicizhan-controls-deepening')
class T(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[]
 def handle_starttag(self,t,a):
  if t in ['script','style']:self.skip+=1
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.text.append(d.strip())
name,url=sys.argv[1:3]
record={'name':name,'url':url,'checked_on':'2026-10-27','access_mode':'ordinary_anonymous_GET'}
try:
 with urlopen(Request(quote(url,safe=':/?=&%'),headers={'User-Agent':'Mozilla/5.0'}),timeout=25) as r:
  body=r.read();record.update(status=r.status,final_url=r.url,bytes=len(body))
 raw=W/name;raw.write_bytes(body);record.update(raw_work_path=str(raw.relative_to(W.parents[1])),raw_sha256=hashlib.sha256(body).hexdigest())
 if name.endswith('.html'):
  t=T();t.feed(body.decode('utf-8'));raw.with_suffix('.txt').write_text('\n'.join(t.text)+'\n')
except (HTTPError,URLError,TimeoutError) as error:
 record.update(status=getattr(error,'code',None),error=str(error),stopped=True)
(W/(name+'.capture.json')).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(record,ensure_ascii=False))
