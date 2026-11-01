from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json,csv,hashlib,requests
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/bigme-controls-deepening'
(W/'raw').mkdir(exist_ok=True);assert not (W/'captures.json').exists()
(W/'case-before.md').write_bytes((L/'cases/M-011-bigme.md').read_bytes())
for n,rows in [('coverage',list(csv.DictReader((L/'coverage.csv').open()))),('evidence',[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()])]:
 (W/(n+'-before.json')).write_text(json.dumps([x for x in rows if x['product_id']=='M-011'],ensure_ascii=False,indent=2)+'\n')
urls=[('manual.pdf','https://device.report/m/f9c833a5a200f1b7aad371249f065497aa4c9e0caf1023379311412b5579ff50.pdf'),('product.html','https://store.bigme.vip/pages/hibreak-pro-landing-page')]
def get(x):
 n,u=x;a=dict(name=n,url=u,checked_on='2026-10-27')
 try:
  r=requests.get(u,timeout=40);p=W/'raw'/n;p.write_bytes(r.content);a.update(status=r.status_code,final_url=r.url,content_type=r.headers.get('Content-Type'),raw_work_path=str(p.relative_to(R)),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),pdf_header=r.content.startswith(b'%PDF'))
 except Exception as e:a['error']=str(e)
 return a
with ThreadPoolExecutor(max_workers=2) as pool:out=list(pool.map(get,urls))
(W/'captures.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print(json.dumps(out,ensure_ascii=False))
