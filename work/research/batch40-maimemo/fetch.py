from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from urllib.parse import urljoin
import hashlib,json,requests
from html.parser import HTMLParser

class Body(HTMLParser):
 def __init__(self):
  super().__init__();self.stack=[];self.parts=[];self.links=[];self.images=[];self.active_a=None;self.title=[];self.found=False
 def handle_starttag(self,tag,attrs):
  a=dict(attrs);parent=self.stack[-1] if self.stack else ('',False,False)
  active=parent[1] or 'sl-markdown-content' in a.get('class','').split();skip=parent[2] or tag in ('style','script')
  if active:self.found=True
  if active and tag=='img':self.images.append({'src':a.get('src',''),'alt':a.get('alt','')})
  if active and tag=='a' and a.get('href'):self.active_a={'url':a['href'],'parts':[]}
  if tag not in ('area','base','br','col','embed','hr','img','input','link','meta','param','source','track','wbr'):self.stack.append((tag,active,skip))
 def handle_endtag(self,tag):
  if tag=='a' and self.active_a:
   self.links.append({'text':' '.join(self.active_a['parts']),'url':self.active_a['url']});self.active_a=None
  for i in range(len(self.stack)-1,-1,-1):
   if self.stack[i][0]==tag:del self.stack[i:];break
 def handle_data(self,s):
  if self.stack and self.stack[-1][1] and not self.stack[-1][2] and s.strip():
   self.parts.append(s.strip())
   if self.active_a:self.active_a['parts'].append(s.strip())
  if any(x[0]=='h1' for x in self.stack) and s.strip():self.title.append(s.strip())

out=Path('/home/workspace/reader/work/research/batch40-maimemo')
items=[('controller','OxvKwPNcTi8PR4k6Xzyc8Ad5nMd'),('harmony-features','memo-get-started-harmonyos'),('reminders','SAWgwCEyZimLSRkZv98cqglEnUg'),('rating-guide','RPC9w0jZFi4K4Qk5BlrcgBcNnWg'),('web-study','memo-web_study'),('android-log','product-log-memo-android'),('web-log','product-log-memo-web'),('open-terms','open-terms')]
def get(item):
 name,path=item;url='https://memodocs.maimemo.com/docs/'+path
 row={'name':name,'url':url,'checked_on':'2026-10-27','method':'one anonymous requests GET; timeout 25s; max2 parallel'}
 try:
  r=requests.get(url,timeout=25);row.update(status=r.status_code,final_url=r.url)
  (out/(name+'.headers.json')).write_text(json.dumps(dict(r.headers),ensure_ascii=False,indent=2)+'\n')
  raw=out/(name+'.html');raw.write_bytes(r.content);row.update(raw_path=str(raw),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content))
  if r.status_code in (401,403,412,429) or r.headers.get('cf-mitigated')=='challenge':row['body_status']='blocked_stopped';return row
  if r.status_code!=200:row['body_status']='non200_stopped';return row
  body=Body();body.feed(r.content.decode('utf-8','replace'))
  row['body_selector']='.sl-markdown-content';row['title']=' '.join(body.title)
  if not body.found:row['body_status']='body_selector_missing';return row
  text='\n'.join(body.parts);dest=out/(name+'.txt');dest.write_text(text+'\n')
  row.update(body_status='extracted_not_yet_read',text_path=str(dest),text_sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),chars=len(text))
  row['links']=[{'text':a['text'],'url':urljoin(r.url,a['url'])} for a in body.links]
  row['images']=[{'src':urljoin(r.url,x['src']),'alt':x['alt']} for x in body.images]
 except Exception as e:row['error']=type(e).__name__+': '+str(e);row['body_status']='error_stopped'
 return row
rows=list(ThreadPoolExecutor(max_workers=2).map(get,items))
(out/'fetch-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
print(json.dumps([{k:v for k,v in r.items() if k not in ('images','links')} for r in rows],ensure_ascii=False,indent=2))
