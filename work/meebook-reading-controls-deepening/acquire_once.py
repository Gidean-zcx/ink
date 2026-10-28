from pathlib import Path
from html.parser import HTMLParser
from concurrent.futures import ThreadPoolExecutor
import requests,json,hashlib
W=Path('work/meebook-reading-controls-deepening');assert not (W/'acquisition.json').exists()
sources=[('helpix','https://www.helpix.ru/opinion/202409/60517-meebook_m7.html'),('issue16100','https://github.com/koreader/koreader/issues/16100'),('androidkeys','https://github.com/koreader/koreader/wiki/Android-tips-and-tricks'),('keymapping','https://github.com/koreader/koreader/wiki/Keymapping'),('reddit','https://www.reddit.com/r/eink/comments/1825l9d/meebook_m7_review_my_optimizations/')]
class Text(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.parts=[];self.images=[];self.links=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ['script','style']:self.skip+=1
  if t=='img':self.images.append(d)
  if t=='a' and d.get('href'):self.links.append(d['href'])
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.parts.append(d.strip())
def get(pair):
 key,url=pair;row=dict(key=key,url=url,checked_on='2026-10-27',method='ordinary_anonymous_GET')
 try:
  r=requests.get(url,timeout=30);p=W/'raw'/f'{key}.html';p.write_bytes(r.content);row.update(status=r.status_code,final_url=r.url,path=str(p),sha256=hashlib.sha256(r.content).hexdigest())
  if r.status_code in [401,403,412,429]:row['stopped']='refusal; no retry'
  elif r.status_code==200:
   parser=Text();parser.feed(r.content.decode(r.apparent_encoding or 'utf-8',errors='replace'));(W/'raw'/f'{key}.txt').write_text('\n'.join(parser.parts));(W/'raw'/f'{key}-media.json').write_text(json.dumps(dict(images=parser.images,links=parser.links),ensure_ascii=False,indent=2)+'\n');row['text_chars']=sum(map(len,parser.parts))
 except Exception as e:row['error']=type(e).__name__+': '+str(e)
 return row
with ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(get,sources))
(W/'acquisition.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n');print(json.dumps(rows,ensure_ascii=False,indent=2))
