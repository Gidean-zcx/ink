from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
p=Path('work/ihuman-controls-deepening')
urls={
 'apple.html':'https://apps.apple.com/cn/app/id1139733348',
 'operator.html':'https://www.ihuman.com/agreement/privacy/operator.html',
 'official-map.png':'https://www.ihuman.com/agreement/privacy/images/images1.png',
 'official-settings.png':'https://www.ihuman.com/agreement/privacy/images/images2.png',
 'experience-2018.html':'https://jingyan.baidu.com/article/f54ae2fc6c21ec1e92b849c7.html',
 'logout-2022.html':'https://jingyan.baidu.com/article/75ab0bcba4d91997874db22b.html',
}
def get(kv):
 name,url=kv
 try:
  r=requests.get(url,timeout=(5,15));d={'file':name,'url':url,'status':r.status_code,'content_type':r.headers.get('content-type'),'bytes':len(r.content)}
  if r.status_code==200:
   (p/name).write_bytes(r.content);d['sha256']=hashlib.sha256(r.content).hexdigest()
  return d
 except Exception as e:return {'file':name,'url':url,'error':str(e)}
with ThreadPoolExecutor(max_workers=5) as pool:rs=list(pool.map(get,urls.items()))
(p/'fetch-pages-result.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
for r in rs:print(json.dumps(r,ensure_ascii=False))
