from pathlib import Path
from html.parser import HTMLParser
import json
class Article(HTMLParser):
 def __init__(self):super().__init__();self.active=False;self.done=False;self.skip=0;self.data=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if d.get('id')=='main':self.active=True
  if self.active and t in ['script','style']:self.skip+=1
  if self.active and t in ['p','li','h1','h2','h3','h4','dt','dd','br']:self.data.append('\n')
  if self.active and t=='img' and d.get('alt'):self.data.append(' ['+d['alt']+'] ')
 def handle_endtag(self,t):
  if self.active and t in ['script','style']:self.skip=max(0,self.skip-1)
  if t=='main':self.active=False;self.done=True
 def handle_data(self,d):
  if self.active and not self.skip:self.data.append(d)
root=Path('work/sony-controls-deepening')
for row in json.loads((root/'source-manifest.json').read_text()):
 p=Article();p.feed(Path(row['path']).read_text());text='\n'.join(' '.join(line.split()) for line in ''.join(p.data).splitlines() if line.strip())
 (root/'extracted'/f"{row['name']}.txt").write_text(text+'\n')
 print(row['name'],len(text))
