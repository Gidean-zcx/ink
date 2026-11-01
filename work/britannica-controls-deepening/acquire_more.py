from pathlib import Path
from html.parser import HTMLParser
import urllib.request,urllib.error,json,hashlib,re
R=Path('/home/workspace/reader');W=R/'work/britannica-controls-deepening';log=json.loads((W/'captures.json').read_text())
def sha(b):return hashlib.sha256(b).hexdigest()
class Parse(HTMLParser):
 def __init__(self):super().__init__();self.text=[];self.skip=0;self.links=[];self.frames=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ['script','style','noscript']:self.skip+=1
  if t in ['p','div','h1','h2','h3','h4','li','br']:self.text.append('\n')
  if t=='a' and d.get('href'):self.links.append(d['href'])
  if t=='iframe':self.frames.append(d)
 def handle_endtag(self,t):
  if t in ['script','style','noscript']:self.skip-=1
 def handle_data(self,d):
  if not self.skip:self.text.append(d)
def parse(d,b):
 p=Parse();p.feed(b.decode(errors='replace'));txt=re.sub(r'\n\s*\n+','\n',''.join(p.text));tp=W/f"raw/{d['name']}.txt";tp.write_text(txt);sp=W/f"raw/{d['name']}-structure.json";sp.write_text(json.dumps(dict(links=p.links,frames=p.frames),ensure_ascii=False,indent=2)+'\n');d.update(text_work_path=str(tp.relative_to(R)),text_sha256=sha(tp.read_bytes()),structure_path=str(sp.relative_to(R)),structure_sha256=sha(sp.read_bytes()));return p
for d in log:
 if not d.get('text_work_path'):parse(d,(R/d['raw_work_path']).read_bytes())
sources=[('early-product','https://britannicaeducation.com/solutions/prek-12/britannica-school/early-elementary/'),('content-catalog','https://britannicaeducation.com/resources/britannica-school-early-elementary-content-catalog/'),('early-checklist','https://britannicaeducation.com/wp-content/uploads/2026/01/be-resource-BSchool-Early-Elementary-Getting-Started-Checklist-1.pdf'),('school-checklist','https://britannicaeducation.com/wp-content/uploads/2026/01/be-resource-BSchool-Grades-3-12-Getting-Started-Checklist.pdf')]
for name,u in sources:
 assert not any(x['name']==name for x in log)
 d=dict(name=name,url=u,checked_on='2026-10-27')
 try:
  req=urllib.request.Request(u,headers={'User-Agent':'Mozilla/5.0 (research; public documentation)'})
  with urllib.request.urlopen(req,timeout=40) as r:b=r.read();d.update(status=r.status,final_url=r.geturl(),content_type=r.headers.get('Content-Type'))
  ext='.pdf' if b.startswith(b'%PDF') else '.html';p=W/f'raw/{name}{ext}';p.write_bytes(b);d.update(raw_work_path=str(p.relative_to(R)),raw_sha256=sha(b),bytes=len(b))
  if ext=='.html':
   s=parse(d,b);print(name,'frames',s.frames)
 except urllib.error.HTTPError as e:d.update(status=e.code,error=str(e))
 except Exception as e:d.update(error=repr(e))
 log.append(d);(W/'captures.json').write_text(json.dumps(log,ensure_ascii=False,indent=2)+'\n');print(name,d.get('status'),d.get('bytes'),d.get('error'))
