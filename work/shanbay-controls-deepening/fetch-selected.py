from pathlib import Path
import requests,re,json,hashlib
from concurrent.futures import ThreadPoolExecutor
w=Path('work/shanbay-controls-deepening')
s=json.loads((w/'cms-mijwu.json').read_text())['content_html']
imgs=re.findall(r'<img[^>]*src="([^"]+)"',s)
items=[(f'ielts-{n}.jpeg',imgs[n-1]) for n in [7,8]]+[(f'apple-{i}.html','https://apps.apple.com/cn/app/id'+i) for i in ['698013609','598541783','897432803','1089026929']]
(w/'selected-urls.json').write_text(json.dumps(items,ensure_ascii=False,indent=2))
def get(it):
 name,u=it
 try:
  r=requests.get(u,timeout=(5,15))
  if r.status_code!=200:return name,r.status_code,'STOP no retry'
  (w/name).write_bytes(r.content)
  return name,200,r.headers.get('content-type'),len(r.content),hashlib.sha256(r.content).hexdigest()
 except Exception as e:return name,type(e).__name__,str(e)
with ThreadPoolExecutor(max_workers=4) as ex:
 for r in ex.map(get,items): print(json.dumps(r,ensure_ascii=False),flush=True)
