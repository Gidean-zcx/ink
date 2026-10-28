import csv, hashlib, json, re
from pathlib import Path
from html.parser import HTMLParser
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from concurrent.futures import ThreadPoolExecutor

ROOT = Path('/home/workspace/reader')
W = ROOT / 'work/mudita-app-controls-deepening'
LIB = ROOT / 'docs/research/interaction-library'
SELECT = [22,25,26,27,28,31,44,45,46,47,48,52,54,56,58,60,91,95]
rows = list(csv.DictReader((LIB/'artifacts/mudita-controls-deepening/function-inventory.csv').open()))
jobs = [r for r in rows if int(r['inventory_id'].rsplit('-',1)[1]) in SELECT]
all_e = [json.loads(x) for x in (LIB/'evidence.jsonl').read_text().splitlines() if x.strip()]

class Parser(HTMLParser):
    def __init__(self):
        super().__init__(); self.text=[]; self.skip=0; self.images=[]
    def handle_starttag(self,t,a):
        d=dict(a)
        if t in ('script','style'): self.skip+=1
        if t in ('p','h1','h2','h3','h4','li','br','div'): self.text.append('\n')
        if t=='img': self.images.append(d)
    def handle_endtag(self,t):
        if t in ('script','style'): self.skip=max(0,self.skip-1)
    def handle_data(self,s):
        if not self.skip:self.text.append(s)

def get(r):
    key=r['inventory_id']; p=W/'raw'/f'{key}.html'
    assert not p.exists(), f'no automatic repeat: {p}'
    out={'inventory_id':key,'url':r['url'],'title':r['title'],'checked_on':'2026-10-27',
         'existing_evidence_ids':[e['evidence_id'] for e in all_e if e.get('url')==r['url']],
         'request_method':'anonymous ordinary GET; no authentication; no retry'}
    try:
        with urlopen(Request(r['url'],headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as f:
            b=f.read(); out.update(status=f.status,final_url=f.url)
        p.write_bytes(b); q=Parser(); q.feed(b.decode('utf-8'))
        text=re.sub(r'\n\s*\n','\n',re.sub(r'[ \t]+',' ',''.join(q.text)))
        start=text.find('Modified on:'); end=text.find('Sorry!',start)
        assert start>=0 and end>start, 'article boundary missing'
        body=r['title']+'\n'+text[start:end].strip()+'\n'
        p.with_suffix('.txt').write_text(body)
        p.with_suffix('.images.json').write_text(json.dumps(q.images,ensure_ascii=False,indent=2)+'\n')
        out.update(raw_work_path=str(p.relative_to(ROOT)),raw_sha256=hashlib.sha256(b).hexdigest(),
                   bytes=len(b),text_path=str(p.with_suffix('.txt').relative_to(ROOT)),
                   body_sha256=hashlib.sha256(body.encode()).hexdigest(),
                   source_modified_text=body.splitlines()[1], image_count=len(q.images))
    except HTTPError as e:
        out.update(status=e.code,error=str(e),stopped=True)
    except Exception as e:
        out.update(error=repr(e),stopped=True)
    return out

with ThreadPoolExecutor(max_workers=3) as ex:
    results=list(ex.map(get,jobs))
(W/'captures.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results: print(r['inventory_id'],r.get('status'),r.get('existing_evidence_ids'),r.get('error',''))
assert all(r.get('status')==200 and not r.get('error') for r in results), 'failed paths recorded; do not retry'
