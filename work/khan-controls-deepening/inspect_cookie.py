import asyncio,json
from playwright.async_api import async_playwright
async def main():
 async with async_playwright() as p:
  b=await p.chromium.launch(headless=True);page=await b.new_page()
  await page.goto('https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/',wait_until='domcontentloaded',timeout=40000)
  await page.wait_for_timeout(800)
  print(json.dumps(await page.get_by_role('button').all_text_contents()))
  print((await page.locator('body').inner_text())[-2500:])
  await b.close()
asyncio.run(main())
