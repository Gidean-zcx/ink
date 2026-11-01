from pathlib import Path
from html.parser import HTMLParser
import hashlib,json
W=Path('work/sony-dpt-controls-deepening')
class Body(HTMLParser):
    def __init__(self):super().__init__();self.depth=0;self.out=[]
    def handle_starttag(self,t,a):
        d=dict(a)
        if t=='div' and (self.depth or d.get('id')=='id_content'):self.depth+=1
        if self.depth and t in ('p','li','h1','h2','h3','br','tr','div'):self.out.append('\n')
    def handle_endtag(self,t):
        if t=='div' and self.depth:self.depth-=1
        if self.depth and t in ('p','li','h1','h2','h3','tr','div'):self.out.append('\n')
    def handle_data(self,s):
        if self.depth:self.out.append(s)
log=json.loads((W/'fetch-log.json').read_text());changes=[]
for r in log:
    if not r.get('raw_work_path'):continue
    p=Path(r['text_work_path']);old=hashlib.sha256(p.read_bytes()).hexdigest()
    a=Body();a.feed(Path(r['raw_work_path']).read_text(encoding='utf-8-sig'))
    s='\n'.join(x.strip() for x in ''.join(a.out).splitlines() if x.strip());assert len(s)>200,r['url']
    p.write_text(s);new=hashlib.sha256(p.read_bytes()).hexdigest()
    changes.append({'path':str(p),'before_sha256':old,'after_sha256':new,'chars':len(s)})
    r['text_sha256']=new;r['extraction']='id_content; includes title and complete step-list; raw unchanged'
(W/'fetch-log.json').write_text(json.dumps(log,ensure_ascii=False,indent=2))
(W/'local-reparse-audit.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2))
print('Reparsed',len(changes),'existing HTML files; no network')
