import json,datetime,hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/tolino-controls-deepening/browser')
out.mkdir(exist_ok=True)
p=sync_playwright().start();b=p.chromium.launch(headless=True)
ctx=b.new_context(viewport={'width':1280,'height':950},locale='de-DE')
page=ctx.new_page();page.set_default_timeout(6000)
def capture(name):
 try:page.screenshot(path=str(out/(name+'.png')),full_page=False,timeout=20000)
 except Exception as e:print('Screenshot failed: '+str(e),flush=True)
 (out/(name+'.html')).write_text(page.content())
 data={'name':name,'url':page.url,'title':page.title(),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'text':page.locator('body').inner_text(),'buttons':page.locator('button').evaluate_all('(es)=>es.map(e=>({text:e.innerText,aria:e.getAttribute("aria-label"),test:e.getAttribute("data-test"),visible:!!(e.offsetWidth||e.offsetHeight)}))'),'inputs':page.locator('input').evaluate_all('(es)=>es.map(e=>({type:e.type,placeholder:e.placeholder,value:e.type==="password"?"REDACTED":e.value,visible:!!(e.offsetWidth||e.offsetHeight)}))')}
 (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n');print(json.dumps(data,ensure_ascii=False),flush=True)
try:
 res=page.goto('https://mytolino.de/onleihe/',wait_until='domcontentloaded',timeout=25000)
 print('HTTP',res.status,'BROWSER',b.version,flush=True)
 if res.status not in [401,403,429]:
  page.wait_for_timeout(2000);capture('01-home')
  while True:
   line=input()
   if line=='quit':break
   with (out/'actions.jsonl').open('a') as f:f.write(json.dumps({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'command':line})+'\n')
   try:exec(line)
   except Exception as e:
    error={'type':type(e).__name__,'message':str(e)}
    with (out/'errors.jsonl').open('a') as f:f.write(json.dumps(error)+'\n')
    print(json.dumps(error),flush=True)
finally:
 ctx.close();b.close();p.stop()
