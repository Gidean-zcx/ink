old_capture=capture
def capture(label):
    old_capture(label)
    frames=[]
    for i,f in enumerate(page.frames[1:],1):
        row={'index':i,'url':f.url}
        try:
            row['text_prefix']=f.locator('body').inner_text(timeout=3000)[:250]
            row['controls']=f.locator('button,a,input,select,audio,video,[role=button]').evaluate_all('els=>els.map(e=>({tag:e.tagName,visible:!!e.getClientRects().length,text:(e.innerText||"").slice(0,60),aria:e.getAttribute("aria-label"),title:e.title,id:e.id,type:e.type,value:e.value,role:e.getAttribute("role"),href:e.tagName==="A"?e.getAttribute("href"):null,paused:e.paused,currentTime:e.currentTime,duration:Number.isFinite(e.duration)?e.duration:null,readyState:e.readyState}))')
            row['time_labels']=f.locator('[id*=time],[class*=time],[id*=Time],[class*=Time]').evaluate_all('els=>els.filter(e=>e.getClientRects().length).map(e=>({id:e.id,cls:e.className,text:(e.innerText||"").slice(0,100)}))')
        except Exception as e:row['error']=str(e)
        frames.append(row)
    states[-1]['frames']=frames;write_logs()
    print('FRAMES '+json.dumps([{'url':x['url'],'controls':[c for c in x.get('controls',[]) if c.get('visible') or c.get('tag') in ['AUDIO','VIDEO']],'time_labels':x.get('time_labels',[]),'error':x.get('error')} for x in frames],ensure_ascii=False),flush=True)
