import json
from pathlib import Path
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/jiakao-controls-deepening/browser')
p=sync_playwright().start();b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':950});ctx.tracing.start(screenshots=False,snapshots=False,sources=False);page=ctx.new_page();page.set_default_timeout(5000)
def capture(name):
 page.screenshot(path=str(out/(name+'.png')),full_page=False)
 (out/(name+'.html')).write_text(page.content())
 data={'name':name,'url':page.url,'title':page.title(),'text':page.locator('body').inner_text(),'buttons':page.locator('button').evaluate_all('(els)=>els.map(e=>({text:e.innerText,aria:e.getAttribute("aria-label"),title:e.title,id:e.id,visible:!!(e.offsetWidth||e.offsetHeight)}))'),'inputs':page.locator('input').evaluate_all('(els)=>els.map(e=>({type:e.type,placeholder:e.placeholder,aria:e.getAttribute("aria-label"),value:e.type==="password"?"REDACTED":e.value,id:e.id,visible:!!(e.offsetWidth||e.offsetHeight)}))')}
 (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2));print(json.dumps(data,ensure_ascii=False),flush=True)
url='https://www.jiakaobaodian.com/mnks/exercise/0-car-kemu1.html'
try:
 resp=page.goto(url,wait_until='domcontentloaded',timeout=45000); print('HTTP',resp.status if resp else None,flush=True);page.wait_for_timeout(3000);capture('01-initial');print('BROWSER',b.version,flush=True)
 if resp and resp.status in [401,403,429]:print('ACCESS_LIMIT_STOP',flush=True)
 else:
  while True:
   line=input()
   if line=='quit':break
   try:exec(line)
   except Exception as e:print(type(e).__name__+': '+str(e),flush=True)
finally:
 ctx.tracing.stop(path=str(out/'trace.zip'));ctx.close();b.close();p.stop()
