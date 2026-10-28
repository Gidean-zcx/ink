from acquire import *
from concurrent.futures import ThreadPoolExecutor
ls=json.loads((W/'help-index-links.json').read_text());jobs=list(dict.fromkeys(x['url'] for x in ls if '/categories/' in x['url']))
with ThreadPoolExecutor(max_workers=3) as ex:rs=list(ex.map(lambda u:get('category-'+u.split('/')[-1].split('-')[0],u),jobs))
(W/'sources-categories.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
for x in rs:print(x['name'],x.get('status',x.get('error')))
for x in rs:
 if x.get('status')!=200:continue
 for a in json.loads((W/(x['name']+'-links.json')).read_text()):
  if '/articles/' in a['url'] or '/categories/' in a['url'] and a['url'] not in jobs:print(x['name'],a['title'],a['url'])
