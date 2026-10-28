import json,concurrent.futures
from urllib.parse import urlparse,urljoin
from fetch import work,fetch
links=json.loads((work/'01-help.links.json').read_text());cats=[];seen=set()
for a in links:
 url=urljoin('https://www.inoreader.com',a['href']);p=urlparse(url)
 if p.netloc=='www.inoreader.com' and len(p.path.strip('/').split('/'))==3 and p.path.startswith('/help/') and url not in seen:
  seen.add(url);cats.append(dict(title=a['text'],url=url,key='cat-'+p.path.split('/')[-2]+'-'+p.path.split('/')[-1]))
assert len(cats)==58,len(cats)
# Existing cache prevents repeated GETs; stop scheduling subsequent chunks after a restricted response.
results=[]
for start in range(0,len(cats),4):
 group=cats[start:start+4]
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:chunk=list(ex.map(lambda c:fetch(c['key'],c['url']),group))
 results+=chunk
 print(json.dumps(dict(processed=len(results),statuses=[x.get('status') for x in chunk])),flush=True)
 if any(x.get('status') in (401,403,429) for x in chunk):break
(work/'help-categories.json').write_text(json.dumps(cats,ensure_ascii=False,indent=2)+'\n')
(work/'category-fetch.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
articles=[]
for c in cats:
 p=work/(c['key']+'.links.json')
 if not p.exists():continue
 for a in json.loads(p.read_text()):
  url=urljoin(c['url'],a['href'])
  if url.startswith(c['url']+'/'):articles.append(dict(category=c['title'],category_url=c['url'],title=a['text'],url=url))
assert len({x['url'] for x in articles})==len(articles)
(work/'help-articles.json').write_text(json.dumps(articles,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(categories=len(cats),articles=len(articles))))
