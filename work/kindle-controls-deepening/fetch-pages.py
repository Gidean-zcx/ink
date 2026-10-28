import concurrent.futures,hashlib,json,re
from html.parser import HTMLParser
from pathlib import Path
import requests
ROOT=Path(__file__).resolve().parent
N='https://digprjsurvey.amazon.com/csad/help/node/'
SOURCES=[
 ('01','guide-index',N+'G7N7RPHV2SW8CKBW'),
 ('02','scribe-2024','https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/html/kug.html'),
 ('03','delete-notebooks',N+'TyR30Qzqu0hGzFbFVx?theme=light'),
 ('04','fix-notebooks',N+'TyNFaxgPlqhHz1d4B4?theme=light'),
 ('05','create-notebooks',N+'TTkdXCk1A3rZ68MXt7'),
 ('06','move-notebooks',N+'TlI0Gva20JDq8Yfc4P'),
 ('07','multiple-pages',N+'Tq6OTco5USdWqZ91ZH'),
 ('08','writing',N+'T0MT2p2D8t046zUxih'),
 ('09','compatibility',N+'TNtSm13k3txFI4EvDV'),
 ('10','share',N+'TJE2UYmdw0ppUuR3Rs'),
 ('11','sync',N+'GDCAMDFMC2LZP6BR'),
 ('12','cloud-drive',N+'TIh9JOMGAKr7bY4zqu'),
 ('13','quick-notes',N+'T54Isf2DDrox4TckQR'),
]
class Extract(HTMLParser):
 def __init__(self):super().__init__();self.out=[];self.skip=0;self.links=[];self.a=None;self.at=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t in ('script','style'):self.skip+=1
  if t in ('h1','h2','h3','h4','p','li','tr','br','div'):self.out.append('\n')
  if t=='a':self.a=d.get('href');self.at=[]
 def handle_endtag(self,t):
  if t in ('script','style'):self.skip=max(0,self.skip-1)
  if t in ('h1','h2','h3','h4','p','li','tr','div'):self.out.append('\n')
  if t=='a' and self.a:self.links.append({'href':self.a,'text':' '.join(''.join(self.at).split())});self.a=None
 def handle_data(self,d):
  if not self.skip:self.out.append(d)
  if self.a:self.at.append(d)
def fetch(s):
 n,slug,url=s;r={'evidence_id':'E-KDCD-'+n,'slug':slug,'requested_url':url,'checked_on':'2026-10-27'}
 try:
  v=requests.get(url,timeout=(5,18));r.update(status=v.status_code,final_url=v.url,headers={k:v.headers.get(k) for k in ['Last-Modified','ETag','Content-Type']})
  if v.status_code!=200:r['stopped']=True;return r
  p=ROOT/(n+'-'+slug+'.html');p.write_bytes(v.content)
  r.update(raw_work_path=str(p.relative_to(ROOT.parents[1])),raw_sha256=hashlib.sha256(v.content).hexdigest())
  parser=Extract();parser.feed(v.content.decode('utf-8'))
  content='\n'.join(x.strip() for x in ''.join(parser.out).splitlines() if x.strip())
  (ROOT/(n+'-'+slug+'.txt')).write_text(content+'\n')
  (ROOT/(n+'-'+slug+'-links.json')).write_text(json.dumps(parser.links,ensure_ascii=False,indent=2)+'\n')
  r['text_characters']=len(content)
 except requests.RequestException as e:r.update(error=type(e).__name__+': '+str(e),stopped=True)
 return r
if __name__=='__main__':
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(fetch,SOURCES))
 (ROOT/'fetch-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
 for r in results:print(r['evidence_id'],r['slug'],r.get('status',r.get('error')),r.get('text_characters',0))
