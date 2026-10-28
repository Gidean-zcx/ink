from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit,urlunsplit,unquote
import urllib.request,urllib.error,json,hashlib,sys,concurrent.futures
W=Path('work/yoto-controls-deepening');(W/'raw').mkdir(exist_ok=True);(W/'extracted').mkdir(exist_ok=True)
class Initial(HTMLParser):
 def handle_starttag(self,t,a):
  d=dict(a)
  if d.get('id')=='initial-data':self.x=json.loads(d['data-json'])
class Text(HTMLParser):
 def __init__(self):super().__init__();self.parts=[];self.links=[];self.images=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ['h1','h2','h3','h4','p','li','tr','br']:self.parts.append('\n')
  if t=='a':self.links.append(d.get('href'))
  if t=='img':self.images.append(d)
 def handle_data(self,d):self.parts.append(d)
def norm(u):
 s=urlsplit(u);return urlunsplit((s.scheme,s.netloc.lower(),unquote(s.path).rstrip('/'),'',''))
def get(item):
 name,url=item;raw=W/'raw'/f'{name}.html'
 try:
  if raw.exists():d=raw.read_bytes();status=200;final=url;mode='reuse_this_batch_capture'
  else:
   r=urllib.request.urlopen(url,timeout=30);d=r.read();status=r.status;final=r.url;raw.write_bytes(d);mode='anonymous_public_html_GET'
  p=Initial();p.feed(d.decode());x=p.x
  out=W/'extracted'/f'{name}.json';out.write_text(json.dumps(x['data'],ensure_ascii=False,indent=2)+'\n')
  body=x['data'].get('article',{}).get('body','');p=Text();p.feed(body)
  (W/'extracted'/f'{name}.txt').write_text(''.join(p.parts)+'\n')
  rec={'name':name,'url':url,'final_url':final,'http_status':status,'access_mode':mode,'raw_work_path':str(raw),'raw_sha256':hashlib.sha256(d).hexdigest(),'extracted_work_path':str(out),'extracted_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'checked_on':'2026-10-27','title':x.get('title'),'article_meta':{k:v for k,v in x['data'].get('article',{}).items() if k not in ['body']},'links':p.links,'images':p.images}
  print(name,status,len(d),'article' if body else 'category',flush=True);return rec
 except urllib.error.HTTPError as e:
  print(name,'STOP_HTTP',e.code,flush=True);return {'name':name,'url':url,'http_status':e.code,'access_mode':'stopped','checked_on':'2026-10-27'}
if __name__=='__main__':
 tasks=json.loads(Path(sys.argv[1]).read_text());mp=W/'source-manifest.json';old=json.loads(mp.read_text()) if mp.exists() else []
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:rows=list(ex.map(get,tasks))
 idx={r['name']:r for r in old};idx.update({r['name']:r for r in rows});mp.write_text(json.dumps(list(idx.values()),ensure_ascii=False,indent=2)+'\n')
