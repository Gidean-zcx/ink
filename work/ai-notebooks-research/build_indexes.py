import csv,json,pathlib,re
b=pathlib.Path('docs/research/interaction-library')
items=[('M-007','M-007-viwoods.md','AiPaper 10.65 inch Android13; V3.15 current documentation; older manual/blog paths explicitly qualified; not Mini/Reader','V3.15 documentation baseline; device build unknown'),('M-008','M-008-hanvon.md','Hanvon N10 Pro gen2 China; firmware/app builds unknown; no dedicated model manual obtained',None),('M-009','M-009-iflytek-office.md','iFLYTEK Smart Office Notebook X2 China historical model; indexed X2 help and qualified current multi-model policy; firmware/apps unknown',None)]
e=[];c=[]
dates={'E-VIW-04':'2026-06-02','E-VIW-06':'2025-05-26','E-VIW-08':'2025-05-22','E-HANV-03':'2025-07-18','E-IFLYO-04':'2026-02-26'}
for pid,fn,scope,fw in items:
 s=(b/'cases'/fn).read_text()
 for line in s.splitlines():
  m=re.match(r'\| (E-[A-Z]+-\d+) \| \[([^\]]+)\]\(([^)]+)\) \| (.*?) \|$',line)
  if m:
   eid,title,url,loc=m.groups();typ='official_help';access='opened_text';v=scope;sourcefw=fw
   if eid=='E-VIW-01' or eid in ['E-HANV-01','E-HANV-02']:typ='official_product_page'
   if eid in ['E-VIW-02','E-VIW-03','E-VIW-04','E-VIW-05','E-VIW-07']:typ='official_release_notes'
   if eid in ['E-VIW-06','E-VIW-08','E-IFLYO-03','E-HANV-03']:typ='official_blog'
   if eid in ['E-VIW-02','E-VIW-07','E-VIW-09','E-VIW-10','E-VIW-11'] or pid=='M-008':access='official_html_body_read_via_requests'
   if eid=='E-VIW-11':typ='official_manual_index';v+='; PDF URLs discovered, original PDF not read'
   if eid=='E-VIW-12':typ='manufacturer_manual_mirror';access='mirror_extracted_text_read';v='AiPaper historical 70-page manufacturer manual; mirror dated 2025-04-30 not confirmed publication date; partial font-encoding corruption; only clear text used';sourcefw=None
   if eid in ['E-VIW-06','E-VIW-08']:sourcefw=None;v+='; 2025 historical blog not complete V3.15 menu verification'
   if eid=='E-VIW-07':sourcefw='URL/title 3.8.5; body Version3.8; unresolved label discrepancy'
   if eid in ['E-IFLYO-01','E-IFLYO-02']:access='official_index_full_article_text_original_url_404';sourcefw='X2 help says latest system without build number'
   if eid=='E-IFLYO-03':access='official_index_article_text';sourcefw='2023 X2 AI update announcement without build number'
   if eid=='E-IFLYO-04':typ='official_service_policy';v+='; conditional multi-model service and data rules, not evidence all features shipped to X2'
   e.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=typ,access_mode=access,version_scope=v,app_build=None,firmware_version=sourcefw,source_updated_on=dates.get(eid),locator=loc,note='仅支持case指定范围；宣传仅D1，未知不等于不支持；无实机或连续视觉操作。',hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))
  if line.startswith('| F-M'):
   cols=[x.strip() for x in line.split('|')[1:-1]];assert len(cols)==7,(fn,line)
   fid,task,pre,steps,data,es,gap=cols
   c.append(dict(flow_id=fid,product_id=pid,task=task,depth='D1' if 'D1' in task else 'D2',evidence_ids=';'.join(re.findall(r'E-[A-Z]+-\d+',es)),known_branches=data,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/'+fn,checked_on='2026-10-26'))
ids={x['evidence_id'] for x in e};fids={x['flow_id'] for x in c}
assert len(ids)==len(e) and len(fids)==len(c)
mainkeys=set(json.loads((b/'evidence.jsonl').read_text().splitlines()[0]));assert all(set(x)==mainkeys for x in e)
with (b/'coverage.csv').open() as f:fields=next(csv.reader(f))
assert set(fields)==set(c[0])
for pid,fn,scope,fw in items:
 s=(b/'cases'/fn).read_text();assert '\ufffd' not in s
 assert all(x in ids for x in re.findall(r'E-[A-Z]+-\d+',s))
for row in c:assert all(x in ids for x in row['evidence_ids'].split(';'))
with (b/'batches/ai-notebooks-evidence.jsonl').open('w') as f:
 for row in e:f.write(json.dumps(row,ensure_ascii=False)+'\n')
with (b/'batches/ai-notebooks-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(c)
for pid,*_ in items:print(pid,'evidence',sum(x['product_id']==pid for x in e),'D2',sum(x['product_id']==pid and x['depth']=='D2' for x in c),'D1',sum(x['product_id']==pid and x['depth']=='D1' for x in c))
print('PASS schema/reference/unique/UTF8/hands_on checks',len(e),'evidence',len(c),'flows')
