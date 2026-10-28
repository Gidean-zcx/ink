import csv, hashlib, json, re, sys
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import urljoin, urlparse
from urllib.request import Request, urlopen

ROOT=Path('/home/workspace/reader')
INV=ROOT/'docs/research/interaction-library/batches/mudita-app-controls-deepening-inventory.csv'
OUT=ROOT/'work/research/batch37-mudita'
RAW=OUT/'raw'
CHECKED_ON='2026-10-27'
BLOCK={401,403,412,429}

class Parser(HTMLParser):
    def __init__(self):
        super().__init__(); self.parts=[]; self.images=[]; self.skip=0
    def handle_starttag(self,tag,attrs):
        d=dict(attrs)
        if tag in ('script','style','noscript'): self.skip += 1
        if not self.skip and tag in ('p','h1','h2','h3','h4','li','br','div','section','article'): self.parts.append('\n')
        if tag=='img' and d.get('src'): self.images.append({'src':d.get('src'),'alt':d.get('alt','')})
    def handle_endtag(self,tag):
        if tag in ('script','style','noscript'): self.skip=max(0,self.skip-1)
    def handle_data(self,data):
        if not self.skip: self.parts.append(data)

def normalize_text(s):
    s=s.replace('\xa0',' ')
    s=re.sub(r'[ \t]+',' ',s)
    s=re.sub(r'\n[ \t]+','\n',s)
    return re.sub(r'\n{2,}','\n',s).strip()

rows=[r for r in csv.DictReader(INV.open()) if r['reading_status']=='index_only']
if len(rows)!=46 or len({r['url'] for r in rows})!=46:
    raise SystemExit(f'candidate invariant failed: rows={len(rows)} urls={len({r["url"] for r in rows})}')
for r in rows:
    key=r['inventory_id']
    for suffix in ('.html','.txt','.images.json'):
        if (RAW/f'{key}{suffix}').exists(): raise SystemExit(f'refusing repeat existing output: {key}{suffix}')
if (OUT/'fetch-log.jsonl').exists(): raise SystemExit('refusing repeat existing fetch log')

results=[]; blocked_domain=None
for r in rows:
    key=r['inventory_id']; url=r['url']; domain=urlparse(url).netloc
    rec={'inventory_id':key,'title':r['title'],'url':url,'checked_on':CHECKED_ON,
         'requested_at_utc':datetime.now(timezone.utc).isoformat(),
         'request_method':'ordinary anonymous GET; one attempt; sequential; 30s timeout; no authentication or protocol fallback'}
    if blocked_domain==domain:
        rec.update(result='skipped_after_domain_restriction',body_readable=False)
        results.append(rec); continue
    try:
        req=Request(url,headers={'User-Agent':'reader-public-doc-research'})
        with urlopen(req,timeout=30) as resp:
            data=resp.read(); status=resp.status; final=resp.url
        rec.update(http_status=status,final_url=final,redirected=(final!=url),bytes=len(data),html_sha256=hashlib.sha256(data).hexdigest())
        html_path=RAW/f'{key}.html'; html_path.write_bytes(data)
        parser=Parser(); parser.feed(data.decode('utf-8',errors='replace'))
        alltext=normalize_text(''.join(parser.parts))
        start=alltext.find('Modified on:')
        end=alltext.find('Sorry!',start if start>=0 else 0)
        if start>=0 and end>start:
            body=normalize_text(r['title']+'\n'+alltext[start:end])+'\n'
            boundary='Modified on: ... Sorry!'
        else:
            body=''; boundary='article boundary missing'
        images=[]; seen=set()
        for img in parser.images:
            absolute=urljoin(final,img['src'])
            item={'url':absolute,'alt':img['alt']}
            sig=(absolute,img['alt'])
            if sig not in seen: images.append(item); seen.add(sig)
        txt_path=RAW/f'{key}.txt'; txt_path.write_text(body)
        img_path=RAW/f'{key}.images.json'; img_path.write_text(json.dumps(images,ensure_ascii=False,indent=2)+'\n')
        substantive_lines=[x.strip() for x in body.splitlines()[2:] if x.strip()]
        readable=bool(body and sum(len(x) for x in substantive_lines)>=20)
        rec.update(html_path=str(html_path.relative_to(ROOT)),text_path=str(txt_path.relative_to(ROOT)),images_path=str(img_path.relative_to(ROOT)),
                   text_sha256=hashlib.sha256(body.encode()).hexdigest(),text_chars=len(body),image_url_count=len(images),
                   extraction_boundary=boundary,body_readable=readable,result='readable_body' if readable else 'shell_or_unreadable_body')
    except HTTPError as e:
        rec.update(http_status=e.code,final_url=e.geturl(),result='http_error',error=str(e),body_readable=False)
        if e.code in BLOCK: blocked_domain=domain; rec['domain_restriction_stop']=True
    except Exception as e:
        rec.update(result='request_or_parse_error',error=repr(e),body_readable=False)
    results.append(rec)
    with (OUT/'fetch-log.jsonl').open('a') as f: f.write(json.dumps(rec,ensure_ascii=False)+'\n')
    print(key,rec.get('http_status'),rec['result'])
    if rec['result']=='request_or_parse_error':
        (OUT/'summary.json').write_text(json.dumps({'checked_on':CHECKED_ON,'candidate_count':len(rows),'results':results},ensure_ascii=False,indent=2)+'\n')
        sys.exit(2)

summary={'checked_on':CHECKED_ON,'candidate_count':len(rows),'attempted':sum('http_status' in r for r in results),
         'readable':sum(r['result']=='readable_body' for r in results),'shell_or_unreadable':sum(r['result']=='shell_or_unreadable_body' for r in results),
         'http_errors':sum(r['result']=='http_error' for r in results),'skipped':sum(r['result']=='skipped_after_domain_restriction' for r in results),
         'results':results}
(OUT/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='results'},ensure_ascii=False))
if summary['http_errors'] or summary['skipped'] or summary['shell_or_unreadable']: sys.exit(1)
