import json,hashlib,time
from pathlib import Path
from playwright.sync_api import sync_playwright
W=Path('/home/workspace/reader/work/iflytek-video-controls-deepening');out=W/'browser'
p=sync_playwright().start();b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1440,'height':1000});page=ctx.new_page();page.set_default_timeout(8000)
events=[]
def response(r):
 if 'xunfei' in r.url and ('.mp4' in r.url or '/assistant/' in r.url):
  events.append(dict(url=r.url,status=r.status,content_type=r.headers.get('content-type'),content_length=r.headers.get('content-length'),content_range=r.headers.get('content-range')))
page.on('response',response)
def capture(name):
 path=out/(name+'.png');page.screenshot(path=str(path),full_page=False)
 data=dict(name=name,url=page.url,text=page.locator('body').inner_text(),videos=page.locator('video').evaluate_all('(vs)=>vs.map(v=>({src:v.currentSrc,duration:v.duration,time:v.currentTime,paused:v.paused,readyState:v.readyState,error:v.error?{code:v.error.code,message:v.error.message}:null,width:v.videoWidth,height:v.videoHeight}))'),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
 (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n');print(json.dumps(data,ensure_ascii=False),flush=True)
try:
 resp=page.goto('https://xxj.xunfei.cn/h5-help/',wait_until='domcontentloaded',timeout=30000);page.wait_for_timeout(2500)
 print('HTTP',resp.status if resp else None,'BROWSER',b.version,flush=True);capture('01-help-home')
 if resp and resp.status in [401,403,412,429]:print('ACCESS_STOP',flush=True)
 else:
  while True:
   line=input()
   if line=='quit':break
   try:exec(line)
   except Exception as e:print(type(e).__name__+': '+str(e),flush=True)
finally:
 (W/'network-events.json').write_text(json.dumps(events,ensure_ascii=False,indent=2)+'\n');ctx.close();b.close();p.stop()
