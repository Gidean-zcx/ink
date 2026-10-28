from pathlib import Path
import json,csv,hashlib,re,urllib.request,urllib.error,concurrent.futures
from html.parser import HTMLParser
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/xiaoyuan-app-controls-deepening';D=W/'raw'
assert not (W/'baseline.json').exists(),'one-time acquisition'
D.mkdir(parents=True,exist_ok=True)
def sha(b):return hashlib.sha256(b).hexdigest()
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines() if s];fs=list(csv.DictReader((L/'coverage.csv').open()));case=L/'cases/M-028-xiaoyuan-ai.md'
(W/'case-before.md').write_bytes(case.read_bytes());dump(W/'evidence-before.json',[e for e in es if e['product_id']=='M-028']);dump(W/'coverage-before.json',[f for f in fs if f['product_id']=='M-028']);dump(W/'baseline.json',dict(case_sha256=sha(case.read_bytes()),requirements_sha256=sha((R/'docs/requirements.md').read_bytes()),checked_on='2026-10-27'))
class P(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.text=[];self.links=[];self.images=[];self.head=''
 def handle_starttag(self,tag,a):
  a=dict(a)
  if tag in ('script','style'):self.skip+=1
  if tag in ('p','h1','h2','h3','h4','li','br','div'):self.text.append('\n')
  if tag=='a' and a.get('href'):self.links.append(a['href'])
  if tag=='img':self.images.append(a)
 def handle_endtag(self,tag):
  if tag in ('script','style') and self.skip:self.skip-=1
 def handle_data(self,s):
  if not self.skip:self.text.append(s)
def fetch(item):
 name,url=item;p=D/(name+'.html');r=dict(name=name,url=url,checked_on='2026-10-27',access_mode='anonymous_public_GET_no_login')
 try:
  with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=25) as h:b=h.read();r.update(status=h.status,final_url=h.url,content_type=h.headers.get('Content-Type'))
 except urllib.error.HTTPError as e:b=e.read();r.update(status=e.code,final_url=e.url,content_type=e.headers.get('Content-Type'),stopped=True)
 except Exception as e:r.update(status=None,error=repr(e),stopped=True);return r
 p.write_bytes(b);r.update(raw_work_path=str(p.relative_to(R)),raw_sha256=sha(b),bytes=len(b))
 if r['status']==200:
  s=b.decode('utf-8','replace');q=P();q.feed(s);tp=D/(name+'.txt');tp.write_text('\n'.join(t.strip() for t in ''.join(q.text).splitlines() if t.strip())+'\n');r.update(text_work_path=str(tp.relative_to(R)),text_sha256=sha(tp.read_bytes()),links=list(dict.fromkeys(q.links)),images=q.images)
 return r
items=[('home','https://leo.fbcontent.cn/'),('faq','https://xykspt.yuanfudao.com/faq'),('store','https://apps.apple.com/cn/app/id1325419855'),('privacy','https://xyks.yuanfudao.com/common/mobile-privacy-policy'),('child-policy','https://solar-online.oss-cn-beijing.aliyuncs.com/solar-user-profile/user-privacy-policy/leo-child-privacy-xyks.html')]
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:caps=list(pool.map(fetch,items))
dump(W/'captures.json',caps)
# Local re-read of the historical primary author, not a fresh HTTP capture.
ep=next(e for e in es if e['evidence_id']=='E-XYCD-01');p=R/ep['raw_work_path'];assert sha(p.read_bytes())==ep['raw_sha256'];s=p.read_text();start=s.index('<div class="article--content grap">');end=s.index('<div class="article--copyright">',start);body=s[start:end];q=P();q.feed(body)
tp=D/'historical-author.txt';tp.write_text('\n'.join(t.strip() for t in ''.join(q.text).splitlines() if t.strip())+'\n')
blocks=[]
parts=re.split(r'(<h[23][^>]*>.*?</h[23]>)',body,flags=re.S)
for i in range(1,len(parts),2):
 h=P();h.feed(parts[i]);p2=P();p2.feed(parts[i+1] if i+1<len(parts) else '');blocks.append(dict(heading=''.join(h.text).strip(),text=' '.join(''.join(p2.text).split()),images=p2.images))
dump(W/'historical-blocks.json',blocks);dump(W/'historical-local-capture.json',dict(evidence_id='E-XYCD-01',checked_on='2026-10-27',access_mode='prior_local_HTML_reread',raw_work_path=ep['raw_work_path'],raw_sha256=ep['raw_sha256'],text_work_path=str(tp.relative_to(R)),text_sha256=sha(tp.read_bytes())))
for r in caps:print(r['name'],r['status'],r.get('bytes'),r.get('error',''))
print('article_blocks',len(blocks),'article_images',len(q.images))
print('home_links',next(r for r in caps if r['name']=='home').get('links'))
print('blocks',[(x['heading'],len(x['images'])) for x in blocks])
