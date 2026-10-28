from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json,csv,hashlib,base64,requests,subprocess
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/douyin-controls-deepening';OLD=R/'work/feed-interactions'
assert not (W/'baseline.json').exists(),'one-time baseline/acquisition'
(W/'raw').mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
(W/'case-before.md').write_bytes((L/'cases/M-051-douyin.md').read_bytes())
for n,rs in [('coverage',list(csv.DictReader((L/'coverage.csv').open()))),('evidence',list(map(json.loads,(L/'evidence.jsonl').read_text().splitlines())))]:dump(W/f'{n}-before.json',[x for x in rs if x['product_id']=='M-051'])
dump(W/'baseline.json',dict(case_sha256=sha(W/'case-before.md'),requirements_sha256=sha(R/'docs/requirements.md'),checked_on='2026-10-27'))
sources=[];images=[]
for pid in ['15710','3111768381106164','9841769585856405','5411770782085474']:
 p=OLD/f'douyin-article-{pid}.json';d=json.loads(p.read_text())['data'];content=json.loads(base64.b64decode(d['content']).decode());out=W/'raw'/f'article-{pid}-decoded.json';dump(out,content)
 sources.append(dict(id=pid,title=d['title'],source_published_on=d['publish_date'],url=f'https://trust.douyin.com/article/{pid}',raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),decoded_work_path=str(out.relative_to(R)),decoded_sha256=sha(out),source_captured_on='2026-10-26',checked_on='2026-10-27',access_mode='prior_local_public_API_response_reread_not_refetched'))
 for zone in content.get('_deltas',{}).values():
  for i,op in enumerate(zone.get('ops',[])):
   a=op.get('attributes',{})
   if a.get('image')=='true' and a.get('src'):images.append(dict(article_id=pid,op_index=i,url=a['src']))
dump(W/'sources.json',sources)
def get(x):
 d=dict(x);r=requests.get(x['url'],timeout=40);p=W/'raw'/f"image-{x['article_id']}-{x['op_index']}.image";p.write_bytes(r.content);d.update(status=r.status_code,content_type=r.headers.get('Content-Type'),raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),bytes=len(r.content),checked_on='2026-10-27')
 if r.status_code==200 and r.headers.get('Content-Type','').startswith('image/'):
  q=subprocess.run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,nb_frames','-of','json',str(p)],capture_output=True,text=True);d.update(probe_exit=q.returncode,probe=json.loads(q.stdout))
 return d
with ThreadPoolExecutor(max_workers=3) as pool:out=list(pool.map(get,images))
dump(W/'image-captures.json',out);print(json.dumps(out,ensure_ascii=False))
