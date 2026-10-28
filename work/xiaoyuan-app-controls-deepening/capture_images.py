from pathlib import Path
import json,urllib.request,urllib.error,hashlib,concurrent.futures,subprocess
R=Path('/home/workspace/reader');W=R/'work/xiaoyuan-app-controls-deepening';blocks=json.loads((W/'historical-blocks.json').read_text())
assert not (W/'image-captures.json').exists(),'one-time capture'
selections={5:[0,1,4,5,6,7,8],6:[0],7:[0],8:[0],9:[0],12:[3,4,5,7,9],13:[0,1,2,3,4],14:[0,1,2],15:[0],16:[0],19:[0,1,2,3,4,5],20:[0],21:[0,2]}
items=[(b,i,blocks[b]['images'][i]['src']) for b,idxs in selections.items() for i in idxs]
def capture(v):
 b,i,url=v;p=W/f'raw/practice-{b:02}-{i:02}.png';d=dict(block=b,index=i,heading=blocks[b]['heading'],url=url,checked_on='2026-10-27')
 try:
  with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=25) as r:data=r.read();d.update(status=r.status,content_type=r.headers.get('Content-Type'))
 except urllib.error.HTTPError as e:data=e.read();d.update(status=e.code,stopped=True)
 except Exception as e:d.update(status=None,error=repr(e),stopped=True);return d
 p.write_bytes(data);d.update(raw_work_path=str(p.relative_to(R)),raw_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
 if d['status']==200:
  q=subprocess.run(['ffprobe','-v','error','-show_entries','stream=width,height','-of','json',str(p)],capture_output=True,text=True);d.update(probe_exit=q.returncode,probe=json.loads(q.stdout) if q.returncode==0 else q.stderr)
 return d
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:results=list(pool.map(capture,items))
(W/'image-captures.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print(json.dumps([dict(block=x['block'],index=x['index'],status=x['status'],bytes=x.get('bytes'),dimensions=x.get('probe')) for x in results]))
