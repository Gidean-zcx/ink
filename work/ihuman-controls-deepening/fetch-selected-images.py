from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
p=Path('work/ihuman-controls-deepening');base='https://exp-picture.cdn.bcebos.com/'
urls={
 '2018-proposal-image-1.jpg':base+'e076d77622bc7dc5d6797feb5e460596b91429b6.jpg',
 '2018-proposal-image-2.jpg':base+'b442d6d246fe474ee19d670bb0ef354f51b81fb6.jpg',
 '2022-logout-1.jpg':base+'304f0999e92abab87299af494814f1c594eea170.jpg',
 '2022-logout-2.jpg':base+'bbf95c406afec314c21c5059cac1b727ad539c70.jpg',
 '2022-logout-3.jpg':base+'16d8f72abab842405b2706a37ac595ee40c19e70.jpg'}
def get(kv):
 name,url=kv
 try:
  r=requests.get(url,timeout=(5,15));d={'file':name,'url':url,'status':r.status_code,'type':r.headers.get('content-type'),'bytes':len(r.content)}
  if r.status_code==200:
   (p/name).write_bytes(r.content);d['sha256']=hashlib.sha256(r.content).hexdigest()
  return d
 except Exception as e:return {'file':name,'url':url,'error':str(e)}
with ThreadPoolExecutor(max_workers=5) as pool:rs=list(pool.map(get,urls.items()))
(p/'fetch-selected-images-result.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
for r in rs:print(json.dumps(r,ensure_ascii=False))
