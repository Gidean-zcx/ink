from pathlib import Path
from html.parser import HTMLParser
import urllib.request,urllib.error,json,hashlib,sys,concurrent.futures
W=Path('work/hiby-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.inside=0;self.parts=[];self.heads=[];self.links=[];self.images=[];self.h=None;self.htext=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='article':self.inside+=1
  if t=='a':self.links.append(d.get('href',''))
  if self.inside:
   if t in ['h1','h2','h3','h4','h5']:self.h=(t,d.get('id'));self.htext=[]
   if t in ['h1','h2','h3','h4','h5','p','li','tr','br']:self.parts.append('\n')
   if t=='img':self.images.append(d)
 def handle_endtag(self,t):
  if self.h and t==self.h[0]:self.heads.append({'level':t,'id':self.h[1],'text':''.join(self.htext)});self.h=None
  if t=='article':self.inside-=1
 def handle_data(self,d):
  if self.inside:
   self.parts.append(d)
   if self.h:self.htext.append(d)
def get(z):
 name,u=z;p=W/'raw'/(name+'.html')
 try:
  if p.exists():b=p.read_bytes();status=200;final=u
  else:
   r=urllib.request.urlopen(u,timeout=30);b=r.read();status=r.status;final=r.url;p.write_bytes(b)
  t=P();t.feed(b.decode());q=W/'extracted'/(name+'.txt');q.write_text(''.join(t.parts)+'\n');r={'name':name,'url':u,'final_url':final,'http_status':status,'checked_on':'2026-10-27','raw_work_path':str(p),'raw_sha256':hashlib.sha256(b).hexdigest(),'extracted_work_path':str(q),'extracted_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'headings':t.heads,'images':t.images,'links':t.links};print(name,status,'headings',len(t.heads),'images',len(t.images),flush=True);return r
 except urllib.error.HTTPError as e:print(name,'STOP_HTTP',e.code,flush=True);return {'name':name,'url':u,'http_status':e.code,'access_mode':'stopped'}
if __name__=='__main__':
 mp=W/'source-manifest.json';old=json.loads(mp.read_text()) if mp.exists() else [];tasks=json.loads(Path(sys.argv[1]).read_text())
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:new=list(ex.map(get,tasks))
 z={r['name']:r for r in old};z.update({r['name']:r for r in new});mp.write_text(json.dumps(list(z.values()),ensure_ascii=False,indent=2)+'\n')
