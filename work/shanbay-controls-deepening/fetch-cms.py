from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,re,html
w=Path('work/shanbay-controls-deepening')
ids=['cfucx','blksqe','anjli','buxtne','puica','mjvss','uurvp','eaqsx','gvdcn','mijwu']
def get(i):
 u='https://apiv3.shanbay.com/cms/articles/'+i
 try:
  r=requests.get(u,timeout=(5,15)); code=r.status_code
  if code!=200:return i,code,'STOP; no retry'
  d=r.json();(w/f'cms-{i}.json').write_text(json.dumps(d,ensure_ascii=False,indent=2))
  s=html.unescape(re.sub('<[^>]+>','\n',d.get('content_html','')))
  s='\n'.join(x.strip() for x in s.splitlines() if x.strip())
  (w/f'cms-{i}.txt').write_text(s+'\n')
  return i,code,d.get('title'),d.get('assign_date'),s
 except Exception as e:return i,type(e).__name__,str(e)
with ThreadPoolExecutor(max_workers=4) as ex:
 for r in ex.map(get,ids):print(json.dumps(r,ensure_ascii=False),flush=True)
