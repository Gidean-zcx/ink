from pathlib import Path
import json,re,hashlib
b=Path('work/quizlet-controls-deepening'); (b/'parsed').mkdir(exist_ok=True)
rows=[]
for p in sorted(b.glob('quizlet-response-*.txt')):
 s=p.read_text()
 for chunk in re.split(r'\n?-{80}\n',s):
  m=re.search(r'^(.*?)\((https://help.quizlet.com/[^\s)]+)\)',chunk,re.M)
  if not m:continue
  url=m.group(2);red=re.search(r'Redirected to URL: (https://help.quizlet.com/[^;\s]+)',chunk)
  if red:url=red.group(1)
  identity=re.search(r'/(articles|sections|categories)/(\d+)',url)
  if not identity:continue
  key=identity.group(1)+'-'+identity.group(2)
  text='\n'.join(re.findall(r'^L\d+: ?(.*)$',chunk,re.M))+'\n'
  out=b/'parsed'/(key+'.txt');out.write_text(text)
  rows.append({'key':key,'title':m.group(1).replace(' – Quizlet Help Center','').strip(),'url':url,'raw_work_path':str(p),'raw_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'parsed_text_path':str(out),'parsed_lines':len(text.splitlines()),'raw_capture_kind':'web_tool_response_text; not source HTML; no images observed'})
last={r['key']:r for r in rows};(b/'source-map.json').write_text(json.dumps(last,ensure_ascii=False,indent=2)+'\n')
print('Parsed',len(last),'official pages from 5 saved web-tool responses; no network requests.')
for k in ['sections-360004698771','sections-360005938691','sections-360005877232']:
 p=Path(last[k]['parsed_text_path']);txt=p.read_text();body=txt.split('* * *',1)[1].split('##### About us',1)[0];titles=re.findall(r'^\s*\* 【\d+†([^】]+)】',body,re.M)
 print(k,len(titles));(b/(k+'-titles.json')).write_text(json.dumps(titles,ensure_ascii=False,indent=2)+'\n')
