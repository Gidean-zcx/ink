import json
from pathlib import Path
from datetime import datetime,timezone
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/time-controls-deepening/browser')
p=sync_playwright().start(); b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':950});ctx.tracing.start(screenshots=False,snapshots=False,sources=False);page=ctx.new_page();page.set_default_timeout(5000);page.set_default_navigation_timeout(45000)
errors=[]
page.on('pageerror',lambda e:errors.append({'kind':'pageerror','error':str(e)}))
page.on('requestfailed',lambda r:errors.append({'kind':'requestfailed','url':r.url,'failure':r.failure}))
def capture(name):
 page.screenshot(path=str(out/(name+'.png')),full_page=False)
 (out/(name+'.html')).write_text(page.content())
 data={'name':name,'captured_at':datetime.now(timezone.utc).isoformat(),'url':page.url,'title':page.title(),'text':page.locator('body').inner_text(),'controls':page.locator('button,a,input,[role="button"]').evaluate_all('(es)=>es.filter(e=>e.getClientRects().length&&getComputedStyle(e).visibility!=="hidden").map(e=>({tag:e.tagName,text:e.innerText,aria:e.getAttribute("aria-label"),title:e.title,href:e.href,id:e.id,type:e.type}))'),'media':page.locator('audio,video').evaluate_all('(es)=>es.map(e=>({tag:e.tagName,src:e.currentSrc,paused:e.paused,ended:e.ended,time:e.currentTime,duration:Number.isFinite(e.duration)?e.duration:null,ready:e.readyState,network:e.networkState,error:e.error?.message,rate:e.playbackRate}))'),'frames':[f.url for f in page.frames]}
 (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2))
 print(json.dumps({'capture':name,'url':data['url'],'title':data['title'],'text_excerpt':data['text'][:500],'controls':data['controls'][:35],'media':data['media'],'frames':data['frames']},ensure_ascii=False),flush=True)
try:
 r=page.goto('https://www.timeforkids.com/k1/spotlight-on-plants-k1/',wait_until='domcontentloaded');print('HTTP',r.status,flush=True);page.wait_for_timeout(2500);capture('01-article-initial'); print('BROWSER',b.version,flush=True)
 if r.status not in [401,403,429]:
  while True:
   line=input()
   if line=='quit':break
   try:exec(line)
   except Exception as e:print(type(e).__name__+': '+str(e),flush=True)
finally:
 (out/'browser-errors.json').write_text(json.dumps(errors,ensure_ascii=False,indent=2));ctx.tracing.stop(path=str(out/'trace.zip'));ctx.close();b.close();p.stop()
