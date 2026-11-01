import urllib.request,json,hashlib,concurrent.futures
from pathlib import Path
from html.parser import HTMLParser
P=Path('work/khan-controls-deepening')
items={
'01-create':'https://khankids.zendesk.com/hc/en-us/articles/21188738836763-How-to-use-creative-tools-inside-the-Khan-Kids-app',
'02-export':'https://khankids.zendesk.com/hc/en-us/articles/6477686235419-How-do-I-save-artwork-from-the-Gallery',
'03-library':'https://khankids.zendesk.com/hc/en-us/articles/4403657703821-Find-books-and-lessons-in-the-Khan-Kids-Library',
'04-progress':'https://khankids.zendesk.com/hc/en-us/articles/360041615571-How-does-the-learning-level-adjust-and-how-do-I-view-my-child-s-progress',
'05-hide-faq':'https://khankids.zendesk.com/hc/en-us/articles/13849896234395-FAQ-How-do-I-turn-off-videos-and-coloring',
'06-manage-access':'https://khankids.zendesk.com/hc/en-us/articles/17031195532059--NEW-Manage-student-access-to-areas-of-the-app',
'07-add-user':'https://khankids.zendesk.com/hc/en-us/articles/360006538192-How-do-I-add-a-new-user',
'08-sign-out':'https://khankids.zendesk.com/hc/en-us/articles/360043669551-FAQ-How-do-I-sign-out',
'09-password':'https://khankids.zendesk.com/hc/en-us/articles/360007054191-How-do-I-set-up-reset-or-manage-a-password',
'10-parent':'https://khankids.zendesk.com/hc/en-us/articles/360006764812-Parent-Guide-Using-Khan-Academy-Kids-at-Home',
'11-design':'https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/',
'12-sections':'https://khankids.zendesk.com/api/v2/help_center/en-us/sections.json?per_page=100'
}
class Article(HTMLParser):
 def __init__(self):super().__init__();self.on=False;self.level=0;self.data=[];self.images=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='div' and 'article-body' in d.get('class',''):self.on=True;self.level=1
  elif self.on and t=='div':self.level+=1
  if self.on and t=='img':self.images.append(d)
  if self.on and t in ['p','li','br','h1','h2','h3']:self.data.append('\n')
 def handle_endtag(self,t):
  if self.on and t=='div':
   self.level-=1
   if self.level==0:self.on=False
 def handle_data(self,d):
  if self.on:self.data.append(d)
def fetch(it):
 k,u=it
 try:
  with urllib.request.urlopen(u,timeout=35) as r:b=r.read();s=r.status;final=r.url
  suffix='json' if k=='12-sections' else 'html';file=P/(k+'.'+suffix);file.write_bytes(b)
  a=Article();a.feed(b.decode())
  if a.data:(P/(k+'.txt')).write_text(''.join(a.data));(P/(k+'-images.json')).write_text(json.dumps(a.images,ensure_ascii=False,indent=2))
  return {'key':k,'url':u,'final_url':final,'status':s,'path':str(file),'sha256':hashlib.sha256(b).hexdigest(),'article_chars':len(''.join(a.data)),'images':len(a.images)}
 except Exception as e:return {'key':k,'url':u,'error':str(e)}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:results=list(ex.map(fetch,items.items()))
(P/'fetch-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
for r in results: print(r['key'],r.get('status',r.get('error')),r.get('article_chars'),r.get('images'))
if (P/'12-sections.json').exists():
 d=json.loads((P/'12-sections.json').read_text());print('SECTIONS',[(s['id'],s['name']) for s in d.get('sections',[])]);print('NEXT',d.get('next_page'))
