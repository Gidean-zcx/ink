from acquire import *
from concurrent.futures import ThreadPoolExecutor
jobs=[('category-2273153','https://help.ground.news/en/categories/2273153-account-questions'),('category-2273217','https://help.ground.news/en/categories/2273217-subscription-questions')]
ids=['245249','245313','722625','6049601','950337','263489','263425','5237505','1750273','6220033','3189505','6049857','866177','3523649']
jobs += [('article-'+i,'https://help.ground.news/en/articles/'+i) for i in ids]
with ThreadPoolExecutor(max_workers=3) as ex:rs=list(ex.map(lambda j:get(*j),jobs))
(W/'sources-selected.json').write_text(json.dumps(rs,ensure_ascii=False,indent=2)+'\n')
for x in rs:print(x['name'],x.get('status',x.get('error')))
for n in ['category-2273153','category-2273217']:
 for a in json.loads((W/(n+'-links.json')).read_text()):
  if '/articles/' in a['url']:print(a['title'],a['url'])
