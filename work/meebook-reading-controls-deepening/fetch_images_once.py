from pathlib import Path
import json,requests,hashlib
W=Path('work/meebook-reading-controls-deepening');assert not (W/'images-acquisition.json').exists()
rows=[];keys=['stkiW','scm0D','scX3I','sc9Rw','suUvY','sehon','sedDf']
images=json.loads((W/'raw/helpix-media.json').read_text())['images']
for key in keys:
 url=next(x['src'] for x in images if '/'+key+'.md.' in x.get('src',''));item=dict(key=key,url=url,source_page='https://www.helpix.ru/opinion/202409/60517-meebook_m7.html',checked_on='2026-10-27')
 try:
  r=requests.get(url,timeout=20);p=W/'images'/f'{key}.jpg';p.write_bytes(r.content);item.update(status=r.status_code,content_type=r.headers.get('content-type'),path=str(p),sha256=hashlib.sha256(r.content).hexdigest())
  if r.status_code!=200 or not (r.headers.get('content-type') or '').startswith('image/'):
   item['stopped']='unsuccessful image; stop route';rows.append(item);break
 except Exception as e:item['error']=str(e);rows.append(item);break
 rows.append(item)
(W/'images-acquisition.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
