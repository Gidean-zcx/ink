from pathlib import Path
import json,hashlib
from playwright.sync_api import sync_playwright
W=Path('work/xiaoyuan-camera-preview');assert not (W/'root-capture-result.json').exists();D=W/'frames';D.mkdir(exist_ok=True)
source=json.loads((W/'source.json').read_text());url=source['media_url'];assert hashlib.sha256(Path(source['source_frontend_path']).read_bytes()).hexdigest()==source['source_frontend_sha256']
out=dict(url=url,checked_on='2026-10-27',method='root_single_normal_browser_media_seek_after_helper_parameter_failure',audio_observed=False,product_operated=False,continuous_observation=False,frames=[],network=[])
with sync_playwright() as p:
 b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':800});page=ctx.new_page();out['browser']=b.version
 page.on('response',lambda r:out['network'].append(dict(url=r.url,status=r.status,content_type=r.headers.get('content-type'),content_range=r.headers.get('content-range'))) if r.url==url else None)
 try:
  page.set_content('<html><body style="margin:0"><video muted preload="metadata" style="width:1280px;height:auto" src="'+url+'"></video></body></html>')
  page.wait_for_function('document.querySelector("video").readyState>=1 || document.querySelector("video").error',timeout=25000)
  v=page.locator('video');out['metadata']=v.evaluate('(v)=>({duration:v.duration,width:v.videoWidth,height:v.videoHeight,error:v.error?{code:v.error.code,message:v.error.message}:null})')
  if any(r['status'] in [401,403,412,429] for r in out['network']):out['stopped']='access refusal'
  elif out['metadata']['error']:out['stopped']='media error'
  else:
   for t in [0,2,4,6,8,10,12,14]:
    if t>=out['metadata']['duration']:continue
    v.evaluate('(v,t)=>{v.pause();v.currentTime=t}',t)
    page.wait_for_function('(t)=>{let v=document.querySelector("video");return !v.seeking&&v.readyState>=2&&Math.abs(v.currentTime-t)<0.2}',arg=t,timeout=12000)
    actual=v.evaluate('(v)=>v.currentTime');path=D/f'{t:02d}s.png';v.screenshot(path=str(path));raw=path.read_bytes();out['frames'].append(dict(requested_seconds=t,actual_seconds=actual,path=str(path),pixels=[int.from_bytes(raw[16:20],'big'),int.from_bytes(raw[20:24],'big')],sha256=hashlib.sha256(raw).hexdigest()))
 except Exception as e:out['error']=type(e).__name__+': '+str(e);out['stopped']='root attempt failed; no retry'
 finally:ctx.close();b.close()
(W/'root-capture-result.json').write_text(json.dumps(out,ensure_ascii=False,indent=2,allow_nan=False)+'\n');print(json.dumps(out,ensure_ascii=False,indent=2))
