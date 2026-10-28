import urllib.request,urllib.error,hashlib,json,sys
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
work=Path('/home/workspace/reader/work/inoreader-controls-deepening')
class Parser(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.parts=[];self.links=[];self.a=None
 def handle_starttag(self,t,attrs):
  a=dict(attrs)
  if t in ('script','style'):self.skip+=1
  if self.skip:return
  if t in ('h1','h2','h3','h4','p','li','br','div','section'):self.parts.append('\n')
  if t=='a':self.a=[a.get('href',''),'']
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
  if self.skip:return
  if t=='a' and self.a:self.links.append(dict(href=self.a[0],text=self.a[1].strip()));self.a=None
  if t in ('h1','h2','h3','h4','p','li','div','section'):self.parts.append('\n')
 def handle_data(self,d):
  if self.skip:return
  self.parts.append(d)
  if self.a:self.a[1]+=d

def fetch(key,url):
 p=work/(key+'.html');meta=work/(key+'.meta.json')
 if meta.exists():return json.loads(meta.read_text())
 try:
  with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=25) as resp:
   data=resp.read();record=dict(key=key,url=url,final_url=resp.url,status=resp.status,raw_work_path=str(p.relative_to('/home/workspace/reader')),raw_sha256=hashlib.sha256(data).hexdigest())
  p.write_bytes(data);parser=Parser();parser.feed(data.decode('utf-8','replace'))
  (work/(key+'.txt')).write_text('\n'.join(x.strip() for x in ''.join(parser.parts).splitlines() if x.strip())+'\n')
  (work/(key+'.links.json')).write_text(json.dumps(parser.links,ensure_ascii=False,indent=2)+'\n')
 except urllib.error.HTTPError as e:record=dict(key=key,url=url,status=e.code)
 except Exception as e:record=dict(key=key,url=url,error=str(e))
 meta.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return record
if __name__=='__main__':
 for arg in sys.argv[1:]:
  key,url=arg.split('=',1);print(json.dumps(fetch(key,url),ensure_ascii=False))
