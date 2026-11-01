from pathlib import Path
from html.parser import HTMLParser
import json,re
class N:
 def __init__(self,tag='',attrs=None,parent=None):self.tag=tag;self.attrs=dict(attrs or []);self.parent=parent;self.children=[]
 def text(self):return ''.join(c if isinstance(c,str) else c.text() for c in self.children)
 def walk(self):
  yield self
  for c in self.children:
   if isinstance(c,N):yield from c.walk()
class P(HTMLParser):
 def __init__(self):super().__init__();self.root=N();self.stack=[self.root]
 def handle_starttag(self,t,a):
  n=N(t,a,self.stack[-1]);self.stack[-1].children.append(n)
  if t not in ['br','hr','img','input','link','meta','source','wbr','area','base','col','embed','param','track']:self.stack.append(n)
 def handle_endtag(self,t):
  for i in range(len(self.stack)-1,0,-1):
   if self.stack[i].tag==t:self.stack=self.stack[:i];break
 def handle_data(self,d):self.stack[-1].children.append(d)
def clean(s):return re.sub(r'\s+',' ',s.replace('\x00','').replace('\u200b','')).strip()
w=Path('work/hiby-controls-deepening');p=P();p.feed((w/'raw/manual-en.html').read_text());root=p.root
items=[]
for anchor in ['quick-operation','playback-settings','bluetooth','wi-fi','10-system']:
 n=next(x for x in root.walk() if x.attrs.get('id')==anchor);siblings=[x for x in n.parent.children if isinstance(x,N)];start=siblings.index(n);section=[]
 for x in siblings[start+1:]:
  if x.tag in ['h1','h2','h3']:break
  section.append(x)
 ols=[z for x in section for z in x.walk() if z.tag=='ol'];ol=max(ols,key=lambda z:sum(isinstance(c,N) and c.tag=='li' for c in z.children)) if ols else None
 assert ol is not None,anchor
 lis=[x for x in ol.children if isinstance(x,N) and x.tag=='li']
 print(anchor,len(lis))
 for i,li in enumerate(lis,1):
  direct=''.join(x if isinstance(x,str) else x.text() for x in li.children if isinstance(x,str) or x.tag not in ['ul','ol'])
  title=next((clean(z.text()) for z in li.children if isinstance(z,N) and z.tag in ['p','strong']),clean(direct))
  # first strong within first paragraph for menu labels
  strong=next((z for z in li.walk() if z.tag=='strong'),None)
  if strong:title=clean(strong.text())
  items.append({'anchor':anchor,'position':i,'title':title,'direct_text':clean(direct),'body':clean(li.text())})
  print(i,title[:100])
(w/'menu-items.json').write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n')
