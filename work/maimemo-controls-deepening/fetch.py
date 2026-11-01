from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from html.parser import HTMLParser
import requests,json,hashlib
b=Path('work/maimemo-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[];self.links=[];self.a=None;self.at=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ('script','style'):self.skip+=1
  if t=='a':self.a=d.get('href');self.at=[]
 def handle_endtag(self,t):
  if t in ('script','style') and self.skip:self.skip-=1
  if t=='a' and self.a:self.links.append({'title':''.join(self.at).strip(),'url':self.a});self.a=None
 def handle_data(self,s):
  if not self.skip and s.strip():self.text.append(s.strip())
  if self.a:self.at.append(s)
def get(item):
 name,u=item;row={'name':name,'url':u,'method':'requests GET timeout=(5,18), one attempt'}
 try:
  r=requests.get(u,timeout=(5,18));row.update(status=r.status_code,final_url=r.url)
  if r.status_code in (401,403,429):row['result']='stopped route; no retry';return row
  if r.status_code==200:
   p=b/(name+'.html');p.write_bytes(r.content);p1=P();p1.feed(r.content.decode('utf-8','replace'))
   (b/(name+'.txt')).write_text('\n'.join(p1.text)+'\n');(b/(name+'-links.json')).write_text(json.dumps(p1.links,ensure_ascii=False,indent=2)+'\n');row.update(raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
 except requests.RequestException as e:row['error']=type(e).__name__+': '+str(e)
 return row
items=[('01-start','https://memodocs.maimemo.com/docs/memo-get-started/'),('02-basics','https://memodocs.maimemo.com/docs/GKfswhD6Ei4TdMk0naQcfMBtnJg'),('03-daily','https://memodocs.maimemo.com/docs/PulqwOB1xi5jV0kb8Y1cPE7ZnKc/'),('04-pie','https://memodocs.maimemo.com/docs/IAdywC1b0if5N4knUqocOxphnwr'),('05-makeup','https://memodocs.maimemo.com/docs/WVyFwrbiXiexP7k6N7gcYY0Vnzh'),('06-checkin-error','https://memodocs.maimemo.com/docs/Ai7rwDzjpi9RelkODxmcGcPnncA')]
rows=list(ThreadPoolExecutor(max_workers=3).map(get,items));(b/'fetch-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
