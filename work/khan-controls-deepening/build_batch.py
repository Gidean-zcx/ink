import csv,json,hashlib,shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/khan-controls-deepening';A=L/'artifacts/khan-controls-deepening';B=L/'batches'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
urls=[
('01','创作工具、录音与保存','https://khankids.zendesk.com/hc/en-us/articles/21188738836763-How-to-use-creative-tools-inside-the-Khan-Kids-app','2026-02-26','Library/Create; blue pencil; microphone/stop/play; green check','创作与回放、Gallery保存分开；数学故事和复述是活动范例，不是自动判分。','web-response-1.json'),
('02','Gallery导出','https://khankids.zendesk.com/hc/en-us/articles/6477686235419-How-do-I-save-artwork-from-the-Gallery','2026-02-27','steps 1-5','Gallery底部启用导出，再点作品箭头到相册；未观察内嵌GIF，格式及失败提示未知。','web-response-1.json'),
('03','图书馆顶层栏目','https://khankids.zendesk.com/hc/en-us/articles/4403657703821-Find-books-and-lessons-in-the-Khan-Kids-Library','2026-02-27','Explore tabs; Books/Create/Math/Reading/Videos/Letters/Logic+','8个标签及部分级别选择；7月活动描述不当10月当前活动。','web-response-2.json'),
('04','家长偏好设置','https://khankids.zendesk.com/hc/en-us/articles/360047566151-How-do-I-access-parental-controls','2026-02-27','Parent Section; Preferences','隐藏Videos/Create/Home与季节主题；Home隐藏不证明系统退出被禁用。家长版限定与教师页分别保留。','web-response-3.json'),
('05','教师隐藏栏目FAQ','https://khankids.zendesk.com/hc/en-us/articles/13849896234395-FAQ-How-do-I-turn-off-videos-and-coloring','2026-02-26','teacher avatar; Settings; Class Preferences','FAQ写OFF隐藏Create/Videos，与另一教师页的ON描述冲突。','web-response-1.json'),
('06','教师区域访问与作业优先','https://khankids.zendesk.com/hc/en-us/articles/17031195532059--NEW-Manage-student-access-to-areas-of-the-app','2026-02-26','Show students Assignments first/Create tab/Videos tab','作业优先ON要求完成作业；另两开关写ON不可访问，与FAQ冲突，实际方向未知。','web-response-1.json'),
('07','增加儿童档案','https://khankids.zendesk.com/hc/en-us/articles/360006538192-How-do-I-add-a-new-user','2026-02-27','To create additional users','同一家长账户可增孩子：New或头像、成人区、名字年龄头像、Next。删除和合并未知。','web-response-2.json'),
('08','按账户类型退出','https://khankids.zendesk.com/hc/en-us/articles/360043669551-FAQ-How-do-I-sign-out','2026-02-26','Parent Account; Class Account; Clever','家庭、课堂及Clever入口不同；没有退出前同步完成或未上传数据的保证。','web-response-1.json'),
('09','密码恢复与多设备冲突','https://khankids.zendesk.com/hc/en-us/articles/360007054191-How-do-I-set-up-reset-or-manage-a-password','2026-02-20','recover/reset; change; password keeps resetting','先单设备改密确认，再逐台登录；Kids不沿用主站密码；邻近重置入口可能误触。','web-response-2.json'),
('10','学习反馈动画抽样帧','https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/','2025-05-01','Step-by-Step Animations; A Visual Reward System','两GIF的离散抽样，非完整题目、非App操作；首次答对粒子条件仍依据正文。','11-design.html')]
notes=['# Khan Kids：来源与画面核查','', '查阅：2026-10-27。帮助正文来自web工具成功读取；普通HTTP直取10页均403后停止。work保存web原返回文本（JSON编码字符串），不是成功抓取的HTML。公开section列表与博客200；前者未展开，未计完整功能目录。','', '| E编号 | 来源与位置 | 本次短摘记 |','| --- | --- | --- |']
for n,t,u,d,loc,note,raw in urls:notes.append(f'| E-KHCD-{n} | [{t}]({u})；文档更新{d}；{loc} | {note} |')
notes += ['', '## 动画抽样边界（E-KHCD-10）','', 'Chromium156.0.8078.4、1280×950、新建匿名上下文。正常打开公开博客，页面200。初次8帧有Cookie浮层；第二次通过页面Strictly Necessary Only关闭浮层后抽样8帧。两次共16帧均已实看。没有下载GIF、安装App、提交答案或实际听音；不是本项目儿童观察。', '', '每个GIF取4个局部截图，截图间请求等待800毫秒，加截图耗时后间隔约1秒；具体采样相对时间见元数据。GIF在页面加载后已循环，记录时间不是媒体起点或逐帧时间码。两条循环不能拼接为一题的错误→提示→奖励链。', '', 'ThisIsHow示例可见“Sandy is …”及draw/drawing选项、提交勾和角色。抽样显示词在句中/选项间变化，不证明提示入口、触发阈值或点击动作。Sparkles示例可见三只熊、1/2/3、礼物车、角色姿态及两处粒子位置，不证明奖励车到达或完整结算。', '', '正式保留第二次的4个必要帧；首轮遮挡图及其余帧留work。静态页不用于推断准确颜色、动效时长、响应速度或E-Ink显示效果。', '']
selected=['ThisIsHow_LIGHT-1.png','ThisIsHow_LIGHT-4.png','Sparkles_FINAL-3.png','Sparkles_FINAL-4.png']
for f in selected:
 shutil.copyfile(W/'browser-clean'/f,A/f)
 notes.append(f'- [{f}]({f})')
meta=json.loads((W/'browser-clean/samples.json').read_text())
(A/'illustration-samples.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n')
notes+=['','原件及脚本在work/khan-controls-deepening；正式证据索引保存网页返回文本、博客原HTML及摘帧散列。缺口与具体任务见[M030案例](../../cases/M-030-khan-kids.md)。']
(A/'source-notes.md').write_text('\n'.join(notes)+'\n')
rows=[]
for n,t,u,d,loc,note,raw in urls:
 e={'evidence_id':f'E-KHCD-{n}','product_id':'M-030','title':t,'url':u,'source_type':'official_design_blog' if n=='10' else 'official_help','access_mode':'public_browser_illustration_sampling_and_html' if n=='10' else 'web_tool_public_page_text','version_scope':'Khan Academy Kids mobile; build/platform/region unknown; teacher and parent scopes distinguished','locator':loc,'checked_on':'2026-10-27','source_updated_on':d,'app_build':None,'firmware_version':None,'hands_on':False,'visual_observed':n=='10','archived_artifact':None,'sha256':None,'supersedes':None,'note':note,'raw_work_path':str((W/raw).relative_to(R)),'raw_sha256':sha(W/raw),'capture_state':'durable_notes_temporary_source_return','source_notes_path':str((A/'source-notes.md').relative_to(L))}
 if n=='10':e.update(derived_from='E-KHD-01',visual_kind='sampled_official_animation_frames',visual_audit_path=str((A/'source-notes.md').relative_to(L)),visual_audit_sha256=sha(A/'source-notes.md'),observed_on='2026-10-27',artifact_hashes={str((A/f).relative_to(L)):sha(A/f) for f in selected+['illustration-samples.json']})
 rows.append(e)
(B/'khan-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rows))
fields=list(next(csv.DictReader((L/'coverage.csv').open())))
flows=[
('07','画图、录故事并保存作品','01;03','背景/贴纸/画笔、撤销、录音停止回放、绿色勾存Gallery','权限拒绝、退出未保存、覆盖、可编辑数据与同步未知'),
('08','将Gallery作品导出到设备','02','Enable export后逐作品箭头，预期保存到相册','文件格式、动画音频范围、拒绝反馈、重复导出和回导未知'),
('09','同一家长账户增加儿童档案','07','New或头像入口、成人区、名字年龄头像、Next','已有成员切换/删除/合并、作品与进度隔离及跨端冲突未知'),
('10','退出账户与找回邮件密码登录','08;09','家庭/课堂/Clever退出不同；单设备重置后逐台登录','未同步数据、过期链接、其他登录方式及实测恢复未知'),
('11','家长与教师控制可访问区域','04;05;06','家长可隐藏Home等；教师作业优先；ON/OFF文档冲突','默认值、作用范围、实际开关方向、系统退出与同步未知')]
fr=[]
for n,t,es,k,g in flows:fr.append(dict(zip(fields,[f'F-M030-{n}','M-030',t,'D2',';'.join('E-KHCD-'+s for s in es.split(';')),k,g,'not_observed','false','cases/M-030-khan-kids.md','2026-10-27'])))
with (B/'khan-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(fr)
old=[]
for path in sorted(B.glob('*-coverage.csv')):
 rs=list(csv.DictReader(path.open()));changed=False
 for r in rs:
  n=r['flow_id']
  if n not in ['F-M030-01','F-M030-02','F-M030-04','F-M030-05']:continue
  before=r.copy();eid='E-KHCD-10' if n.endswith(('04','05')) else 'E-KHCD-03'
  assert eid not in r['evidence_ids'], 'Already applied; do not rerun'
  r['evidence_ids']+=';'+eid;r['checked_on']='2026-10-27'
  if n.endswith(('04','05')):r['visual_status']='static_reference_only'
  old.append({'batch':str(path.relative_to(R)),'before':before,'after':r.copy()});changed=True
 if changed:
  with path.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rs)
(W/'applied-old-flow-updates.json').write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n')
ifields=['entry_id','scope','entry_type','title','upstream_id','url','evidence_id','review_status','related_flow_ids','checked_on','gap']
inv=[]
for i,(t,fs,g) in enumerate([('Books','F-M030-01','中途读听切换与同页恢复'),('Videos','','播放控制/退出与恢复未拆'),('Create','F-M030-07;F-M030-08','作品编辑、权限失败、重装恢复'),('Offline','F-M030-03','离线范围更新与删除未核'),('Letters','','逐题交互/错误链未拆'),('Reading','F-M030-01;F-M030-02','题型与提示/结算未全拆'),('Math','F-M030-02;F-M030-04;F-M030-05','示例不覆盖全部数学题型'),('Logic+','F-M030-02','记忆/匹配/情绪等活动逐项未拆')],1):
 inv.append(dict(zip(ifields,[f'M030-LIB-{i:02d}','official library help, mobile build unknown','top_level_tab',t,'4403657703821',urls[2][2],'E-KHCD-03','document_text_read',fs,'2026-10-27',g])))
with (B/'khan-controls-deepening-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=ifields);w.writeheader();w.writerows(inv)
print(json.dumps({'new_evidence':len(rows),'new_flows':len(fr),'old_flows':len(old),'inventory_rows':len(inv),'selected_frames':len(selected)}))
