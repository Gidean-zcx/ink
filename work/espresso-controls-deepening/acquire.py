from pathlib import Path
from html.parser import HTMLParser
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from urllib.parse import urljoin
import json, csv, hashlib

R=Path('/home/workspace/reader'); W=R/'work/espresso-controls-deepening'; L=R/'docs/research/interaction-library'
(W/'raw').mkdir(parents=True, exist_ok=True)
assert not (W/'captures.json').exists(), 'one-time acquisition'
(W/'case-before.md').write_bytes((L/'cases/M-045-espresso.md').read_bytes())
(W/'coverage-before.json').write_text(json.dumps([r for r in csv.DictReader((L/'coverage.csv').open()) if r['product_id']=='M-045'],ensure_ascii=False,indent=2)+'\n')
(W/'evidence-before.json').write_text(json.dumps([json.loads(l) for l in (L/'evidence.jsonl').read_text().splitlines() if json.loads(l)['product_id']=='M-045'],ensure_ascii=False,indent=2)+'\n')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
class Parse(HTMLParser):
 def __init__(self): super().__init__();self.skip=0;self.parts=[];self.links=[];self.images=[];self.headings=[];self.heading=None;self.link=None
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if tag in ['script','style','noscript']:self.skip+=1
  if tag in ['p','div','li','h1','h2','h3','h4','section','tr','br']:self.parts.append('\n')
  if tag in ['h1','h2','h3','h4']:self.heading={'tag':tag,'text':'','id':d.get('id')}
  if tag=='a' and d.get('href'):self.link={'href':d['href'],'text':''}
  if tag=='img':self.images.append(d)
 def handle_endtag(self,tag):
  if tag in ['script','style','noscript']:self.skip=max(self.skip-1,0)
  if tag in ['h1','h2','h3','h4'] and self.heading:self.headings.append(self.heading);self.heading=None
  if tag=='a' and self.link:self.links.append(self.link);self.link=None
  if tag in ['p','div','li','h1','h2','h3','h4','section','tr']:self.parts.append('\n')
 def handle_data(self,data):
  if not self.skip:
   self.parts.append(data)
   if self.heading:self.heading['text']+=data
   if self.link:self.link['text']+=data
captures=[]
for name,url in [
 ('hku-guide','https://libguides.lib.hku.hk/espresso'),
 ('historical-finish','https://alearningaday.blog/2017/07/26/delight-to-use-and-the-economist-espresso-thinking-product/'),
 ('content-api','https://developer.economist.com/docs')]:
 c={'name':name,'url':url,'checked_on':'2026-10-27'}
 try:
  with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=25) as res:
   body=res.read();c.update(status=res.status,final_url=res.url,content_type=res.headers.get('Content-Type'))
 except HTTPError as ex:
  body=ex.read();c.update(status=ex.code,final_url=ex.url,content_type=ex.headers.get('Content-Type'),readable_source=False)
 except Exception as ex:
  c['error']=str(ex);captures.append(c);continue
 p=W/'raw'/f'{name}.html';p.write_bytes(body);c.update(raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p))
 if c['status']==200:
  parser=Parse();parser.feed(body.decode('utf-8',errors='replace'))
  txt='\n'.join(s.strip() for s in ''.join(parser.parts).splitlines() if s.strip())+'\n'
  t=p.with_suffix('.txt');t.write_text(txt);c.update(text_work_path=str(t.relative_to(R)),text_sha256=sha(t))
  st=p.with_name(name+'-structure.json');st.write_text(json.dumps({'headings':parser.headings,'links':parser.links,'images':parser.images},ensure_ascii=False,indent=2)+'\n');c.update(structure_path=str(st.relative_to(R)),structure_sha256=sha(st))
 captures.append(c)
(W/'captures.json').write_text(json.dumps(captures,ensure_ascii=False,indent=2)+'\n')
print(json.dumps([{k:v for k,v in x.items() if k in ['name','status','error']} for x in captures]))
