import csv,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/kindle-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
DATE='2026-10-27'
rs=json.loads((WORK/'fetch-results.json').read_text());assert len(rs)==14 and all(r['status']==200 for r in rs)
meta=[
 ('型号与指南对应目录','Kindle Scribe - 2022 Release / Kindle Scribe - 2024 Release / 2025+ entries','official_index','仅证明型号到指南的链接；不是发布日期或固件版本。'),
 ('Scribe 2024在线用户指南','TOC 56 entries; #h2_20 / #h2_23 / #h2_28 / #h2_48–50','official_manual','主样本为2024 Release；HTTP Last-Modified不是固件或功能上线日；未拆章节登记目录。'),
 ('删除笔记本与文件夹','Remove download / Permanently delete','official_help','永久删除笔记本无法恢复；移除本地下载保留重下载入口；不能混同删除单页。'),
 ('笔记缺失与模板排查','Latest updates / Template / Page numbers','official_help','刷新、重开、同步与重新下载为不同排查分支；重下载前要分清本地和云副本。'),
 ('创建笔记本和文件夹','Notebook icon / + / Storage requirement','official_help','关闭时自动保存；至少100MB空闲空间是保存新笔记和新笔迹的条件。'),
 ('移动笔记本','Three-dot menu / Move / Notebook tab','official_help','移动整个笔记本到文件夹或顶层；不能把书与个人文档放入这些文件夹。'),
 ('多页整理与退出','Select / Add / Move / Share / 2025+ / X','official_help','页多选与整本删除不同；Duplicate及跨本移页列为2025+。'),
 ('书中画布、便签和工具','Active canvas / Sticky note / Hide toolbar','official_help','画布删除有确认；笔迹Undo/Redo不等于任意删除恢复；语言方向有限制。'),
 ('个人文档笔记兼容表','Opening note / Table 1','official_help','按传入渠道和内容类型限制；表中手机手写范围与同步帮助的泛化说法不一致。'),
 ('邮件导出与2025+连接条件','Share / Convert to text / 2025+ links','official_help','预览校订不进入可搜索手写PDF；末尾把云盘连接/分享限定为2025+。'),
 ('同步与云备份条件','Sync / Sync off','official_help','联网且开启Sync才备份笔记；关闭时不能云下载/备份/永久删除。'),
 ('云盘连接生命周期','FAQ 4–5 / 8–15 / 18–22','official_help','手动进出、重复副本与断开连接；与Kindle云库同步分开；F07按E-KDCD-10限定2025+。'),
 ('2025+首页随手记','Requirements / Quick notes / Workspace','official_help','补原F08的正文；缩放、加末页和定位Workspace，不套2024。'),
 ('合集内项目下载','Library / Sort By / Collections / Download all items','official_help','合集已存在不等于内容已离线；下载需另外执行。'),
]
ev=[]
for r,(title,loc,kind,note) in zip(rs,meta):
 scope='Scribe 2024 Release主指南；现行en-US文档；精确固件/地区与构建未知'
 if r['slug'] in ('cloud-drive','quick-notes'):scope='2025及以后Scribe分支；不套2024主样本；精确机型/固件/地区未知'
 elif r['slug'] not in ('scribe-2024','guide-index'):scope='现行Scribe/Kindle通用帮助；原文2025+分支单列；未标最低固件'
 ev.append(dict(evidence_id=r['evidence_id'],product_id='M-001',title=title,url=r['final_url'],checked_on=DATE,source_type=kind,access_mode='opened_text',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,visual_audit=None,archived_artifact=None,sha256=None,capture_state='raw_html_in_work_and_short_case_summary',raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],http_status=200,http_last_modified=r['headers'].get('Last-Modified'),supersedes=None,version_warning='v1为URL路径，不能当固件；未回推到2024出厂版本；2025+另列。'))
(LIB/'batches/kindle-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in ev))
old=list(csv.DictReader((LIB/'batches/reader-coverage.csv').open()));header=list(old[0])
flow=dict(flow_id='F-M001-09',product_id='M-001',task='整理合集并下载或移除书库内容',depth='D2',evidence_ids='E-KDCD-02;E-KDCD-14',known_branches='2024指南：Library创建合集→命名→勾选→Done；合集菜单可增删成员，Back/Home退出。合集删除同步但不删内容；合集内项目需另行Download all items。本地移除与永久删除分开，USB文件无云副本。',gaps='删除确认/撤销、下载失败/部分成功、跨端合集冲突未知；指南Downloaded选取/取消选取措辞内部不一致，不能据此拼造准确筛选状态',visual_status='not_observed',hands_on='false',case_path='cases/M-001-kindle.md',checked_on=DATE)
with (LIB/'batches/kindle-controls-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,header,lineterminator='\n');w.writeheader();w.writerow(flow)
updates={
'01':('E-KDL-01;E-KDCD-01;E-KDCD-02','2024在线指南补明确路径：正文查词→阅读菜单Vocabulary Builder→Words/Books→Flashcards→See Definition→Exit Flashcards。Learning显示学习/掌握统计，但正文未给标记掌握或撤销按钮；关闭自动收词后新查询不入本。','掌握标记/误标撤销、删词恢复、跨端同步与复习调度未知；Word Wise有内容语言条件'),
'02':('E-KDL-01;E-KDCD-02','Page Flip预览后Back to page/loc重定位原阅读点，点预览页才跳转；历史箭头在退出书籍后清空；不能把预览位置当持久阅读进度。','不同格式可用导航、跨会话返回栈、断电恢复位置未知'),
'03':('E-KDL-02;E-KDL-03;E-KDCD-08;E-KDCD-09','画布框可调整；删除需指针选框→垃圾桶→确认。便签X关闭自动保存；工具可隐藏再显示。USB PDF及2022-11-11前已传入库的PDF不支持该手写路径；漫画/儿童EPUB不泛化。','最低固件、删除画布后恢复、PDF变更对笔迹锚点影响未知；手机手写与同步帮助范围差异未解'),
'04':('E-KDL-04;E-KDCD-05;E-KDCD-06;E-KDCD-07','Notebook页+新建文件夹/笔记本，至少需100MB空闲才能保存新对象/笔迹；整本More→Move选文件夹或Notebook顶层，书与个人文档不能混入。页多选Add逐页后加页，Move选页间位置，X退多选；2025+才另列Duplicate与跨本移页。','删除单页确认/撤销、空间不足实际提示和未保存笔迹、跨本移动断网冲突未知'),
'05':('E-KDL-05;E-KDCD-10','Share邮件入口保留，下载链接7天有效。Convert to text预览校订不会进入可搜索手写PDF；定向邮箱需勾选附带搜索PDF，快发另链。不能批量分享多本，Family不自动共享个人笔记。','发送失败重试、7天后重新分享与重复导出命名、文字校订回写设备规则未知'),
'06':('E-KDL-06;E-KDL-07;E-KDCD-03;E-KDCD-04;E-KDCD-11','联网Sync备份；关闭Sync不能云下载/备份/永久删除。缺失先核同账号设备删除；模板问题重选→重开，仍异常时移除本地后重下载。More的Remove download保留云对象；Permanently delete影响所有设备且不能恢复。','本地未同步笔迹保护、误删确认、冲突优先级、删除文件夹内对象范围未知；跨端手写支持文档冲突保留'),
'07':('E-KDL-08;E-KDCD-10;E-KDCD-12','限定2025+：Connections扫码同Amazon账户授权；Import/长按定制，历史图标查状态。重复导入创建新副本，原文件和笔记不变；受密码保护者只读。Share回云盘是PDF副本；断开/重置移除连接，已导入Kindle云库文件保留。','重复导出命名/覆盖、并发修改、授权失效/导入失败恢复未知；不适用于2024主样本；USB内容不能回传云盘'),
'08':('E-KDL-09;E-KDCD-13','2025+且注册联网；首页显示末次页，双箭头调展示大小，+在末尾添页；可用Move跨本移动/复制，Workspace能找到同一Quick Notes笔记本。','离线持续编辑与回网冲突、末次页选择规则、删页确认/撤销未知'),
}
changes=[]
for r in old:
 if r['product_id']!='M-001':continue
 n=r['flow_id'].rsplit('-',1)[1]
 fields=dict(zip(['evidence_ids','known_branches','gaps'],updates[n]));fields['checked_on']=DATE
 if n=='04':fields['task']='创建并整理笔记页与文件夹'
 if n=='07':fields['task']='从云盘导入再带回（2025+ Scribe）'
 for field,after in fields.items():
  if r[field]!=after:changes.append(dict(flow_id=r['flow_id'],field=field,before=r[field],after=after,reason='保留原F补证；D2/未实操状态不变；root合并旧行'))
with (WORK/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader();w.writerows(changes)

topics=['入门','设置注册','实体控制','键盘','网络','笔与快捷键','读屏','电子纸刷新','使用概览','系统导航','状态标识','快捷设置','内容管理','项目菜单','购书','文档传入','排序筛选','云合集','本地与云','导出','移除删除','电脑连接','阅读概览','阅读控制','书内导航','文字排版','PDF显示','内容交互','跨端阅读','内容类型','书写概览','书写工具','笔记本','书中笔记','PDF笔记','有声概览','蓝牙配对','播放器','音频书签','音频下载','音频库','其他功能','关联百科','前情回顾','词典','词间提示','生词本','浏览器','家庭','书友社区','预计时间','辅助朗读','设置概览','设置项','设置菜单','帮助']
links=json.loads((WORK/'02-scribe-2024-links.json').read_text())[:56];assert len(links)==len(topics)==56
by={13:'09',15:'03',16:'09',17:'09',18:'09',19:'05',20:'09',21:'03',24:'02',27:'01;F-M001-03',28:'06',31:'03',32:'04',33:'03',34:'03',44:'01',45:'01',46:'01'}
inventory=[]
parent=0
for i,(topic,link) in enumerate(zip(topics,links)):
 if link['href'].startswith('#h1_'):parent=i+1
 fs=('F-M001-'+by[i]) if i in by else ''
 inventory.append(dict(order=i+1,entry_kind='parent_group' if link['href'].startswith('#h1_') else 'child_topic',parent_order='' if link['href'].startswith('#h1_') else parent,functional_domain=topic,source_url=rs[1]['final_url']+link['href'],evidence_ids='E-KDCD-02',flow_ids=fs,coverage='部分任务' if fs else '目录登记',missing_controls_or_branches='余下分支未拆' if fs else '正文可继续；未建立流程'))
inventory.extend([
 dict(order=57,entry_kind='body_subtopic',parent_order=33,functional_domain='笔记AI子项',source_url=rs[1]['final_url']+'#h2_36',evidence_ids='E-KDCD-02',flow_ids='',coverage='入口声明已读',missing_controls_or_branches='未拆失败/回退/资格；不倒推2024出厂具备'),
 dict(order=58,entry_kind='body_subtopic',parent_order=34,functional_domain='页边栏子项',source_url=rs[1]['final_url']+'#h2_37',evidence_ids='E-KDCD-02;E-KDCD-08',flow_ids='F-M001-03',coverage='部分任务',missing_controls_or_branches='页边栏完整显示模式/定位分支未拆'),
])
with (LIB/'batches/kindle-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,list(inventory[0]),lineterminator='\n');w.writeheader();w.writerows(inventory)
print('WROTE 14 E, 1 new F, 58 inventory rows; 8 old F /',len(changes),'field suggestions')
