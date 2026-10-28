from pathlib import Path
from playwright.sync_api import sync_playwright
import json,hashlib
W=Path(__file__).resolve().parent;O=W/'renderer-observations'
with sync_playwright() as pw:
 b=pw.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':320,'height':640});p=ctx.new_page();responses=[];p.on('response',lambda r:responses.append(dict(url=r.url,status=r.status)))
 try:
  p.goto((W/'synthetic-graph.html').as_uri(),wait_until='domcontentloaded',timeout=30000);p.wait_for_function('typeof Graph!=="undefined"',timeout=15000);p.wait_for_timeout(1800)
  coords=p.evaluate('()=>Graph.graphData().nodes.map(n=>({id:n.id,...Graph.graph2ScreenCoords(n.x,n.y)}))')
  visible=[n for n in coords if 8<n['x']<312 and 75<n['y']<600];assert visible,coords
  n=visible[0];p.mouse.click(n['x'],n['y']);p.wait_for_timeout(950)
  state=p.evaluate('''()=>({selected:selectedNode?selectedNode.id:null,filter:filterBookId,panelOpen:document.querySelector('#panel').classList.contains('open'),panelRect:document.querySelector('#panel').getBoundingClientRect().toJSON(),textRect:document.querySelector('#pt').getBoundingClientRect().toJSON(),text:document.querySelector('#pt').textContent,canvasRect:document.querySelector('canvas').getBoundingClientRect().toJSON()})''')
  path=O/'10-narrow-fresh-panel.png';p.screenshot(path=str(path));r=dict(environment=dict(browser=b.version,viewport=p.viewport_size),action=dict(method='visible_canvas_mouse_click',target=n),state=state,screenshot=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),network=responses)
  (O/'narrow-fresh.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k!='network'},ensure_ascii=False))
 finally:ctx.close();b.close()
