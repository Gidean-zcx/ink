from pathlib import Path
from html.parser import HTMLParser
import re,json
P=Path('work/shortcuts-controls-deepening')
class Body(HTMLParser):
 def __init__(self):super().__init__();self.on=False;self.text=[];self.links=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='body' and d.get('id'):self.on=True
  if self.on and t in ['p','li','h1','h2','h3','tr']:self.text.append('\n')
  if self.on and t=='img' and d.get('alt'):self.text.append(' [IMAGE: '+d['alt']+'] ')
  if self.on and t=='a':self.links.append(d)
 def handle_endtag(self,t):
  if t=='body':self.on=False
 def handle_data(self,d):
  if self.on:self.text.append(d)
for p in P.glob('*.html'):
 s=p.read_text();start=s.find("<div id='article-section'")
 if start<0:continue
 x=Body();x.feed(s[start:]);txt='\n'.join(l.strip() for l in ''.join(x.text).splitlines() if l.strip());(P/(p.stem+'.txt')).write_text(txt+'\n');(P/(p.stem+'-links.json')).write_text(json.dumps(x.links,ensure_ascii=False,indent=2))
 print(p.stem,len(txt))
