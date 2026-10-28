import json,hashlib,time,traceback
from pathlib import Path
from playwright.sync_api import sync_playwright
R=Path('/home/workspace/reader');W=R/'work/iflytek-planning-reading-deepening'
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
p=sync_playwright().start();b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1440,'height':1000});page=ctx.new_page();page.set_default_timeout(8000)
events=[];commands=[]
def response(r):
 if 'xunfei' in r.url and ('.mp4' in r.url or '/assistant/' in r.url):
  events.append(dict(url=r.url,status=r.status,content_type=r.headers.get('content-type'),content_length=r.headers.get('content-length'),content_range=r.headers.get('content-range')))
  dump(W/'network-events.json',events)
page.on('response',response)
def capture(name):
 path=W/'browser'/(name+'.png');assert not path.exists();page.screenshot(path=str(path))
 d=dict(name=name,url=page.url,text=page.locator('body').inner_text(),sha256=sha(path),videos=page.locator('video').evaluate_all('(vs)=>vs.map(v=>({src:v.currentSrc,duration:v.duration,time:v.currentTime,paused:v.paused,readyState:v.readyState,width:v.videoWidth,height:v.videoHeight}))'))
 dump(path.with_suffix('.json'),d);print(json.dumps(d,ensure_ascii=False),flush=True)
def prepare():
 page.wait_for_function('()=>{const v=document.querySelector("video");return v&&v.readyState>=2}',timeout=15000)
 page.locator('video').evaluate('(v)=>{v.pause();v.muted=true;v.controls=false;Object.assign(v.style,{position:"fixed",left:"80px",top:"80px",width:"1280px",height:"720px",maxWidth:"none",zIndex:"10000"})}')
 page.mouse.move(10,10)
 if any(e['status'] in [401,403,412,429] for e in events):raise RuntimeError('ACCESS_STOP')
def sample(prefix,positions):
 v=page.locator('video');duration=v.evaluate('(v)=>v.duration');assert all(0<=t<duration for t in positions),(positions,duration)
 dest=W/'frames'/prefix;dest.mkdir(parents=True,exist_ok=True);records=[]
 for t in positions:
  path=dest/f'{t:06.2f}.png';assert not path.exists()
  v.evaluate('(v,t)=>{v.currentTime=t}',t)
  page.wait_for_function('(t)=>{const v=document.querySelector("video");return !v.seeking&&v.readyState>=2&&Math.abs(v.currentTime-t)<0.15}',arg=t,timeout=12000)
  v.screenshot(path=str(path));raw=path.read_bytes();dims=[int.from_bytes(raw[16:20],'big'),int.from_bytes(raw[20:24],'big')]
  meta=v.evaluate('(v)=>({url:v.currentSrc,duration:v.duration,position:v.currentTime,width:v.videoWidth,height:v.videoHeight,paused:v.paused,muted:v.muted,readyState:v.readyState})')
  meta.update(requested=t,path=str(path.relative_to(R)),sha256=sha(path),actual_pixels=dims,help_player_url=page.url,method='official_help_video_browser_seek_and_element_screenshot',audio_observed=False,product_hands_on=False)
  dump(path.with_suffix('.json'),meta);records.append(meta)
 print(json.dumps(dict(prefix=prefix,count=len(records),duration=duration,positions=positions),ensure_ascii=False),flush=True)
def overview(prefix,step=15):
 d=page.locator('video').evaluate('(v)=>v.duration');sample(prefix,list(range(10,int(d)-2,step))+[round(d-2,1)])
def open_title(title,name):
 if '#/play' in page.url:page.go_back()
 page.get_by_text(title,exact=True).click();prepare();capture(name)
try:
 r=page.goto('https://xxj.xunfei.cn/h5-help/',wait_until='domcontentloaded',timeout=30000);page.wait_for_timeout(1500);print('HTTP',r.status,'BROWSER',b.version,flush=True);capture('01-help-home')
 if r.status not in [401,403,412,429]:
  while True:
   line=input()
   if line=='quit':break
   record=dict(command=line,started=time.time())
   try:exec(line);record['status']='ok'
   except Exception as e:record.update(status='error',error=repr(e));print(type(e).__name__+': '+str(e),flush=True)
   record['finished']=time.time();commands.append(record);dump(W/'commands.json',commands)
finally:
 dump(W/'network-events.json',events);dump(W/'commands.json',commands);ctx.close();b.close();p.stop()
