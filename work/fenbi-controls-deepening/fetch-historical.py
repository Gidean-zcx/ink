from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
out=Path('work/fenbi-controls-deepening')
urls={
 'historical-author.html':'https://www.woshipm.com/evaluating/2492895.html',
 'historical-practice-flow.png':'https://image.woshipm.com/wp-files/2019/06/7UXHte8DaP5qC8xIyDPF.png',
 'historical-practice-screens.jpg':'https://image.woshipm.com/wp-files/2019/06/SpJTKydk5g80ceQLfY2o.jpg',
 'historical-mock-flow.png':'https://image.woshipm.com/wp-files/2019/06/BdjbNYUlvaDdMgfxf2ph.png',
 'historical-mock-screens.png':'https://image.woshipm.com/wp-files/2019/06/j2CA2WiioOVT3MPNyGaC.png',
 'kb-50542.html':'https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50542',
 'kb-50784.html':'https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50784',
 'kb-50643.html':'https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50643',
 'official-main.js':'https://nodestatic.fbstatic.cn/weblts_spa_online/page/main-UEZZC3F7.js',
}
def fetch(kv):
 name,url=kv
 try:
  r=requests.get(url,timeout=(5,15));d={'file':name,'url':url,'status':r.status_code,'content_type':r.headers.get('content-type'),'bytes':len(r.content)}
  if r.status_code==200:
   (out/name).write_bytes(r.content);d['sha256']=hashlib.sha256(r.content).hexdigest()
  return d
 except Exception as e:return {'file':name,'url':url,'error':str(e)}
with ThreadPoolExecutor(max_workers=6) as p:results=list(p.map(fetch,urls.items()))
(out/'fetch-historical-result.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results:print(json.dumps(r,ensure_ascii=False))
