from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json,requests,hashlib
base=Path('work/baicizhan-device-controls-deepening'); images=json.loads((base/'02-52audio-ace-max-links.json').read_text())['images']
def get(x):
 i,name,locator=x;url=images[i]['src'];row={'name':name,'url':url,'locator':locator,'method':'GET; timeout=(5,18); one attempt'}
 try:
  r=requests.get(url,timeout=(5,18));row['status']=r.status_code
  if r.status_code in (401,403,429):row['result']='stopped; no retry';return row
  if r.status_code==200:
   p=base/(name+'.jpg');p.write_bytes(r.content);row.update(path=str(p),sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
 except requests.RequestException as e:row['error']=type(e).__name__+': '+str(e)
 return row
rows=list(ThreadPoolExecutor(max_workers=3).map(get,[(7,'03-box-contents','开箱第5张；wp-image-239598；正文“取出内部物品”'),(8,'04-front','开箱第6张；wp-image-239602；正文“外观采用了类似于游戏机的设计”'),(13,'05-rear-model','开箱第11张；wp-image-239554；正文“机身背面”')]))
(base/'image-fetch-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
