from html.parser import HTMLParser
from pathlib import Path
import sys,json,re
class P(HTMLParser):
 def __init__(self):super().__init__();self.text=[];self.links=[];self.a=None;self.skip=0
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ('script','style'):self.skip+=1
  if t in ('p','h1','h2','h3','h4','li','br'):self.text.append('\n')
  if t=='a':self.a=[d.get('href',''),'']
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
  if t=='a' and self.a:self.links.append({'url':self.a[0],'title':self.a[1].strip()});self.a=None
 def handle_data(self,d):
  if not self.skip:self.text.append(d)
  if self.a:self.a[1]+=d
for arg in sys.argv[1:]:
 p=Path(arg);h=P();h.feed(p.read_text());s=re.sub('[ \t]+',' ',''.join(h.text));s=re.sub('\n\s*\n','\n',s);p.with_suffix('.txt').write_text(s)
 p.with_suffix('.links.json').write_text(json.dumps(h.links,ensure_ascii=False,indent=2))
 print(p.name)
 if 'index' in p.name:
  for a in h.links:
   if '/solutions/folders/' in a['url'] or '/solutions/articles/' in a['url']:print(a['title'],a['url'])
 else:
  m=re.search(r'Modified on:',s)
  if m:print(s[m.start():s.find("Sorry!",m.start())][:10000])
