from pathlib import Path
import hashlib,json,urllib.request,urllib.error
from html.parser import HTMLParser
w=Path('work/ibasso-controls-deepening');lib=Path('docs/research/interaction-library')
assert not any('E-IBDC' in p.read_text() for p in (lib/'batches').glob('*evidence.jsonl')),'Prefix collision'
(w/'case-before.md').write_bytes((lib/'cases/M-064-ibasso.md').read_bytes())
old=[lib/'batches'/x for x in ['hifi-extension-evidence.jsonl','hifi-extension-coverage.csv','hifi-controls-deepening-evidence.jsonl','hifi-controls-deepening-coverage.csv']]
(w/'old-batch-hashes.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in old},indent=2)+'\n')
class Parser(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.data=[]
 def handle_starttag(self,t,a):
  if t in ('script','style'):self.skip+=1
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.data.append(d.strip())
urls=[('firmware-cn.html','https://ibasso.cn/support/dx180firmware'),('firmware-en.html','https://ibasso.com/dx180-firmware/'),('musin-news.html','https://musinltd.com/News/1165.html'),('distributors.html','https://ibasso.com/where-to-buy/'),('musin-release.html','https://prtimes.jp/main/html/rd/p/000000144.000139013.html')]
log=[]
for name,url in urls:
 r={'url':url,'checked_on':'2026-10-27','network_refetched':True}
 try:
  req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})
  with urllib.request.urlopen(req,timeout=35) as res:b=res.read();r.update(status=res.status,final_url=res.url,content_type=res.headers.get('Content-Type'))
  p=w/name;p.write_bytes(b);r.update(raw_work_path=str(p),raw_sha256=hashlib.sha256(b).hexdigest(),bytes=len(b));pa=Parser();pa.feed(b.decode('utf-8',errors='replace'));p.with_suffix('.txt').write_text('\n'.join(pa.data)+'\n')
 except Exception as ex:r.update(error=str(ex));r['status']=getattr(ex,'code',None)
 log.append(r)
(w/'fetch-log.json').write_text(json.dumps(log,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(log,ensure_ascii=False))
