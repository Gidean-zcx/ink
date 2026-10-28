from pathlib import Path
import json,requests,hashlib
out=Path('work/huawei-controls-deepening');res=[]
for source,indices in [('record-nz.html',[1,2,3,4]),('help-15898848.html',[1])]:
 a=json.loads((out/(source+'.images.json')).read_text())
 for i in indices:
  u=a[i]['src'];n=f'{Path(source).stem}-image-{i}.png';d={'source_html':source,'image_index_zero_based':i,'url':u,'file':n,'checked_on':'2026-10-27'}
  try:
   r=requests.get(u,timeout=(5,18));d.update(status=r.status_code,content_type=r.headers.get('content-type'),final_url=r.url)
   if r.status_code==200 and r.headers.get('content-type','').startswith('image/'):
    (out/n).write_bytes(r.content);d.update(raw_work_path=str(out/n),raw_sha256=hashlib.sha256(r.content).hexdigest())
   else:d['stopped_without_retry']=True
  except Exception as e:d['error']=str(e)
  res.append(d);print(n,d.get('status',d.get('error')),d.get('content_type'))
(out/'image-fetch-log.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n')
