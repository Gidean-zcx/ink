from pathlib import Path
from html.parser import HTMLParser
import requests,json,hashlib,sys
W=Path('work/palma-controls-deepening')
class Parser(HTMLParser):
 def __init__(self):super().__init__();self.parts=[];self.skip=0;self.links=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ['script','style']:self.skip+=1
  if t=='a' and d.get('href'):self.links.append(d['href'])
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.parts.append(d.strip())
for key,url in zip(sys.argv[1::2],sys.argv[2::2]):
 row={'key':key,'url':url,'checked_on':'2026-10-27','access_mode':'ordinary_anonymous_GET','hands_on':False,'visual_observed':False}
 try:
  r=requests.get(url,timeout=40)
  pdf=r.content.startswith(b'%PDF');suffix='.pdf' if pdf else '.html';p=W/'raw'/(key+suffix);p.write_bytes(r.content)
  row.update(http_status=r.status_code,final_url=r.url,redirects=[{'status':x.status_code,'url':x.url} for x in r.history],raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),content_type=r.headers.get('Content-Type'))
  if r.status_code==200 and not pdf:
   text=r.content.decode('utf-8',errors='replace');obj=Parser();obj.feed(text);txt=p.with_suffix('.txt');txt.write_text('\n'.join(obj.parts))
   row.update(extracted_work_path=str(txt),extracted_sha256=hashlib.sha256(txt.read_bytes()).hexdigest())
   p.with_suffix('.links.json').write_text(json.dumps(obj.links,ensure_ascii=False,indent=2)+'\n')
   if any(x in text.lower() for x in ['cf-chl-opt','captcha.show()','challenge-platform']):row.update(route_stopped=True,readability='challenge_detected')
  elif r.status_code!=200:row['route_stopped']=True
 except Exception as exc:row.update(error=repr(exc),route_stopped=True)
 with (W/'captures.jsonl').open('a') as f:f.write(json.dumps(row,ensure_ascii=False)+'\n')
 print(json.dumps(row,ensure_ascii=False),flush=True)
