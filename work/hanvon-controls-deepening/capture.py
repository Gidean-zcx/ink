from pathlib import Path
from html.parser import HTMLParser
import requests,json,hashlib,sys
W=Path('work/hanvon-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.text=[];self.skip=0;self.links=[]
 def handle_starttag(self,t,attrs):
  a=dict(attrs)
  if t in ['script','style']:self.skip+=1
  if t=='a' and a.get('href'):self.links.append(a['href'])
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.text.append(d.strip())
for key,url in zip(sys.argv[1::2],sys.argv[2::2]):
 rec=dict(key=key,url=url,checked_on='2026-10-27',access_mode='ordinary_anonymous_GET',hands_on=False,visual_observed=False)
 try:
  r=requests.get(url,timeout=35)
  p=W/'raw'/f'{key}.html';p.write_bytes(r.content)
  rec.update(http_status=r.status_code,final_url=r.url,raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),content_type=r.headers.get('Content-Type'))
  if r.status_code==200:
   obj=P();obj.feed(r.content.decode('utf-8',errors='replace'));t=p.with_suffix('.txt');t.write_text('\n'.join(obj.text))
   rec.update(extracted_work_path=str(t),extracted_sha256=hashlib.sha256(t.read_bytes()).hexdigest())
   p.with_suffix('.links.json').write_text(json.dumps(obj.links,ensure_ascii=False,indent=2)+'\n')
  else:rec['route_stopped']=True
 except Exception as exc:rec.update(error=repr(exc),route_stopped=True)
 with (W/'captures.jsonl').open('a') as f:f.write(json.dumps(rec,ensure_ascii=False)+'\n')
 print(json.dumps(rec,ensure_ascii=False),flush=True)
