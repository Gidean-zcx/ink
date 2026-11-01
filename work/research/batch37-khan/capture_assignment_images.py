from pathlib import Path
import asyncio, json, hashlib
from playwright.async_api import async_playwright

OUT = Path('/home/workspace/reader/work/research/batch37-khan/assignment-images')
OUT.mkdir(parents=True, exist_ok=True)
SOURCES = [
    ('dialog', 'https://khankids.zendesk.com/hc/article_attachments/4403590761883'),
    ('assignments', 'https://khankids.zendesk.com/hc/article_attachments/4403593871515'),
]

async def main():
    records = []
    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True, args=['--no-sandbox'])
        page = await browser.new_page(viewport={'width': 1440, 'height': 1200}, device_scale_factor=1)
        for name, url in SOURCES:
            row = {'name': name, 'url': url, 'method': 'ordinary_public_browser_static_image', 'checked_on': '2026-10-27'}
            try:
                response = await page.goto(url, wait_until='load', timeout=30000)
                row['http_status'] = response.status if response else None
                row['final_url'] = page.url
                if row['http_status'] != 200:
                    row['stopped'] = True
                    records.append(row)
                    break
                img = page.locator('img').first
                await img.wait_for(state='visible', timeout=10000)
                row['image_size'] = await img.evaluate('(x)=>({width:x.naturalWidth,height:x.naturalHeight,complete:x.complete})')
                target = OUT / f'{name}.png'
                await img.screenshot(path=str(target))
                row['path'] = str(target)
                row['sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
                row['actual_product_operation'] = False
            except Exception as e:
                row['error'] = str(e)
                row['stopped'] = True
                records.append(row)
                break
            records.append(row)
        await browser.close()
    (OUT/'capture-log.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(records, ensure_ascii=False))

asyncio.run(main())
