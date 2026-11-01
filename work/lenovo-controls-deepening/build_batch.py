from pathlib import Path
import csv,json,hashlib,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/lenovo-controls-deepening';B=L/'batches';A=L/'artifacts/lenovo-controls-deepening'
assert not (B/'lenovo-controls-deepening-coverage.csv').exists(),'one-time builder'
A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jw(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def cw(p,x):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(x[0]));w.writeheader();w.writerows(x)
oldF={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open()) if r['product_id']=='M-062'}
oldE={r['evidence_id']:r for r in map(json.loads,(L/'evidence.jsonl').read_text().splitlines()) if r['product_id']=='M-062'}
assert len(oldF)==8 and 'E-LNCD-01' not in (L/'evidence.jsonl').read_text()
pdf=R/'work/research/extension-manuals/lenovo-smart-paper.pdf';pdfurl=oldE['E-LNP-01']['url']
assert sha(pdf)=='b30235ca552e3e7b4f6f038203e2e2b0c3e0842ee06079438606e2d8405e5ef4'
for n in (9,10):shutil.copyfile(W/f'guide-p{n}.png',A/f'guide-p{n}.png')
data=[
(9,'跳过联网或账号完成初次设置','选择语言→同意条款→选WLAN或Skip→惯用手→Lenovo ID登录或Skip→Get started。','网络与账号可后补，入口分别在Settings > WLAN / Lenovo ID；同步可现在或稍后选择。','只证明向导允许跳过，不能推导离线/无账号下全部功能可用；本地内容与后续账号绑定、取消条款未知。','E-LNP-01','3'),
(10,'创建并修改笔记本','主页新建图标→命名、选封面与模板→右上勾号打开；笔记本设置可改名和封面。','创建参数包含模板；后续设置只明确名称/封面，不能据此说已有笔记模板一定可整体更换。','同名、空名、模板切换影响、保存时机和创建取消未知。','E-LNP-01;E-LNCD-01','9–10'),
(11,'选取笔迹并撤销近期编辑','笔记工具栏选择工具、笔触/颜色、橡皮类型；Selection圈选；Undo/Redo处理最近编辑。','指南限最近十步。页管理和图层入口列有增删、移动、合并、隐藏，但未说明哪些动作进入同一撤销历史。','跨页/退出/重启后历史、页或层删除恢复、合并拆回及圈选后的全部菜单未知；不能套用其他设备规则。','E-LNP-01;E-LNCD-01','9'),
(12,'分享笔记或将手写转换为文字','Share→邮件分支选择页与导出类型；Convert to text分支先选目标语言→转换→邮件发送。','不清晰手写可能识别错误；说明只支持文字转换，不能推出公式/图表结构化。导出副本和原笔记是不同对象。','识别结果校订入口、离线/服务额度、取消发送、邮件失败及原笔记是否改写均未说明。','E-LNP-01;E-LNCD-01','10'),
(13,'整理笔记与区分回收和彻底删除','Notebook列表长按条目→改名/复制/分享/移动/删除/收藏；主页Trash中可还原或永久删除文件。','手册证明两个生命周期入口；未逐步证明每一类删除对象都进入Trash，不能把删除录音或单页等同删文件。','Trash保留期限、原位置冲突、确认弹窗、云端删除传播，以及复制是否包含录音关联未知。','E-LNP-01','5、11'),
(14,'在书内找页并管理书签','打开书→Overview查看页面或跳指定页；Search输入关键词；书签按钮添加，再点取消，右上入口管理本书标记页。','搜索内容与首页文件搜索不同。显示菜单随格式变化，PDF调边距与文本排版分别列出。','跳转后返回历史、无命中反馈、书签同步、PDF边距是否写回源文件未知。','E-LNP-01','12–13'),
(15,'管理本地书与识别在线书限制','Library列表长按本地书→改名/复制/分享/移动/删除/收藏；书城提供预览，登录后购买下载到Library。','指南明确在线书不能执行前述长按管理动作；不能假设所有Library条目都有同一套能力。','预览、已购下载、DRM与离线资格的进一步划分未给；重复文件、删除恢复及内容授权未知。','E-LNP-01','13'),
(16,'设置锁屏内容与自动停机','设置锁屏内容选壁纸/时钟/月历或年历/Quick Sheets；Battery配置Auto sleep、Auto power off与Smart Cover；Security改或删密码。','Quick Sheets显示最近浏览页；时钟每分钟后台刷新。2023指南自动关机为启用后闲置十天，Auto sleep是锁屏；改/删密码须旧密码。','锁屏是否受密码遮盖、忘记密码恢复、各计时范围和版本差异未核；电池保护/维护模式仅有名称。','E-LNP-01','16–17'),
(17,'从快捷设置向其他屏幕投影','顶部下滑→先使目标显示端启用Miracast并接同一Wi-Fi→点设备Miracast→按屏幕提示连接。','笔记与阅读分享也有Miracast入口；投影是画面呈现，不证明可编辑导出或可远程控制。','选设备每屏、断线/重连、锁屏后状态、帧率及笔画延迟未说明。','E-LNP-01','7、10、12'),
(18,'检查应用状态并辨认安装和清理后果','FAQ给Settings > General > App info，可打开、停用、强停和清数据/缓存；导入APK的入口承诺另列。','各动作作用不同；定制WPS卸载后不能按普通方式重装。不能把APK可导入当完整应用恢复方案。','安装确认/权限/错误、清理范围、卸载数据保留、强停恢复均无连续步骤或实测。','E-LNP-02','HT514875 Q11、19、29（搜索索引）'),
(19,'文件突然不可见时核对登录身份','FAQ Q32描述突然文件消失时，检查Settings > Lenovo ID，再登录原账号。','该帮助把所述故障解释为账号导致隐藏，并称重登原账号后重新出现；这是特定排查分支，不能诊断一切丢失。','本地文件与账号绑定规则、访客/换号可见性、损坏/误删/云过期均未被该说明解决。','E-LNP-02','HT514875 Q32（搜索索引）'),
]
flows=[]
for n,task,steps,branch,gap,eids,pages in data:
 flows.append(dict(flow_id=f'F-M062-{n:02}',product_id='M-062',task=task,depth='D2',evidence_ids=eids,known_branches=branch,gaps=gap,visual_status='static_manual_pages_only' if 'E-LNCD' in eids else 'not_observed',hands_on='false',case_path='cases/M-062-lenovo-paper.md',checked_on='2026-10-27'))
revisions={
1:('Wi-Fi/蓝牙短按开关、长按分别进入网络/配对；音量必须先接蓝牙音频。系统边缘导航与内部翻页分开；刷新模式有入口但无波形或刷新率参数。','触发阈值/误触、刷新质量、快捷控制持久化和实际音频重连未知。','E-LNP-01;E-LNP-04'),
3:('可加速回放；录音列表长按改名/删除；Note replay显示对应手写内容。播放需蓝牙音频；图标表不是连续回放观察。','离开笔记/断电保存、删除音频后的笔迹关系、转写、精确位置和跨端同步未知。','E-LNP-01;E-LNCD-01'),
4:('选词还可高亮、复制或Wiki搜索；搜索/翻译/朗读需网络。词级窗口不等于整本听书或持久续播。','词典是否全离线、资格/付费、选词失败与读听断点未知。','E-LNP-01'),
7:('限定Notebook及Library/指定格式文件；有效云空间、联网，锁屏自动或文件页手动同步。配套App导入与设备同步是不同入口。','过期/满空间/冲突/刷机恢复、录音与进度覆盖未核；Q32账号隐藏另见F-M062-19，不当通用恢复。','E-LNP-02'),
8:('2023指南路径含General且清内部数据后自动重启；FAQ简写Settings > Factory Reset，不能据简写判定界面已改版。启用同步不证明最后编辑已上传。','完整恢复、上传完成回执与失败/断电回滚未知；无重置实测。','E-LNP-01;E-LNP-02')}
updates=[]
for n,(branch,gap,eids) in revisions.items():
 row=oldF[f'F-M062-{n:02}'];keys=['known_branches','gaps','evidence_ids','checked_on'];after=dict(known_branches=branch,gaps=gap,evidence_ids=eids,checked_on='2026-10-27')
 if n==3:keys.append('visual_status');after['visual_status']='static_manual_pages_only'
 updates.append({'flow_id':row['flow_id'],'before':{k:row[k] for k in keys},'after':after})
# Inventory records actual documented headings and controls; counts include parents.
inv=[]
def add(kind,title,locator,fnums=(),e='E-LNP-01',gap='仅文档；保存、取消、异常与实机反馈未逐控件验证'):
 inv.append(dict(entry_id=f'M062-LNCD-{len(inv)+1:03}',entry_type=kind,title=title,source_url=pdfurl if e=='E-LNP-01' else oldE[e]['url'],locator=locator,evidence_ids=e,flow_ids=';'.join(f'F-M062-{n:02}' for n in fnums),review_status='功能正文已读' if kind!='printed_toc' else '印刷目录核对；含父项与非功能附录',gap=gap))
toc=[('Getting Started',3,[9]),('Icons',3,[]),('Setup wizard',3,[9]),('Basics',4,[1,2]),('Home screen',4,[10,13]),('Quick settings',6,[1,17]),('Gestures',8,[1]),('Files transfer',8,[2]),('Force restart',8,[]),('Notebooks',9,[3,10,11,12,13]),('New Notebook',9,[10]),('Notebook instructions',9,[11,12]),('Recording',10,[3]),('More actions',11,[13]),('Reading',12,[4,5,14,15]),('Library',12,[14,15]),('Bookstore',13,[15]),('Apps',14,[18]),('E-mail',14,[12]),('Google Drive',14,[6]),('Settings',15,[7,8,16]),('Lenovo ID',15,[7,19]),('Wireless connection',15,[1]),('Sound',15,[1,3]),('Display',15,[1]),('General',16,[8,18]),('Lock screen',16,[16]),('Security',17,[16]),('Battery',17,[16]),('Accessibility',17,[]),('Appendix',18,[])]
for t,p,fs in toc:add('printed_toc',t,f'目录p2；正文p{p}',fs,gap='目录父子项重叠；附录仅定位，未作为功能闭环')
groups=[
(5,[10,13],'Show/Hide toolbar|Quick Sheets|Add new Notebook|Create new folder|Filename / keyword search|Favorites|Trash|Downloads|Notebook|Library'),
(7,[1,17],'Wi-Fi|Bluetooth|Screen rotation|Miracast|Screen refresh modes|Brightness and color temperature|Adaptive brightness|Volume'),
(9,[10,11],'Notebook name / cover / template|Show/Hide taskbar|Writing tools|Stroke|Eraser|Selection|Undo and redo|Recording|Page|Layer'),
(10,[3,10,12],'Share: Send by e-mail|Share: Convert to text|Share: Miracast|Share: Google Drive|Notebook settings|Record|Play|Recording list|Note replay'),
(11,[13],'Rename|Duplicate|Share|Move|Delete|Favourites'),
(12,[5,14],'Immersive Reading|Study mode|Overview|Share: Send by e-mail|Share: Miracast|Share: Google Drive|Adjust view-PDF files|Text settings'),
(13,[4,14,15],'Add Bookmarks|Search|Notes|Word selection window|Library management|Bookstore preview / download'),
(14,[6,12],'Apps > Email|iCloud e-mail setup|Google Drive sign-in|Google Drive bullet / grid view|Google Drive file download'),
(15,[1,7],'Lenovo ID Login|Cloud sync|Service status|WLAN|Bluetooth / Device name|Sound|Display'),
(16,[8,16],'System updates / Check for updates|Use 24-hour format|Use network-provided time|Time zone|Languages|Keyboards|Manage on-screen keyboards > Language|Manage on-screen keyboards > Input|Factory Reset|Lock screen: Wallpaper|Lock screen: Clock'),
(17,[16],'Lock screen: Calendar|Lock screen: Quick sheets|Security|Battery percentage|Auto sleep|Auto power off|Battery saver|Smart Cover mode|Battery optimization|Dominant hand|Font size')]
for p,fs,ts in groups:
 for t in ts.split('|'):add('documented_control_or_group',t,f'PDF p{p}',fs)
for q,t,fs in [(11,'APK import',[18]),(19,'App info',[18]),(29,'Customized WPS removal',[18]),(32,'Files hidden by account state',[19]),(46,'Cloud file scope',[7])]:
 add('faq_selected_topic',t,f'HT514875 Q{q}；本轮官方区域索引',fs,'E-LNP-02','仅相关FAQ选题，不是51题完整目录；区域日期/构建不统一')
cw(B/'lenovo-controls-deepening-inventory.csv',inv);cw(B/'lenovo-controls-deepening-coverage.csv',flows)
notes='''# Lenovo SP101FU：本轮证据说明

查阅2026-10-27。复用已取得的官方2023指南，SHA保持不变。逐页读p3–17功能正文，核p2印刷目录；附录p18–23仅背景/定位，没有把全书23页称为逐功能实测。目录含31章/节父子项；正文控件与FAQ选题另列，并非独立功能完成数。

## 原件与观察

- [官方指南](https://download.lenovo.com/consumer/mobiles_pub/lenovo_smart_paper_ug_en_2023.pdf)：手势、笔记、阅读、云入口和设置依据。原PDF及抽取文字在work；本轮没有重新联网下载。
- E-LNCD-01实际查看p9/10两张静态渲染页：p9创建以右上勾号提交，工具栏图标表列圈选、撤销/重做、页与层；p10分享分支和录音控制图标可见。这不是完整编辑页面，更不是执行前后图。
- [HT514875官方FAQ](https://support.lenovo.com/ca/en/solutions/ht514875-lenovo-smart-paper-lenovo-sp101fu-frequently-asked-questions-faqs)：旧直接路径403未重试。本轮普通公开搜索得到LB/IN等区域索引，只作为同FAQ的补充采集；未另行请求区域原站，不视为已打开正文。CA日期仍不能赋给其他区域。

## 可复用的差别

手册给出的十步撤销，没有说明能撤回哪些页/图层动作；圈选范围和复制范围也不能互换。录音有独立改名/删除入口，删录音不等于删笔记。锁屏可以呈现最近的速记页，时钟每分钟刷新；自动锁屏、闲置十天关机和保护套锁屏是三种条件。阅读在线书与本地文件管理不同，文档菜单又受格式约束。

FAQ补足App info入口和账号导致文件隐藏的特定排查。它们没有给出完整安装、卸载恢复或云过期方案。未实际登录、安装、录音、发送、删除、重置或连接设备。

## 保留的未知

图层合并和页删除回退、识别结果校订、录音/笔迹关联持久化、回收站时限、跳转历史、账号换入换出、云端冲突/过期/满空间、应用权限与后台状态仍不完整。文档旧规则不当当前固件事实；本轮没有增加用户需求。
'''
(A/'source-notes.md').write_text(notes)
note_rel=str((A/'source-notes.md').relative_to(L));note_sha=sha(A/'source-notes.md')
e=dict(evidence_id='E-LNCD-01',product_id='M-062',title='笔记创建、工具及录音控件静态页',url=pdfurl,checked_on='2026-10-27',source_type='official_manual',access_mode='existing_pdf_render_viewed',version_scope=oldE['E-LNP-01']['version_scope'],app_build=None,firmware_version=None,source_updated_on=None,locator='PDF p9/10实际查看；p9右上勾号和工具表，p10分享/录音表',note='控件说明表静态摘页，不是设备页面连拍或操作结果。',hands_on=False,visual_observed=True,visual_kind='static_manual_pages',derived_from='E-LNP-01',supersedes=None,archived_artifact='artifacts/lenovo-controls-deepening/guide-p9.png',sha256=sha(A/'guide-p9.png'),artifact_hashes={f'artifacts/lenovo-controls-deepening/guide-p{n}.png':sha(A/f'guide-p{n}.png') for n in (9,10)},raw_work_path=str(pdf.relative_to(R)),raw_sha256=sha(pdf),visual_audit_path=note_rel,visual_audit_sha256=note_sha,capture_state='selected_static_manual_pages')
(B/'lenovo-controls-deepening-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
caps=[{'evidence_id':'E-LNP-01','capture':dict(checked_on='2026-10-27',access_mode='existing_pdf_reread',capture_url=pdfurl,raw_work_path=str(pdf.relative_to(R)),raw_sha256=sha(pdf),text_work_path='work/lenovo-controls-deepening/guide-pages.json',text_sha256=sha(W/'guide-pages.json'),note='核p2目录及p3–17功能正文；附录不作已读功能链',note_artifact=note_rel,note_sha256=note_sha)},
{'evidence_id':'E-LNP-02','capture':dict(checked_on='2026-10-27',access_mode='official_search_index_only',capture_url=oldE['E-LNP-02']['url'],raw_work_path='work/lenovo-controls-deepening/search-index-b.txt',raw_sha256=sha(W/'search-index-b.txt'),note='普通公开搜索索引；本轮使用LB/IN区域Q11/19/29/32/46，CA更新日期仅属CA；未重试403或访问其他区域原站。',note_artifact=note_rel,note_sha256=note_sha)}]
jw(W/'existing-coverage-updates.json',updates);jw(W/'additional-captures-updates.json',caps)
case=L/'cases/M-062-lenovo-paper.md';s=case.read_text().replace('查阅：2026-10-26。','查阅：2026-10-26；2026-10-27补目录、控件及状态分支。',1)
lines=s.splitlines()
for i,line in enumerate(lines):
 for u in updates:
  if line.startswith('| '+u['flow_id']+' '):
   cols=line.split('|');cols[3]=' '+u['after']['known_branches']+' ';cols[4]=' '+u['after']['evidence_ids'].replace(';','、')+' ';lines[i]='|'.join(cols)
s='\n'.join(lines)+'\n'
addition='''## 2026-10-27：逐项目录与补充任务

已核2023指南p2目录及p3–17功能正文；[目录台账](../batches/lenovo-controls-deepening-inventory.csv)保留章/节父项、正文控件和本轮FAQ选题，含重叠，不是最新版全菜单。图层名称存在不代表已经重建全部操作。

| F编号 | 入口与动作 | 对象、结果与分支 | 中断/恢复与缺口 | 证据定位 |
| --- | --- | --- | --- | --- |
'''
for n,task,steps,branch,gap,eids,pages in data:addition+=f'| F-M062-{n:02} {task} | {steps} | {branch} | {gap} | {eids.replace(";","、")}；{pages} |\n'
addition+='''
### 静态控件与研究边界

实际查看[第9页](../artifacts/lenovo-controls-deepening/guide-p9.png)与[第10页](../artifacts/lenovo-controls-deepening/guide-p10.png)，确认创建提交勾号、工具/分享/录音图标。这些是手册说明表，不是完整页面操作观察。重读和区域索引的范围见[本轮来源说明](../artifacts/lenovo-controls-deepening/source-notes.md)。

对G-002的候选启发：描述动作时同时指明“哪一份文件、哪一种对象、什么账号状态、能恢复到哪一步”；这仍是研究建议，没有决定系统实现。锁屏速记显示与密码保护如何组合、十步撤销是否覆盖图层/页删除，均须后续验证。

'''
s=s.replace('## 待核查\n',addition+'## 待核查\n');case.write_text(s)
jw(W/'build-summary.json',dict(new_evidence=1,new_flows=len(flows),inventory_rows=len(inv),toc_rows=len(toc),old_flows=len(updates),old_evidence_captures=len(caps),static_pages=2,actual_operations=0))
print((W/'build-summary.json').read_text())
