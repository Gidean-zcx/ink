from pathlib import Path
import hashlib, json, re, sys, time
from playwright.sync_api import sync_playwright, TimeoutError as PlaywrightTimeoutError

ROOT=Path('work/research/batch36-moaan')
CAND=ROOT/'media-candidates.json'
OUT=ROOT/'samples'
OUT.mkdir(parents=True,exist_ok=True)
RESULT=ROOT/'media-sampling.json'
if RESULT.exists():
    raise SystemExit('refuse rerun: media-sampling.json already exists')
items=[]
for group in json.loads(CAND.read_text()):
    for m in group['media']:
        if m.get('alt'):
            items.append({'source_file':group['file'],'evidence_id':group['evidence_id'],**m})
assert len(items)==9, len(items)
records=[]
run={'started_at':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'candidate_file':str(CAND),'candidate_sha256':hashlib.sha256(CAND.read_bytes()).hexdigest(),'browser':None,'records':records,'stopped':False,'stop_reason':None}
blocked={403,412,429}
try:
  with sync_playwright() as pw:
    browser=pw.chromium.launch(headless=True)
    run['browser']=browser.version
    context=browser.new_context(viewport={'width':1600,'height':1200},device_scale_factor=1)
    for idx,item in enumerate(items,1):
      rec={**item,'index':idx,'responses':[],'request_failures':[],'page_errors':[],'console_errors':[],'screenshots':[],'http_status':None,'natural_size':None,'complete':None,'error':None}
      records.append(rec)
      page=context.new_page()
      target=item['url']
      page.on('response',lambda r,rec=rec,target=target: rec['responses'].append({'url':r.url,'status':r.status,'content_type':r.headers.get('content-type')}) if r.url==target else None)
      page.on('requestfailed',lambda r,rec=rec,target=target: rec['request_failures'].append({'url':r.url,'failure':r.failure}) if r.url==target else None)
      page.on('pageerror',lambda e,rec=rec: rec['page_errors'].append(str(e)))
      page.on('console',lambda m,rec=rec: rec['console_errors'].append(m.text) if m.type=='error' else None)
      try:
        html='''<!doctype html><meta charset="utf-8"><style>html,body{margin:0;background:white;min-width:100%;min-height:100%}img{display:block;max-width:none;max-height:none}</style><img id="target" src="'''+target.replace('&','&amp;').replace('"','&quot;')+'''">'''
        page.set_content(html,wait_until='domcontentloaded',timeout=30000)
        page.locator('#target').wait_for(state='attached',timeout=5000)
        page.wait_for_function("img=>img.complete",page.locator('#target').element_handle(),timeout=30000)
        dims=page.locator('#target').evaluate("img=>({naturalWidth:img.naturalWidth,naturalHeight:img.naturalHeight,width:img.width,height:img.height,complete:img.complete,currentSrc:img.currentSrc})")
        rec['natural_size']=[dims['naturalWidth'],dims['naturalHeight']]
        rec['rendered_size']=[dims['width'],dims['height']]
        rec['complete']=dims['complete'];rec['current_src']=dims['currentSrc']
        for sample,delay in [('0s',0),('2s',2000),('5s',3000)]:
          if delay: page.wait_for_timeout(delay)
          safe=re.sub(r'[^A-Za-z0-9._-]+','-',item['alt']).strip('-')
          fn=f'{idx:02d}-{item["evidence_id"]}-{safe}-{sample}.png'
          path=OUT/fn
          page.locator('#target').screenshot(path=str(path),timeout=30000,animations='allow')
          raw=path.read_bytes()
          rec['screenshots'].append({'timepoint':sample,'path':str(path),'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw),'png_pixels':[int.from_bytes(raw[16:20],'big'),int.from_bytes(raw[20:24],'big')]})
        if rec['responses']:
          rec['http_status']=rec['responses'][-1]['status']
        if rec['http_status'] in blocked:
          run['stopped']=True;run['stop_reason']=f'access stop HTTP {rec["http_status"]} at {target}'
          page.close();break
        if rec['natural_size']==[0,0]:
          rec['error']='image completed with zero natural dimensions'
          run['stopped']=True;run['stop_reason']=f'image load failed at {target}'
          page.close();break
      except Exception as e:
        rec['error']=f'{type(e).__name__}: {e}'
        if rec['responses']: rec['http_status']=rec['responses'][-1]['status']
        run['stopped']=True;run['stop_reason']=f'capture error at {target}: {rec["error"]}'
        page.close();break
      page.close()
    context.close();browser.close()
except Exception as e:
  run['stopped']=True
  run['stop_reason']=f'environment error: {type(e).__name__}: {e}'
finally:
  run['finished_at']=time.strftime('%Y-%m-%dT%H:%M:%S%z')
  RESULT.write_text(json.dumps(run,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'browser':run['browser'],'records':len(records),'stopped':run['stopped'],'stop_reason':run['stop_reason'],'result':str(RESULT)},ensure_ascii=False))
sys.exit(1 if run['stopped'] else 0)
