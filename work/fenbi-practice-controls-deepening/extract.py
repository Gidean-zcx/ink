from pathlib import Path
from html.parser import HTMLParser
import json
w=Path('work/fenbi-practice-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.s=0;self.a=[];self.images=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ('script','style'):self.s+=1
  if t in ('p','div','br','h1','h2','h3','li'):self.a.append('\n')
  if t=='img' and 'exp-image-default' in d.get('class',''):self.images.append(d)
 def handle_endtag(self,t):
  if t in ('script','style'):self.s=max(0,self.s-1)
 def handle_data(self,d):
  if not self.s:self.a.append(d)
out={}
for f in sorted((w/'raw').glob('*.html')):
 p=P();p.feed(f.read_text(errors='replace'));o='\n'.join(x.strip() for x in ''.join(p.a).splitlines() if x.strip());f.with_suffix('.txt').write_text(o+'\n');out[f.stem]=p.images
(w/'image-directory.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
