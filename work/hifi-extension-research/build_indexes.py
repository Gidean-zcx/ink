import csv,json,pathlib,re
b=pathlib.Path('docs/research/interaction-library')
items=[('M-063','M-063-shanling.md','Shanling M1 Plus non-Android MTouch; V2.1 release baseline; older quick guide qualified; not M1/M1s/M0 or Android models','V2.1 release baseline; device build unknown'),('M-064','M-064-ibasso.md','iBasso DX180 Android and Mango app; V1.05 download listing; undated manual paths; actual Android major and app/device builds unconfirmed','V1.05 listing 2026-08-27; device build unknown'),('M-065','M-065-astell-kern.md','Astell&Kern SR35 V1.22 release baseline; 2023 manual and linked regional shared FAQs qualified; actual build unknown','V1.22 release baseline; device build unknown')]
dates={'E-IBA-03':'2026-08-27','E-AK-01':'2023-05-02','E-AK-03':'2025-07-14','E-AK-04':'2025-07-15','E-AK-07':'2025-05-01'}
e=[];c=[]
for pid,fn,scope,fw in items:
 s=(b/'cases'/fn).read_text()
 for line in s.splitlines():
  m=re.match(r'\| (E-[A-Z]+-\d+) \| \[([^\]]+)\]\(([^)]+)\) \| (.*?) \|$',line)
  if m:
   eid,title,url,loc=m.groups();typ='official_help';access='opened_text';v=scope;sourcefw=fw
   if eid=='E-SHAN-01':typ='official_manual';access='official_pdf_extracted_text_read';sourcefw=None;v='M1 Plus multilingual quick guide V1.0 document revision; firmware unspecified; Chinese/English power control discrepancy retained'
   if eid in ['E-SHAN-02','E-SHAN-03','E-IBA-03','E-AK-03','E-AK-04']:typ='official_release_notes'
   if eid=='E-SHAN-04':typ='official_product_page';access='official_index_text';sourcefw=None
   if eid=='E-IBA-01':typ='official_manual';access='official_pdf_extracted_text_read_then_followup_timeout';sourcefw=None;v+='; manual revision not stated; PDF also downloaded HTTP200 to temporary work only'
   if eid=='E-IBA-02':typ='manufacturer_manual_mirror';access='mirror_manual_text_only';sourcefw=None;v+='; 63-page same manual as E-IBA-01; mirror generated Q&A excluded'
   if eid=='E-IBA-03':access='official_html_read_then_followup_timeout';v+='; listing version/date only, not complete V1.05 changelog'
   if eid=='E-AK-01':typ='official_manual_index';sourcefw=None;v+='; official English PDF link discovered, original PDF not extracted'
   if eid=='E-AK-02':typ='manufacturer_manual_mirror';access='mirror_extracted_text_read';sourcefw=None;v='SR35 manufacturer manual associated with 2023 release; exact revision and firmware not stated; not V1.22 full menu verification'
   if eid=='E-AK-03':access='official_index_article_text_original_url_redirects_home';v+='; indexed 2025-07-14 article, direct URL now redirects home'
   if pid=='M-065' and eid not in ['E-AK-01','E-AK-02','E-AK-03','E-AK-04']:
    typ='official_regional_help';sourcefw=None;v+='; linked from Japanese SR35 support; shared FAQ example model retained'
   if eid=='E-AK-04':v+='; regional entry created 2025-07-08 and updated 2025-07-15'
   if eid=='E-AK-07':v+='; tested compatibility list as of 2025-05-01, not current login assurance'
   e.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=typ,access_mode=access,version_scope=v,app_build=None,firmware_version=sourcefw,source_updated_on=dates.get(eid),locator=loc,note='文档研究；仅支持case指定型号和版本范围，旧手册/共用FAQ不等于当前版本实测。缺少完整操作链的条目为D1。',hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))
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
assert all(not x['hands_on'] and not x['visual_observed'] for x in e)
with (b/'batches/hifi-extension-evidence.jsonl').open('w') as f:
 for row in e:f.write(json.dumps(row,ensure_ascii=False)+'\n')
with (b/'batches/hifi-extension-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(c)
for pid,*_ in items:print(pid,'evidence',sum(x['product_id']==pid for x in e),'D2',sum(x['product_id']==pid and x['depth']=='D2' for x in c),'D1',sum(x['product_id']==pid and x['depth']=='D1' for x in c))
print('PASS schema/reference/unique/UTF8/hands_on checks',len(e),'evidence',len(c),'flows')
