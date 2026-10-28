from pathlib import Path
import requests, json, hashlib, sys
from html.parser import HTMLParser
class TextParser(HTMLParser):
    def __init__(self):
        super().__init__();self.text=[];self.links=[];self.skip=0
    def handle_starttag(self,tag,attrs):
        if tag in ('script','style'):self.skip+=1
        a=dict(attrs)
        if tag=='a' and a.get('href'):self.links.append({'href':a['href'],'text':''})
    def handle_endtag(self,tag):
        if tag in ('script','style'):self.skip=max(0,self.skip-1)
    def handle_data(self,data):
        if not self.skip and data.strip():self.text.append(data.strip())

base=Path('work/meebook-controls-deepening')
log=base/'captures.jsonl'
for key,url in zip(sys.argv[1::2],sys.argv[2::2]):
    r=requests.get(url,timeout=35)
    ext='pdf' if r.content.startswith(b'%PDF') else 'html'
    p=base/'raw'/f'{key}.{ext}';p.write_bytes(r.content)
    rec={'key':key,'url':url,'final_url':r.url,'checked_on':'2026-10-27','http_status':r.status_code,'content_type':r.headers.get('Content-Type'),'raw_work_path':str(p),'raw_sha256':hashlib.sha256(r.content).hexdigest(),'bytes':len(r.content),'redirects':[{'status':x.status_code,'url':x.url,'location':x.headers.get('Location')} for x in r.history]}
    if ext=='html' and r.status_code==200:
        s=TextParser();s.feed(r.content.decode('utf-8',errors='replace'))
        t=p.with_suffix('.txt');t.write_text('\n'.join(s.text))
        rec.update(extracted_work_path=str(t),extracted_sha256=hashlib.sha256(t.read_bytes()).hexdigest())
        links=s.links
        p.with_suffix('.links.json').write_text(json.dumps(links,ensure_ascii=False,indent=2))
    with log.open('a') as f:f.write(json.dumps(rec,ensure_ascii=False)+'\n')
    print(json.dumps(rec,ensure_ascii=False))
    if r.status_code in (401,403,429):print('STOP this route; no retry')
