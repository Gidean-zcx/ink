import json,concurrent.futures,re
from pathlib import Path
from html.parser import HTMLParser
from fetch import fetch
P=Path('work/shortcuts-controls-deepening')
targets=[('06-guide27','welcome'),('07-create27','apd84c576f8c'),('08-actions27','apdc33e4f4da'),('09-graph27','apd4618db957'),('10-variable27','apda36b9018b'),('11-filter27','apdbdab3433f'),('12-auto-intro27','apd690170742'),('13-auto-add27','apdfbdbd7123'),('14-auto-events27','apd932ff833f'),('15-auto-travel27','apd8ebfc4e8e'),('16-auto-comms27','apdd711f9dff'),('17-auto-settings27','apde31e9638b'),('18-auto-transaction27','apd65c67538a'),('19-share27','apdf01f8c054'),('20-import27','apdf330fd3a0'),('21-delete27','apd096cd65c6'),('22-alert27','apdb9661c761'),('23-notification27','apd2175adcab'),('24-receive27','apd350ce757a'),('25-app-shortcuts27','apd43295406d')]
items=[(k,f'https://support.apple.com/guide/shortcuts/{pid}/10.0/ios/27') for k,pid in targets]+[('26-release27','https://support.apple.com/en-us/149045'),('27-release26','https://support.apple.com/en-us/123075')]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:out=list(ex.map(fetch,items))
old=json.loads((P/'fetch-results.json').read_text());(P/'fetch-results.json').write_text(json.dumps(old+out,ensure_ascii=False,indent=2))
class Body(HTMLParser):
 def __init__(self):super().__init__();self.on=False;self.text=[];self.links=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='body' and d.get('id'):self.on=True
  if self.on and t in ['p','li','h1','h2','h3','tr']:self.text.append('\n')
  if self.on and t=='img' and d.get('alt'):self.text.append(' [IMAGE: '+d['alt']+'] ')
  if self.on and t=='a':self.links.append(d)
 def handle_endtag(self,t):
  if t=='body' and self.on:self.on=False
 def handle_data(self,d):
  if self.on:self.text.append(d)
for p in P.glob('*.html'):
 s=p.read_text();b=Body();b.feed(s)
 if b.text:(P/(p.stem+'.txt')).write_text(''.join(b.text));(P/(p.stem+'-links.json')).write_text(json.dumps(b.links,ensure_ascii=False,indent=2))
for d in out:print(d['key'],d.get('status',d.get('error')),d.get('final_url'))
