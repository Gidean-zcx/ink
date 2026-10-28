from pathlib import Path
import re,json,hashlib
W=Path('work/research/batch42-child-inventory')
base=Path('work/khan-library-controls-deepening')
items=[
 ('E-KLCD-02','360048828572','https://khankids.zendesk.com/hc/en-us/articles/360048828572-Learn-more-about-the-Learning-Path'),
 ('E-KLCD-07','4457349308059','https://khankids.zendesk.com/hc/en-us/articles/4457349308059-Chinese-Parent-Guide-%E5%AE%B6%E9%95%BF%E6%8C%87%E5%8D%97-%E5%BC%80%E5%A7%8B%E4%BD%BF%E7%94%A8%E5%8F%AF%E6%B1%97%E5%AD%A6%E9%99%A2%E5%84%BF%E7%AB%A5%E7%89%88'),
 ('E-KLCD-10','4403614100109','https://khankids.zendesk.com/hc/en-us/articles/4403614100109-Progress-reports-in-the-Khan-Academy-Kids-app')]
allm=[]
for eid,aid,url in items:
 p=base/'text'/f'articles-{aid}.txt'; text=p.read_text()
 lines=text.splitlines()
 for idx,line in enumerate(lines):
  line_m=re.match(r'L(\d+):',line)
  matches=list(re.finditer(r'【(\d+)†Image: ([^】]+)】',line))
  if not line_m or not matches:continue
  line_no=int(line_m.group(1))
  for match in matches:
   ref=int(match.group(1));name=match.group(2)
   before=[];after=[]
   for s in lines[max(0,idx-2):idx]:
    z=re.sub(r'^L\d+:\s*','',s).strip()
    if z:before.append(z)
   for s in lines[idx+1:idx+4]:
    z=re.sub(r'^L\d+:\s*','',s).strip()
    if z:after.append(z)
   local=[]
   for q in base.rglob('*'):
    if q.is_file() and q.name.lower()==name.lower():
     local.append({'path':str(q),'sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'bytes':q.stat().st_size})
   allm.append({'evidence_id':eid,'article_url':url,'article_text_path':str(p),'article_text_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'article_line':line_no,'link_ref_in_saved_text':ref,'image_label':name,'media_url':None,'media_url_status':'not_preserved_in_saved_web-tool text; no raw article HTML available; do not guess','context_before':before,'context_after':after,'local_files':local})
# manual priority based only on local position/context
priority=[
 ('E-KLCD-10','5-25-21_Report_Dialog_Assignments_iPad.png','attempt history dialog / multiple attempts'),
 ('E-KLCD-10','Progress_article.png','All Progress, low score and Basic scaffolding context'),
 ('E-KLCD-10','ClassReport_Assignments.png','Assignments report and completion/attempt entry'),
 ('E-KLCD-10','ClassReport_AllProgress__1_.png','All Progress return/report state'),
 ('E-KLCD-07','Home.png','home Play button and Kodi help entry context'),
 ('E-KLCD-07','Progress_checkmarks.png','post-activity visible progress state')]
selected=[]
for eid,name,reason in priority:
 x=next((x for x in allm if x['evidence_id']==eid and x['image_label']==name),None)
 if x:selected.append({'evidence_id':eid,'image_label':name,'article_line':x['article_line'],'media_url':x['media_url'],'local_files':x['local_files'],'selection_reason':reason,'limitation':x['media_url_status']})
out={'checked_on':'2026-10-27','network_used':False,'scope':['E-KLCD-02','E-KLCD-07','E-KLCD-10'],'raw_html_found':False,'source_note':'Only saved web-tool text extracts were found for these three articles. Link reference numbers and image filenames are present, but their target attachment URLs were not preserved.','article_media':allm,'selected_up_to_6':selected,'existing_local_images_note':'No local file matched any article image filename. Existing look-at-my-map.png and mute.png belong to other E/articles and were not selected.','known_attachment_urls_excluded':[{'url':'https://khankids.zendesk.com/hc/article_attachments/4420861003803','local_path':'work/khan-library-controls-deepening/mute.png','reason':'belongs to background-music article, not E-KLCD-02/07/10'},{'url':'https://khankids.zendesk.com/hc/article_attachments/4409150302747','local_path':'work/khan-library-controls-deepening/look-at-my-map.png','reason':'belongs to fluency article, not E-KLCD-02/07/10'},{'url':'https://khankids.zendesk.com/hc/article_attachments/360007362331','local_path':None,'reason':'previous timeout and source article mapping is not one of the three target E; not requested'}]}
(W/'local-media.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'media_entries':len(allm),'selected':len(selected),'with_real_url':sum(x['media_url'] is not None for x in allm),'with_local_file':sum(bool(x['local_files']) for x in allm)},ensure_ascii=False))
