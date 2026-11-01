from pathlib import Path
from urllib.parse import urlsplit
import json,requests,hashlib
w=Path('work/bbdc-learning-controls-deepening');a=json.load((w/'c05-agirls2017-images.json').open());b=json.load((w/'c06-sspai2023-images.json').open());items=[('c05-first-word.jpg',a[3]['src']),('c05-spelling.jpg',a[4]['src']),('c06-word-tips.jpg',b[11]['src']),('c06-review.jpg',b[13]['src']),('c06-spelling.jpg',b[16]['src'])];out=[];blocked=set()
for name,url in items:
 if urlsplit(url).hostname in blocked:continue
 try:
  r=requests.get(url,timeout=(5,20));p=w/'raw'/name;p.write_bytes(r.content);x=dict(path=str(p),url=url,status=r.status_code,bytes=len(r.content),sha256=hashlib.sha256(r.content).hexdigest());out.append(x);print(json.dumps(x),flush=True)
  if r.status_code in (401,403,412,429):x['stop']='no retry';blocked.add(urlsplit(url).hostname)
 except Exception as e:out.append(dict(url=url,error=str(e),stop='no retry'));print(e,flush=True)
(w/'fetch-detail-images-results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
