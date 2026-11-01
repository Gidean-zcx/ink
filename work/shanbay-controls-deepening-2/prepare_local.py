from pathlib import Path
import json,shutil,re,hashlib,html
w=Path('work/shanbay-controls-deepening-2');v=Path('work/vocab-research');rows=[]
for p in sorted(v.glob('sby-col-*.json')):
 shutil.copyfile(p,w/p.name);d=json.loads(p.read_text())
 for x in d['objects']:
  rows.append({'id':x['id'],'column_id':x['column_id'],'column_name':x['column']['name'],'title':x['title'],'assign_date':x.get('assign_date'),'raw_work_path':str(w/p.name),'raw_sha256':hashlib.sha256((w/p.name).read_bytes()).hexdigest(),'body_available':bool(x.get('content_html'))})
  if x['id'] in ['bchgxx','zguwz','coeml','orqdq','jdubu','qexpe','vkign']:
   tx=html.unescape(re.sub('<[^>]+>','\n',x['content_html']));tx='\n'.join(a.strip() for a in tx.splitlines() if a.strip());(w/('article-'+x['id']+'.txt')).write_text(tx)
for p in [v/'shanbayhelp.html',v/'shanbayhelp.js']:shutil.copyfile(p,w/p.name)
(w/'saved-column-index.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2))
s=(v/'shanbayhelp.js').read_text();i=s.index('name:"简介"');j=s.index('];function',i);heads=re.findall(r'name:"([^"]+)",id:"([^"]+)"',s[i:j]);(w/'help-columns.json').write_text(json.dumps(heads,ensure_ascii=False,indent=2))
print('columns',heads,'articles',len(rows))
for a in ['bchgxx','zguwz','coeml','orqdq','jdubu']:print('\nARTICLE',a,'\n'+(w/('article-'+a+'.txt')).read_text())
