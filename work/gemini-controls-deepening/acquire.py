from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
from concurrent.futures import ThreadPoolExecutor
import csv, json, hashlib, requests

R=Path('/home/workspace/reader'); W=R/'work/gemini-controls-deepening'; L=R/'docs/research/interaction-library'
assert not (W/'captures.json').exists(), 'one-time acquisition'
(W/'raw').mkdir(exist_ok=True)
(W/'case-before.md').write_bytes((L/'cases/M-042-gemini.md').read_bytes())
for name, rows in [('coverage',list(csv.DictReader((L/'coverage.csv').open()))),('evidence',[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()])]:
 (W/(name+'-before.json')).write_text(json.dumps([x for x in rows if x['product_id']=='M-042'],ensure_ascii=False,indent=2)+'\n')
sources=[
 ('screen','https://support.google.com/gemini/answer/15850607?hl=en'),
 ('connectors','https://support.google.com/gemini/answer/13695044?co=GENIE.Platform%3DAndroid&hl=en'),
 ('availability','https://support.google.com/gemini/table/17434654'),
 ('custom','https://support.google.com/gemini/answer/17209137'),
 ('phone','https://support.google.com/gemini/answer/15575143'),
 ('messages','https://support.google.com/gemini/answer/15574940'),
 ('live','https://support.google.com/gemini/answer/15274899'),
 ('locked','https://support.google.com/gemini/answer/14576209?hl=en&ref_topic=14554810')]
class Parse(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.parts=[];self.links=[];self.a=None;self.headings=[];self.heading=None
 def handle_starttag(self,t,attrs):
  d=dict(attrs)
  if t in ('script','style'):self.skip+=1
  if t in ('p','li','h1','h2','h3','h4','tr','td','div'):self.parts.append('\n')
  if t=='a':self.a=[d.get('href',''),'']
  if t in ('h1','h2','h3','h4'):self.heading=[t,'']
 def handle_data(self,s):
  if self.skip:return
  self.parts.append(s)
  if self.a:self.a[1]+=s
  if self.heading:self.heading[1]+=s
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
  if t=='a' and self.a:self.links.append(self.a);self.a=None
  if t in ('h1','h2','h3','h4') and self.heading:self.headings.append(self.heading);self.heading=None
  if t in ('p','li','h1','h2','h3','h4','tr','td','div'):self.parts.append('\n')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fetch(x):
 name,url=x;record=dict(name=name,url=url,checked_on='2026-10-27')
 try:
  res=requests.get(url,timeout=40);record.update(status=res.status_code,final_url=res.url)
  raw=W/'raw'/(name+'.html');raw.write_bytes(res.content)
  record.update(raw_work_path=str(raw.relative_to(R)),raw_sha256=digest(raw),bytes=len(res.content))
  if res.status_code==200:
   p=Parse();p.feed(res.text);txt=W/'raw'/(name+'.txt');txt.write_text('\n'.join(s.strip() for s in ''.join(p.parts).splitlines() if s.strip())+'\n')
   record.update(text_work_path=str(txt.relative_to(R)),text_sha256=digest(txt),headings=p.headings,links=[dict(url=urljoin(res.url,u),title=t.strip()) for u,t in p.links if u and t.strip()])
 except Exception as e:record['error']=str(e)
 return record
with ThreadPoolExecutor(max_workers=4) as pool:records=list(pool.map(fetch,sources))
(W/'captures.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
print(json.dumps([{k:x[k] for k in ('name','status','bytes','error') if k in x} for x in records]))
