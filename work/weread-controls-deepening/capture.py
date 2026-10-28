from pathlib import Path
from urllib.request import urlopen, Request
from urllib.parse import quote
from html.parser import HTMLParser
import json,hashlib,sys
W=Path('/home/workspace/reader/work/weread-controls-deepening')
class Text(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[]
 def handle_starttag(self,t,a):
  if t in ['script','style']:self.skip+=1
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.text.append(d.strip())
def get(name,url):
 rec={'name':name,'url':url,'checked_on':'2026-10-27'}
 try:
  with urlopen(Request(quote(url,safe=':/?=&'),headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as r:b=r.read();rec.update(status=r.status,final_url=r.url,bytes=len(b))
  p=W/name;p.write_bytes(b);rec.update(raw_work_path=str(p.relative_to(W.parents[1])),raw_sha256=hashlib.sha256(b).hexdigest())
  if name.endswith('.html'):
   parser=Text();parser.feed(b.decode('utf-8'));p.with_suffix('.txt').write_text('\n'.join(parser.text)+'\n')
 except Exception as e:rec.update(error=str(e),status=getattr(e,'code',None))
 log=W/'captures.json';data=json.loads(log.read_text()) if log.exists() else [];data.append(rec);log.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rec,ensure_ascii=False))
if __name__=='__main__':get(sys.argv[1],sys.argv[2])
