"""Read-only public article fetches; never execute Shortcuts or example APIs."""
import gzip,hashlib,json,re,sys
from pathlib import Path
from html.parser import HTMLParser
from urllib.request import urlopen
from urllib.error import HTTPError
W=Path('/home/workspace/reader/work/research/batch55-shortcuts-entry');W.mkdir(exist_ok=True)
class Article(HTMLParser):
 def __init__(self):super().__init__();self.on=False;self.text=[];self.images=[];self.links=[];self.body_id=None
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='body' and d.get('id'):self.on=True;self.body_id=d['id']
  if self.on and t in ['p','li','h1','h2','h3','h4','tr','pre']:self.text.append('\n')
  if self.on and t=='img':self.images.append(d)
  if self.on and t=='a':self.links.append(d)
 def handle_endtag(self,t):
  if t=='body':self.on=False
 def handle_data(self,d):
  if self.on:self.text.append(d)
def run(items):
 manifest=W/'http-manifest.json';rows=json.loads(manifest.read_text()) if manifest.exists() else []
 for item in items:
  key=item['key'];url=item['url'];assert not any(r['key']==key for r in rows),key
  try:
   with urlopen(url,timeout=25) as response:
    raw=response.read(2000000);status=response.status;final=response.url
  except Exception as ex:
   rows.append(dict(**item,error=str(ex),status=getattr(ex,'code',None),stopped=True));manifest.write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(key,str(ex));break
  p=W/(key+'.html.gz');p.write_bytes(gzip.compress(raw,mtime=0));html=raw.decode('utf-8');start=html.find("<div id='article-section'")
  parser=Article()
  if start>=0:parser.feed(html[start:])
  txt='\n'.join(' '.join(x.split()) for x in ''.join(parser.text).splitlines() if x.strip())+'\n';tp=W/(key+'.txt');tp.write_text(txt)
  selected=[]
  for opt in re.findall(r'<option\b[^>]*>.*?</option>',html,re.S):
   if re.search(r'<option\b[^>]*\bselected\b',opt):
    selected.append(dict(text=re.sub(r'<[^>]+>','',opt).strip(),url=(re.search(r"data-direct-url=['\"]([^'\"]+)",opt).group(1) if re.search(r"data-direct-url=['\"]([^'\"]+)",opt) else None)))
  row=dict(**item,status=status,final_url=final,raw_work_path=str(p.relative_to(W.parents[2])),raw_sha256=hashlib.sha256(raw).hexdigest(),compressed_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),raw_encoding='gzip',text_work_path=str(tp.relative_to(W.parents[2])),text_sha256=hashlib.sha256(txt.encode()).hexdigest(),text_chars=len(txt),body_id=parser.body_id,selected_versions=selected,images=parser.images,links=parser.links,checked_on='2026-10-27')
  rows.append(row);manifest.write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
  print(key,status,parser.body_id,selected,len(txt),flush=True)
  if start<0 or not parser.body_id or len(txt)<20:break
if __name__=='__main__':run(json.loads(Path(sys.argv[1]).read_text()))
