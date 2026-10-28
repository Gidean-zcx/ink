from pathlib import Path
from html.parser import HTMLParser
import json,collections
P=Path('work/shortcuts-controls-deepening')
class TOC(HTMLParser):
 def __init__(self):super().__init__();self.stack=[];self.hit=None;self.rows=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='li':self.stack.append(None)
  if (t=='a' and 'toc-item' in d.get('class','').split()) or (t=='button' and d.get('data-ss-analytics-link-component_type')=='toc list'):
   group=t=='button';ident=d.get('aria-controls','').removeprefix('node-children-') if group else d.get('data-tocid')
   self.hit={'kind':'group' if group else 'topic','id':ident,'parent':next((x for x in reversed(self.stack) if x),''),'title':'','url':d.get('href','')}
 def handle_endtag(self,t):
  if self.hit and t in ['a','button']:
   self.hit['title']=' '.join(self.hit['title'].split());self.rows.append(self.hit)
   if self.hit['kind']=='group' and self.stack:self.stack[-1]=self.hit['id']
   self.hit=None
  if t=='li' and self.stack:self.stack.pop()
 def handle_data(self,d):
  if self.hit:self.hit['title']+=d
x=TOC();x.feed((P/'06-guide27.html').read_text());assert len(x.rows)==len({r['id'] for r in x.rows})
(P/'toc27-fixed.json').write_text(json.dumps(x.rows,ensure_ascii=False,indent=2)+'\n')
print('COUNTS',dict(collections.Counter(r['kind'] for r in x.rows)))
for r in x.rows:
 if r['kind']=='group': print(r['id'],r['parent'],r['title'])
assert all('/10.0/ios/27' in r['url'] or (r['id']=='APDA2B83D0E0' and r['url']=='https://support.apple.com/guide/shortcuts/welcome/ios') for r in x.rows if r['kind']=='topic')
