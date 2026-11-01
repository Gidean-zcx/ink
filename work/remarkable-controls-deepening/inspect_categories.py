import json,hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
W=Path('/home/workspace/reader/work/remarkable-controls-deepening')
with sync_playwright() as p:
 b=p.chromium.launch(headless=True);c=b.new_context(viewport={'width':1280,'height':900});page=c.new_page()
 for key,u in [('features-index','https://support.remarkable.com/s/topic/0TOQD0000002LDF4A2/features-and-functionality'),('troubleshooting-index','https://support.remarkable.com/s/topic/0TO7Q000000sxrZWAQ/troubleshooting'),('account-index','https://support.remarkable.com/s/topic/0TOQD0000002LA14AM/account-and-subscription')]:
  try:
   resp=page.goto(u,wait_until='domcontentloaded',timeout=25000);status=resp.status if resp else None
   if status in [401,403,429]:print(key,'stop',status);continue
   page.wait_for_timeout(2000);raw=page.content();body=page.locator('body').inner_text();links=page.locator('a').evaluate_all('(es)=>es.map(e=>({text:e.innerText,url:e.href}))')
   (W/(key+'.rendered.html')).write_text(raw);(W/(key+'.rendered.txt')).write_text(body);(W/(key+'.rendered-links.json')).write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n')
   print(json.dumps(dict(key=key,url=page.url,status=status,body=body[:500],links=[x for x in links if "/article/" in x["url"] or "/topic/" in x["url"]]),ensure_ascii=False),flush=True)
  except Exception as e:print(key,str(e),flush=True)
 c.close();b.close()
