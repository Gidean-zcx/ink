import requests,re,json,pathlib
from html.parser import HTMLParser
class Text(HTMLParser):
 def __init__(self): super().__init__();self.parts=[];self.skip=0
 def handle_starttag(self,t,a):
  if t in ['script','style']:self.skip+=1
  if t in ['p','h1','h2','h3','h4','li','br']:self.parts.append('\n')
 def handle_endtag(self,t):
  if t in ['script','style']:self.skip=max(0,self.skip-1)
 def handle_data(self,d):
  if not self.skip:self.parts.append(d.strip()+' ')
urls={'viw385':'https://viwoods.com/pages/update-version-3-8-5','viw315':'https://viwoods.com/pages/system-update-3-15','viwfaq':'https://viwoods.com/pages/frequently-asked-questions-about-viwoods','viwtrouble':'https://viwoods.com/pages/troubleshooting','viwmanuals':'https://viwoods.com/pages/user-manuals','hanv399':'https://www.hanwang.com.cn/index.php?a=show&c=index&catid=66&id=399&m=content','hanvlist':'https://www.hanwang.com.cn/index.php?a=lists&c=index&catid=136&m=content','iflyexport':'https://zskzs.iflytek.com/myfiles/d0a8ad2d-836d-4a8f-a587-18c0077db6c1/gzhqd.html'}
base=pathlib.Path('work/ai-notebooks-research');report=[]
for n,u in urls.items():
 try:
  r=requests.get(u,timeout=30);p=Text();p.feed(r.text)
  s='\n'.join(x.strip() for x in ''.join(p.parts).splitlines() if x.strip());(base/(n+'.txt')).write_text(s);(base/(n+'.html')).write_text(r.text)
  report.append({'name':n,'url':u,'http_status':r.status_code,'chars':len(s)})
 except Exception as e:report.append({'name':n,'error':str(e)})
print(json.dumps(report,ensure_ascii=False));(base/'fetch-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
