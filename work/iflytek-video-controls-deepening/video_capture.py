# Executed inside the live research browser namespace, not standalone.
cdp=ctx.new_cdp_session(page)
cdp.send('Emulation.setDeviceMetricsOverride',dict(width=1440,height=1000,deviceScaleFactor=3,mobile=False))
def sample_video(prefix,positions):
 video=page.locator('video');video.evaluate('(v)=>{v.pause();v.muted=true}')
 dest=W/'frames'/prefix;dest.mkdir(parents=True,exist_ok=True)
 records=[]
 for t in positions:
  path=dest/f'{t:06.2f}.png'
  if path.exists():raise ValueError('refuse duplicate frame '+str(path))
  video.evaluate('(v,t)=>{v.currentTime=t}',t)
  page.wait_for_function('(t)=>{const v=document.querySelector("video");return !v.seeking&&v.readyState>=2&&Math.abs(v.currentTime-t)<0.15}',arg=t,timeout=12000)
  video.screenshot(path=str(path),scale='device')
  meta=video.evaluate('(v)=>({url:v.currentSrc,duration:v.duration,position:v.currentTime,width:v.videoWidth,height:v.videoHeight,paused:v.paused,muted:v.muted,readyState:v.readyState})')
  meta.update(requested=t,path=str(path.relative_to(W.parents[1])),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),method='official_help_video_browser_seek_and_element_screenshot',device_scale_factor=3,audio_observed=False,product_hands_on=False)
  (dest/f'{t:06.2f}.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n');records.append(meta)
 print(json.dumps(dict(prefix=prefix,count=len(records),duration=records[0]['duration'] if records else None,positions=[x['position'] for x in records]),ensure_ascii=False),flush=True)
