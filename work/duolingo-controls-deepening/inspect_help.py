import asyncio, json, hashlib
from pathlib import Path
from playwright.async_api import async_playwright
w=Path('/home/workspace/reader/work/duolingo-controls-deepening')
async def main():
 async with async_playwright() as p:
  browser=await p.chromium.launch(headless=True)
  context=await browser.new_context(viewport={'width':1440,'height':1000},locale='en-US')
  page=await context.new_page()
  res=await page.goto('https://www.duolingo.com/help',wait_until='domcontentloaded',timeout=25000)
  record={'url':page.url,'status':res.status,'browser':browser.version,'product_operated':False}
  if res.status==200:
   await page.wait_for_timeout(3000)
   text=await page.locator('body').inner_text()
   (w/'help-rendered.txt').write_text(text)
   (w/'help-rendered.html').write_text(await page.content())
   links=await page.locator('a').evaluate_all('(es)=>es.map(e=>({text:e.innerText,href:e.href}))')
   (w/'help-rendered-links.json').write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n')
   record['body_preview']=text[:900];record['links']=len(links)
   record['files']={str(x.relative_to('/home/workspace/reader')):hashlib.sha256(x.read_bytes()).hexdigest() for x in w.glob('help-rendered*')}
  (w/'help-browser.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
  print(json.dumps(record,ensure_ascii=False));await browser.close()
asyncio.run(main())
