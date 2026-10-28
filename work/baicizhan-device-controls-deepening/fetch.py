from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from html.parser import HTMLParser
import requests, hashlib, json
BASE=Path('work/baicizhan-device-controls-deepening')
class Page(HTMLParser):
 def __init__(self): super().__init__(); self.links=[]; self.scripts=[]; self.images=[]; self.text=[]; self.skip=0
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='a' and d.get('href'):self.links.append(d['href'])
  if t=='script' and d.get('src'):self.scripts.append(d['src'])
  if t=='img':self.images.append(d)
  if t in ('script','style'):self.skip+=1
 def handle_endtag(self,t):
  if t in ('script','style') and self.skip:self.skip-=1
 def handle_data(self,d):
  if not self.skip and d.strip():self.text.append(d.strip())
def fetch(x):
 name,url=x; row={'name':name,'url':url,'method':'GET; requests; timeout=(5,18); one attempt; no account interface'}
 try:
  r=requests.get(url, timeout=(5,18)); row.update(status=r.status_code,final_url=r.url)
  if r.status_code in (401,403,429):row['result']='stopped; no retry'; return row
  if r.status_code==200:
   data=r.content; path=BASE/(name+'.html'); path.write_bytes(data)
   p=Page();p.feed(data.decode('utf-8','replace'))
   (BASE/(name+'.txt')).write_text('\n'.join(p.text)+'\n')
   (BASE/(name+'-links.json')).write_text(json.dumps({'links':p.links,'scripts':p.scripts,'images':p.images},ensure_ascii=False,indent=2)+'\n')
   row.update(path=str(path),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),links=len(p.links),scripts=len(p.scripts),images=len(p.images))
 except requests.RequestException as e:row['error']=type(e).__name__+': '+str(e)
 return row
rows=list(ThreadPoolExecutor(max_workers=2).map(fetch,[('01-official-home','https://danciji.baicizhan.com/'),('02-52audio-ace-max','https://www.52audio.com/archives/239551.html')]))
(BASE/'fetch-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
