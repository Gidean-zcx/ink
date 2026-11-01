from pathlib import Path
exec(Path('work/huawei-controls-deepening/fetch.py').read_text().split('urls=')[0])
urls={'record-nz.html':'https://consumer.huawei.com/nz/support/content/en-gb15879066/','notes-ca.html':'https://consumer.huawei.com/ca/support/content/en-us15879070/','dock-lk.html':'https://consumer.huawei.com/lk/support/content/en-us15892864/','gestures-bd.html':'https://consumer.huawei.com/bd/support/content/en-gb15879072/','support-product-original.js':'https://consumer.huawei.com/etc/designs/huawei-cbg-site/clientlib-support-v3/multichannel-product-detail.min.a85cb92293989c016b91455efb79d525.js'}
def fetch(it):
 n,u=it;d={'file':n,'url':u,'checked_on':'2026-10-27'}
 try:
  r=requests.get(u,timeout=(5,18));d.update(status=r.status_code,final_url=r.url,content_type=r.headers.get('content-type'))
  if r.status_code==200:
   (out/n).write_bytes(r.content)
   if n.endswith('.html'):(out/(Path(n).stem+'.txt')).write_text(extract(r.content))
   d.update(raw_work_path=str((out/n).relative_to(root)),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),article_body='id="tkb-content"' in r.text)
  else:d['stopped_without_retry']=True
 except Exception as e:d['error']=str(e)
 return d
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as p:res=list(p.map(fetch,urls.items()))
(out/'fetch-regional-log.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n')
for r in res:print(r['file'],r.get('status',r.get('error')),r.get('article_body'),r.get('raw_sha256'))
# Keep earlier transformed JS explicitly distinct from a raw capture.
p=out/'fetch-extra-log.json';a=json.loads(p.read_text())
for r in a:
 if r['file']=='support-product.js':
  r['raw_work_path']=None;r['raw_sha256']=None;r['capture_limitation']='Temporary JS was overwritten by text extraction; not a raw snapshot. Separately reacquired to support-product-original.js with no parser overwrite; no interface invoked.'
p.write_text(json.dumps(a,ensure_ascii=False,indent=2)+'\n')
