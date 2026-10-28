from pathlib import Path
import requests,json,hashlib,concurrent.futures
from html.parser import HTMLParser
root=Path('/home/workspace/reader');out=root/'work/huawei-controls-deepening'
class Text(HTMLParser):
 def __init__(self):super().__init__();self.parts=[];self.skip=0
 def handle_starttag(self,t,a):
  if t in ['script','style']:self.skip+=1
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip and d.strip():self.parts.append(d.strip())
def extract(raw):
 s=raw.decode('utf-8','replace'); start=s.find('<div id="tkb-content">'); end=s.find('<div id="tkb-attachements"',start)
 if start!=-1:s=s[start:end if end!=-1 else None]
 p=Text();p.feed(s);return '\n'.join(p.parts)+'\n'
urls={'support-cn.html':'https://consumer.huawei.com/cn/support/tablets/matepad-paper/','support-en.html':'https://consumer.huawei.com/en/support/tablets/matepad-paper/'}
for n in ['15931676','15879275','16009712','15898848','16086936']:
 urls['help-'+n+'.html']='https://consumer.huawei.com/cn/support/content/zh-cn'+n+'/'
def fetch(it):
 n,u=it;d={'file':n,'url':u,'checked_on':'2026-10-27'}
 try:
  r=requests.get(u,timeout=(5,18));d.update(status=r.status_code,final_url=r.url,content_type=r.headers.get('content-type'))
  if r.status_code==200:
   (out/n).write_bytes(r.content);(out/n.replace('.html','.txt')).write_text(extract(r.content));d.update(raw_work_path=str((out/n).relative_to(root)),raw_sha256=hashlib.sha256(r.content).hexdigest(),bytes=len(r.content),article_body='id="tkb-content"' in r.text)
  else:d['stopped_without_retry']=True
 except Exception as e:d['error']=str(e)
 return d
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as p: res=list(p.map(fetch,urls.items()))
(out/'fetch-log.json').write_text(json.dumps(res,ensure_ascii=False,indent=2)+'\n')
for r in res:print(r['file'],r.get('status',r.get('error')),r.get('article_body'),r.get('final_url'))
old=[]
for p in (root/'work/research/extension-manuals').glob('huawei-*.html'):
 txt=extract(p.read_bytes());(out/(p.stem+'-reused.txt')).write_text(txt);old.append(dict(raw_work_path=str(p.relative_to(root)),raw_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),original_acquisition_date='2026-10-26',checked_on='2026-10-27',network_refetched=False))
(out/'old-local-captures.json').write_text(json.dumps(old,indent=2)+'\n')
(root/'work/huawei-controls-deepening/case-before.md').write_bytes((root/'docs/research/interaction-library/cases/M-061-huawei-paper.md').read_bytes())
oldb={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'docs/research/interaction-library/batches').glob('extension-manuals-*') if p.is_file()}
(out/'old-batch-hashes.json').write_text(json.dumps(oldb,indent=2)+'\n')
