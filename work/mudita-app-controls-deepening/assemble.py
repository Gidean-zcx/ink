import csv, hashlib, json, re, copy
from pathlib import Path
from collections import Counter
ROOT=Path('/home/workspace/reader'); W=ROOT/'work/mudita-app-controls-deepening'; L=ROOT/'docs/research/interaction-library'
A=L/'artifacts/mudita-app-controls-deepening'; B=L/'batches'; A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows,fields):
 with p.open('w',newline='') as f:
  d=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');d.writeheader();d.writerows(rows)
captures=json.loads((W/'captures.json').read_text()); oldE=json.loads((W/'evidence-before.json').read_text()); oldF=json.loads((W/'coverage-before.json').read_text())
for p in L.rglob('*.jsonl'):
 assert not re.search(r'E-MACD-\d+',p.read_text()), 'new prefix already used'
assert len(captures)==18
meta={
22:('阻止未知/隐藏号码及解封','Hidden/unrecognised; manual blocked list; incoming call; unblock','F-M012-01','隐藏与不在联系人中的号码两类；Recents设置阻止、Contacts设置减号解封；解除后历史短信补收未知'),
25:('联系人短信拆分帮助','Steps 1–5; keyboard note','F-M012-02','Pencil新建；与旧汇总正文重复，不作独立实测'),
26:('新号码短信拆分帮助','Steps 1–6; keyboard note','F-M012-02','Plus新建并确认Send to对象；与旧汇总正文重复'),
27:('联系人MMS拆分帮助','Steps 1–6','F-M012-02','Plus与Image附件；没有送达、压缩或失败规则'),
28:('新号码MMS拆分帮助','Steps 1–7','F-M012-02','确认号码后加图发送；不因此保存为联系人'),
31:('Notes新建与多选删除','How to create notes; How to delete notes','F-M012-13','首条另有Create new note；标题上限54字符；Done与二次Delete；无回收说明'),
44:('Recorder录制、停止、命名保存','Steps 1–7','F-M012-14','Pause另列；第一次Done停止，命名后的Done保存；未说明暂停后恢复键'),
45:('Recorder回放与关闭画面','Steps 1–6','F-M012-14','Menu选录音、Play、Done关闭录音画面；不推定关闭就停止音频'),
46:('Recorder单个及批量删除','Single recording; many recordings','F-M012-14','单个Bin/Delete；多选Pencil/checkbox/two Delete；未说明回收或恢复'),
47:('机内联系人创建、删除及阻止','Add/delete/block/unblock contacts','F-M012-11;F-M012-01','Create、Pencil/Bin/Delete contact；解封段中间步骤仍写want to block，末键明确Unblock contact'),
48:('Center批量删除联系人','Steps 1–7; select-all tip','F-M012-11','勾选后两次Delete；顶部减号样式复选框全选；不同于阻止列表的减号解封'),
52:('Center传入音乐及格式筛选','Steps 1–6; Important','F-M012-06','Music/Add file/确认；不支持格式不能选择或上传；无格式清单与播放验证'),
54:('Center文件多选/全选删除','Steps 1–7','F-M012-07','类别称Manage(Music,Photos,E-books)；全选复选框、Delete/Delete file；无回收政策'),
56:('指纹添加与删除的口令前置','How to add fingerprints; How to delete fingerprints','F-M012-16','Security/Manage Fingerprints需passcode；震动后换手指部位；无最终完成弹窗/失败重试规则'),
58:('全局与逐App定位共享','All location sharing; individual apps','F-M012-15;F-M012-08','Location Sharing总开关或选App后选等级；等级名称/优先级/其他权限未说明'),
60:('屏幕休眠超时入口','Screen timeout steps 1–5','','Display/Screen Timeout/选时长；未列选项或默认值，不证明后台录音/播放停止'),
91:('忘记PIN/图案后的有损恢复','Factory reset procedure','F-M012-09','官方仅给Recovery恢复出厂；未备份资料丢失；无无损找回承诺'),
95:('Center联系人导入与管理综合帮助','CSV/vCard; Google; Outlook; source exports; delete','F-M012-12;F-M012-11','可选部分/全部再Import selected contacts；传输勿断线；导入不是持续同步，去重/覆盖未知')}
notes='''# M012 核心 App 帮助：范围与歧义

2026-10-27，固定 Mudita Kompakt；已取得的系统基线仍为 K1.6.1、Center发布基线4.0.0。18篇新增正文均未写对应构建，不能把页面修改日当固件号。本轮只作匿名普通GET及本地阅读，没有设备/账号操作、实际发送、安装或音频获取，hands_on=false。没有新增静态观察；HTML中的网站装饰图未看，不标visual。

## 取得方式与目录范围

18个原目录URL均HTTP200，正文原件及提取在work；本轮实际HTML均显示2026-10-03修改。web缓存曾把Notes/Contacts显示为2025-08-08，正式记录采用已保存HTML，不据缓存日期推定版本。原111项仍完整保留，18项由index_only改body_read；剩余46项仅目录。联系人导入的6个拆分URL本轮未读取；综合帮助虽包含同题，仍保留拆页未读状态，注明相关正文依据。

SMS/MMS四个拆页与旧E-MDCD-06汇总内容重复，Center删除联系人拆页与综合页重复；本轮补的是具体URL阅读状态，不称独立观察或再计同一F。目录和正文段落也不是功能完成率。

## 关键对象和限制

| E | 本轮可核的范围 | 未得到的结果 |
| --- | --- | --- |
| E-MACD-01/10/11/18 | 电话阻止、删除联系人和Center导入是不同动作。阻止规则覆盖来电及消息；Contacts解封用减号，Center全选的减号是复选框。 | 解封后补收、删除传播与导入合并/去重。机内解封段选择对象一句仍写want to block，保留文案疑点。 |
| E-MACD-02—05 | 联系人SMS用Pencil，新号码SMS及MMS用Plus；先确认收件号码再选图。 | 不能凭Send推定送达、已读或新增联系人；没有离线队列、取消/草稿和失败重试。 |
| E-MACD-06 | Notes分首次入口、标题/正文和Done；删除有多选与二次确认。 | 未说自动保存、退出未保存草稿、撤销或回收。旧K1.6附件p4的Task list仍只是增量声明。 |
| E-MACD-07—09 | 录制先Done停止，命名后再Done保存；回放Done被文字定义为关闭画面；单删与批删分别列。 | 暂停后恢复键、保存前离开、关闭后播放状态、来电/锁屏、导出及误删恢复未核。K1.6附件p5只说save and leave弹窗排版修复，不补造按钮。 |
| E-MACD-12/13 | 文件选择/上传筛选、类别多选及确认删除。 | 文件可选择不等于解码/入库成功；没获得回收、断线半文件及播放记录的规则。 |
| E-MACD-14/17 | 指纹管理先输passcode；删除指纹有确认。忘PIN/图案FAQ另给清空恢复。 | 指纹能否绕过忘口令、录入失败次数、删除末枚后行为不明；恢复出厂不是密码找回。 |
| E-MACD-15/16 | 定位总开关与逐App等级入口、显示超时入口。 | 未列定位等级、默认超时、保存/返回反馈；不能扩大为通用权限管理或后台音频规则。 |

正式案例保留任务步骤，目录保留文章与定位，原件只在work。上述全部为官方文档描述，不是实机结果；没有时间戳、延迟、复现次数或当前构建的性能结论。
'''
note_path=A/'source-notes.md'; note_path.write_text(notes)
newE=[]; bynum={}
for i,c in enumerate(captures,1):
 n=int(c['inventory_id'].rsplit('-',1)[1]); title,loc,fs,note=meta[n]; eid=f'E-MACD-{i:02d}';bynum[n]=eid
 assert not c['existing_evidence_ids']
 newE.append({'evidence_id':eid,'product_id':'M-012','title':title,'url':c['url'],'checked_on':'2026-10-27',
 'source_type':'official_help','access_mode':'anonymous_http_get_html_body_read','version_scope':'Mudita Kompakt only; K1.6.1 and Center4.0.0 are previously obtained release baselines, not the unversioned help UI build; Pure excluded',
 'app_build':None,'firmware_version':None,'source_updated_on':'2026-10-03','locator':loc,'note':note,'hands_on':False,'visual_observed':False,
 'archived_artifact':str(note_path.relative_to(L)),'sha256':sha(note_path),'capture_state':'raw_source_local_with_independent_short_note','supersedes':None,
 'raw_work_path':c['raw_work_path'],'raw_sha256':c['raw_sha256'],'raw_text_path':c['text_path'],'raw_text_sha256':c['body_sha256']})
(B/'mudita-app-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in newE))
newF=[]
def flow(n,task,es,known,gaps):
 newF.append(dict(flow_id=f'F-M012-{n:02d}',product_id='M-012',task=task,depth='D2',evidence_ids=';'.join(es),known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-012-mudita.md',checked_on='2026-10-27'))
flow(11,'创建及删除联系人（机内/Center）',[bynum[47],bynum[48],bynum[95]],'机内Add contact/Create；Pencil/Bin/Delete contact；Center勾选或全选并两次Delete；阻止另见F01。','编辑保存、数据存储位置、误删回收/恢复、删除与历史消息关联未知。')
flow(12,'选择联系人来源并导入Kompakt',[bynum[95]],'Center Contacts/Import contacts；CSV或vCard文件、Google、Outlook三路；选择部分/全部后Import selected contacts；过程勿断线。','不等于持续同步；授权细目、重复/字段映射、失败重试、完成反馈与远端删除传播未知。')
flow(13,'创建便笺并批量删除',[bynum[31],'E-MDCD-04'],'Apps/Notes；首条Create new note或Plus；54字符标题、Enter、正文、Done；Edit勾选后两次Delete。','退出草稿、自动保存/编辑、超长标题反馈、回收/恢复未知；Task list仅K1.6版本声明。')
flow(14,'录音保存、回听及删除',[bynum[44],bynum[45],bynum[46],'E-MDCD-04'],'录制Pause与Done停止分开；命名后Done保存。Menu选录音回放；Done仅说明关画面。单个Bin/Delete，批量Edit选择并二次Delete。','暂停续录、保存前离开、关闭画面是否停播、来电/锁屏、导出格式、误删恢复与备份范围未知。')
flow(15,'选择全局或逐App定位共享范围',[bynum[58]],'Control Center/Settings/Location Sharing；总开关或选择App再选共享等级。','具体等级名、总开关优先级、保存/退出、后台或精确定位、其他权限均未说明。')
flow(16,'添加和删除解锁指纹',[bynum[56]],'Settings/Security/Manage Fingerprints先输passcode；Plus后按提示录入，震动后换部位；Bin/Delete移除所选指纹。','最终录入完成确认、重复/失败次数、上限、末枚删除及失去口令时指纹作用未知。')
csvout(B/'mudita-app-controls-deepening-coverage.csv',newF,list(oldF[0]))
changes={
1:dict(evidence_ids=oldF[0]['evidence_ids']+';'+bynum[22]+';'+bynum[47],known_branches='原拨号/来电阻止保留；增加隐藏/非联系人号码来电和消息阻止、手动名单及减号解封；与删除联系人不同。',gaps='接听/拒接、占线/无服务/SIM选择、媒体打断、阻止反馈及解封后补收未知。'),
2:dict(evidence_ids=oldF[1]['evidence_ids']+';'+';'.join(bynum[n] for n in [25,26,27,28]),known_branches='四个拆页与原汇总重复；Pencil/Plus差异仍在；新号码MMS先Send to确认收件人再Image；Send不证明送达或建联系人。',gaps='草稿保留/取消、离线重试、送达/已读、MMS失败与附件保留未知。'),
6:dict(evidence_ids=oldF[5]['evidence_ids']+';'+bynum[52],known_branches='Center Music/Add file/确认复制；不支持格式不可选或上传。USB文件可见性另走USB Preferences，未连接不显示；No data transfer收回访问。',gaps='不假设Center依赖MTP；未给完整格式清单；重复、容量/断线、解码、入库反馈和断点未知。'),
7:dict(evidence_ids=oldF[6]['evidence_ids']+';'+bynum[54],known_branches='Center类别选文件；顶部减号样式复选框全选；Delete/Delete file。删APK不卸载应用；类别文案Manage与传入Manage Files分别保留。',gaps='删除后的回收/单项恢复、图书笔记和播放历史依赖、断线结果未知。'),
8:dict(evidence_ids=oldF[7]['evidence_ids']+';'+bynum[58],known_branches=oldF[7]['known_branches']+' 定位的全局/逐App入口另见F15，不能当全部应用权限已核。',gaps='兼容性、覆盖更新、后台/崩溃与数据保留未知；定位等级/撤销结果及其他权限仍缺。'),
9:dict(evidence_ids=oldF[8]['evidence_ids']+';'+bynum[91],known_branches=oldF[8]['known_branches']+' 忘PIN/图案FAQ也指向Recovery清空，未备份资料丢失。',gaps='安装中断/回滚、重置后SIM/eSIM细项及备份覆盖恢复未知；没有无损找回口令依据。')}
updates=[]
for n,ch in changes.items():
 before=oldF[n-1];after={**before,**ch};updates.append({'flow_id':before['flow_id'],'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':v} for k,v in ch.items() if v!=before[k]}})
dump(W/'old-coverage-updates.json',updates)
captures_old=[]
for eid,path,loc in [('E-MDCD-04','work/mudita-controls-deepening/release-1.6.pdf','PDF p2 Contacts; p4 Notes; p5 SMS/Voice Recorder; local text reread only'),('E-MDCD-06','work/mudita-controls-deepening/messaging.html','Send SMS/MMS contact/new number; delete conversations; compare dedicated articles')]:
 e=next(e for e in oldE if e['evidence_id']==eid)
 captures_old.append({'evidence_id':eid,'expected_url':e['url'],'before_additional_captures':e.get('additional_captures',[]),'append_additional_captures':[{'checked_on':'2026-10-27','access_mode':'existing_local_source_text_reread_no_network','locator':loc,'raw_work_path':path,'raw_sha256':sha(ROOT/path),'hands_on':False,'visual_observed':False,'note':'本轮复读旧原件；没有新请求或新修订；限定与新帮助对照见独立摘记。','archived_artifact':str(note_path.relative_to(L)),'sha256':sha(note_path)}]})
dump(W/'old-evidence-additional-captures.json',captures_old);dump(W/'old-evidence-field-updates.json',[])
inventory=list(csv.DictReader((L/'artifacts/mudita-controls-deepening/function-inventory.csv').open()))
controls={22:'隐藏/非联系人开关；手动名单；来电阻止；名单减号解封',25:'Pencil/选联系人/Send；光标呼出键盘',26:'Plus/Send to确认/Send；光标',27:'Plus/选联系人/Image/Send',28:'Plus/号码/Send to/Image/Send',31:'首次新建/Plus；标题54字符/Enter/正文/Done；Edit/多选/两次Delete',44:'Record/Pause/Done停止/New recording标题/Done保存',45:'Menu/选录音/Play/Done关闭画面',46:'单个Bin/Delete；Edit多选/Delete再确认',47:'Add contact/Create；Pencil/Bin/Delete contact；Block/Unblock',48:'勾选/多选/全选复选框/两次Delete',52:'Music/Add file/选择/确认；不支持格式不可选或上传',54:'类别/勾选/全选/Delete/Delete file',56:'Security/Manage Fingerprints/passcode；Plus/震动换部位；Bin/Delete',58:'Location Sharing总开关；选App/共享等级',60:'Display/Screen Timeout/选择时长',91:'关机/Boot Mode/Recovery/清空确认；数据丢失',95:'文件/Google/Outlook三路导入；选择部分/全部；外部来源导出三段；Center删除'}
for r in inventory:
 n=int(r['inventory_id'].rsplit('-',1)[1]); r['read_basis']='inherited_prior_capture_not_refetched';r['covered_controls']='';r['body_updated_on']='';r['checked_on']='2026-10-27'
 if n in meta:
  r.update(reading_status='body_read',flow_ids=meta[n][2],evidence_ids='E-MDCD-02;'+bynum[n],locator=meta[n][1],gap=meta[n][3],read_basis='current_body_read_anonymous_GET',covered_controls=controls[n],body_updated_on='2026-10-03')
 if 38<=n<=43:
  r['gap']='本URL仍只读目录；同题已见综合帮助E-MACD-18，不能算此页正文已读；未登录外部来源。'
 if n in [100,105,108,110]:
  r['read_basis']='existing_release_text_reread_no_network'; r['flow_ids']={100:'F-M012-11;F-M012-12',105:'F-M012-13',108:'F-M012-01;F-M012-02',110:'F-M012-14'}[n]
csvout(B/'mudita-app-controls-deepening-inventory.csv',inventory,list(inventory[0]))
dump(W/'inventory-summary.json',{'rows':len(inventory),'states':dict(Counter(r['reading_status'] for r in inventory)),'newly_read_article_rows':18,'new_controls_are_not_new_flows':True})
dump(W/'new-evidence-map.json',bynum)
print('assembled',len(newE),'E',len(newF),'F',len(updates),'old F',sum(len(x['changed_fields']) for x in updates),'fields',len(inventory),'inventory')
