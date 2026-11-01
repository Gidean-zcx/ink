from pathlib import Path
import urllib.request,urllib.error,hashlib,json,concurrent.futures
from html.parser import HTMLParser
w=Path('work/shanling-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.out=[];self.skip=0;self.links=[];self.a=None
 def handle_starttag(self,t,a):
  if t in ('script','style'):self.skip+=1
  if t=='a':self.a=[dict(a).get('href',''),'']
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
  if t=='a' and self.a:self.links.append(self.a);self.a=None
 def handle_data(self,d):
  if not self.skip and d.strip():self.out.append(d.strip())
  if self.a:self.a[1]+=d.strip()
urls={'eddict-download':'https://shanling.com/download/76','mtouch-old':'https://en.shanling.com/article/101376','eddict-old':'https://en.shanling.com/article/101309','m1plus-v21':'https://en.shanling.com/download/118','m1plus-v18':'https://shanling.com/download/134','review':'https://www.hifioasis.com/reviews/shanling-m1-plus-review/'}
def get(kv):
 name,url=kv
 try:
  with urllib.request.urlopen(url,timeout=30) as r:b=r.read();code=r.status;final=r.url
  p=w/'raw'/f'{name}.html';p.write_bytes(b);s=P();s.feed(b.decode('utf-8'));t=w/'extracted'/f'{name}.txt';t.write_text('\n'.join(s.out)+'\n');j=w/'extracted'/f'{name}-links.json';j.write_text(json.dumps(s.links,ensure_ascii=False,indent=2)+'\n')
  return dict(name=name,url=url,status=code,final_url=final,raw_work_path=str(p),raw_sha256=hashlib.sha256(b).hexdigest(),extracted_work_path=str(t),extracted_sha256=hashlib.sha256(t.read_bytes()).hexdigest())
 except urllib.error.HTTPError as e:return dict(name=name,url=url,status=e.code,route_stopped=True)
 except Exception as e:return dict(name=name,url=url,error=str(e),route_stopped=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(get,urls.items()))
(w/'captures.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
for x in rows:print(x['name'],x.get('status'),x.get('error',''))
