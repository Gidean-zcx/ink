import csv, hashlib, json, re, threading
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urljoin, urlparse
import requests

ROOT=Path('work/research/batch41-inoreader-help')
INV=Path('docs/research/interaction-library/batches/inoreader-controls-deepening-inventory.csv')
EVID=Path('docs/research/interaction-library/evidence.jsonl')
SUFFIXES=('article-view','saved-read-later','archive','web-pages','preferences-customization','syncing-mobile-apps','text-to-speech','annotations','offline-mode')
BLOCK={401,403,412,429}
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/156.0.0.0 Safari/537.36'
stop=threading.Event()

class Extractor(HTMLParser):
    def __init__(self, base):
        super().__init__(convert_charrefs=True); self.base=base; self.stack=[]; self.h1=[]; self.in_h1=0; self.candidates=[]; self.links=[]; self.images=[]; self.meta={}; self.title=[]; self.in_title=0
    def handle_starttag(self, tag, attrs):
        a=dict(attrs); self.stack.append((tag,a))
        if tag=='h1': self.in_h1+=1
        if tag=='title': self.in_title+=1
        if tag=='meta':
            k=a.get('name') or a.get('property'); v=a.get('content')
            if k and v: self.meta[k]=v
        if tag=='a' and a.get('href'): self.links.append(urljoin(self.base,a['href']))
        if tag=='img' and (a.get('src') or a.get('data-src')): self.images.append(urljoin(self.base,a.get('src') or a.get('data-src')))
    def handle_endtag(self, tag):
        if tag=='h1' and self.in_h1: self.in_h1-=1
        if tag=='title' and self.in_title: self.in_title-=1
        if self.stack:
            for i in range(len(self.stack)-1,-1,-1):
                if self.stack[i][0]==tag: del self.stack[i:]; break
    def handle_data(self,data):
        t=' '.join(data.split())
        if not t: return
        if self.in_h1: self.h1.append(t)
        if self.in_title: self.title.append(t)
        skip={'script','style','noscript','svg','nav','header','footer'}
        if any(tag in skip for tag,_ in self.stack): return
        score=0
        for tag,a in self.stack:
            ident=' '.join([a.get('id',''),a.get('class','')]).lower()
            if tag in ('article','main'): score+=5
            if any(x in ident for x in ('article','content','help','entry','post')): score+=2
            if any(x in ident for x in ('nav','menu','sidebar','related','footer','header')): score-=5
        if self.h1 and score>0: self.candidates.append((score,t))

def extract(html,base):
    p=Extractor(base)
    try: p.feed(html)
    except Exception as e: return {'status':'unknown','error':repr(e),'h1':None,'text':'','links':[],'image_urls':[],'meta':{}}
    h1=' '.join(p.h1).strip() or None
    vals=[]
    for score,t in p.candidates:
        if t.lower().startswith('related questions'): break
        if score>=2 and (not vals or vals[-1]!=t): vals.append(t)
    text='\n'.join(vals)
    status='extracted' if h1 and len(text)>=120 else 'unknown'
    return {'status':status,'h1':h1,'html_title':' '.join(p.title).strip() or None,'text':text,'links':list(dict.fromkeys(p.links)),'image_urls':list(dict.fromkeys(p.images)),'meta':p.meta}

rows=list(csv.DictReader(INV.open(encoding='utf-8')))
selected=[r for r in rows if r.get('entry_type')=='topic' and r.get('review_status')=='directory_only' and any(r.get('parent_id','').endswith(x) for x in SUFFIXES)]
ev=[]
for line in EVID.read_text(encoding='utf-8').splitlines():
    if line.strip(): ev.append(json.loads(line))
seen={r.get('url'):r.get('evidence_id') for r in ev if r.get('url')}
items=[]
for n,r in enumerate(selected,1):
    items.append({'order':n,'entry_id':r.get('entry_id'),'parent_id':r.get('parent_id'),'title':r.get('title'),'url':r.get('url'),'duplicate_evidence_id':seen.get(r.get('url'))})

def fetch(item):
    slug=f"{item['order']:02d}-{item['entry_id']}"
    out=dict(item)
    if item['duplicate_evidence_id']:
        out.update(status='skipped_duplicate',body_status='not_fetched'); return out
    if stop.is_set():
        out.update(status='skipped_domain_stopped',body_status='not_fetched'); return out
    try:
        r=requests.get(item['url'],headers={'User-Agent':UA,'Accept':'text/html,application/xhtml+xml'},timeout=25,allow_redirects=False)
        body=r.content; ctype=r.headers.get('content-type','')
        raw=ROOT/f'{slug}.html'; raw.write_bytes(body)
        sha=hashlib.sha256(body).hexdigest()
        text=body.decode(r.encoding or 'utf-8',errors='replace')
        low=text[:20000].lower()
        verification=any(x in low for x in ('captcha','cf-chl-','verify you are human','attention required','cloudflare challenge'))
        blocked=r.status_code in BLOCK or verification
        if blocked: stop.set()
        ex=extract(text,item['url']) if r.status_code==200 and 'html' in ctype.lower() and not verification else {'status':'unknown','h1':None,'html_title':None,'text':'','links':[],'image_urls':[],'meta':{},'error':None}
        txt=ROOT/f'{slug}.txt'; txt.write_text(ex.get('text',''),encoding='utf-8')
        meta={'order':item['order'],'entry_id':item['entry_id'],'parent_id':item['parent_id'],'url':item['url'],'requested_at':datetime.now(timezone.utc).isoformat(),'http_status':r.status_code,'headers':dict(r.headers),'sha256':sha,'bytes':len(body),'content_type':ctype,'verification_page':verification,'body_status':ex['status'],'h1':ex.get('h1'),'html_title':ex.get('html_title'),'page_meta':ex.get('meta',{}),'links':ex.get('links',[]),'image_urls':ex.get('image_urls',[]),'raw_html':str(raw),'body_text':str(txt),'extract_error':ex.get('error')}
        mp=ROOT/f'{slug}.meta.json'; mp.write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
        out.update(status='blocked_domain_stop' if blocked else 'fetched',http_status=r.status_code,body_status=ex['status'],h1=ex.get('h1'),bytes=len(body),sha256=sha,raw_html=str(raw),body_text=str(txt),meta_path=str(mp),link_count=len(ex.get('links',[])),image_url_count=len(ex.get('image_urls',[])),verification_page=verification)
    except Exception as e:
        out.update(status='request_error',body_status='unknown',error=repr(e))
    return out

results=[None]*len(items)
with ThreadPoolExecutor(max_workers=2) as pool:
    futs=[pool.submit(fetch,it) for it in items]
    for i,f in enumerate(futs): results[i]=f.result()
(ROOT/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
counts={}
for r in results: counts[r['status']]=counts.get(r['status'],0)+1
lines=['# Inoreader help one-shot fetch report','',f"Selected: {len(items)}; counts: `{json.dumps(counts,ensure_ascii=False,sort_keys=True)}`.",'','Each non-duplicate URL received at most one anonymous GET, redirects disabled, timeout 25s, maximum two workers. No images downloaded.','', '| # | entry_id | parent | HTTP | body | status | h1 |', '|---:|---|---|---:|---|---|---|']
for r in results:
    lines.append(f"| {r['order']} | {r['entry_id']} | {r['parent_id']} | {r.get('http_status','')} | {r.get('body_status','')} | {r['status']} | {(r.get('h1') or '').replace('|','\\|')} |")
lines += ['','Details, response headers, hashes, extracted links and image URLs are in each `.meta.json`; ordered machine results are in `results.json`.']
(ROOT/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(json.dumps({'selected':len(items),'counts':counts,'stopped':stop.is_set()},ensure_ascii=False))
