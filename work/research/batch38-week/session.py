from pathlib import Path
import json, time, hashlib, traceback
from playwright.sync_api import sync_playwright

OUT=Path('/home/workspace/reader/work/research/batch38-week')
OUT.mkdir(parents=True,exist_ok=True)
PROFILE=OUT/'desktop-profile'
URL='https://reader.emagazines.com/issue/the-week-junior/october-23-2026?ipt=F0826FC3-4129-46CF-A5F3-FF70A931F1FF'
p=sync_playwright().start()
states=[]; actions=[]; network=[]; errors=[]; context_name='desktop'
def write_logs():
    for name,values in [('observed-states',states),('trace-actions',actions),('network',network),('errors',errors)]:
        (OUT/(name+'.json')).write_text(json.dumps(values,ensure_ascii=False,indent=2)+'\n')
def attach(pg):
    pg.on('response',lambda r: network.append({'url':r.url,'status':r.status,'method':r.request.method,'type':r.request.resource_type,'context':context_name}))
    pg.on('pageerror',lambda e:errors.append({'error':str(e),'context':context_name}))
def launch_desktop():
    ctx=p.chromium.launch_persistent_context(str(PROFILE),headless=True,viewport={'width':1200,'height':900},args=['--no-sandbox'])
    pg=ctx.pages[0] if ctx.pages else ctx.new_page();attach(pg)
    return ctx,pg
ctx,page=launch_desktop()
(OUT/'environment.json').write_text(json.dumps({'checked_on':'2026-10-27','browser':ctx.browser.version if ctx.browser else 'persistent Chromium','desktop_viewport':{'width':1200,'height':900},'method':'automated_public_browser','fresh_local_profile':not any(PROFILE.glob('*bookmarks*')),'physical_device':False,'site_build':None},indent=2)+'\n')
def capture(label):
    num=len(states);name=f'{num:02}-{label}';path=OUT/(name+'.png')
    page.screenshot(path=str(path),full_page=False)
    (OUT/(name+'.html')).write_text(page.content())
    data={'index':num,'label':label,'context':context_name,'url':page.url,'title':page.title(),'screenshot':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'text':page.locator('body').inner_text(),'controls':page.locator('button,a,input,select,audio,video').evaluate_all('els=>els.filter(e=>e.getClientRects().length).map(e=>({tag:e.tagName,text:(e.innerText||" ").slice(0,100),aria:e.getAttribute("aria-label"),title:e.title,id:e.id,type:e.type,value:e.value,href:e.tagName==="A"?e.getAttribute("href"):null,paused:e.paused,currentTime:e.currentTime,duration:Number.isFinite(e.duration)?e.duration:null,readyState:e.readyState}))')}
    states.append(data);write_logs();print(json.dumps(data,ensure_ascii=False),flush=True)
def act(label,fn,wait_ms=800):
    entry={'index':len(actions),'label':label,'before_state':len(states)-1,'context':context_name,'url_before':page.url}
    try:
        fn();page.wait_for_timeout(wait_ms);entry['result']='action_returned'
    except Exception as e:
        entry['result']='error';entry['error']=str(e)
    entry['url_after']=page.url;actions.append(entry);capture(label);entry['after_state']=len(states)-1;write_logs()
def restart():
    global ctx,page
    ctx.close();ctx,page=launch_desktop();page.goto(URL,wait_until='domcontentloaded',timeout=30000);page.wait_for_timeout(2500)
def mobile():
    global ctx,page,context_name
    ctx.close();context_name='mobile-emulation'
    global browser
    browser=p.chromium.launch(headless=True,args=['--no-sandbox'])
    ctx=browser.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,device_scale_factor=1)
    page=ctx.new_page();attach(page);page.goto(URL,wait_until='domcontentloaded',timeout=30000);page.wait_for_timeout(2500)
r=page.goto(URL,wait_until='domcontentloaded',timeout=30000)
if not r or r.status!=200:
    print('INITIAL_STATUS '+str(r.status if r else None),flush=True);write_logs();ctx.close();p.stop();raise SystemExit(1)
page.wait_for_timeout(3000);capture('initial-desktop')
while True:
    try:line=input()
    except EOFError:break
    if line=='quit':break
    try:exec(line)
    except Exception as e:print(type(e).__name__+': '+str(e),flush=True);errors.append({'command_error':str(e)});write_logs()
write_logs();ctx.close()
if 'browser' in globals():browser.close()
p.stop()
