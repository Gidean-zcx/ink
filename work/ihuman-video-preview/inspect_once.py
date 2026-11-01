import json,hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
W=Path('work/ihuman-video-preview');D=W/'video';D.mkdir(exist_ok=True)
url='https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4'
events=[];out={'url':url,'source_page':'https://www.ihuman.com/shizi/','checked_on':'2026-10-27','audio_observed':False,'device_hands_on':False,'method':'normal_browser_public_video_paused_samples_no_mp4_saved'}
with sync_playwright() as p:
 b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':800});page=ctx.new_page()
 def response(r):
  if r.url==url:events.append({'url':r.url,'status':r.status,'content_type':r.headers.get('content-type'),'content_length':r.headers.get('content-length'),'content_range':r.headers.get('content-range')})
 page.on('response',response)
 try:
  page.set_content('<html><body style="margin:0;background:white"><video muted preload="metadata" style="width:1280px;height:720px" src="'+url+'"></video></body></html>')
  page.wait_for_function('document.querySelector("video").readyState>=1 || document.querySelector("video").error',timeout=25000)
  out['metadata']=page.locator('video').evaluate('(v)=>({duration:v.duration,width:v.videoWidth,height:v.videoHeight,error:v.error?{code:v.error.code,message:v.error.message}:null})')
  out['browser']=b.version
  if any(x['status'] in [401,403,412,429] for x in events):out['stopped']='HTTP refusal'
  elif out['metadata']['error']:out['stopped']='video error'
  else:
   d=out['metadata']['duration'];positions=sorted(set(round(min(d-0.5,t),2) for t in [10,30,50,70,90,120] if t<d));out['samples']=[]
   for t in positions:
    v=page.locator('video');v.evaluate('(v,t)=>{v.pause();v.currentTime=t}',t)
    page.wait_for_function('(t)=>{const v=document.querySelector("video");return !v.seeking&&v.readyState>=2&&Math.abs(v.currentTime-t)<0.2}',arg=t,timeout=12000)
    dest=D/f'{t:06.2f}.png';v.screenshot(path=str(dest));out['samples'].append({'seconds':t,'path':str(dest),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
 except Exception as e:out['error']=type(e).__name__+': '+str(e);out['stopped']='tool or loading error; no retry'
 finally:
  out['network']=events;(W/'video-inspection.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');ctx.close();b.close()
print(json.dumps(out,ensure_ascii=False,indent=2))
