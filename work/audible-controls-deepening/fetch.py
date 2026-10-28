import urllib.request,urllib.error,json,hashlib,pathlib,sys
base=pathlib.Path(__file__).parent
entries=[]
for spec in sys.argv[1:]:
 name,url=spec.split('=',1); p=base/name; p.parent.mkdir(parents=True,exist_ok=True)
 try:
  r=urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'reader-public-doc-research'}),timeout=30); data=r.read(); p.write_bytes(data)
  row={'path':str(p),'url':url,'status':r.status,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
 except urllib.error.HTTPError as e: row={'path':str(p),'url':url,'status':e.code,'error':str(e)}
 except Exception as e: row={'path':str(p),'url':url,'status':None,'error':str(e)}
 entries.append(row); print(json.dumps(row,ensure_ascii=False))
with (base/'fetch-log.jsonl').open('a') as f:
 for row in entries:f.write(json.dumps(row,ensure_ascii=False)+'\n')
if any(x['status']!=200 for x in entries):sys.exit(1)
