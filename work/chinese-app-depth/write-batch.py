from pathlib import Path
import csv,json,hashlib
root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
batch=lib/'batches'
base=dict(checked_on='2026-10-26',app_build=None,firmware_version=None,source_updated_on=None,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
article='https://www.woshipm.com/evaluating/6214837.html'
scope='Chinese Xiaoyuan AI; author experience published 2025-05-12; build and platform not fixed'
e=[dict(base,evidence_id='E-XYCD-01',product_id='M-028',title='作者体验：练习题型操作记录',url=article,source_type='third_party_firsthand_report',access_mode='opened_text',version_scope=scope,source_published_on='2025-05-12',locator='口算练习、字词听写、口语练习；题型小节标题',note='历史局部路径；未采纳技术归因和效果评价。标题与正文目录数量不一致。',raw_work_path='work/chinese-app-depth/xiaoyuan-firsthand.html',raw_sha256=hashlib.sha256((root/'work/chinese-app-depth/xiaoyuan-firsthand.html').read_bytes()).hexdigest())]
audit='artifacts/chinese-app-controls-audit.md'
log=json.loads((root/'work/chinese-app-depth/fetch-log.json').read_text())
for n,item in enumerate(log[:3],2):
 e.append(dict(base,evidence_id=f'E-XYCD-{n:02}',product_id='M-028',title=['口算静态拼图','听写静态拼图','跟读静态拼图'][n-2],url=item['url'],source_type='third_party_firsthand_screenshot',access_mode='downloaded_static_image_viewed',version_scope=scope,locator='对应同一文章题型小节',note='静态观察范围见正式记录；没有连续过程。',visual_observed=True,visual_audit_path=audit,visual_audit_sha256=hashlib.sha256((lib/audit).read_bytes()).hexdigest(),capture_state='durable_visual_audit_original_temporary',derived_from='E-XYCD-01',raw_work_path=item['path'],raw_sha256=item['sha256']))
item=log[3]
e.append(dict(base,evidence_id='E-IPAD-01',product_id='M-014',title='掌阅商店所链2024隐私政策中的功能路径',url=item['url'],source_type='official_policy_function_description',access_mode='downloaded_html_text',version_scope='Chinese mobile app; policy 2024-06-27; App Store link checked 2026-10-26; not build 8.6.2 operation',source_updated_on='2024-06-27',locator='第二节2阅读记录、5智能对话、7个性化服务',note='政策书面说明；不证明当前菜单、恢复能力或履行结果。',raw_work_path=item['path'],raw_sha256=item['sha256']))
(batch/'chinese-app-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
rows=[['F-M028-05','M-028','在线字词听写：历史任务局部','D2','E-XYCD-01;E-XYCD-03','提交与后续操作边界','当前版本、撤回、强退恢复和识别误判修订','static_reference_only','false','cases/M-028-xiaoyuan-ai.md','2026-10-26'],['F-M028-06','M-028','单词跟读：历史失败分支','D2','E-XYCD-01;E-XYCD-04','失败次数对应不同路径','评分门槛、漏录和中断保存；拼图非连续','static_reference_only','false','cases/M-028-xiaoyuan-ai.md','2026-10-26'],['F-M014-06','M-014','手机关闭个性化推荐：历史政策路径','D2','E-IPAD-01','个性化与非个性化栏目区别','当前版本路径、生效时间、跨设备及历史清除','not_observed','false','cases/M-014-ireader.md','2026-10-26']]
with (batch/'chinese-app-controls-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.writer(f,lineterminator='\n');w.writerow(fields);w.writerows(rows)
updates={'F-M028-02':dict(evidence_ids='E-XYAI-03;E-XYCD-01;E-XYCD-02',known_branches='历史口算局部补证；其他题型不套用',gaps='当前构建、识别误判修订、保存粒度',visual_status='static_reference_only')}
for name in ['F-M014-02','F-M014-03']:
 updates[name]={}
for path in batch.glob('*-coverage.csv'):
 with path.open() as f:reader=csv.DictReader(f);fs=reader.fieldnames;rs=list(reader)
 changed=False
 for row in rs:
  if row['flow_id'] in updates:
   row.update(updates[row['flow_id']]);changed=True
   if row['product_id']=='M-014' and 'E-IPAD-01' not in row['evidence_ids'].split(';'):row['evidence_ids']+=';E-IPAD-01'
 if changed:
  with path.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fs,lineterminator='\n');w.writeheader();w.writerows(rs)
invfields=['entry_id','product_id','scope','entry_type','title','upstream_id','url','evidence_id','review_status','related_flow_ids','checked_on']
titles=['口算练习','同步练','竖式计算','单位换算','听算练习','字词听写','趣味识字','背古诗','语文练习','练字挑战','背单词','单词听写','口语练习']
flows={0:'F-M028-02',5:'F-M028-05',12:'F-M028-06'}
with (lib/'inventories/M028-historical-practice-inventory.csv').open('w',newline='') as f:
 w=csv.writer(f,lineterminator='\n');w.writerow(invfields)
 for i,title in enumerate(titles):w.writerow([f'I-M028-H{i+1:02}','M-028',scope,'third_party_article_subsection',title,title,article,'E-XYCD-01','text_read_partial_task_analysis' if i in flows else 'text_read_not_task_analyzed',flows.get(i,''),'2026-10-26'])
print('5 evidence, 3 new flows, 3 original flows refined, 13 historical article subsections')
