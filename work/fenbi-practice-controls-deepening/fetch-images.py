from pathlib import Path
import json,requests,hashlib
w=Path('work/fenbi-practice-controls-deepening');a=json.load((w/'image-directory.json').open());out=[]
select={'c05-wrong2019':[1,4,5],'c06-quick2022':[1,3,4],'c07-print2020':[5,7],'c08-mock2020':[5,6,7]}
stop=False
for key,indices in select.items():
 for i in indices:
  if stop:break
  u=a[key][i-1]['data-src'];p=w/'raw'/f'{key}-img{i}.jpg'
  try:
   r=requests.get(u,timeout=(5,20));p.write_bytes(r.content);d=dict(source=key,image_index=i,url=u,path=str(p),status=r.status_code,sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
   if r.status_code in (401,403,412,429):stop=True;d['stop']='refusal on image route; remaining images not requested'
   out.append(d);print(json.dumps(d,ensure_ascii=False),flush=True)
  except Exception as e:out.append(dict(url=u,error=str(e)));print(e,flush=True)
(w/'fetch-images-results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
