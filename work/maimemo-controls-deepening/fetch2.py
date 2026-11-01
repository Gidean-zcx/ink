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
items=[('07-filter', 'https://memodocs.maimemo.com/docs/Uv65whPGIih1x2kMNOCc99k4nUe'), ('08-super', 'https://memodocs.maimemo.com/docs/AXhuwXpfFibceakKPpdc6VbVnFc'), ('09-exam', 'https://memodocs.maimemo.com/docs/TrSUwLo8wiwrUtkkmSBcMipgncb'), ('10-pdf', 'https://memodocs.maimemo.com/docs/QbA7wb0gKiTSSyk66RzcYJxln3g'), ('11-open', 'https://memodocs.maimemo.com/docs/open/'), ('12-api-site', 'https://open.maimemo.com/'), ('13-streak', 'https://memodocs.maimemo.com/docs/XMenwad9oiNVWAktTqOca6dKnqc'), ('14-ios-audio', 'https://memodocs.maimemo.com/docs/SFzIwQTQniAAP2kZzggcgdNNntb'), ('15-harmony', 'https://memodocs.maimemo.com/docs/product-log-memo-harmonyos')]
rows=list(ThreadPoolExecutor(max_workers=3).map(get,items));(b/'fetch-results2.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
