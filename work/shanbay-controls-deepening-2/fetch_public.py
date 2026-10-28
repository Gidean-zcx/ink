from pathlib import Path
import requests,hashlib,json
from html.parser import HTMLParser
import re,html
class TextParser(HTMLParser):
 def __init__(self):super().__init__();self.parts=[];self.links=[];self.href=None;self.linktext=[];self.skip=0
 def handle_starttag(self,tag,attrs):
  if tag in ["script","style","noscript"]:self.skip+=1
  if tag=="a":self.href=dict(attrs).get("href");self.linktext=[]
 def handle_endtag(self,tag):
  if tag in ["script","style","noscript"]:self.skip=max(0,self.skip-1)
  if tag=="a" and self.href:self.links.append({"text":" ".join(self.linktext),"url":self.href});self.href=None
 def handle_data(self,data):
  if not self.skip and data.strip():self.parts.append(data.strip())
  if self.href:self.linktext.append(data.strip())
from concurrent.futures import ThreadPoolExecutor
W=Path('work/shanbay-controls-deepening-2')
items=[('translate-support','https://web.shanbay.com/bay-translate/support'),('translate-home','https://web.shanbay.com/bay-translate/'),('translate-privacy','https://web.shanbay.com/bay-translate/privacy'),('company','https://web.shanbay.com/company/jieshao')]
def get(x):
 name,url=x;out={'name':name,'url':url}
 try:
  r=requests.get(url,timeout=(5,18));out.update(http_status=r.status_code,final_url=r.url,content_type=r.headers.get('content-type'))
  p=W/(name+'.html');p.write_bytes(r.content);out.update(raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest())
  if r.status_code in [401,403,429]:out['result']='STOP no retry';return out
  if r.status_code!=200:out['result']='non200 no retry';return out
  so=TextParser();so.feed(r.content.decode(r.encoding or 'utf-8',errors='replace'))
  text='\n'.join(so.parts);(W/(name+'.txt')).write_text(text)
  out.update(result='public_body' if len(text)>300 else 'shell_only',text_length=len(text))
  (W/(name+'-links.json')).write_text(json.dumps(so.links,ensure_ascii=False,indent=2))
 except Exception as e:out.update(result='error no retry',error=str(e))
 return out
with ThreadPoolExecutor(max_workers=3) as ex:res=list(ex.map(get,items))
(W/'http-results.json').write_text(json.dumps(res,ensure_ascii=False,indent=2));print(json.dumps(res,ensure_ascii=False,indent=2))
