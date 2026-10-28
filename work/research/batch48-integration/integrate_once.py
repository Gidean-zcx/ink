import csv,hashlib,json,shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/research/batch48-integration';S=R/'work/research/batch48-maimemo-device';A=L/'artifacts/maimemo-device-controls'
DATE='2026-10-27';URL='https://memodocs.maimemo.com/docs/support_wordmachine/'
SCOPE='MM-WM01 official help and static illustrations; installed firmware unknown; App V5.6.06 (0906) RLC visible only in one sample'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
assert not (B/'maimemo-device-controls-evidence.jsonl').exists(),'Do not rerun'
req='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087';assert sha(R/'docs/requirements.md')==req
http=json.loads((S/'fetch.json').read_text());assert http['status']==200 and sha(S/'device.html')==http['sha256']
captures=json.loads((S/'image-fetch.json').read_text());assert len(captures)==24
keep=[1,2,3,4,5,9,10,11,12,14,15,16,17,18,19,20,21,22,23,24,25]
observations={
1:'版本局部为V5.6.06 (0906) RLC；不能确认iOS门槛5.60.7或设备固件。',
2:'手机WM01管理页：换书、合并数据及示例4天前，底部v0.1.1对象未明；非同步完成记录。',
3:'添加单词机词书的云词本页，含同名列表；未验证搜索、空集合或选中身份。',
4:'已选择词书与确认换书按钮；未见完成资源同步或设备打开结果。',
5:'实体键/接口位置示意，长短按职责是图中文字；未实际操作。',
7:'绑定概念拼图，非实际绑定过程，样例统计不作真实记录；不正式归档此图。',
8:'两篇论文首页拼图，非本机UI或学习效果实验；不正式归档此图。',
9:'答案未揭示的英中/中英示例，底部四键均显示答案；未观察按下。',
10:'同词释义/助记示例，底部三种自评；不是从09真实推进的记录。',
11:'总览模式、确认与入列表提示；箭头方向与正文按键映射不一致。',
12:'设置中英中/中英、合并/分开及两类例句提示；方向键选择和确认提示。',
13:'手机换书入口概念拼图，与02部分重复；不正式归档此图。',
14:'播放中确认键暂停、圆键设置；随机/顺序/循环、重复次数和无单位定时数字。',
15:'个人与艾宾浩斯曲线示例，没有测得效果或点选行为。',
16:'跨日学习图示含熟知/认识/模糊/忘记/待学，比正文当日三类描述更广。',
17:'持久度图有历史轴和阈值浮框；无交互记录及完整图例，不猜选中行为。',
18:'回顾列表及确认进入提示；底部箭头说明还有未显示项，不声称菜单穷尽。',
19:'回顾词卡含收藏星标和回顾模式文字，答案未揭示提示。',
20:'复习释义词卡含收藏星标及自评；不从19/20拼成真实模式切换。',
21:'详情示例同时收藏/熟知，圆与叉用于前后词；不等同此页记忆评价。',
22:'详情示例同时收藏/不想背，不证明所有标记组合优先级。',
23:'熟知确认弹窗，说明不再安排复习并有取消；未点按。',
24:'不想背确认弹窗，说明移出规划和选词排除；不是删除全部记忆。',
25:'收藏确认弹窗与取消；未证明取消结果或已同步。',
}
assert set(observations)=={r['index'] for r in captures}
obs=[];hashes={}
for r in sorted(captures,key=lambda x:x['index']):
 n=r['index'];p=R/r['path'];assert r['status']==200 and sha(p)==r['sha256']
 a=None
 if n in keep:
  target=A/p.name;shutil.copyfile(p,target);a=str(target.relative_to(L));hashes[a]=sha(target)
 obs.append(dict(**r,actual_view='view_image',observed_on=DATE,hands_on=False,continuous_sequence=False,archived_artifact=a,observation=observations[n]))
elements=json.loads((S/'elements.json').read_text());assert len(elements['images'])==26 and len(elements['headings'])==41
audit=dict(checked_on=DATE,source=URL,http=http,raw_work_path=str((S/'device.html').relative_to(R)),extracted_work_path=str((S/'device.txt').relative_to(R)),extracted_sha256=sha(S/'device.txt'),all_source_images=elements['images'],body_heading_count=41,images_viewed=24,images_archived=21,observations=obs,scope=SCOPE,web_locator_failure='Follow-up line open timed out; ordinary public HTTP obtained body; no access-control bypass')
dump(A/'capture-audit.json',audit);hashes[str((A/'capture-audit.json').relative_to(L))]=sha(A/'capture-audit.json')
notes=A/'source-notes.md'
e=dict(evidence_id='E-MMDS-01',product_id='M-023',title='MM-WM01官方静态控件：揭示/自评、确认、换书、听词和统计',url=URL,checked_on=DATE,source_type='official_help_static_image_bundle',access_mode='public_http_200_body_read_and_static_images_actually_viewed',version_scope=SCOPE,app_build=None,firmware_version=None,source_updated_on=None,locator='Q12及核心/更多功能；按原DOM图片顺序编号，21张正式静态附件',note='查看24图，保留21相关静态图；文图按键/统计范围差异保留。无设备操作、声音或连续录像。',hands_on=False,visual_observed=True,archived_artifact=str(notes.relative_to(L)),sha256=sha(notes),capture_state='static_images_and_observation_audit_archived',supersedes=None,derived_from='E-MMOH-01',raw_work_path=str((S/'device.html').relative_to(R)),raw_sha256=sha(S/'device.html'),visual_audit_path=str((A/'capture-audit.json').relative_to(L)),artifact_hashes=hashes)
(B/'maimemo-device-controls-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
owners=[]
for p in B.glob('*-evidence.jsonl'):
 rows=[json.loads(x) for x in p.read_text().splitlines()];changed=False
 for row in rows:
  if row['evidence_id']=='E-MMDVC-01':
   before=json.loads(json.dumps(row));row.setdefault('additional_captures',[]).append(dict(checked_on=DATE,access_mode='ordinary_public_http_200_body_read',raw_work_path=str((S/'device.html').relative_to(R)),raw_sha256=sha(S/'device.html'),extracted_work_path=str((S/'device.txt').relative_to(R)),extracted_sha256=sha(S/'device.txt'),locator='41正文标题对应全文；图片另见派生E-MMDS-01',hands_on=False,visual_observed=False,archived_artifact=str(notes.relative_to(L)),sha256=sha(notes),note='保留公测/未来能力、iOS版本格式与文图按键方向差异。'));owners.append(dict(owner=str(p.relative_to(R)),before=before,after=row));changed=True
 if changed:p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows))
assert len(owners)==1;dump(W/'old-evidence-updates.json',owners)
path=B/'vocab-coverage.csv';flows=list(csv.DictReader(path.open()));updates=[]
details={
1:('补手机云词本选择→已选→确认换书静态步骤；尚未证明资源到设备可读。','配网/二维码失败；同名/空云词本；确认后下载/首题；iOS版本疑点'),
2:('答案揭示前四键可显示答案，揭示后才三类自评；学习提示设置和日量确认补静态图，方向箭头与正文映射不一致。','同构建真实按键映射；误评撤销/重排；空任务；结算；提示设置保存'),
3:('长按说明配套熟知/不想背/收藏确认弹窗；详情圆/叉切词，不作自评；状态可并列示意。','确认/取消后状态；取消标记排期；组合优先级；同步和恢复'),
4:('回顾列表→确认进入，回顾不记反馈；听词页确认键暂停、圆键设置，另有顺序/重复/定时静态选项。','列表全部项目；空队列；定时单位/到期动作；倍速入口；缺媒体；后台/重启断点'),
5:('手机合并数据与设备同步仍分步；管理页4天前是静态样例，不能当本次同步成功。','冲突/失败和重试；未同步解绑；已清除记忆重传；换机恢复'),
}
for row in flows:
 if row['product_id']!='M-023':continue
 n=int(row['flow_id'].split('-')[-1]);before=dict(row);known,gaps=details[n]
 row['evidence_ids']=';'.join(dict.fromkeys((row['evidence_ids']+';E-MMDS-01').split(';')));row.update(known_branches=known,gaps=gaps,visual_status='static_reference_only',checked_on=DATE)
 updates.append(dict(before=before,after=dict(row)))
assert len(updates)==5;csvout(path,flows);dump(W/'old-flow-updates.json',updates)
new=dict(flow_id='F-M023-07',product_id='M-023',task='查阅并区分三类学习统计（入口/交互待补）',depth='D1',evidence_ids='E-MMDVC-01;E-MMDS-01',known_branches='官方静态图分别示遗忘曲线、跨日学习情况、持久度；历史反馈/未来待学/模型估计不混为掌握。',gaps='统计入口、时间窗/点选、图例全文、空数据、重复反馈口径、刷新/保存；文图范围差异；学习效果未验证',visual_status='static_reference_only',hands_on='false',case_path='cases/M-023-maimemo-device.md',checked_on=DATE)
csvout(B/'maimemo-device-controls-coverage.csv',[new])
# The source headings remain source headings, not new feature/task counts.
qmap={1:[1],2:[1],3:[1,5],4:[2],5:[2],6:[3],7:[3,5],8:[5],9:[6],10:[],11:[4],12:[1],13:[],14:[1,5],15:[5],16:[5],17:[1],18:[6],19:[],20:[5]}
headmap={23:[],24:[2,3,4,6],25:[1],27:[],28:[2],29:[1,2],30:[4],32:[7],33:[7],34:[7],35:[4],36:[3],37:[2,3,4],38:[],40:[6],41:[6]}
inv=[];parents={};groups={1,22,26,31,39}
for i,h in enumerate(elements['headings'],1):
 level=int(h['tag'][1]);parent=next((parents[k] for k in range(level-1,1,-1) if k in parents),'');item=f'MMDH-{i:02d}';parents[level]=item
 for k in list(parents):
  if k>level:del parents[k]
 nums=qmap[i-1] if 2<=i<=21 else headmap.get(i,[])
 status='source_group_not_task' if i in groups else 'body_read_partial_static_or_documented'
 inv.append(dict(entry_id=item,parent_id=parent,entry_type='group' if i in groups else 'topic',title=h['text'],url=URL+'#'+h['id'],scope=SCOPE,evidence_ids='E-MMDVC-01;E-MMDS-01',related_flow_ids=';'.join(f'F-M023-{n:02d}' for n in nums),review_status=status,gaps='正文/相关静态图已读，不等于固件完整功能；具体缺口见案例。签到/蓝牙未来声明及非交互内容不升流程。',checked_on=DATE))
csvout(B/'maimemo-device-controls-inventory.csv',inv)
rpfile=L/'reference-points.csv';rp=list(csv.DictReader(rpfile.open()));assert len(rp)==98 and not any(r['product_id']=='M-023' for r in rp)
data=[
('学习;按键;揭示;自评',[2],9,'核心功能/今日复习，图09/10','答案未显示或已显示，模式明确','按对应显示/自评键','静态底部提示随阶段变化；未实际按键','排期写入及误评撤销未知','双击;长按;提示后评分;结算'),
('学习;收藏;熟知;确认;取消',[3],23,'更多功能/设置单词状态，图23–25','处于支持状态标记的词卡/详情','按文档长按，再处理配套确认窗','静态窗分取消和确认，并说明影响','确认写入/取消无变化均未实际验证','默认焦点;误按;标记取消;排期恢复'),
('学习;键位;上下文;详情',[3,4],21,'更多功能/状态，图18–22','详情页或回顾/复习卡','圆/叉切词或三键自评，依底部提示判断','详情示例圆/叉为前后词；可有多个标记','翻词不证明撤销前次评价','状态优先级;返回;跨模式'),
('学习;计划;设置;文图冲突',[2],11,'词书总览及Q4，图11/12','合并或分开模式已明确','调日量并确认；另设例句提示','静态图有确认控件；箭头方向与正文不一致','保存及当日重排未观察','物理键映射;低于完成量;退出;重启'),
('听词;暂停;计时;循环',[4],14,'随身听，图14和按键说明','播放页或随身听设置页','确认键暂停，圆键入设置，方向键选后确认','静态显示顺序/循环、重复和定时；数字无单位','时间到/退出/关机后的播放状态未知','定时单位;倍速入口;缺音频;外设变化'),
('导入;云词本;确认;同步',[1,5],4,'Q12，图01–04','手机设备管理页，版本范围见来源','更换词书→云词本→选择→确认换书','静态步骤只到确认；设备资源与学习数据同步另核','没有设备首题/离线可读完成证据','同名词本;取消;空本;资源更新失败;版本格式'),
('学习;统计;预测;能力',[7],16,'更多功能/三统计，图15–17','已积累学习记录，统计入口未知','查阅不同图表，实际点选未取得','静态历史反馈、未来待学、模型估计不能互作掌握证据','没有图表更新/导出/重装恢复记录','入口;时间窗;空数据;去重口径;效果测量'),
('学习;回顾;复习;副作用',[2,4],19,'更多功能/单词回顾，图18–20','正式复习与回顾模式分别明确','按列表进入，再揭示/查看词卡','文档回顾不记反馈，图19有回顾模式；未实测调度','模式切换及同期排期对照未知','退出续接;长按标记是否仍写状态;误评'),
]
for i,(tags,nums,img,loc,pre,act,result,persist,gap) in enumerate(data,1):
 rp.append(dict(reference_id=f'RP-M023-{i:02d}',product_id='M-023',question_tags=tags,flow_ids=';'.join(f'F-M023-{n:02d}' for n in nums),evidence_ids='E-MMDVC-01;E-MMDS-01',source_locator=loc,artifact_path=f'artifacts/maimemo-device-controls/image-{img:02d}.png',precondition=pre,action=act,visible_result=result,evidence_basis='official_text_and_actually_viewed_static_illustration_no_operation',persistence_result=persist,unknown_branches=gap,version_scope=SCOPE,checked_on=DATE))
csvout(rpfile,rp)
dump(W/'integration-result.json',dict(new_flows=1,new_depth='D1',updated_flows=5,new_evidence=1,existing_evidence_capture=1,new_reference_points=8,source_heading_rows=41,images_viewed=24,images_archived=21,requirements_sha256=req))
print('Integrated 1 D1 statistics task, 5 existing tasks, 1 static E, 1 existing capture, 8 RP, 41 source headings, 21 archived images.')
