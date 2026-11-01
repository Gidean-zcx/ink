from acquire import *
from concurrent.futures import ThreadPoolExecutor
jobs=[('article-'+x,'https://help.ground.news/en/articles/'+x) for x in ['6692033','334913','245697','335041','648577','244865']]
with ThreadPoolExecutor(max_workers=3) as ex:rs=list(ex.map(lambda j:get(*j),jobs))
(W/'sources-more.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
for r in rs:print(r['name'],r.get('status',r.get('error')))
imgs=json.loads((W/'factuality-images.json').read_text());rs=[]
for i in [3,4,5]:
 u=imgs[i]['src'];r=requests.get(u,timeout=(8,20));p=W/f'factuality-image-{i}.png';p.write_bytes(r.content);rs.append(dict(name=p.name,url=u,status=r.status_code,path=str(p),sha256=hashlib.sha256(r.content).hexdigest()));print(p.name,r.status_code)
(W/'image-manifest.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
(W/'browser-access-attempt.json').write_text(json.dumps(dict(checked_on='2026-10-27',url='https://ground.news/search',method='Playwright Chromium156.0.8078.4 Linux1280x950 en-US anonymous',navigation_status=403,session_exit=0,product_actions=0,screenshots=0,note='Stopped on403; no bypass, authentication, retry or product interaction. No D4 evidence created.'),ensure_ascii=False,indent=2)+'\n')
