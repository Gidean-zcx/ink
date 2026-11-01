import csv, hashlib, json, pathlib, urllib.request, urllib.error
from datetime import datetime, timezone

root=pathlib.Path('work/research/batch34-anki')
raw=root/'raw'
raw.mkdir(parents=True,exist_ok=True)
inv=pathlib.Path('docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv')
rows=[r for r in csv.DictReader(inv.open(encoding='utf-8')) if r['status']=='directory_only']
stop_status={401,403,412,429}
challenge_markers=(b'captcha',b'cloudflare',b'attention required',b'verify you are human',b'access denied')
log=[]
stopped_hosts={}
for r in rows:
    url=r['source_url']
    host=urllib.request.urlparse(url).netloc
    rec={'item_id':r['item_id'],'title':r['title'],'request_url':url,'checked_at':datetime.now(timezone.utc).isoformat()}
    if host in stopped_hosts:
        rec.update({'skipped':True,'reason':f'source_stopped_after_{stopped_hosts[host]}'})
        log.append(rec); continue
    rel=urllib.request.urlparse(url).path.lstrip('/')
    dest=raw/rel
    req=urllib.request.Request(url,headers={'User-Agent':'reader-research/1.0 (+ordinary-public-GET)','Accept':'text/markdown,text/plain;q=0.9,*/*;q=0.1'})
    try:
        with urllib.request.urlopen(req,timeout=30) as resp:
            status=resp.status
            final=resp.geturl()
            body=resp.read()
            ctype=resp.headers.get('Content-Type')
        rec.update({'final_url':final,'http_status':status,'content_type':ctype,'bytes':len(body),'sha256':hashlib.sha256(body).hexdigest()})
        low=body[:200000].lower()
        challenged=any(m in low for m in challenge_markers)
        rec['challenge_detected']=challenged
        if status in stop_status or challenged:
            rec['saved']=False
            stopped_hosts[host]=f'http_{status}' if status in stop_status else 'challenge'
        elif status==200:
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_bytes(body)
            rec.update({'saved':True,'path':str(dest)})
        else:
            rec['saved']=False
    except urllib.error.HTTPError as e:
        body=e.read()
        rec.update({'final_url':e.geturl(),'http_status':e.code,'content_type':e.headers.get('Content-Type'),'bytes':len(body),'sha256':hashlib.sha256(body).hexdigest(),'saved':False})
        if e.code in stop_status:
            stopped_hosts[host]=f'http_{e.code}'
    except Exception as e:
        rec.update({'saved':False,'error':type(e).__name__+': '+str(e)})
    log.append(rec)
(root/'fetch-log.json').write_text(json.dumps({'generated_at':datetime.now(timezone.utc).isoformat(),'inventory':str(inv),'count_requested':len(rows),'stopped_hosts':stopped_hosts,'records':log},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'records':len(log),'saved':sum(bool(x.get('saved')) for x in log),'stopped_hosts':stopped_hosts,'errors':[x for x in log if x.get('error')]},ensure_ascii=False))
