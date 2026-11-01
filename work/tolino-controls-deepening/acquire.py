import requests,json,re,hashlib,concurrent.futures
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
W=Path('work/tolino-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[];self.links=[];self.heads=[];self.head=None;self.attrs={}
 def handle_starttag(self,t,attrs):
  a=dict(attrs)
  if t in ['script','style']:self.skip+=1
  if t in ['h1','h2','h3','h4','h5']:self.head=[];self.attrs=dict(tag=t,**a);self.text.append('\n')
  if t in ['p','li','div','br','tr']:self.text.append('\n')
  if t=='a' and a.get('href'):self.links.append(a)
 def handle_data(self,s):
  if not self.skip:self.text.append(s)
  if self.head is not None:self.head.append(s)
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip-=1
  if t in ['h1','h2','h3','h4','h5'] and self.head is not None:self.heads.append(dict(title=''.join(self.head).strip(),**self.attrs));self.head=None
  if t in ['p','li','h1','h2','h3','h4','h5']:self.text.append('\n')
def get(x):
 name,u=x;r=requests.get(u,timeout=(8,25));p=W/(name+'.html');p.write_bytes(r.content);z=P();z.feed(r.text);s='\n'.join(t.strip() for t in ''.join(z.text).splitlines() if t.strip());(W/(name+'.txt')).write_text(s+'\n');(W/(name+'-links.json')).write_text(json.dumps([dict(a,url=urljoin(r.url,a['href'])) for a in z.links],ensure_ascii=False,indent=2)+'\n');(W/(name+'-headings.json')).write_text(json.dumps(z.heads,ensure_ascii=False,indent=2)+'\n');return dict(name=name,url=u,final_url=r.url,status=r.status_code,raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),parsed_text_path=str(W/(name+'.txt')))
jobs=[('faq','https://mytolino.com/faq-group/tolino-shine-5-generation-en/page/2/'),('landing','https://mytolino.com/faq-group/tolino-shine-5-gen-en/'),('updates','https://mytolino.com/software-updates-for-tolino-ereaders/')]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:rs=list(ex.map(get,jobs))
(W/'sources.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rs,ensure_ascii=False))
