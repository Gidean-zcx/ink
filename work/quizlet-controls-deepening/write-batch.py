from pathlib import Path
import csv,json,hashlib,re
b=Path('work/quizlet-controls-deepening');lib=Path('docs/research/interaction-library');batch=lib/'batches';bn='quizlet-controls-deepening';src=json.loads((b/'source-map.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
spec=[
('categories-360001601132','类目分组及 Folders 两项','只登记官方学习类目的父级与已列入口；不代表整个产品功能清单。'),
('sections-360004698771','Creating/editing/saving：23篇目录','标题仅作目录，未读项明确保持未分析。'),
('sections-360005938691','Studying：27篇目录','区分已研究活动与新列出的音频、游戏、社交及外部集成入口。'),
('sections-360005877232','Accessing/sharing：12篇目录','访问、共享、权限、导出、离线等入口；目录不证明按钮行为。'),
('articles-360030212131','To edit / change language / delete terms；正文 L19–104','补网站与手机删卡、保存/插卡分支；单卡误删恢复仍未知。'),
('articles-360030171011','To delete a set；正文 L17–43','自建集才可删除，官方明确不可恢复；未给额外确认框，不补猜。'),
('articles-360029004272','To remove；正文 L19–29','网站动态隐藏有 Remove→Hide this set；两手机 App 不提供，不能混同删集。'),
('articles-360030255191','To change visibility / Who can see / age exception；L17–81','卡组可见性设置与年龄例外；不推断生成指南/卡组权限级联。'),
('articles-360030990531','To study / Settings / Tips；L19–80','补两轮错题、每题正确两次、揭答与误拼纠正；不当长期掌握测量。'),
('articles-360030645752','To study / Settings / mobile limitation；L21–42','网站听写的错字提示、重播、慢速、重试与结束条件。'),
('articles-360048314692','To set up / saving condition；L21–57','必须开始学习才保存路径；声称跨网站/移动进度同步。'),
('articles-360048806511','To turn off / Reset Progress / last platform statement；L19–41','重学清除 Learn 进度；页末仅列 Web/iOS，与设置页移动范围存在差异。'),
('articles-360030642972','To study / Tips；L21–59','卡组 Test 提交/分数/回顾；未完离开不保存。直接 requests 403 后停止该路线。'),
('articles-360048803491','Progress definition / targeted studying；L17–34','跨活动对错统计与分组复习；游戏不计，分组阈值及误判重算未知。'),
('articles-360031172312','To star / Settings；L19–49','手动星标子集、移动取消星标、跨集文件夹分支；不是自动错题迁移。'),
('articles-25946589648013','Generate/take / Settings / mobile Tips；L17–82','AI试卷独立于卡组Test；题数题型计时和资格/字符限制，未证明逐题校订或超时恢复。'),
('articles-360048313652','Grading options in Test / Settings；L55–72、110–153','补网站单答案评分及偏好传播；与 Learn 宽松度、误判按钮分开。')]
evs=[]
for i,(key,locator,note) in enumerate(spec,1):
 s=src[key];e={'evidence_id':f'E-QZCD-{i:02}','product_id':'M-021','title':s['title'],'url':s['url'],'checked_on':'2026-10-27','source_type':'official_help_index' if i<=4 else 'official_help','access_mode':'opened_text_via_web_tool','version_scope':'查阅日 en-US 官方公开帮助；Web/iOS/Android 条件按正文分别解释；精确构建与功能上线日期未知','app_build':None,'firmware_version':None,'source_updated_on':None,'locator':locator,'note':note,'hands_on':False,'visual_observed':False,'visual_audit':None,'archived_artifact':None,'sha256':None,'capture_state':'official_web_tool_response_saved_in_work; no_source_HTML; no_images','raw_capture_kind':s['raw_capture_kind'],'raw_work_path':s['raw_work_path'],'raw_sha256':s['raw_sha256'],'parsed_text_path':s['parsed_text_path'],'http_status':None,'supersedes':None}
 if i==5:e['qualifies']='E-QZLT-04';e['same_article_warning']='与旧 en-AU 编辑页同一 article ID，不是新的独立操作体系；本轮补正文分支。'
 if i==17:e['qualifies']='E-QZLT-07';e['same_article_warning']='同旧来源补 Test 章节，不作独立新文献数量。'
 if i==13:e['direct_http_probe_status']=403;e['direct_http_probe_record']='work/quizlet-controls-deepening/fetch-probe.json'
 evs.append(e)
(batch/f'{bn}-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evs))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
new=[
('11','卡组 Test 作答与结果','E-QZCD-13;E-QZCD-17','选题量/题型→开始→提交后分数/回顾；网站可打印；离开未完成测试不保存；网站单答案评分','漏答确认、超时、改答案、提交后误判按钮、重考和成绩历史未知'),
('12','AI Practice Test 生成与作答','E-QZCD-16','资料/卡组→生成→题数题型计时→作答→提交看分数；手机字符限制、地区年龄/套餐条件；标题/权限可改','生成题干和答案校订、重生成覆盖、计时到零、暂停续做及删除恢复未知；不套普通 Test 规则'),
('13','Progress 与星标定向复习','E-QZCD-14;E-QZCD-15','分组或手动星标→子集→选择活动；星标私有，手机可取消；Progress 对错统计跨端、游戏不计','Mastered 阈值、误判重算、源集修订映射、星标与间隔调度互通未知')]
with (batch/f'{bn}-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader()
 for n,t,es,k,g in new:w.writerow(dict(flow_id=f'F-M021-{n}',product_id='M-021',task=t,depth='D2',evidence_ids=es,known_branches=k,gaps=g,visual_status='not_observed',hands_on='false',case_path='cases/M-021-quizlet.md',checked_on='2026-10-27'))
old={r['flow_id']:r for r in json.loads((b/'existing-coverage-before.json').read_text())}
changes={
'02':{'evidence_ids':'E-QZLT-02;E-QZCD-08','known_branches':'地区年龄/免费额度；指南与生成卡组各管可见性，卡组设置与年龄例外另有正文','gaps':'指南正文或生成题目逐项编辑、源更新、重生成覆盖、删指南与卡组的联动/恢复未知'},
'04':{'evidence_ids':'E-QZLT-04;E-QZLT-11;E-QZLT-12;E-QZCD-05;E-QZCD-06;E-QZCD-07;E-QZCD-08','known_branches':'自有/复制/导出限制；网站行垃圾桶与手机左滑删卡；整集只能删自建且不可恢复；动态隐藏仅网站；可见性有年龄例外','gaps':'单卡误删/版本撤销、并发编辑、高级内容权限、隐藏找回、整集删除确认与指南级联未知'},
'07':{'evidence_ids':'E-QZLT-06;E-QZCD-09;E-QZCD-10;E-QZCD-11;E-QZCD-12','known_branches':'有限免费轮次；Write两轮及重输，Spell错字高亮/拼读/重试，每题或词正确两次结束；路径开始才保存；重学清除Learn进度','gaps':'普通Learn中断/跨日队列、重置撤销及对其他模式影响未知；Study path设置与重置帮助的Android范围不一致'},
'08':{'evidence_ids':'E-QZLT-07;E-QZLT-08;E-QZCD-09;E-QZCD-17','known_branches':'语言/三卡条件、I was correct、切题型重置连胜；网站Test单答案要求按分隔符识别，偏好影响该集其他活动且可取消','gaps':'长期记忆状态修正、Test提交后误判修正、改变评分对已提交成绩的影响未知；不将Learn/Write按钮套到Test'}}
with (b/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader()
 for n,vals in changes.items():
  vals['checked_on']='2026-10-27'; fid='F-M021-'+n
  for field,value in vals.items():w.writerow(dict(flow_id=fid,field=field,before=old[fid][field],after=value,reason='同任务补证；D2/未实操/未观察状态不变；root核验后更新旧所属批次'))
# Inventory: article titles only, not new tasks. Index source remains traceable when article body is unread.
old_e=[json.loads(x) for x in (batch/'dictionary-evidence.jsonl').read_text().splitlines() if x]
old_e=[x for x in old_e if x['product_id']=='M-021'];e_bytitle={x['title']:x for x in old_e+evs}
maprows={
'Creating study sets with Smart Assist':('01','E-QZLT-01','生成失败、来源核验、重生成覆盖未知'),
'Studying with Study Guides':('02','E-QZLT-02','指南内容校订、源更新和删除级联未知'),
'Creating sets by importing content':('03','E-QZLT-03','空行/重复/分隔异常与失败回退未知'),
'Editing sets':('04','E-QZLT-04;E-QZCD-05','单卡撤销与并发编辑未知'),
'Editing draft sets':('05','E-QZLT-05','进程终止后的恢复边界未知'),
'Copying and editing a set':('04','E-QZLT-12','高级内容与复制品后续授权未知'),
'Deleting a set':('04','E-QZCD-06','额外确认、跨对象删除级联未知；正文称不可恢复'),
'Removing sets from your Recent activity feed':('04','E-QZCD-07','隐藏后的找回与历史学习关联未知'),
'Changing a set\'s visibility':('02;04','E-QZCD-08','旧链接生效时点和指南权限联动未知'),
'Exporting your sets':('04','E-QZLT-11','图片导出不支持；导出是否含各类标注未知'),
'Studying offline with Quizlet mobile apps':('10','E-QZLT-10','精确同步字段、冲突和取消套餐后缓存未知'),
'Studying with Flashcards':('06','E-QZLT-13','会话位置与跨活动统计边界未知'),
'Studying with Learn':('07','E-QZLT-06','普通Learn结束/中断队列仍未知'),
'Studying with Write mode':('07;08','E-QZCD-09','中断及误判修正对长期调度影响未知'),
'Studying with Spell mode':('07','E-QZCD-10','中断续接、错误统计与重学影响未知'),
'Studying with Test':('11','E-QZCD-13','漏答/超时/重考、成绩历史与误判修正未知'),
'Studying with Practice Tests':('12','E-QZCD-16','逐题校订、计时到零、续做及删除恢复未知'),
'Studying most-missed terms with stars':('13','E-QZCD-15','自动标错与星标的关系未证实'),
'Using Progress for targeted studying':('13','E-QZCD-14','阈值/误判重算/内容修订映射未知'),
'Using grading options (US)':('08;11','E-QZLT-07;E-QZCD-17','变更对既有成绩/其他调度的作用未知'),
'Setting up a study path':('07','E-QZCD-11','Android范围与重置页不一致'),
'Resetting study path progress':('07','E-QZCD-12','取消/撤销与跨模式清除范围未知'),
'Studying with Answer Streaks':('08','E-QZLT-08','长期学习状态与连胜的关系未知'),
'Studying with Spaced Repetition':('09','E-QZLT-09','误评撤销和其他模式互通未知')}
sections=[('Creating, editing, and saving sets','sections-360004698771','E-QZCD-02'),('Accessing and sharing','sections-360005877232','E-QZCD-04'),('Studying','sections-360005938691','E-QZCD-03'),('Folders','categories-360001601132','E-QZCD-01')]
inv=[]
for title,key,eid in sections:
 parent=len(inv)+1;s=src[key];inv.append(dict(order=parent,entry_kind='parent_group',parent_order='',functional_domain=title,source_url=s['url'],source_section=title,article_url='',evidence_ids=eid,flow_ids='',coverage='目录登记',missing_controls_or_branches='父级不计独立任务；以子项状态为准',checked_on='2026-10-27'))
 titles=json.loads((b/(key+'-titles.json')).read_text()) if title!='Folders' else ['Organizing study content with tags','Organizing study content with folders']
 for t in titles:
  m=maprows.get(t);ee=e_bytitle.get(t);ids=eid+((';'+m[1]) if m else '')
  inv.append(dict(order=len(inv)+1,entry_kind='child_topic',parent_order=parent,functional_domain=t,source_url=s['url'],source_section=title+' / '+t,article_url=ee['url'] if ee else '',evidence_ids=ids,flow_ids=';'.join('F-M021-'+n for n in m[0].split(';')) if m else '',coverage='D2部分；非观察' if m else '仅目录；正文未在本例展开',missing_controls_or_branches=m[2] if m else '可从官方目录继续；入口/动作/反馈/结束/异常恢复均未逐项核定',checked_on='2026-10-27'))
assert len(inv)==68 and sum(x['entry_kind']=='child_topic' for x in inv)==64
with (batch/f'{bn}-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
case=lib/'cases/M-021-quizlet.md';s=case.read_text();s=s.replace('I was correct 可纠正相关误判。单答案偏好','I was correct 可纠正相关误判，切题型重置连胜。单答案偏好')
marker='\n本轮补证（2026-10-27）：\n'
assert marker not in s
s+=marker+'\n'
for e in evs:s+=f"- {e['evidence_id']}：[{e['title']}]({e['url']})；{e['locator']}。{'目录，非操作证明' if e['source_type']=='official_help_index' else '官方正文，经网页工具读取'}。\n"
case.write_text(s)
(b/'batch-summary.json').write_text(json.dumps({'new_E':len(evs),'new_F':len(new),'old_flows_with_proposals':list(changes),'old_field_proposals':sum(len(x) for x in changes.values()),'inventory_parents':4,'inventory_articles':64,'inventory_partial_topics':sum(x['entry_kind']=='child_topic' and x['flow_ids']!='' for x in inv),'visual_observed':False,'hands_on':False,'new_formal_artifacts':0},ensure_ascii=False,indent=2)+'\n')
print((b/'batch-summary.json').read_text())
