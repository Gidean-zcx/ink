import csv, hashlib, json, re, threading, requests
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
from concurrent.futures import ThreadPoolExecutor

ROOT=Path('/home/workspace/reader')
OUT=ROOT/'work/research/batch41-inoreader-help'
LIB=ROOT/'docs/research/interaction-library'
old=json.loads((OUT/'results.json').read_text())
assert not (OUT/'root-results.json').exists()
(OUT/'worker-results-original.json').write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n')
oldby={x['url']:x for x in old}
categories={'article-view','saved-read-later','archive','web-pages','preferences-customization','syncing-mobile-apps','text-to-speech','annotations','offline-mode','newsletter-feeds'}
rows=[r for r in csv.DictReader((LIB/'batches/inoreader-controls-deepening-inventory.csv').open()) if r['entry_type']=='topic' and r['review_status']=='directory_only' and r['parent_id'].split('--')[-1] in categories]
ev=[json.loads(s) for s in (LIB/'evidence.jsonl').read_text().splitlines()]
assert not any(r['url'].rstrip('/')==e['url'].rstrip('/') for r in rows for e in ev)
stop=threading.Event()
VOID={'br','img','hr','input','meta','link','source','wbr','embed','area','base','col','param','track'}
class Body(HTMLParser):
 def __init__(self,url):
  super().__init__();self.url=url;self.on=False;self.done=False;self.depth=0;self.parts=[];self.links=[];self.images=[]
 def handle_starttag(self,t,attrs):
  a=dict(attrs)
  if self.done:return
  if not self.on:
   if t=='div' and 'public_help_center_article_body' in a.get('class','').split():self.on=True;self.depth=1
   return
  if t=='div':self.depth+=1
  if t in {'p','li','br','div','h2','h3','ul','ol'}:self.parts.append('\n')
  if t=='a' and a.get('href'):self.links.append(urljoin(self.url,a['href']))
  if t=='img' and a.get('src'):self.images.append(urljoin(self.url,a['src']))
 def handle_endtag(self,t):
  if not self.on or self.done:return
  if t=='div':
   self.depth-=1
   if self.depth==0:self.done=True;self.on=False
  if t in {'p','li','div','h2','h3','ul','ol'}:self.parts.append('\n')
 def handle_data(self,d):
  if self.on and not self.done:self.parts.append(d)

def run(pair):
 n,r=pair; x=dict(order=n,entry_id=r['entry_id'],title=r['title'],url=r['url'],checked_on='2026-10-27')
 prior=oldby.get(r['url'],{})
 key=f'{n:02}-{r["entry_id"]}'
 if prior.get('raw_html'):
  raw=ROOT/prior['raw_html']; data=raw.read_bytes(); x.update(http_status=prior['http_status'],acquisition='existing_worker_raw_no_new_GET',raw_path=str(raw.relative_to(ROOT)),prior_classification='worker false-positive challenge: global reCAPTCHA script; root confirmed actual article body')
 else:
  if stop.is_set():return dict(x,status='skipped_after_real_access_block',body_status='not_obtained')
  try:
   response=requests.get(r['url'],timeout=25)
   data=response.content; raw=OUT/(key+'.html');raw.write_bytes(data)
   hp=OUT/(key+'.headers.json');hp.write_text(json.dumps(dict(response.headers),ensure_ascii=False,indent=2)+'\n')
   x.update(http_status=response.status_code,final_url=response.url,acquisition='one_anonymous_normal_GET',raw_path=str(raw.relative_to(ROOT)),headers_path=str(hp.relative_to(ROOT)))
  except Exception as exc:return dict(x,status='request_error_no_retry',error=str(exc),body_status='not_obtained')
 x.update(raw_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
 s=data.decode('utf-8','replace');parser=Body(r['url']);parser.feed(s)
 body='\n'.join(z.strip() for z in ''.join(parser.parts).splitlines() if z.strip())
 title=re.search(r'<title[^>]*>(.*?)</title>',s,re.S|re.I)
 is_challenge=not body and bool(title and re.search(r'Just a moment|Attention Required|Access Denied|Verify',title.group(1),re.I))
 if x['http_status'] in (401,403,412,429) or is_challenge:
  stop.set();x.update(status='blocked_stopped',body_status='not_obtained',verification_page=is_challenge)
 elif x['http_status']==200 and body and parser.done:
  dest=OUT/(key+'.body.txt');dest.write_text(body+'\n')
  x.update(status='body_obtained',body_status='extracted_not_yet_root_read',body_path=str(dest.relative_to(ROOT)),body_sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),body_chars=len(body),links=parser.links,images=parser.images)
 else:x.update(status='body_unknown_no_retry',body_status='not_reliably_extracted')
 (OUT/(key+'.root-meta.json')).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({k:x.get(k) for k in ['order','http_status','status','body_chars']},ensure_ascii=False),flush=True)
 return x

with ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,enumerate(rows,1)))
(OUT/'root-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print('TOTAL',len(results),'new_gets',sum(x.get('acquisition')=='one_anonymous_normal_GET' for x in results),'bodies',sum(x.get('status')=='body_obtained' for x in results),flush=True)
