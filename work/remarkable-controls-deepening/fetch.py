import requests,re,json,hashlib,concurrent.futures
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urljoin
W=Path('/home/workspace/reader/work/remarkable-controls-deepening')
class Parse(HTMLParser):
 def __init__(self):super().__init__();self.depth=0;self.parts=[];self.current=[];self.links=[];self.images=[];self.alltext=[]
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if tag=='div':
   if self.depth:self.depth+=1
   elif 'sfdc_richtext' in a.get('class',''):self.depth=1;self.current=[]
  if tag in ['p','li','h1','h2','h3','br','tr']:self.alltext.append('\n');self.current.append('\n') if self.depth else None
  if tag=='a' and a.get('href'):self.links.append(a['href'])
  if tag=='img' and self.depth:self.images.append(a)
 def handle_endtag(self,tag):
  if tag=='div' and self.depth:
   self.depth-=1
   if self.depth:self.current.append('\n')
   else:self.parts.append(''.join(self.current))
  if tag in ['p','li','h1','h2','h3','tr'] and self.depth:self.current.append('\n')
 def handle_data(self,t):
  self.alltext.append(t)
  if self.depth:self.current.append(t)
def clean(s):return '\n'.join(x.strip() for x in s.splitlines() if x.strip())
def fetch(slug):
 u='https://support.remarkable.com/articles/Knowledge/'+slug
 try:
  p=W/(slug+'.html')
  if slug=='Release-notes-overview' and (W/'release.html').exists():r=None;data=(W/'release.html').read_bytes();status=200;final=u
  else:
   r=requests.get(u,timeout=25);data=r.content;status=r.status_code;final=r.url
  p.write_bytes(data);q=Parse();q.feed(data.decode('utf-8','replace'));parts=[clean(x) for x in q.parts];body=max(parts,key=len) if parts else '';alltext=clean(''.join(q.alltext));date=re.search(r'Last Published Date\s*\n?([^\n]+)',alltext)
  (W/(slug+'.txt')).write_text(body+'\n');(W/(slug+'.all.txt')).write_text(alltext+'\n')
  links=list(dict.fromkeys(urljoin(final,x) for x in q.links if not x.startswith('javascript:')))
  (W/(slug+'.links.json')).write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n');(W/(slug+'.images.json')).write_text(json.dumps(q.images,ensure_ascii=False,indent=2)+'\n')
  m=dict(key=slug,url=u,final_url=final,status=status,body_chars=len(body),richtext_blocks=len(parts),published_raw=date.group(1) if date else None,raw_work_path=str(p.relative_to(W.parent.parent)),raw_sha256=hashlib.sha256(data).hexdigest())
  (W/(slug+'.meta.json')).write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');return m
 except Exception as e:return dict(key=slug,url=u,error=str(e))
if __name__=='__main__':
 import sys
 names=sys.argv[1:]
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:rs=list(pool.map(fetch,names))
 for r in rs:print(json.dumps(r,ensure_ascii=False))
