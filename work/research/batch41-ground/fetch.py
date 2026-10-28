from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from urllib.parse import urljoin, urlparse
from html.parser import HTMLParser
import datetime, hashlib, json, re, requests

OUT=Path('work/research/batch41-ground')
TARGETS=[
 ('335105','https://help.ground.news/en/articles/335105'),
 ('648513','https://help.ground.news/en/articles/648513'),
 ('485057','https://help.ground.news/en/articles/485057'),
 ('593857','https://help.ground.news/en/articles/593857'),
]
BLOCK={401,403,412,429}
CHALLENGE=[b'cf-chl-',b'challenge-platform',b'captcha',b'attention required! | cloudflare',b'verify you are human',b'enable javascript and cookies to continue']

class Extractor(HTMLParser):
 def __init__(self):
  super().__init__(); self.skip=0; self.text=[]; self.links=[]; self.images=[]; self.anchor=None; self.anchor_text=[]
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  if tag in ('script','style','svg','noscript'): self.skip+=1
  if not self.skip and tag in ('h1','h2','h3','h4','p','li','br'): self.text.append('\n')
  if not self.skip and tag=='a' and a.get('href'):
   self.anchor=a['href']; self.anchor_text=[]
  if not self.skip and tag=='img' and (a.get('src') or a.get('data-src')):
   self.images.append(a.get('src') or a.get('data-src'))
 def handle_data(self,data):
  if not self.skip:
   self.text.append(data)
   if self.anchor is not None:self.anchor_text.append(data)
 def handle_endtag(self,tag):
  if tag in ('script','style','svg','noscript') and self.skip:self.skip-=1
  if tag=='a' and self.anchor is not None:
   self.links.append((self.anchor,' '.join(''.join(self.anchor_text).split())))
   self.anchor=None; self.anchor_text=[]
  if not self.skip and tag in ('h1','h2','h3','h4','p','li'):self.text.append('\n')

def fetch(item):
 ident,url=item
 if urlparse(url).hostname!='help.ground.news': raise ValueError('host rejected')
 now=datetime.datetime.now(datetime.timezone.utc).isoformat()
 try:
  r=requests.get(url,headers={'User-Agent':'Mozilla/5.0 (compatible; reader-research/1.0)','Accept':'text/html,application/xhtml+xml'},timeout=(10,25),allow_redirects=True)
  body=r.content
  low=body.lower()
  challenge=any(x in low for x in CHALLENGE)
  status={'id':ident,'requested_url':url,'final_url':r.url,'status':r.status_code,'checked_utc':now,'bytes':len(body),'sha256':hashlib.sha256(body).hexdigest(),'content_type':r.headers.get('content-type'),'challenge_detected':challenge,'blocked_status':r.status_code in BLOCK}
  (OUT/f'{ident}.html').write_bytes(body)
  (OUT/f'{ident}-headers.json').write_text(json.dumps(dict(r.headers),ensure_ascii=False,indent=2)+'\n')
  (OUT/f'{ident}-status.json').write_text(json.dumps(status,ensure_ascii=False,indent=2)+'\n')
  if r.status_code==200 and not challenge and 'html' in (r.headers.get('content-type') or '').lower():
   e=Extractor(); e.feed(body.decode(r.encoding or 'utf-8',errors='replace'))
   lines=[]
   for line in ''.join(e.text).splitlines():
    clean=' '.join(line.split())
    if clean and (not lines or clean!=lines[-1]):lines.append(clean)
   (OUT/f'{ident}-body.txt').write_text('\n'.join(lines)+'\n')
   links=[]; seen=set()
   for href,text in e.links:
    absolute=urljoin(r.url,href)
    if absolute not in seen:
     seen.add(absolute);links.append({'url':absolute,'text':text})
   images=[]; seen=set()
   for src in e.images:
    absolute=urljoin(r.url,src)
    if absolute not in seen:seen.add(absolute);images.append(absolute)
   (OUT/f'{ident}-links.json').write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n')
   (OUT/f'{ident}-images.json').write_text(json.dumps(images,ensure_ascii=False,indent=2)+'\n')
   status['body_extracted']=True;status['text_lines']=len(lines);status['links']=len(links);status['images']=len(images)
  else: status['body_extracted']=False
  (OUT/f'{ident}-status.json').write_text(json.dumps(status,ensure_ascii=False,indent=2)+'\n')
  return status
 except Exception as exc:
  status={'id':ident,'requested_url':url,'checked_utc':now,'error':type(exc).__name__+': '+str(exc),'body_extracted':False}
  (OUT/f'{ident}-status.json').write_text(json.dumps(status,ensure_ascii=False,indent=2)+'\n')
  return status

def main():
 results=[]; stopped=False
 for start in range(0,len(TARGETS),2):
  batch=TARGETS[start:start+2]
  with ThreadPoolExecutor(max_workers=2) as pool:
   futures=[pool.submit(fetch,x) for x in batch]
   got=[f.result() for f in as_completed(futures)]
  got.sort(key=lambda x:[i for i,(ident,_) in enumerate(TARGETS) if ident==x['id']][0])
  results.extend(got)
  if any(x.get('blocked_status') or x.get('challenge_detected') for x in got):
   stopped=True;break
 (OUT/'results.json').write_text(json.dumps({'targets':len(TARGETS),'attempted':len(results),'stopped_on_block_or_challenge':stopped,'results':results},ensure_ascii=False,indent=2)+'\n')
 for x in results: print(json.dumps(x,ensure_ascii=False))
 if len(results)<len(TARGETS): print(json.dumps({'not_attempted':[x[1] for x in TARGETS[len(results):]]},ensure_ascii=False))
if __name__=='__main__':main()
