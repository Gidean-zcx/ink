from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
w=Path('work/fenbi-practice-controls-deepening')
items=[('c05-wrong2019.html','https://jingyan.baidu.com/article/d2b1d102e627605c7e37d4e0.html'),('c06-quick2022.html','https://jingyan.baidu.com/article/6dad5075034816e023e36ef8.html'),('c07-print2020.html','https://jingyan.baidu.com/article/495ba84133796979b20ede4a.html'),('c08-mock2020.html','https://jingyan.baidu.com/article/6b97984d773fd15da3b0bf2e.html')]
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
(w/'fetch-final-candidates-results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
for x in out:print(json.dumps(x,ensure_ascii=False))
