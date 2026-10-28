import json,datetime
from pathlib import Path
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/ground-controls-deepening/browser');out.mkdir(exist_ok=True)
p=sync_playwright().start();b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':950},locale='en-US');page=ctx.new_page();page.set_default_timeout(6000)
def capture(name):
 page.screenshot(path=str(out/(name+'.png')),full_page=False,timeout=20000)
 (out/(name+'.html')).write_text(page.content())
 data=dict(name=name,url=page.url,title=page.title(),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),text=page.locator('body').inner_text(),buttons=page.locator('button').evaluate_all('(es)=>es.filter(e=>e.offsetWidth||e.offsetHeight).map(e=>({text:e.innerText,aria:e.getAttribute("aria-label"),title:e.title}))'),inputs=page.locator('input').evaluate_all('(es)=>es.filter(e=>e.offsetWidth||e.offsetHeight).map(e=>({type:e.type,placeholder:e.placeholder,value:e.type==="password"?"REDACTED":e.value}))'))
 (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n');print(json.dumps(dict(name=name,url=data['url'],text=data['text'][:4500],buttons=data['buttons'],inputs=data['inputs']),ensure_ascii=False),flush=True)
try:
 res=page.goto('https://ground.news/search',wait_until='domcontentloaded',timeout=25000);print('HTTP',res.status,'Browser',b.version,flush=True)
 if res.status not in [401,403,429]:
  page.wait_for_timeout(2000);capture('01-search')
  while True:
   line=input()
   if line=='quit':break
   with (out/'actions.jsonl').open('a') as f:f.write(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),command=line))+'\n')
   try:exec(line)
   except Exception as ex:
    err=dict(type=type(ex).__name__,message=str(ex));print(json.dumps(err),flush=True)
    with (out/'errors.jsonl').open('a') as f:f.write(json.dumps(err)+'\n')
finally:ctx.close();b.close();p.stop()
