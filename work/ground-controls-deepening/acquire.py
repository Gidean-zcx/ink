import requests,json,hashlib,datetime
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
W=Path('work/ground-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[];self.links=[];self.a=None;self.a_text=[];self.heads=[];self.h=None;self.htext=[];self.images=[]
 def handle_starttag(self,t,at):
  a=dict(at)
  if t in ['script','style']:self.skip+=1
  if t in ['p','li','div','br','tr','h1','h2','h3','h4']:self.text.append('\n')
  if t=='a' and a.get('href'):self.a=a;self.a_text=[]
  if t in ['h1','h2','h3','h4']:self.h=t;self.htext=[]
  if t=='img':self.images.append(a)
 def handle_data(self,s):
  if not self.skip:self.text.append(s)
  if self.a is not None:self.a_text.append(s)
  if self.h is not None:self.htext.append(s)
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip-=1
  if t=='a' and self.a is not None:self.links.append(dict(**self.a,title=''.join(self.a_text).strip()));self.a=None
  if t==self.h:self.heads.append(dict(tag=t,title=''.join(self.htext).strip()));self.h=None
  if t in ['p','li','h1','h2','h3','h4']:self.text.append('\n')
def get(name,url):
 try:
  r=requests.get(url,timeout=(8,25));p=W/(name+'.html');p.write_bytes(r.content);z=P();z.feed(r.content.decode("utf-8",errors="replace"))
  (W/(name+'.txt')).write_text('\n'.join(t.strip() for t in ''.join(z.text).splitlines() if t.strip())+'\n')
  for suffix,items in [('links',[dict(a,url=urljoin(r.url,a['href'])) for a in z.links]),('headings',z.heads),('images',z.images)]: (W/f'{name}-{suffix}.json').write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n')
  return dict(name=name,url=url,final_url=r.url,status=r.status_code,checked_on='2026-10-27',checked_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),parsed_text_path=str(W/(name+'.txt')))
 except Exception as ex:return dict(name=name,url=url,error=type(ex).__name__+': '+str(ex))
if __name__=='__main__':
 jobs=[('help-index','https://help.ground.news/'),('factuality','https://help.ground.news/en/articles/5224577'),('methodology','https://ground.news/rating-system'),('home','https://ground.news/')]
 out=[]
 for name,url in jobs:
  x=get(name,url);out.append(x);print(json.dumps({k:v for k,v in x.items() if k in ['name','status','error','bytes']},ensure_ascii=False),flush=True)
 (W/'sources-initial.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
