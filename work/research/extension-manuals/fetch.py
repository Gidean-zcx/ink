import concurrent.futures, hashlib, json, urllib.request
from pathlib import Path
base=Path('/home/workspace/reader/work/research/extension-manuals')
urls={
 'sony-dpt.pdf':'https://helpguide.sony.net/dpt/rp1/v1/en/print.pdf',
 'quaderno-gen2.pdf':'https://www.fmworld.net/digital-paper/support/download/pdf/B6FK5131.pdf',
 'quaderno-gen3c.pdf':'https://www.fmworld.net/digital-paper/support/download/pdf/B5FL2101.pdf',
}
for n in ['15881454','15939571','15879064','15898323','15898388','15960354','15881450','15881453']:
 urls['huawei-'+n+'.html']='https://consumer.huawei.com/cn/support/content/zh-cn'+n+'/'
def fetch(item):
 name,url=item
 try:
  with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=40) as r:
   data=r.read(); status=r.status; final=r.url
  (base/name).write_bytes(data)
  return dict(file=name,url=url,final_url=final,status=status,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
 except Exception as e:return dict(file=name,url=url,error=str(e))
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as ex:results=list(ex.map(fetch,urls.items()))
(base/'fetch-log.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results:print(json.dumps(r,ensure_ascii=False))
