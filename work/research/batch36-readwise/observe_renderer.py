from pathlib import Path
from playwright.sync_api import sync_playwright
import json,hashlib,time
W=Path(__file__).resolve().parent;O=W/'renderer-observations';O.mkdir(exist_ok=True)
states=[];actions=[];responses=[];errors=[]
def dump():
 for name,data in [('states.json',states),('actions.json',actions),('network.json',responses),('errors.json',errors)]:
  (O/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
with sync_playwright() as pw:
 b=pw.chromium.launch(headless=True);ctx=b.new_context(viewport={'width':1200,'height':820});p=ctx.new_page();p.set_default_timeout(8000)
 p.on('response',lambda r: responses.append(dict(url=r.url,status=r.status)))
 p.on('pageerror',lambda e: errors.append(str(e)))
 def snap(label):
  state=p.evaluate('''() => ({panelOpen:document.querySelector('#panel').classList.contains('open'),panelText:document.querySelector('#pt').textContent,panelSource:document.querySelector('#ps').textContent,note:document.querySelector('#pn').textContent,stats:document.querySelector('#stats').innerText,legend:[...document.querySelectorAll('#legend .it')].map(e=>({text:e.innerText,active:e.classList.contains('active')})),selected:typeof selectedNode!=='undefined'&&selectedNode?selectedNode.id:null,filter:typeof filterBookId!=='undefined'?filterBookId:null,hover:typeof hoverNode!=='undefined'&&hoverNode?hoverNode.id:null,graph:typeof Graph!=='undefined'?{nodes:Graph.graphData().nodes.map(n=>({id:n.id,bookId:n.bookId,text:n.fullText})),links:Graph.graphData().links.length,zoom:Graph.zoom()}:null})''')
  name=f'{len(states):02}-{label}.png';p.screenshot(path=str(O/name));states.append(dict(index=len(states),label=label,screenshot=name,sha256=hashlib.sha256((O/name).read_bytes()).hexdigest(),viewport=p.viewport_size,**state));dump();print(json.dumps(dict(state=len(states)-1,label=label,selected=state['selected'],filter=state['filter'],panelOpen=state['panelOpen']),ensure_ascii=False),flush=True)
 def click(selector):
  actions.append(dict(method='visible_locator_click',selector=selector));p.locator(selector).click();p.wait_for_timeout(950)
 def node(nid):
  xy=p.evaluate('(id)=>{const n=Graph.graphData().nodes.find(n=>n.id===id);return Graph.graph2ScreenCoords(n.x,n.y)}',nid)
  actions.append(dict(method='mouse_click_canvas_node',target=nid,coordinates=xy));p.mouse.click(xy['x'],xy['y']);p.wait_for_timeout(950)
 try:
  p.goto((W/'synthetic-graph.html').as_uri(),wait_until='domcontentloaded',timeout=30000)
  p.wait_for_function('typeof Graph!=="undefined" && Graph.graphData().nodes.length===5',timeout=15000);p.wait_for_timeout(1800);snap('initial')
  click('#legend .it:first-child');snap('filter-learning')
  node('h_101');snap('selected-with-filter')
  click('#cl .cc:first-child');snap('cross-source-from-panel')
  click('#sl .sb-item:first-child');snap('same-source-from-panel')
  click('#panel .close');snap('onscreen-esc')
  actions.append(dict(method='keyboard_press',key='Escape'));p.keyboard.press('Escape');p.wait_for_timeout(250);snap('keyboard-escape')
  node('h_301');snap('singleton-source')
  actions.append(dict(method='page_reload'));p.reload(wait_until='domcontentloaded');p.wait_for_function('typeof Graph!=="undefined"',timeout=15000);p.wait_for_timeout(1000);snap('reloaded')
  actions.append(dict(method='viewport_resize',width=320,height=640));p.set_viewport_size({'width':320,'height':640});p.wait_for_timeout(700);node('h_101');snap('narrow-panel')
  (O/'environment.json').write_text(json.dumps(dict(browser=b.version,source_commit='2d1ce9627c611d24f510dfc2e05a123fa509d2f6',kind='local_official_example_renderer_with_synthetic_data',readwise_account_access=False,source_implementation_modified=False),ensure_ascii=False,indent=2)+'\n')
 except Exception as e:
  errors.append(dict(error_type=type(e).__name__,error=str(e)));dump();raise
 finally:
  dump();ctx.close();b.close()
