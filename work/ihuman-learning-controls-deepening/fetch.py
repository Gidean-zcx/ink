import urllib.request,urllib.error,json,hashlib
from pathlib import Path
w=Path('work/ihuman-learning-controls-deepening');rows=[]
for name,url in [('bilibili-2020','https://www.bilibili.com/opus/425918819985666271'),('smzdm-2020','https://post.smzdm.com/p/aoo8one6/')]:
 row={'name':name,'url':url,'checked_on':'2026-10-27','method':'ordinary_public_GET_once'}
 try:
  with urllib.request.urlopen(url,timeout=25) as r:
   data=r.read();p=w/'raw'/(name+'.html');p.write_bytes(data);row.update(status=r.status,final_url=r.url,raw_work_path=str(p),raw_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),content_type=r.headers.get('Content-Type'))
 except urllib.error.HTTPError as e:row.update(status=e.code,stopped=True,error=str(e))
 except Exception as e:row.update(status=None,stopped=True,error=type(e).__name__+': '+str(e))
 rows.append(row);print(json.dumps(row,ensure_ascii=False),flush=True)
(w/'http-captures.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
