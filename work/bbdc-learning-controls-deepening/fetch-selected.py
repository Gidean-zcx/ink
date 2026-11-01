from pathlib import Path
import requests,json,hashlib
w=Path('work/bbdc-learning-controls-deepening');items=[('c05-agirls2017.html','https://agirls.aotter.net/post/51654'),('c06-sspai2023.html','https://sspai.com/post/76700'),('c04-version-table.jpg','https://image.woshipm.com/wp-files/2021/03/IBhpD694YflEuTpdXgje.jpg'),('c04-flow.jpg','https://image.woshipm.com/wp-files/2021/03/XK71vXxvHvNW7eF9f3KC.jpg'),('c04-study-ui.jpg','https://image.woshipm.com/wp-files/2021/03/uq5tkM7K5trzj74mkE1Y.jpg')];out=[];blocked=set()
from urllib.parse import urlsplit
for name,url in items:
 host=urlsplit(url).hostname
 if host in blocked:continue
 try:
  r=requests.get(url,timeout=(5,20));p=w/'raw'/name;p.write_bytes(r.content);d=dict(path=str(p),url=url,final_url=r.url,status=r.status_code,bytes=len(r.content),sha256=hashlib.sha256(r.content).hexdigest())
  if r.status_code in (401,403,412,429):d['stop']='no retry';blocked.add(host)
  out.append(d);print(json.dumps(d,ensure_ascii=False),flush=True)
 except Exception as e:out.append(dict(url=url,error=str(e),stop='no retry'));print(e,flush=True)
(w/'fetch-selected-results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
