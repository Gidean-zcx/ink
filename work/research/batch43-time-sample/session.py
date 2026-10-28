import json
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlsplit
from playwright.sync_api import sync_playwright

out = Path('/home/workspace/reader/work/research/batch43-time-sample/browser')
out.mkdir(parents=True, exist_ok=True)
assert not (out / 'session.json').exists(), 'Do not restart an existing capture'
def now(): return datetime.now(timezone.utc).isoformat()
def clean_url(url):
    p = urlsplit(url)
    return p.scheme + '://' + p.netloc + p.path
def save(name, value):
    (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
ops, states, network, errors = [], [], [], []
pw = sync_playwright().start()
profile=out.parent/'profile'
ctx = pw.chromium.launch_persistent_context(str(profile), headless=True, viewport={'width':1280,'height':950}, locale='en-US')
page = ctx.pages[0]
browser = ctx.browser
page.set_default_timeout(6000)
page.set_default_navigation_timeout(30000)
session = dict(started=now(), browser=browser.version, viewport={'width':1280,'height':950}, method='automated_public_browser', logged_in=False, initial_url='https://www.timeforkids.com/g34/service-stars-packed-with-care-g3/?rl=en-650', profile=str(profile))
save('session.json', session)
def response(r):
    if r.request.resource_type in ('document','xhr','fetch'):
        network.append(dict(at=now(), url=clean_url(r.url), status=r.status, resource_type=r.request.resource_type, method=r.request.method))
        save('network-statuses.json', network)
page.on('response', response)
page.on('requestfailed', lambda r: errors.append(dict(at=now(), kind='requestfailed', url=clean_url(r.url), failure=r.failure)))
page.on('pageerror', lambda e: errors.append(dict(at=now(), kind='pageerror', message=str(e))))
def snap(name):
    assert not (out / (name+'.json')).exists(), 'Duplicate capture name'
    page.screenshot(path=str(out/(name+'.png')), timeout=20000)
    data=dict(name=name,at=now(),url=page.url,title=page.title(),text=page.locator('body').inner_text(),controls=page.locator('button,a,input,select,[role="button"]').evaluate_all('(es)=>es.filter(e=>e.getClientRects().length&&getComputedStyle(e).visibility!=="hidden").map(e=>({tag:e.tagName,text:e.innerText,aria:e.getAttribute("aria-label"),test:e.getAttribute("data-test"),type:e.type,disabled:e.disabled}))'))
    (out/(name+'.html')).write_text(page.content())
    save(name+'.json',data); states.append(data); save('observed-states.json',states)
    print(json.dumps({k:v for k,v in data.items() if k!='controls'},ensure_ascii=False),flush=True);print('CONTROLS '+json.dumps(data['controls'],ensure_ascii=False),flush=True)
def go(url):
    r=page.goto(url,wait_until='domcontentloaded')
    print('DOCUMENT_HTTP',r.status if r else None,flush=True)
    if r and r.status in (401,403,412,429): raise RuntimeError('ACCESS_LIMIT_STOP')
    page.wait_for_timeout(1500)
try:
    print('READY',browser.version,flush=True)
    while True:
        line=input()
        if line=='quit': break
        action=dict(command=line,started=now());ops.append(action)
        try:
            exec(line)
            action['result']='ok'
        except Exception as e:
            action['result']='error';action['error']=repr(e)
            print(repr(e),flush=True)
        action['ended']=now()
        save('trace-actions.json',ops);save('browser-errors.json',errors)
finally:
    session['ended']=now();save('session.json',session)
    save('network-statuses.json',network);save('browser-errors.json',errors)
    ctx.close();pw.stop()
