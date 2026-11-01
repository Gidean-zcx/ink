from pathlib import Path
from urllib.request import urlopen,Request
from urllib.error import HTTPError
from html.parser import HTMLParser
import json,hashlib,re
w=Path('work/fiio-controls-deepening')
urls=[('02-manual-link','https://www.fiio.com/newsinfo/984177.html'),('03-modes','https://www.fiio.com/newsinfo/984182.html'),('04-dac','https://www.fiio.com/newsinfo/984183.html'),('05-bt-receiver','https://www.fiio.com/newsinfo/984184.html'),('06-airplay','https://www.fiio.com/newsinfo/984191.html'),('07-cast','https://www.fiio.com/newsinfo/1063630.html'),('08-card','https://www.fiio.com/newsinfo/1070933.html'),('09-playlist','https://fiiosupport.freshdesk.com/support/solutions/articles/69000869676-how-to-create-external-m3u-playlists-for-fiio-m3k-m5-m6-m7-m9-m11-jm21-m21-using-foobar-how-to-import'),('10-lyrics','https://www.fiio.com/newsinfo/984196.html'),('11-update','https://www.fiio.com/newsinfo/984180.html'),('12-product','https://www.fiio.com/jm21')]
class Text(HTMLParser):
 def __init__(self):super().__init__();self.skip=0;self.data=[]
 def handle_starttag(self,t,a):
  if t in ['script','style']:self.skip+=1
  if t in ['p','li','div','h1','h2','h3','br','tr']:self.data.append('\n')
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip:self.data.append(d)
manifest=[]
p=w/'raw/faq-index.html';manifest.append({'name':'01-index','url':'https://www.fiio.com/jm21_faq','path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'http_status':200,'checked_on':'2026-10-27'})
for name,url in urls:
 try:
  with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=40) as r:b=r.read();status=r.status;final=r.url
  p=w/'raw'/f'{name}.html';p.write_bytes(b);row={'name':name,'url':url,'final_url':final,'path':str(p),'sha256':hashlib.sha256(b).hexdigest(),'http_status':status,'checked_on':'2026-10-27'}
  parser=Text();parser.feed(b.decode('utf-8'));full='\n'.join(' '.join(x.split()) for x in ''.join(parser.data).splitlines() if x.strip())
  if 'newsinfo' in url:
   title=re.search(r'<title>(.*?)</title>',b.decode('utf-8'),re.S)
   article=title.group(1).split('-FIIO')[0].split('-FiiO')[0] if title else ''
   pos=full.find(article)
   if pos>=0:full=full[pos:]
   if 'Views：' in full:full=full.split('Views：')[0]
  (w/'extracted'/f'{name}.txt').write_text(full+'\n')
 except HTTPError as e:
  row={'name':name,'url':url,'http_status':e.code,'error':str(e)};manifest.append(row);print(json.dumps(row));continue
 except Exception as e:
  row={'name':name,'url':url,'error':str(e)};manifest.append(row);print(json.dumps(row));continue
 manifest.append(row);print(json.dumps(row))
(w/'source-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
