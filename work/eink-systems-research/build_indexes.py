import csv,json,pathlib,re
root=pathlib.Path('/home/workspace/reader')
base=root/'docs/research/interaction-library'
boox={str(a['id']):a for a in json.loads((root/'work/eink-systems-research/boox-articles.json').read_text())}
items=[('M-004','M-004-boox-neoreader.md','Note Air5 C Android 15; V4.1.2 baseline established by official launch and model page; current generic help may describe later menus; actual build unknown','V4.1.2 feature baseline; no observed device'),('M-010','M-010-palma.md','Palma 2 monochrome Android 13; actual firmware unknown; current generic V4.1+ paths conditional; historical product-page and help differences',None),('M-011','M-011-bigme.md','HiBreak Pro monochrome Android 14; xReader build and firmware unknown; not Pro Color or Dual',None),('M-005','M-005-remarkable.md','reMarkable Paper Pro 11.8 inch; current help and 3.29 release reference; device and companion app build unknown','3.29 documentation reference; no observed device')]
evidence=[];coverage=[]
for pid,fn,scope,firmware in items:
 text=(base/'cases'/fn).read_text()
 for line in text.splitlines():
  m=re.match(r'\| (E-[A-Z]+-\d+) \| \[([^\]]+)\]\(([^)]+)\) \| (.*?) \|$',line)
  if m:
   eid,title,url,locator=m.groups();st='official_help';access='opened_text';date=None;ver=scope
   if 'booxsupport.zendesk.com' in url:
    access='official_public_api_article_body_read'
    ident=re.search(r'/articles/(\d+)',url).group(1)
    if ident in boox:date=boox[ident]['updated_at'].split('T')[0]
   if eid in ['E-BOOX-19','E-PALMA-17']:access='opened_text'
   if eid in ['E-BOOX-01','E-PALMA-01','E-BIGME-01','E-RMK-15']:st='official_product_page'
   if eid in ['E-BOOX-02','E-PALMA-02','E-RMK-05','E-RMK-09']:st='official_blog'
   if eid=='E-BOOX-02':date='2025-08-28'
   if eid=='E-PALMA-02':date='2024-10-22'
   if eid=='E-BOOX-03':st='official_blog';date='2025-10-20'
   if eid=='E-PALMA-03':st='official_blog';date='2025-08-28'
   if eid in ['E-PALMA-15','E-BIGME-02']:
    st='manufacturer_manual_mirror';access='opened_pdf_text'
    if eid=='E-PALMA-15':ver='BOOX Mobile ePaper Series Universal User Manual V4.0; retailer filename contains Palma 2 Pro; not proof of Palma 2-specific or V4.1 menus';firmware_for_source='V4.0 generic manual'
    else:ver='Bigme-signed HiBreak pro monochrome Android 14 26-page manual; firmware unspecified; regulator-document mirror';firmware_for_source=None
   else:firmware_for_source=firmware
   if pid=='M-005' and 'support.remarkable.com' in url:access='official_index_article_text'
   if pid=='M-005':
    for rm in json.loads((root/'work/eink-systems-research/remarkable-fetch.json').read_text()):
     if url.endswith('/'+rm['name']):
      access='official_html_article_body_read'
      day,month,year=rm['date_raw'].split('.')
      date=year+'-'+month+'-'+day
   if eid=='E-RMK-02':st='official_release_notes'
   if eid=='E-RMK-01':
    st='official_manual';access='official_index_text_and_pdf_download';ver='Historical Paper Pro English User Guide; no firmware number; old trial offer excluded; file downloaded but not fully parsed or visually inspected';firmware_for_source=None
   evidence.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=st,access_mode=access,version_scope=ver,app_build=None,firmware_version=firmware_for_source,source_updated_on=date,locator=locator,note='仅支持case指定范围；型号/版本限定及未核实点见正文，文档重建不是操作测试。',hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))
  if line.startswith('| F-M'):
   cols=[s.strip() for s in line.split('|')[1:-1]]
   assert len(cols)==7,(fn,line)
   fid,task,pre,steps,data,es,gap=cols
   eids=re.findall(r'E-[A-Z]+-\d+',es)
   coverage.append(dict(flow_id=fid,product_id=pid,task=task,depth='D1' if 'D1' in task else 'D2',evidence_ids=';'.join(eids),known_branches=data,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/'+fn,checked_on='2026-10-26'))
out=base/'batches'
with (out/'eink-systems-evidence.jsonl').open('w') as f:
 for e in evidence:f.write(json.dumps(e,ensure_ascii=False)+'\n')
with (out/'eink-systems-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']);w.writeheader();w.writerows(coverage)
main_keys=set(json.loads((base/'evidence.jsonl').read_text().splitlines()[0]))
assert all(set(e)==main_keys for e in evidence)
ids={e['evidence_id'] for e in evidence}
assert len(ids)==len(evidence)
assert len({c['flow_id'] for c in coverage})==len(coverage)
for c in coverage:
 assert all(e in ids for e in c['evidence_ids'].split(';'))
 assert c['hands_on']=='false'
for pid,fn,scope,firmware in items:
 text=(base/'cases'/fn).read_text()
 assert '\ufffd' not in text
 assert all(e in ids for e in re.findall(r'E-[A-Z]+-\d+',text))
 print(pid,'evidence',sum(e['product_id']==pid for e in evidence),'D2',sum(c['product_id']==pid and c['depth']=='D2' for c in coverage),'D1',sum(c['product_id']==pid and c['depth']=='D1' for c in coverage))
print('PASS schema/reference/uniqueness/UTF-8/hands_on checks:',len(evidence),'evidence',len(coverage),'flows')
