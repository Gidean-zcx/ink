import concurrent.futures,html,json,pathlib,re,requests
out=pathlib.Path('/home/workspace/reader/work/eink-systems-research')
names=['Release-notes-overview','Navigating-on-your-reMarkable','Convert-handwritten-notes-into-text','Templates','Tags','Handwriting-search','importing-and-exporting-files','Using-reMarkable-without-a-subscription','Create-a-link','Toolbar','Help-with-sync-and-the-reMarkable-cloud','Pair-your-reMarkable-with-the-cloud']
def fetch(name):
 u='https://support.remarkable.com/articles/Knowledge/'+name
 try:
  r=requests.get(u,timeout=25)
  s=re.sub(r'<(script|style)\b[^>]*>.*?</\1>','',r.text,flags=re.S|re.I)
  s=' '.join(html.unescape(re.sub('<[^>]+>',' ',s)).split())
  (out/('rm-'+name+'.txt')).write_text(s)
  date=re.search(r'Last Published Date\s+([^ ]+),',s)
  return dict(name=name,url=u,status=r.status_code,article_body='Article Body' in s,chars=len(s),date_raw=date.group(1) if date else None)
 except Exception as e:return dict(name=name,url=u,error=str(e))
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool: results=list(pool.map(fetch,names))
(out/'remarkable-fetch.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
for r in results:print(json.dumps(r,ensure_ascii=False))
