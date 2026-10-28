from pathlib import Path
from html.parser import HTMLParser
import json
W=Path('/home/workspace/reader/work/gemini-controls-deepening')
class Table(HTMLParser):
 def __init__(self):super().__init__();self.active=False;self.rows=[];self.row=None;self.cell=None
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='table':self.active=True
  if not self.active:return
  if t=='tr':self.row={'category':d.get('data-category'),'cells':[],'links':[]}
  if t=='td':self.cell=[]
  if t in ('p','li','br') and self.cell is not None:self.cell.append('\n')
  if t=='a' and self.row is not None:self.row['links'].append(d.get('href'))
 def handle_data(self,s):
  if self.cell is not None:self.cell.append(s)
 def handle_endtag(self,t):
  if not self.active:return
  if t=='td' and self.cell is not None:self.row['cells'].append('\n'.join(x.strip() for x in ''.join(self.cell).splitlines() if x.strip()));self.cell=None
  if t=='tr' and self.row:
   if self.row['cells']:self.rows.append(self.row)
   self.row=None
  if t=='table':self.active=False
p=Table();p.feed((W/'raw/availability.html').read_text())
excluded=[r for r in p.rows if not r['category']]
assert len(excluded)==1 and excluded[0]['cells']==['Try your keywords on Google Web Search.']
(W/'table-excluded.json').write_text(json.dumps(excluded,ensure_ascii=False,indent=2)+'\n')
p.rows=[r for r in p.rows if r['category']];assert all(len(r['cells'])==3 for r in p.rows)
(W/'connected-app-table.json').write_text(json.dumps(p.rows,ensure_ascii=False,indent=2)+'\n')
print('rows',len(p.rows))
for i,r in enumerate(p.rows,1):print(i,r['category'],r['cells'][0].split(':')[0])
