import asyncio,json,hashlib,time,datetime
from pathlib import Path
from playwright.async_api import async_playwright
P=Path('work/khan-controls-deepening/browser');P.mkdir(exist_ok=True)
URL='https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/'
async def main():
 async with async_playwright() as p:
  b=await p.chromium.launch(headless=True)
  c=await b.new_context(viewport={'width':1280,'height':950})
  page=await c.new_page();resp=await page.goto(URL,wait_until='domcontentloaded',timeout=40000)
  report={'url':URL,'status':resp.status,'browser':b.version,'observed_on':str(datetime.date.today()),'method':'public_design_blog_illustration_sampling_not_product_operation','samples':[]}
  if resp.status!=200: print(json.dumps(report));await b.close();return
  for key in ['ThisIsHow_LIGHT','Sparkles_FINAL']:
   img=page.locator('img[src*="'+key+'"]').first
   await img.scroll_into_view_if_needed();await page.wait_for_function('(name)=>{const i=document.querySelector(`img[src*="${name}"]`);return i&&i.complete&&i.naturalWidth>0}',arg=key,timeout=15000)
   meta=await img.evaluate('(i)=>({src:i.currentSrc,width:i.naturalWidth,height:i.naturalHeight,alt:i.alt})');t0=time.monotonic()
   for n in range(4):
    if n: await page.wait_for_timeout(800)
    f=P/f'{key}-{n+1}.png';await img.screenshot(path=str(f))
    report['samples'].append({'file':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'after_local_start_s':round(time.monotonic()-t0,3),'media':meta})
  (P/'samples.json').write_text(json.dumps(report,ensure_ascii=False,indent=2));await b.close();print(json.dumps({'status':report['status'],'browser':report['browser'],'samples':len(report['samples'])}))
asyncio.run(main())
