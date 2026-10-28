def snap(name):
    base_snap(name)
    data = dict(
        name=name, at=now(), url=f.url, text=f.locator('body').inner_text(),
        controls=f.locator('button,a,input,select,[role=button]').evaluate_all('''(es)=>es.map(e=>({tag:e.tagName,text:e.innerText,aria:e.getAttribute('aria-label'),title:e.title,id:e.id,visible:!!e.getClientRects().length}))'''),
        media=f.locator('audio,video').evaluate_all('''(es)=>es.map(e=>({tag:e.tagName,paused:e.paused,time:e.currentTime,duration:Number.isFinite(e.duration)?e.duration:null,ended:e.ended,error:e.error?.message}))'''))
    (out/(name+'-frame.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2))
    (out/(name+'-frame.html')).write_text(f.content())
    page.locator('iframe[title="Community Helpers Interactive"]').screenshot(path=str(out/(name+'-frame.png')))
    print(data['text'][:1800],flush=True)
