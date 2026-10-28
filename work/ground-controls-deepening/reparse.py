from acquire import *
for mf in ['sources-initial.json','sources-categories.json','sources-selected.json']:
 for row in json.loads((W/mf).read_text()):
  if row.get('status')!=200:continue
  z=P();z.feed(Path(row['raw_work_path']).read_bytes().decode('utf-8',errors='replace'));name=row['name']
  (W/(name+'.txt')).write_text('\n'.join(t.strip() for t in ''.join(z.text).splitlines() if t.strip())+'\n')
  for suffix,items in [('links',[dict(a,url=urljoin(row['final_url'],a['href'])) for a in z.links]),('headings',z.heads),('images',z.images)]: (W/f'{name}-{suffix}.json').write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n')
print('reparsed existing UTF-8 HTML, no network request')
