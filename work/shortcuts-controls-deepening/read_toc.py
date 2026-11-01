from html.parser import HTMLParser
from pathlib import Path
import json
class Parser(HTMLParser):
 def __init__(self):super().__init__();self.hit=None;self.rows=[];self.body=False;self.text=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='a' and 'toc-item' in d.get('class','').split():self.hit={'kind':'topic','title':'','url':d.get('href'),'topic_id':d.get('data-tocid')}
  if t=='button' and d.get('data-ss-analytics-link-component_type')=='toc list':self.hit={'kind':'group','title':'','url':None,'topic_id':None}
 def handle_endtag(self,t):
  if self.hit and t in ['a','button']:
   self.hit['title']=' '.join(self.hit['title'].split());self.rows.append(self.hit);self.hit=None
 def handle_data(self,d):
  if self.hit:self.hit['title']+=d
if __name__=='__main__':
 p=Path('work/shortcuts-controls-deepening/02-model26.html');x=Parser();x.feed(p.read_text());(p.parent/'toc27.json').write_text(json.dumps(x.rows,ensure_ascii=False,indent=2));print('total',len(x.rows))
 for r in x.rows: print(r['kind'],r['title'],r['topic_id'],r['url'])
