from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
w=Path('work/fenbi-practice-controls-deepening')
items=[('c01-review2019.html','https://www.woshipm.com/evaluating/3139083.html'),('c02-review2020.html','https://www.woshipm.com/evaluating/3331984.html'),('c03-baidu.html','https://jingyan.baidu.com/article/ca00d56c28bc50a89eebcff4.html'),('c04-bili.html','https://www.bilibili.com/read/cv33546391/')]
def run(it):
 name,url=it
 try:
  r=requests.get(url,timeout=(5,20));p=w/'raw'/name;p.write_bytes(r.content)
  d=dict(file=str(p),url=url,final_url=r.url,status=r.status_code,bytes=len(r.content),sha256=hashlib.sha256(r.content).hexdigest(),content_type=r.headers.get('Content-Type'),redirects=[{'status':h.status_code,'url':h.url} for h in r.history])
  if r.status_code in (401,403,412,429):d['stop']='HTTP refusal; no retry or alternate route'
  elif any(x in r.text.lower() for x in ['wafcaptcha','tencentcaptcha','验证后继续访问','安全验证']):d['stop']='challenge; no retry'
  return d
 except Exception as e:return dict(url=url,error=type(e).__name__,message=str(e),stop='no retry')
with ThreadPoolExecutor(max_workers=4) as ex:out=list(ex.map(run,items))
(w/'fetch-candidates-results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
for x in out:print(json.dumps(x,ensure_ascii=False))
