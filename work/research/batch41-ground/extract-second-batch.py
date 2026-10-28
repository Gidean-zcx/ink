from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urljoin
import json,re
OUT=Path('work/research/batch41-ground')
class Article(HTMLParser):
 def __init__(self,base):
  super().__init__();self.base=base;self.active=False;self.depth=0;self.skip=0;self.text=[];self.links=[];self.images=[];self.a=None;self.at=[];self.edited_ms=None
 def handle_starttag(self,t,attrs):
  a=dict(attrs); classes=a.get('class','').split()
  if not self.active and t=='div' and 'article-content' in classes:
   self.active=True;self.depth=1;return
  if not self.active:return
  if t=='div':self.depth+=1
  if t in ('script','style','svg','noscript'):self.skip+=1
  if not self.skip and t in ('h1','h2','h3','h4','p','li','br','hr'):self.text.append('\n')
  if not self.skip and t=='a' and a.get('href'):self.a=a['href'];self.at=[]
  if not self.skip and t=='img' and (a.get('src') or a.get('data-src')):self.images.append(urljoin(self.base,a.get('src') or a.get('data-src')))
 def handle_data(self,d):
  if self.active and not self.skip:
   self.text.append(d)
   if self.a is not None:self.at.append(d)
 def handle_endtag(self,t):
  if not self.active:return
  if t in ('script','style','svg','noscript') and self.skip:self.skip-=1
  if t=='a' and self.a is not None:
   self.links.append({'url':urljoin(self.base,self.a),'text':' '.join(''.join(self.at).split())});self.a=None;self.at=[]
  if not self.skip and t in ('h1','h2','h3','h4','p','li','hr'):self.text.append('\n')
  if t=='div':
   self.depth-=1
   if self.depth==0:self.active=False
for ident in ('485057','593857'):
 html=(OUT/f'{ident}.html').read_text(errors='replace');base=f'https://help.ground.news/en/articles/{ident}'
 p=Article(base);p.feed(html)
 lines=[]
 for x in ''.join(p.text).splitlines():
  x=' '.join(x.split())
  if x and (not lines or lines[-1]!=x):lines.append(x)
 links=[];seen=set()
 for x in p.links:
  k=(x['url'],x['text'])
  if k not in seen:seen.add(k);links.append(x)
 images=[];seen=set()
 for x in p.images:
  if x not in seen:seen.add(x);images.append(x)
 (OUT/f'{ident}-body.txt').write_text('\n'.join(lines)+'\n')
 (OUT/f'{ident}-links.json').write_text(json.dumps(links,ensure_ascii=False,indent=2)+'\n')
 (OUT/f'{ident}-images.json').write_text(json.dumps(images,ensure_ascii=False,indent=2)+'\n')
 st=json.loads((OUT/f'{ident}-status.json').read_text())
 m=re.search(r'id="article-edited"[^>]*data-edited="(\d+)"[^>]*>Edited<noscript>\s*([^<]+)',html)
 st.update({'challenge_review':'false_positive: recaptcha appeared only in normal Content-Security-Policy and empty site-key configuration; article content was present','body_extracted':True,'text_lines':len(lines),'article_links':len(links),'article_images':len(images),'source_updated_display':m.group(2).strip() if m else None,'source_updated_epoch_ms':int(m.group(1)) if m else None})
 (OUT/f'{ident}-status.json').write_text(json.dumps(st,ensure_ascii=False,indent=2)+'\n')
 print(ident,len(lines),len(links),len(images),st['source_updated_display'])
