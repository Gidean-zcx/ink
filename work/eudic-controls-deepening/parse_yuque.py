from pathlib import Path
from urllib.parse import unquote
from html.parser import HTMLParser
import json,re,sys
b=Path('work/eudic-controls-deepening')
for name in sys.argv[1:]:
 s=(b/(name+'.html')).read_text();m=re.search(r'window.appData = JSON.parse\(decodeURIComponent\("([^\"]+)"\)\)',s)
 if not m:print(name,'NO embedded appData');continue
 x=json.loads(unquote(m.group(1)));(b/(name+'-appData.json')).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n');print(name,'TOP',list(x))
 doc=x.get('doc',{});print('DOC',list(doc) if isinstance(doc,dict) else type(doc));
 for k in ['title','description','body','body_html','body_draft','content','format','updated_at','published_at','content_updated_at']:
  v=doc.get(k)
  if v is not None:print(k,str(v)[:250])
 def scan(o,path=''):
  if isinstance(o,dict):
   for k,v in o.items():
    if k in ['body','body_html','content','description'] and isinstance(v,str) and len(v)>100:(b/(name+'-'+path.replace('.','_')+'_'+k+'.txt')).write_text(v);print('TEXT',path,k,len(v))
    elif k=='toc':print('TOC',json.dumps(v,ensure_ascii=False)[:9000])
    elif isinstance(v,(dict,list)):scan(v,path+'.'+k)
  elif isinstance(o,list):
   for i,v in enumerate(o):
    if isinstance(v,(dict,list)):scan(v,path+'.'+str(i))
 scan(x)
