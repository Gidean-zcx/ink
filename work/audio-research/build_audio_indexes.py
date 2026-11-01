import re,json,csv,pathlib
root=pathlib.Path('/home/workspace/reader')
base=root/'docs/research/interaction-library'
items=[('M-034','M-034-fiio.md','JM21 Android and preinstalled FiiO Music; firmware/app build unknown'),('M-036','M-036-sony.md','Sony NW-A306 YY1301B1; English help guide; firmware/app build unknown'),('M-038','M-038-tonie.md','Toniebox 1 and 2 distinguished per source; tonies app build unknown'),('M-039','M-039-audible.md','Audible Android/iOS app US marketplace; Cloud Player only where explicit; app build unknown'),('M-040','M-040-antennapod.md','AntennaPod Android current documentation; app build unknown')]
evidence=[];coverage=[]
for pid,fn,scope in items:
 text=(base/'cases'/fn).read_text()
 for line in text.splitlines():
  m=re.match(r'\| (E-[A-Z]+-\d+) \| \[([^\]]+)\]\(([^)]+)\) \| (.*?) \|$',line)
  if m:
   eid,title,url,locator=m.groups(); st='official_help'; access='opened_text'; visual=False; cap='bibliography_and_short_note_only'; date=None; ver=scope
   if url.endswith('.pdf'): st='official_manual';access='opened_pdf_text'
   if eid in ['E-FIIO-11','E-FIIO-12']:st='official_product_page'
   if eid=='E-TONI-15':st='official_blog';date='2026-08-18'
   if eid=='E-TONI-01':ver='Toniebox 1 original guide EN 1.0 April 2018; not proof of current firmware or Toniebox 2 behavior'
   if pid=='M-038' and eid not in ['E-TONI-01','E-TONI-15','E-TONI-16','E-TONI-17','E-TONI-18']:
    n=int(eid.rsplit('-',1)[-1]);ver=('Toniebox 1' if n<=7 else 'Toniebox 2')+'; English official help; firmware and app build unknown'
   if pid=='M-034' and 2<=int(eid.rsplit('-',1)[-1])<=10:date='2024-12-11'
   if pid=='M-039':
    dm=re.search(r'2026-\d{2}-\d{2}',locator);date=dm.group(0) if dm else None
   if 'github.com/AntennaPod' in url:
    st='official_source_code';access='github_api_and_raw_source_read';ver='AntennaPod develop snapshot d3e41c6cb37c8e1a060b142686f818999de89859; release/build not established; static read only';cap='pinned_source_and_short_note_only'
   if eid=='E-SONY-14':
    st='official_illustration';access='downloaded_image_viewed';visual=True;cap='single_frame_viewed_no_durable_archive';locator+=' 临时文件 work/audio-research/sony-playback-official.png；非实机。'
   evidence.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=st,access_mode=access,version_scope=ver,app_build=None,firmware_version=None,source_updated_on=date,locator=locator,note='仅支持案例中标明的任务和范围；未运行产品。' if st!='official_illustration' else '单张标注示意图；仅证明布局，不证明转换/耗时。',hands_on=False,visual_observed=visual,archived_artifact=None,sha256=None,capture_state=cap,supersedes=None))
  if line.startswith('| F-M'):
   cols=[a.strip() for a in line.split('|')[1:-1]]
   if len(cols)!=7:raise ValueError((fn,len(cols),line))
   fid,task,pre,steps,data,es,gap=cols
   eids=re.findall(r'E-[A-Z]+-\d+',es)
   coverage.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=';'.join(eids),known_branches=data,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/'+fn,checked_on='2026-10-26'))
(base/'batches').mkdir(exist_ok=True)
with (base/'batches/audio-evidence.jsonl').open('w') as f:
 for x in evidence:f.write(json.dumps(x,ensure_ascii=False)+'\n')
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (base/'batches/audio-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(coverage)
ids={x['evidence_id'] for x in evidence}
for c in coverage:
 for e in c['evidence_ids'].split(';'):assert e in ids,(c['flow_id'],e)
assert len(ids)==len(evidence)
assert len({x['flow_id'] for x in coverage})==len(coverage)
for pid,fn,scope in items:print(pid,'evidence',sum(x['product_id']==pid for x in evidence),'flows',sum(x['product_id']==pid for x in coverage))
print('TOTAL',len(evidence),len(coverage))
