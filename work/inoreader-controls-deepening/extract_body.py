import re,json
from fetch import work,Parser
for p in sorted(work.glob('[0-9]*.html')):
 s=p.read_text();m=re.search(r'<h1\b',s,re.I)
 if not m:continue
 a=Parser();a.feed(s[m.start():]);text='\n'.join(x.strip() for x in ''.join(a.parts).splitlines() if x.strip())
 for footer in ["With Inoreader, content comes to you",'In case you missed it']:
  text=text.split(footer)[0]
 (work/(p.stem+'.article.txt')).write_text(text.strip()+'\n')
# Include the successful retry, without overwriting the original 502 record.
cats=json.loads((work/'help-categories.json').read_text());articles=[]
from urllib.parse import urljoin
for c in cats:
 key=c['key']+'-retry' if c['key']=='cat-basic-dashboards' else c['key'];c['source_key']=key
 for a in json.loads((work/(key+'.links.json')).read_text()):
  url=urljoin(c['url'],a['href'])
  if url.startswith(c['url']+'/'):articles.append(dict(category=c['title'],category_url=c['url'],title=a['text'],url=url))
assert len(articles)==len({x['url'] for x in articles})
(work/'help-categories-effective.json').write_text(json.dumps(cats,ensure_ascii=False,indent=2)+'\n')
(work/'help-articles-effective.json').write_text(json.dumps(articles,ensure_ascii=False,indent=2)+'\n')
print(dict(categories=len(cats),articles=len(articles)))
