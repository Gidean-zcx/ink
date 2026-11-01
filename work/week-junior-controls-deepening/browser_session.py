import json, os
from pathlib import Path
from playwright.sync_api import sync_playwright
out=Path('/home/workspace/reader/work/week-junior-controls-deepening/browser')
p=sync_playwright().start()
b=p.chromium.launch(headless=True)
ctx=b.new_context(viewport={'width':1200,'height':900})
ctx.tracing.start(screenshots=True,snapshots=True,sources=True)
page=ctx.new_page()
url='https://reader.emagazines.com/issue/the-week-junior/october-23-2026?ipt=F0826FC3-4129-46CF-A5F3-FF70A931F1FF'
page.goto(url,wait_until='domcontentloaded',timeout=60000)
page.wait_for_timeout(5000)
def capture(name):
    page.screenshot(path=str(out/(name+'.png')),full_page=False)
    (out/(name+'.html')).write_text(page.content())
    data={'name':name,'url':page.url,'title':page.title(),'text':page.locator('body').inner_text(),'buttons':page.locator('button').evaluate_all('(els)=>els.map(e=>({text:e.innerText,aria:e.getAttribute("aria-label"),title:e.title,id:e.id}))'),'inputs':page.locator('input').evaluate_all('(els)=>els.map(e=>({type:e.type,placeholder:e.placeholder,aria:e.getAttribute("aria-label"),value:e.value,id:e.id}))')}
    (out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2))
    print(json.dumps(data,ensure_ascii=False),flush=True)
capture('01-initial')
print('BROWSER '+b.version,flush=True)
while True:
    line=input()
    if line=='quit':break
    try:exec(line)
    except Exception as e:print(type(e).__name__+': '+str(e),flush=True)
ctx.tracing.stop(path=str(out/'trace.zip'))
ctx.close(); b.close(); p.stop()
