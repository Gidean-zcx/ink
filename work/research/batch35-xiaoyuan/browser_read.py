import json,hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
W=Path('/home/workspace/reader/work/research/batch35-xiaoyuan');W.mkdir(parents=True,exist_ok=True)
events=[]
with sync_playwright() as pw:
    browser=pw.chromium.launch(headless=True)
    ctx=browser.new_context(viewport={'width':1440,'height':1000})
    page=ctx.new_page();page.set_default_timeout(8000)
    for name,url in [('e1-video','https://www.bilibili.com/video/BV1TCe8zNEcV/'),('original-review','https://post.smzdm.com/p/a3x3nk5d/'),('followup-review','https://post.smzdm.com/p/a4x56p6x/')]:
        rec={'name':name,'url':url,'method':'normal_anonymous_browser_navigation','browser':browser.version}
        try:
            resp=page.goto(url,wait_until='domcontentloaded',timeout=30000)
            rec.update(status=resp.status if resp else None,final_url=page.url)
            body=page.locator('body').inner_text(timeout=8000)
            (W/(name+'.txt')).write_text(body)
            rec['initial_text']=body[:600]
            if rec['status'] in [401,403,412,429] or any(s in body[:1500].lower() for s in ['verify you are human','验证码','访问受限','安全验证']):
                rec['access_stop']=True
            else:
                page.wait_for_timeout(1800)
                (W/(name+'.html')).write_text(page.content())
                (W/(name+'.txt')).write_text(page.locator('body').inner_text())
                page.screenshot(path=str(W/(name+'.png')))
                rec['videos']=page.locator('video').evaluate_all('(vs)=>vs.map(v=>({time:v.currentTime,duration:Number.isFinite(v.duration)?v.duration:null,readyState:v.readyState,paused:v.paused,width:v.videoWidth,height:v.videoHeight}))')
                rec['images']=page.locator('img').evaluate_all('(xs)=>xs.map(x=>({src:x.currentSrc,alt:x.alt})).filter(x=>x.src)')
        except Exception as e: rec['error']=type(e).__name__+': '+str(e)
        events.append(rec)
        print(json.dumps({k:v for k,v in rec.items() if k!='images'},ensure_ascii=False),flush=True)
        if rec.get('access_stop') and 'smzdm' in url:break
    ctx.close();browser.close()
(W/'browser-read-log.json').write_text(json.dumps(events,ensure_ascii=False,indent=2)+'\n')
