import json,hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
from urllib.parse import urlsplit,urlunsplit
W=Path('/home/workspace/reader/work/research/batch35-xiaoyuan');O=W/'clip-observations';O.mkdir(exist_ok=True)
records=[]
with sync_playwright() as pw:
    b=pw.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1440,'height':1000});p=ctx.new_page();p.set_default_timeout(10000)
    resp=p.goto('https://post.smzdm.com/p/a3x3nk5d/',wait_until='domcontentloaded',timeout=30000)
    assert resp.status==200,resp.status
    p.locator('video').first.wait_for()
    for i in range(p.locator('video').count()):
        v=p.locator('video').nth(i);v.scroll_into_view_if_needed();v.evaluate('(v)=>{v.pause();v.muted=true;}')
        p.wait_for_function('(i)=>document.querySelectorAll("video")[i].readyState>=2',arg=i,timeout=12000)
        data=v.evaluate('(v)=>({duration:v.duration,width:v.videoWidth,height:v.videoHeight,src:v.currentSrc})')
        parts=urlsplit(data.pop('src'));data['media_url_without_query']=urlunsplit((parts.scheme,parts.netloc,parts.path,'',''))
        for t in [0,min(4,data['duration']-.15),data['duration']-.15]:
            v.evaluate('(v,t)=>{v.pause();v.currentTime=t;}',t);p.wait_for_timeout(500)
            fn=f'clip-{i+1}-{t:.2f}s.png';v.screenshot(path=str(O/fn));r=dict(clip=i+1,requested_time=t,actual_time=v.evaluate('(v)=>v.currentTime'),image=fn,sha256=hashlib.sha256((O/fn).read_bytes()).hexdigest(),**data);records.append(r)
            print(json.dumps(r,ensure_ascii=False),flush=True)
    ctx.close();b.close()
(O/'observations.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
