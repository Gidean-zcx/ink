import json,time,hashlib
from pathlib import Path
from datetime import datetime,timezone
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/britannica-controls-deepening/browser');out.mkdir(parents=True,exist_ok=True)
pw=sync_playwright().start();browser=pw.chromium.launch(headless=True);ctx=browser.new_context(viewport={'width':1280,'height':950});ctx.tracing.start(screenshots=False,snapshots=False,sources=False);page=ctx.new_page();page.set_default_timeout(5000);page.set_default_navigation_timeout(45000)
ops=[];states=[];errs=[];statuses=[]
def now():return datetime.now(timezone.utc).isoformat()
page.on('pageerror',lambda e:errs.append(dict(kind='pageerror',message=str(e),at=now())))
page.on('requestfailed',lambda r:errs.append(dict(kind='requestfailed',url=r.url,failure=r.failure,at=now())))
page.on('response',lambda r:statuses.append(dict(url=r.url,status=r.status)) if r.request.resource_type=='document' else None)
def snap(name):
 page.screenshot(path=str(out/(name+'.png')),full_page=False)
 (out/(name+'.html')).write_text(page.content())
 state=dict(name=name,at=now(),url=page.url,title=page.title(),text=page.locator('body').inner_text(),controls=page.locator('button,a,input,select,[role="button"]').evaluate_all('(es)=>es.filter(e=>e.getClientRects().length&&getComputedStyle(e).visibility!=="hidden").map(e=>({tag:e.tagName,text:e.innerText,aria:e.getAttribute("aria-label"),title:e.title,href:e.href,id:e.id,type:e.type}))'),scroll=page.evaluate('({x:scrollX,y:scrollY})'))
 (out/(name+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2));states.append({k:state[k] for k in ['name','at','url','title','scroll']})
 print(json.dumps(dict(state=name,url=state['url'],scroll=state['scroll']),ensure_ascii=False),flush=True)
def go(url):
 r=page.goto(url,wait_until='domcontentloaded');print('HTTP',r.status if r else None,flush=True)
 if r and r.status in (401,403,429):raise RuntimeError('ACCESS_LIMIT_STOP')
 page.wait_for_timeout(1500)
try:
 print('BROWSER',browser.version,flush=True)
 while True:
  line=input()
  if line=='quit':break
  item=dict(action=line,started=now());ops.append(item)
  try:exec(line);item['result']='ok'
  except Exception as e:item['result']='error';item['error']=repr(e);print(repr(e),flush=True)
  item['ended']=now();(out/'trace-actions.json').write_text(json.dumps(ops,ensure_ascii=False,indent=2));(out/'observed-states.json').write_text(json.dumps(states,ensure_ascii=False,indent=2))
finally:
 (out/'browser-errors.json').write_text(json.dumps(errs,ensure_ascii=False,indent=2));(out/'document-statuses.json').write_text(json.dumps(statuses,ensure_ascii=False,indent=2));ctx.tracing.stop(path=str(out/'trace.zip'));ctx.close();browser.close();pw.stop()
