from pathlib import Path
from hashlib import sha256
from html.parser import HTMLParser
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
import json
import sys

WORK=Path(__file__).resolve().parent
ROOT=WORK.parents[1]
class Text(HTMLParser):
    def __init__(self): super().__init__(); self.out=[]; self.skip=0
    def handle_starttag(self,t,a):
        if t in ('script','style'): self.skip+=1
    def handle_endtag(self,t):
        if t in ('script','style'): self.skip=max(0,self.skip-1)
    def handle_data(self,s):
        if not self.skip and s.strip(): self.out.append(s.strip())

for key,url in zip(sys.argv[1::2],sys.argv[2::2]):
    row={'key':key,'url':url,'checked_on':'2026-10-27','access_mode':'anonymous_urllib_GET'}
    try:
        with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as r:
            body=r.read();row.update(status=r.status,final_url=r.url,content_type=r.headers.get('Content-Type'))
    except HTTPError as e:
        body=e.read();row.update(status=e.code,final_url=e.url,stopped=True)
    except (URLError,TimeoutError) as e:
        body=b'';row.update(error=str(e),stopped=True)
    if body:
        p=WORK/'raw'/(key+'.html');p.write_bytes(body)
        row.update(raw_work_path=str(p.relative_to(ROOT)),raw_sha256=sha256(body).hexdigest())
        if row.get('status')==200:
            parser=Text();parser.feed(body.decode('utf-8',errors='replace'))
            txt='\n'.join(parser.out)+'\n';q=p.with_suffix('.txt');q.write_text(txt)
            row.update(extracted_work_path=str(q.relative_to(ROOT)),extracted_sha256=sha256(txt.encode()).hexdigest())
    with (WORK/'captures.jsonl').open('a') as f:f.write(json.dumps(row,ensure_ascii=False)+'\n')
    print(json.dumps(row,ensure_ascii=False),flush=True)
    if row.get('status') in (401,403,412,429) or 'error' in row:break
