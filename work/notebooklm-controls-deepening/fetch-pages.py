import concurrent.futures, hashlib, json, re
from html.parser import HTMLParser
from pathlib import Path
import requests

ROOT=Path(__file__).resolve().parent
SOURCES=[
 ('01','directory','https://support.google.com/notebooklm/?hl=en'),
 ('02','sources','https://support.google.com/gemininotebook/answer/16215270?hl=en'),
 ('03','faq','https://support.google.com/gemininotebook/answer/16269187?hl=en'),
 ('04','create','https://support.google.com/gemininotebook/answer/16206563?hl=en'),
 ('05','cards','https://support.google.com/gemininotebook/answer/16958963?hl=en'),
 ('06','notes','https://support.google.com/gemininotebook/answer/16262519?hl=en'),
 ('07','mindmap','https://support.google.com/gemininotebook/answer/16212283?hl=en'),
 ('08','reports','https://support.google.com/gemininotebook/answer/18323649?hl=en'),
 ('09','infographic','https://support.google.com/gemininotebook/answer/16758265?hl=en'),
 ('10','slides','https://support.google.com/gemininotebook/answer/16757456?hl=en'),
 ('11','video','https://support.google.com/gemininotebook/answer/16454555?hl=en'),
 ('12','mobile','https://support.google.com/gemininotebook/answer/16296687?hl=en&co=GENIE.Platform%3DAndroid'),
 ('13','tables-release','https://workspaceupdates.googleblog.com/2025/12/transform-sources-structured-data-tables-notebooklm.html'),
 ('14','learning-release','https://workspaceupdates.googleblog.com/2026/03/new-ways-to-customize-and-interact-with-your-content-in-NotebookLM.html'),
]
class Text(HTMLParser):
 def __init__(self): super().__init__();self.out=[];self.skip=0
 def handle_starttag(self,tag,attrs):
  if tag in ('script','style'): self.skip+=1
  if tag in ('h1','h2','h3','h4','p','li','div','tr','br'): self.out.append('\n')
 def handle_endtag(self,tag):
  if tag in ('script','style'): self.skip=max(0,self.skip-1)
  if tag in ('h1','h2','h3','h4','p','li','div','tr'): self.out.append('\n')
 def handle_data(self,data):
  if not self.skip:self.out.append(data)
def fetch(item):
 n,slug,url=item;entry={'evidence_id':'E-NBCD-'+n,'slug':slug,'requested_url':url,'checked_on':'2026-10-26'}
 try:
  response=requests.get(url,timeout=(5,15));entry.update(status=response.status_code,final_url=response.url,redirects=[{'status':r.status_code,'url':r.url} for r in response.history])
  if response.status_code!=200:entry['stopped']=True;return entry
  data=response.content; p=ROOT/(n+'-'+slug+'.html');p.write_bytes(data)
  entry.update(raw_work_path=str(p.relative_to(ROOT.parents[1])),raw_sha256=hashlib.sha256(data).hexdigest())
  html=response.text; article=re.search(r'<article\b.*?</article>',html,re.S)
  if article:html=article.group(0)
  elif 'blogspot' in html or 'googleblog.com' in url:
   a=re.search(r'<div class=[\'\"]post-body[^>]*>',html)
   if a:html=html[a.start():html.find('<!--',a.end()) if '<!--' in html[a.end():] else len(html)]
  parser=Text();parser.feed(html)
  content='\n'.join(x.strip() for x in ''.join(parser.out).splitlines() if x.strip())
  (ROOT/(n+'-'+slug+'.txt')).write_text(content+'\n')
  entry['text_characters']=len(content)
 except requests.RequestException as e:entry.update(error=type(e).__name__+': '+str(e),stopped=True)
 return entry
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 results=list(pool.map(fetch,SOURCES))
(ROOT/'fetch-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
for r in results:print(r['evidence_id'],r['slug'],r.get('status',r.get('error')),r.get('text_characters',0))
