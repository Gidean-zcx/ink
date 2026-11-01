from pathlib import Path
import json, hashlib, requests
from playwright.sync_api import sync_playwright
W=Path('work/dragonbox-gameplay-deepening')
assert not (W/'acquisition.json').exists(), 'one-time acquisition'
results=[]
for key,url in [('geekdad','https://geekdad.com/2015/12/dragonbox-osmo-numbers/'),('youngminds','https://www.gamesforyoungminds.com/blog/2018/3/16/dragonbox-numbers')]:
    item={'key':key,'url':url,'method':'ordinary_anonymous_GET','date':'2026-10-27'}
    try:
        r=requests.get(url,timeout=25);item.update(status=r.status_code,final_url=r.url)
        path=W/'raw'/f'{key}.html';path.write_bytes(r.content)
        item.update(path=str(path),sha256=hashlib.sha256(r.content).hexdigest())
        if r.status_code in [401,403,412,429]: item['stopped']='refusal; no retry'
    except Exception as e:item['error']=str(e)
    results.append(item)
    print(json.dumps(item),flush=True)
video={'url':'https://www.youtube.com/watch?v=yxTpyBpVCuA','method':'single_normal_anonymous_browser_navigation','audio_observed':False,'product_operated':False,'network':[]}
with sync_playwright() as p:
    b=p.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1280,'height':900});page=ctx.new_page()
    page.on('response',lambda r:video['network'].append({'url':r.url,'status':r.status}) if r.request.resource_type in ['document','media'] else None)
    try:
        response=page.goto(video['url'],wait_until='domcontentloaded',timeout=25000)
        video.update(status=response.status if response else None,browser=b.version,title=page.title())
        page.wait_for_timeout(2500)
        text=page.locator('body').inner_text(timeout=5000)
        (W/'browser/page-text.txt').write_text(text)
        (W/'browser/page.html').write_text(page.content())
        page.screenshot(path=str(W/'browser/page.png'))
        video['body_excerpt']=text[:1800]
        video['videos']=page.locator('video').evaluate_all('(vs)=>vs.map(v=>({duration:v.duration,currentTime:v.currentTime,readyState:v.readyState,paused:v.paused,width:v.videoWidth,height:v.videoHeight,error:v.error?{code:v.error.code,message:v.error.message}:null}))')
        video['stopped']='single inspection only; no challenge bypass, login, alternate embed or retry'
    except Exception as e:video['error']=type(e).__name__+': '+str(e);video['stopped']='navigation failed; no retry'
    finally:ctx.close();b.close()
(W/'acquisition.json').write_text(json.dumps({'pages':results,'video':video},ensure_ascii=False,indent=2)+'\n')
print(json.dumps(video,ensure_ascii=False))
