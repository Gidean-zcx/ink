import asyncio,json,hashlib
from pathlib import Path
from playwright.async_api import async_playwright
w=Path('/home/workspace/reader/work/inoreader-controls-deepening')
async def main():
 async with async_playwright() as p:
  browser=await p.chromium.launch(headless=True)
  context=await browser.new_context(viewport={'width':1440,'height':1000},device_scale_factor=1)
  page=await context.new_page();res=await page.goto('https://www.inoreader.com/blog/2026/10/introducing-article-edit-history.html',wait_until='domcontentloaded',timeout=25000)
  result={'url':page.url,'status':res.status,'browser':browser.version,'viewport':{'width':1440,'height':1000},'product_operated':False,'captures':[]}
  if res.status!=200:print(json.dumps(result));await browser.close();return
  for n in (1,2):
   image=page.locator(f'img[src$="/{n}-edit-history.png"]');await image.scroll_into_view_if_needed(timeout=10000);await image.wait_for(state='visible',timeout=10000)
   await page.wait_for_function('(n)=>{const i=document.querySelector(`img[src$="/${n}-edit-history.png"]`);return i&&i.complete&&i.naturalWidth>0}',arg=n,timeout=10000)
   await image.evaluate('(i)=>i.scrollIntoView({block:"center"})')
   await page.wait_for_timeout(250)
   info=await image.evaluate('(i)=>({src:i.currentSrc,width:i.naturalWidth,height:i.naturalHeight})')
   path=w/f'edit-history-reference-clean-{n:02}.png';await image.screenshot(path=str(path),timeout=10000)
   result['captures'].append(dict(**info,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
  (w/'image-observation-clean-capture.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result));await browser.close()
asyncio.run(main())
