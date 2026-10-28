import urllib.request,urllib.error,pathlib,hashlib,json,concurrent.futures
from html.parser import HTMLParser
w=pathlib.Path('work/quaderno-controls-deepening')
class P(HTMLParser):
 def __init__(self):super().__init__();self.text=[];self.skip=0
 def handle_starttag(self,t,a):
  if t in ('script','style'):self.skip+=1
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.text.append(d.strip())
def parse(name,b):
 s=P();s.feed(b.decode('cp932' if b'Shift_JIS' in b[:1000] else 'utf-8'));p=w/'extracted'/f'{name}.txt';p.write_text('\n'.join(s.text)+'\n');return p
rows=json.loads((w/'captures.json').read_text());b=(w/'raw/mobile-index.html').read_bytes();parse('mobile-index',b)
rows.insert(0,{'name':'mobile-index','url':'https://www.fmworld.net/digital-paper/mobileapp/helpguide/','status':200,'raw_work_path':str(w/'raw/mobile-index.html'),'raw_sha256':hashlib.sha256(b).hexdigest(),'encoding':'cp932 (declared Shift_JIS)'})
def fetch(i):
 name=f'QHG{i:05d}';url=f'https://www.fmworld.net/digital-paper/mobileapp/helpguide/contents/{name}.html'
 try:
  with urllib.request.urlopen(url,timeout=30) as r:b=r.read();status=r.status
  p=w/'raw'/f'{name}.html';p.write_bytes(b);t=parse(name,b)
  return {'name':name,'url':url,'status':status,'raw_work_path':str(p),'raw_sha256':hashlib.sha256(b).hexdigest(),'extracted_work_path':str(t),'extracted_sha256':hashlib.sha256(t.read_bytes()).hexdigest(),'encoding':'cp932 (declared Shift_JIS)'}
 except urllib.error.HTTPError as e:return {'name':name,'url':url,'status':e.code,'route_stopped':True}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 for r in pool.map(fetch,range(1,8)):rows.append(r);print(r['name'],r['status'])
(w/'captures.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
