from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
w=Path('work/fenbi-controls-deepening')
items=[('apple.html','https://apps.apple.com/cn/app/id584660358'),('mobile-download.html','https://www.fenbi.com/page/download/ios'),('xingce-qa.html','https://www.fenbi.com/page/mokaodasai-qa/gwy/xingce'),('hub-10063.html','https://www.fenbi.com/page/information-hub-detail/10063')]
def get(it):
 n,u=it
 try:
  r=requests.get(u,timeout=(5,15));out={'file':n,'url':u,'status':r.status_code,'content_type':r.headers.get('content-type'),'bytes':len(r.content)}
  if r.status_code==200:
   (w/n).write_bytes(r.content);out['sha256']=hashlib.sha256(r.content).hexdigest()
  else:out['stop']='no retry'
  return out
 except Exception as e:return {'file':n,'error':type(e).__name__,'message':str(e)}
with ThreadPoolExecutor(max_workers=4) as ex:rows=list(ex.map(get,items))
(w/'fetch-pages-result.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
for row in rows:print(json.dumps(row,ensure_ascii=False),flush=True)
