from pathlib import Path
from html.parser import HTMLParser
from urllib.request import urlopen
from urllib.parse import urljoin
import json,hashlib,datetime
W=Path('/home/workspace/reader/work/readwise-controls-deepening');W.joinpath('raw').mkdir(exist_ok=True)
class Parser(HTMLParser):
 def __init__(self):
  super().__init__();self.article=0;self.ignore=0;self.text=[];self.links=[];self.images=[];self.headings=[];self.a=None;self.h=None
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if tag=='article':self.article+=1
  if tag in ['script','style']:self.ignore+=1
  if tag=='a':self.a={'href':d.get('href',''),'text':'','in_article':bool(self.article)}
  if self.article:
   if tag in ['h1','h2','h3']:self.h={'level':tag,'id':d.get('id',''),'title':''};self.text.append('\n')
   if tag in ['p','li','div','br']:self.text.append('\n')
   if tag=='img':self.images.append(d)
 def handle_data(self,data):
  if self.ignore:return
  if self.a:self.a['text']+=data
  if self.h:self.h['title']+=data
  if self.article:self.text.append(data)
 def handle_endtag(self,tag):
  if tag=='article':self.article-=1
  if tag in ['script','style']:self.ignore=max(0,self.ignore-1)
  if tag=='a' and self.a:self.links.append(self.a);self.a=None
  if tag in ['h1','h2','h3'] and self.h:self.headings.append(self.h);self.h=None
  if self.article and tag in ['p','li','h1','h2','h3']:self.text.append('\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sources={'basics':'docs/faqs','saving':'docs/saving-content','organizing':'docs/organizing-content','global':'guides/ghostreader/global'}
for name in ['daily-digest','action-tags','navigation','eink','appearance','parsing','pdfs','email-newsletters','videos','sharing','filtered-views']:sources[name]='docs/faqs/'+name
results=[]
for name,tail in sources.items():
 u='https://docs.readwise.io/reader/'+tail;p=W/'raw'/f'{name}.html'
 if name=='basics':
  a=json.loads((W/'initial-fetch.json').read_text());assert sha(p)==a['sha256'];status=a['status'];final=u
 else:
  assert not p.exists(),'Do not refetch an already archived source'
  try:
   with urlopen(u,timeout=35) as r:status=r.status;final=r.url;p.write_bytes(r.read())
  except Exception as exc:
   results.append(dict(name=name,url=u,error=str(exc)));print(name,str(exc));continue
 q=Parser();q.feed(p.read_text());text='\n'.join(x.strip() for x in ''.join(q.text).splitlines() if x.strip());assert text
 t=W/'raw'/f'{name}.txt';t.write_text(text+'\n')
 meta=W/'raw'/f'{name}-structure.json';meta.write_text(json.dumps(dict(headings=q.headings,links=q.links,images=q.images),ensure_ascii=False,indent=2)+'\n')
 results.append(dict(name=name,url=u,final_url=final,status=status,raw_work_path=str(p.relative_to(W.parents[1])),raw_sha256=sha(p),text_work_path=str(t.relative_to(W.parents[1])),text_sha256=sha(t),structure_path=str(meta.relative_to(W.parents[1])),structure_sha256=sha(meta),checked_on='2026-10-27'))
 print(name,status,len(q.headings),len(q.images),len(text))
(W/'captures.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
