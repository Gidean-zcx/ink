import urllib.request,concurrent.futures,json,hashlib
from pathlib import Path
P=Path('work/shortcuts-controls-deepening')
items=[('01-guide26','https://support.apple.com/guide/shortcuts/welcome/9.0/ios/26'),('02-model26','https://support.apple.com/guide/shortcuts/tpg3vrvwmclv/9.0/ios/26'),('03-model27','https://support.apple.com/guide/shortcuts/tpg3vrvwmclv/10.0/ios/27'),('04-sync26','https://support.apple.com/guide/shortcuts/apdb3a4240b0/9.0/ios/26'),('05-sync27','https://support.apple.com/guide/shortcuts/apdb3a4240b0/10.0/ios/27')]
def fetch(x):
 k,u=x
 try:
  with urllib.request.urlopen(u,timeout=40) as r:b=r.read();st=r.status;url=r.url
  f=P/(k+'.html');f.write_bytes(b)
  return {'key':k,'url':u,'final_url':url,'status':st,'raw_work_path':str(f),'raw_sha256':hashlib.sha256(b).hexdigest()}
 except Exception as e:return {'key':k,'url':u,'error':str(e)}
if __name__=='__main__':
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as ex:d=list(ex.map(fetch,items))
 (P/'fetch-results.json').write_text(json.dumps(d,ensure_ascii=False,indent=2))
 for x in d:print(x['key'],x.get('status',x.get('error')),x.get('final_url'))
