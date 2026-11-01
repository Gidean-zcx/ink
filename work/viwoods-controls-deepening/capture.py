from pathlib import Path
from urllib.request import Request,urlopen
from urllib.error import HTTPError,URLError
from html.parser import HTMLParser
import json,hashlib
W=Path(__file__).resolve().parent;R=W.parents[1]
class Parser(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.out=[];self.links=[]
 def handle_starttag(self,t,a):
  if t in ('script','style'):self.skip+=1
  if t=='a':
   self.links += [v for k,v in a if k=='href']
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
 def handle_data(self,s):
  if not self.skip and s.strip():self.out.append(s.strip())
jobs=[('manual-index','https://viwoods.com/pages/user-manuals'),('update315','https://viwoods.com/pages/system-update-3-15'),('update312','https://viwoods.com/pages/system-update-3-12'),('faq','https://viwoods.com/pages/frequently-asked-questions-about-viwoods')]
report=[]
for key,url in jobs:
 row={'key':key,'url':url,'checked_on':'2026-10-27'}
 try:
  with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as r:body=r.read();row.update(status=r.status,final_url=r.url)
 except (HTTPError,URLError,TimeoutError) as e:
  row.update(error=str(e),status=getattr(e,'code',None));report.append(row);print(row);break
 p=W/'raw'/f'{key}.html';p.write_bytes(body);parser=Parser();parser.feed(body.decode('utf-8'))
 t=p.with_suffix('.txt');t.write_text('\n'.join(parser.out)+'\n')
 row.update(raw_work_path=str(p.relative_to(R)),raw_sha256=hashlib.sha256(body).hexdigest(),extracted_work_path=str(t.relative_to(R)),extracted_sha256=hashlib.sha256(t.read_bytes()).hexdigest())
 if key=='manual-index':
  links=sorted(set(x for x in parser.links if '.pdf' in x));(W/'manual-links.json').write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n');print('manual links',links)
 report.append(row);print(row)
(W/'captures.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
