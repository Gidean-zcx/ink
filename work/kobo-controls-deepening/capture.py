from pathlib import Path
import requests,json,hashlib
from html.parser import HTMLParser
from urllib.parse import urljoin
W=Path('work/kobo-controls-deepening');(W/'raw').mkdir(exist_ok=True)
class Extract(HTMLParser):
 def __init__(self):super().__init__();self.depth=0;self.body_depth=None;self.text=[];self.links=[];self.images=[];self.headings=[];self.a=None;self.h=None;self.hdepth=None;self.section=''
 def handle_starttag(self,t,attrs):
  a=dict(attrs)
  if t not in ['img','br','input','hr','meta','link','wbr','source','area','base','embed','param','track']:self.depth+=1
  if self.body_depth is None and ('article-body' in a.get('class','') or 'section-content' in a.get('class','')):self.body_depth=self.depth
  if self.body_depth is not None:
   if t=='a':self.a={'title':'','url':a.get('href',''),'section':self.section}
   if t=='img':self.images.append({'src':a.get('src',''),'alt':a.get('alt','')})
   if t in ('h1','h2','h3','h4'):self.h='';self.hdepth=self.depth
 def handle_endtag(self,t):
  if self.body_depth is not None:
   if t=='a' and self.a:self.links.append(self.a);self.a=None
   if t in ('h1','h2','h3','h4') and self.h is not None:self.section=self.h.strip();self.headings.append(self.section);self.h=None
   if t in ('p','li','h1','h2','h3','h4','tr','div'):self.text.append('\n')
   if self.depth==self.body_depth:self.body_depth=None
  if t not in ['img','br','input','hr','meta','link','wbr','source','area','base','embed','param','track']:self.depth-=1
 def handle_data(self,s):
  if self.body_depth is not None:
   if s.strip():self.text.append(s.strip()+' ')
   if self.a is not None:self.a['title']+=s.strip()+' '
   if self.h is not None:self.h+=s.strip()+' '
urls={
 'guide':'https://help.kobo.com/hc/en-us/articles/360019123773-Kobo-eReader-User-Guide',
 'model':'https://help.kobo.com/hc/en-us/articles/17971574287895-Kobo-Libra-Colour',
 'audiobook':'https://help.kobo.com/hc/en-us/articles/4406292712471-Listen-to-audiobooks-on-your-Kobo-eReader',
 'collections':'https://help.kobo.com/hc/en-us/articles/360033887953-Organize-your-eBooks-on-your-Kobo-eReader',
 'toc':'https://help.kobo.com/hc/en-us/articles/360017481254-View-the-table-of-contents-on-your-Kobo-eReader',
 'bookmark':'https://help.kobo.com/hc/en-us/articles/360017481214-Bookmark-a-page-on-your-Kobo-eReader',
 'reading-menu':'https://help.kobo.com/hc/en-us/articles/360020494854-About-the-Kobo-eReader-Reading-Menu',
 'dark-mode':'https://help.kobo.com/hc/en-us/articles/360062231213-About-Dark-mode',
 'comfortlight':'https://help.kobo.com/hc/en-us/articles/360017481174-Adjust-ComfortLight-PRO-on-your-Kobo-eReader',
 'gestures':'https://help.kobo.com/hc/en-us/articles/360017639973-Use-gestures-on-the-touch-screen',
 'bt-troubleshoot':'https://help.kobo.com/hc/en-us/articles/4406284800023-Troubleshoot-Bluetooth-pairing-on-your-Kobo-eReader',
 'reading-index':'https://help.kobo.com/hc/en-us/sections/360002688333-Reading',
 'rainbow':'https://help.kobo.com/hc/en-us/articles/21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content',
 'access-design':'https://help.kobo.com/hc/en-us/articles/32868920931991-Accessibility-Features-Design-Support',
}
caps=[]
for name,url in urls.items():
 r=requests.get(url,timeout=30);c={'name':name,'url':url,'effective_url':r.url,'http_status':r.status_code,'checked_on':'2026-10-27'};print(name,r.status_code)
 p=W/'raw'/f'{name}.html';p.write_bytes(r.content);c.update(raw_work_path=str(p),raw_sha256=hashlib.sha256(r.content).hexdigest())
 if r.status_code==200:
  r.encoding='utf-8';ex=Extract();ex.feed(r.text)
  txt='\n'.join(s.strip() for s in ''.join(ex.text).splitlines() if s.strip());(W/'raw'/f'{name}.txt').write_text(txt+'\n')
  for a in ex.links:a['url']=urljoin(r.url,a['url']);a['title']=a['title'].strip()
  for im in ex.images:im['src']=urljoin(r.url,im['src'])
  (W/'raw'/f'{name}-structure.json').write_text(json.dumps({'links':ex.links,'headings':ex.headings,'images':ex.images},ensure_ascii=False,indent=2))
  c.update(extracted_work_path=str(W/'raw'/f'{name}.txt'),extracted_sha256=hashlib.sha256((txt+'\n').encode()).hexdigest(),text_chars=len(txt),links=len(ex.links))
 caps.append(c)
 # No retries; rejected URL ends this route. Other independent public help URLs remain separate.
(W/'captures.json').write_text(json.dumps(caps,ensure_ascii=False,indent=2)+'\n')
