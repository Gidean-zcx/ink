from pathlib import Path
import requests,json,hashlib
b=Path('work/quizlet-controls-deepening');u='https://help.quizlet.com/hc/en-us/articles/360030642972-Studying-with-Test';row={'url':u,'method':'requests GET timeout=(5,18), one attempt'}
try:
 r=requests.get(u,timeout=(5,18));row.update(status=r.status_code,final_url=r.url)
 if r.status_code==200:
  p=b/'test.html';p.write_bytes(r.content);row.update(path=str(p),sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
 elif r.status_code in (401,403,429):row['result']='stop requests route; do not retry or vary headers'
except requests.RequestException as e:row['error']=type(e).__name__+': '+str(e)
(b/'fetch-probe.json').write_text(json.dumps(row,ensure_ascii=False,indent=2)+'\n');print(json.dumps(row,ensure_ascii=False,indent=2))
