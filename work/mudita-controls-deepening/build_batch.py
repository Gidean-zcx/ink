from pathlib import Path
import csv, json, hashlib, re, shutil
from urllib.parse import urljoin
R=Path('/home/workspace/reader'); W=R/'work/mudita-controls-deepening'; L=R/'docs/research/interaction-library'; A=L/'artifacts/mudita-controls-deepening'; A.mkdir(exist_ok=True)
C=L/'cases/M-012-mudita.md'; old_case=C.read_text(); (W/'case-before.md').write_text(old_case)
logs=[json.loads(x) for x in (W/'fetch-log.jsonl').read_text().splitlines()]; src={Path(x['path']).relative_to(W).as_posix():x for x in logs}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
shutil.copyfile(W/'install-complete.png',A/'install-complete.png')
# Content is from the fetched official pages; labels below are research notes, not full copies.
specs=[
('manual.pdf','官方 Kompakt 英文手册：整册目录与实体导航','official_manual','TOC p4; pp2–5; p9; observed illustration p3','手册无 OS 号；目录15项。p3 单帧确认开关方向和返回/主页/控制中心图标。','manual-p03.png',None),
('how-to-index.html','官方 Kompakt 指南、FAQ、Center 目录快照','official_help_index','10 how-to folders / 55 articles; FAQ 24; Center 2','81个条目逐项登记；目录可见不等于正文已读或任务闭合。',None,None),
('release-1.6.html','MuditaOS K 1.6：1.6.1 发布范围与修复','official_release_entry','Why 1.6.1; key fixes','入口明确附件对应1.6.1；1.6.0暂停扩大推送；未接来电计数及退出Offline+的SIM PIN例外。',None,'2026-10-13'),
('release-1.6.pdf','K 1.6 英文发布附件（入口说明对应1.6.1）','official_release_notes','pp1–6; 15 section headings; Phone p4; Maps p3','只把发布说明作为版本化变化；没有据修复项推定实机音频中断链。',None,'2026-08-24'),
('calling.html','拨号、联系人与来电阻止','official_help','Call a contact / new number / block incoming call','普通接听/拒接的完整状态未说明；无服务和媒体恢复不明。',None,'2026-10-03'),
('messaging.html','短信、图片消息与会话删除','official_help','SMS contact/new number; MMS; delete conversations','同页Pencil/Plus保留差异；Send不证明送达或已读。',None,'2026-10-02'),
('volume.html','主音量、功能音量与提示音','official_help','main volume; silent; Sound > Advance','控制中心和物理音量键入口；独立音量及通知优先级未实测。',None,'2026-10-03'),
('silent.html','静音：控制中心与设置入口','official_help','Method 1 / Method 2','另有Settings > Sound > Silent Mode；闹钟是否越过静音未说明。',None,'2026-10-03'),
('offline.html','Offline+ 与飞行模式开关','official_help','Airplane / Offline+ / full function','飞行模式允许重新开蓝牙；Offline+左侧开关下开上关。',None,'2026-10-03'),
('offline-faq.html','Offline+ 硬件/软件停用边界','official_help','What is the difference?','GSM与3个麦克风硬件停用；相机/Wi-Fi/蓝牙软件停用。不是全部硬件断电的证据。',None,'2026-10-03'),
('usb.html','USB 文件不可见及禁用数据传输','official_help','File Transfer / No data transfer','USB Preferences只在接电脑时可见；未据此声称Center连接必需MTP。',None,'2026-10-03'),
('install.html','Center 安装第三方 APK 与完成提示','official_help','Steps1–10; observed Step9 image','删除安装包不影响已安装应用；实际看了完成弹窗单帧，未执行安装。','install-complete.png','2026-10-03'),
('apps.html','第三方应用前提及当前卸载限制','official_help','How to Sideload / Uninstalling','K1.2.0加入侧载；无Google Services、机内APK loader或内置卸载；与K2.0预告分开。',None,'2026-10-03'),
('hidden.html','隐藏/显示应用图标与保存退出','official_help','App layout / Preview / Back arrow','可预览，返回箭头保存；隐藏图标不等于卸载。',None,'2026-10-03'),
('update.html','系统升级入口与失败处理','official_help','update notification / manual check / fails','下载配置后Restart phone；失败排查将恢复出厂列最后措施。',None,'2026-10-03'),
('factory-reset.html','设置内恢复出厂','official_help','Warning; reset steps','清除用户数据，保留当前OS版本；不是回滚固件。',None,'2026-10-03'),
('bootloop.html','启动循环：Recovery 恢复出厂','official_help','Factory reset procedure','两套菜单按键不同；清空数据，无保留未备份资料的承诺。',None,'2026-10-03'),
('backup.html','Center 创建/恢复备份及密码','official_help','create / restore / find backup file','密码可跳过；确认后不能改，遗忘无法恢复；合并/覆盖与异常回滚未知。',None,'2026-10-03'),
('center-product.html','Mudita Center 的 Kompakt 备份范围','official_product_page','Mudita Kompakt > Backup & Restore; separate media section','仅取Kompakt段五类数据；Pure的settings备份不移用到Kompakt。范围D1，与操作帮助结合。',None,None),
('center4.html','Mudita Center 4.0.0 发布说明','official_release_notes','backup and restore; APK statuses','新增Calendar events备份；APK状态可分已安装/待安装。帮助页面的精确桌面构建仍未标。',None,'2026-09-17'),
('bluetooth.html','蓝牙配对入口','official_help','Connect a device','配对步骤已读，但切换播放输出、断连恢复或通话路由未知；不独立新增F。',None,'2026-10-03'),
('rcs.html','Kompakt 的 RCS 边界','official_help','FAQ answer','文档明确无RCS；不把未读功能误写成不支持。',None,'2026-07-31')]
evs=[]
for i,(fn,title,typ,locator,note,art,date) in enumerate(specs,1):
 s=src[fn]; p=W/fn; eid=f'E-MDCD-{i:02}'
 e=dict(evidence_id=eid,product_id='M-012',title=title,url=s['url'],checked_on='2026-10-27',source_type=typ,access_mode='official_pdf_local_text_and_single_frame' if i==1 else 'official_html_text_and_single_frame' if i==12 else 'official_pdf_local_text' if fn.endswith('.pdf') else 'official_html_local_text',version_scope='Mudita Kompakt only; K 1.6.1 release baseline; unversioned manual/help kept distinct; Center 4.0.0 release separate from unpinned screenshots',app_build='Mudita Center 4.0.0' if i==20 else None,firmware_version='MuditaOS K 1.6.1 (release page maps 1.6 attachment)' if i in (3,4) else None,source_updated_on=date,locator=locator,note=note,hands_on=False,visual_observed=bool(art),archived_artifact=f'artifacts/mudita-controls-deepening/{art}' if art else None,sha256=sha(A/art) if art else None,capture_state='single_official_reference_frame_observed' if art else 'raw_source_local_with_bibliography',supersedes=None,raw_work_path=p.relative_to(R).as_posix(),raw_sha256=sha(p))
 if i==2:
  e['additional_raw_sources']=[dict(url=x['url'],raw_work_path=Path(x['path']).relative_to(R).as_posix(),raw_sha256=x['sha256']) for x in logs if '/directories/' in x['path'] or Path(x['path']).name in ('faq-index.html','center-index.html','support-index.html')]
 if i==12:e['image_url']=src['install-complete.png']['url'];e['image_raw_work_path']='work/mudita-controls-deepening/install-complete.png';e['image_raw_sha256']=src['install-complete.png']['sha256']
 evs.append(e)
(L/'batches/mudita-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evs))
# Detailed inventory: each row is a directory item, never a new task by itself.
I=[]
def item(scope,domain,title,locator,url,read,flows='',evidence='',gap='正文/完整任务未核查'):
 I.append(dict(inventory_id=f'I-M012-{len(I)+1:03}',scope=scope,domain=domain,title=title,locator=locator,url=url,reading_status=read,flow_ids=flows,evidence_ids=evidence,gap=gap))
manual=[('Get Started','2','F-M012-01'),('Charging and Turning on','2',''),('Offline+','3','F-M012-05'),('Navigate the Apps','3','F-M012-04'),('Accessibility','4','F-M012-01;F-M012-04'),('Mudita Center','5','F-M012-06;F-M012-10'),('Important Safety and Legal Information','5',''),('Safety Instructions','5',''),('Frequency Bands and Power Output','7',''),('Charging and Battery Information','9',''),('Regulatory Information','10',''),('Before you use Kompakt','12',''),('Cleaning, Storage and Maintenance','12',''),('Guarantee Information','12',''),('Contact Details','12','')]
for title,page,flows in manual:item('manual_toc','手册',title,'printed p'+page,src['manual.pdf']['url'],'text_read',flows,'E-MDCD-01','只索引章节；目录不等于应用全功能；地区/实机未定' if not flows else '局部关联既有任务；无实操')
flowmap={5:'F-M012-01',6:'F-M012-02;F-M012-03',7:'F-M012-04',8:'F-M012-04',9:'F-M012-05',10:'F-M012-05',11:'F-M012-06',12:'F-M012-08',13:'F-M012-08',14:'F-M012-08',15:'F-M012-09',16:'F-M012-09',17:'F-M012-09',18:'F-M012-10',22:'F-M012-02'}
urlmap={e['url']:(e['evidence_id'],flowmap.get(i,'')) for i,e in enumerate(evs,1)}
folders=['设置与启动','通话与消息','组织工具','联系人导入','录音','联系人管理','文件','隐私与应用布局','系统设置','连接']
for p,domain in [*[(W/f'directories/{77000333478+i}.links.json',d) for i,d in enumerate(folders)],(W/'faq-index.links.json','FAQ'),(W/'center-index.links.json','Center')]:
 for a in json.loads(p.read_text()):
  if '/en/support/solutions/articles/' not in a['url']:continue
  u=urljoin('https://support.mudita.com',a['url']); ee,ff=urlmap.get(u,('','')); status='body_read' if ee else 'index_only';gap='任务详见case；未实测' if ff else '正文已读但本批不闭合独立任务' if ee else '本轮只登记目录；不推定能力/失败行为'
  if '77000596832' in u:ee='E-MUD-08';ff='F-M012-06';status='prior_body_read';gap='旧证据关联；本轮未重读该正文'
  item('help_article',domain,a['title'],'official folder listing',u,status,ff,'E-MDCD-02'+(';' +ee if ee else ''),gap)
release=[('MuditaOS K','1','F-M012-04;F-M012-05;F-M012-09'),('Alarm','1–2',''),('Calendar','2',''),('Contacts','2','F-M012-01'),('eSIM','2',''),('Kompakt Keyboard','2–3',''),('Launcher','3','F-M012-01'),('Maps','3–4',''),('Notes','4',''),('Onboarding','4',''),('Phone','4–5','F-M012-01'),('SMS','5','F-M012-02'),('System Update','5','F-M012-09'),('Voice Recorder','5',''),('Weather','5–6','')]
for title,page,ff in release:item('release_section','K 1.6 / 1.6.1',title,'PDF p'+page,src['release-1.6.pdf']['url'],'text_read',ff,'E-MDCD-03;E-MDCD-04','仅发布变更，不是全设置/完整任务指南')
with (A/'function-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=I[0]);w.writeheader();w.writerows(I)
(W/'source-manifest.json').write_text(json.dumps([dict(x,relative_raw_path=Path(x['path']).relative_to(R).as_posix()) for x in logs],ensure_ascii=False,indent=2)+'\n')
# Only new flows go into the new CSV; seven old rows are proposals, never mutations.
old=[r for r in csv.DictReader((L/'batches/reader-coverage.csv').open()) if r['product_id']=='M-012']
oldadds=[('E-MDCD-01;E-MDCD-03;E-MDCD-04;E-MDCD-05','新增拨号和来电阻止入口；电源结束通话为可选设置；1.6.1修正未接来电计数。','接听/拒接、占线、无服务、SIM选择与音乐打断恢复未知','not_observed'),('E-MDCD-06;E-MDCD-22','同页联系人Pencil/新号码Plus差异仍在；键盘光标补救；RCS明确不支持。','草稿保留、离线重试、送达/已读、MMS失败未知','not_observed'),('E-MDCD-06','统一帮助再次给出选择会话与两次Delete；未见撤销依据。','单条删除、附件生命周期、恢复范围未知','not_observed'),('E-MDCD-01;E-MDCD-04;E-MDCD-07;E-MDCD-08','返回/主页/控制中心有独立图标；主音量、特定用途音量及静音多入口；通知振动有版本变化。','音量/静音/闹钟优先级、媒体音焦点与中断恢复未知','static_reference_only'),('E-MDCD-01;E-MDCD-03;E-MDCD-09;E-MDCD-10','飞行模式允许重开蓝牙；Offline+硬件/软件停用分层；1.6.1仅重启后立刻退出Offline+时再要SIM PIN。','网络恢复时延、离线期间通话/短信补收、正在播放音频行为未知','static_reference_only'),('E-MDCD-11','Center传入沿用；USB文件不可见另走Connected Devices > USB Preferences > File Transfer；未连接不显示入口；No data transfer收回文件访问。','不假设Center依赖MTP；重复文件、容量不足、中断半成品、解析与位置保留未知','not_observed'),('E-MDCD-12;E-MDCD-13;E-MDCD-16','普通文件删除沿用；已安装应用不会随APK安装包删除；卸载限制详F08，清空恢复出厂详F09。','图书笔记/历史依赖、回收站、单项恢复未知','not_observed')]
updates=[]
for r,(ee,branches,gaps,visual) in zip(old,oldadds):
 n=dict(r,evidence_ids=r['evidence_ids']+';'+ee,known_branches=branches,gaps=gaps,visual_status=visual,checked_on='2026-10-27')
 updates.append(dict(flow_id=r['flow_id'],changes={k:dict(before=r[k],after=n[k]) for k in r if r[k]!=n[k]},replacement_row=n))
(W/'old-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
new=[]
for num,title,ids,branch,gap,visual in [
(8,'安装应用并区分隐藏、删除安装包与卸载','E-MDCD-12;E-MDCD-13;E-MDCD-14;E-MDCD-16;E-MDCD-20','Center安装完成弹窗；APK可删但应用保留；隐藏图标返回保存；当前帮助无内置单应用卸载。','不保证应用兼容；更新覆盖、权限撤销、后台/崩溃与数据保留未知。','static_reference_only'),
(9,'更新系统及失败后的恢复出厂分支','E-MDCD-03;E-MDCD-15;E-MDCD-16;E-MDCD-17','自动通知或手查；下载配置后重启；失败先排查；设置与Recovery两种清空入口，OS版本不回退。','安装中断、回滚、锁屏凭据失败、清空后SIM/eSIM细项未核。','not_observed'),
(10,'创建加密备份并选择恢复','E-MDCD-18;E-MDCD-19;E-MDCD-20','可选密码不可更改/找回；本地备份路径；Kompakt五类基础数据，Center4.0增加日历事件。','媒体/应用数据/设置不作全量承诺；覆盖合并、跨版本、断线失败和校验未知。','not_observed')]:
 new.append(dict(flow_id=f'F-M012-{num:02}',product_id='M-012',task=title,depth='D2',evidence_ids=ids,known_branches=branch,gaps=gap,visual_status=visual,hands_on='false',case_path='cases/M-012-mudita.md',checked_on='2026-10-27'))
with (L/'batches/mudita-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=old[0]);w.writeheader();w.writerows(new)
# Keep prior citations exactly, adjusting only the previous unread-attachment caveat.
prior=old_case.split('## 来源\n',1)[1].replace('已确认版本条目，未以未读取的附件推断具体操作。','旧批仅登记条目；本轮 E-MDCD-03/04 已读取正文与英文附件。')
head='''# M-012 Mudita Kompakt：通信、离线开关与系统恢复

查阅：2026-10-27；旧批为 2026-10-26。本例只研究 **Mudita Kompakt**，不套用 Mudita Pure 的桌面短信/设置备份。英语文档；地区、SIM、运营商和安装构建均未实际选定。无登录、安装、拨号、发送、刷机或设备操作；`hands_on=false`。10 条任务最高 D2；两张官方静态参考图不算连续操作。

## 型号和版本边界

- 系统基线取已发布 **MuditaOS K 1.6.1**：官方 1.6 条目解释 1.6.0 曾限量推送后暂停，附件实际说明 1.6.1；条目修改于 2026-10-13。6 页附件仍题为 K 1.6，标注变化日期 2026-08-24，两日期不合并。（E-MDCD-03/04）
- 12 页官方手册未标 OS；PDF 创建元数据 2026-01-27 只作文件线索。10 月帮助说明具体路径，但没有证明全部截图/菜单来自 1.6.1。手册整册目录和现行支持目录均已登记，不宣称已取得一本覆盖每个 app 的完整操作手册。（E-MDCD-01/02）
- 桌面版本参照 **Mudita Center 4.0.0** 已发布说明；现行帮助及安装截图的确切构建未知。K 1.2.0 是侧载加入的系统门槛；Center 产品页仍以 3.1.0 介绍侧载，并不把那段旧文案当桌面当前版本。（E-MDCD-13/19/20）
- K 1.4 的锁屏音乐/文件夹等改进保留为历史 D1；K 2.0 IFA 预告不当成已交付卸载或同步能力。（E-MUD-11/12）

## 功能目录与覆盖

[逐条功能目录](../artifacts/mudita-controls-deepening/function-inventory.csv) 包含手册 15 项、帮助文章 81 项（10 类指南 55、FAQ 24、Center 2）及 K 1.6 附件 15 个功能域，共 111 行。每行标明正文已读、只读目录或旧批正文，关联 F/E 与缺口；这是目录条数，不是 111 条完成流程。

| 功能域 | 官方目录/页码 | 已关联任务或证据 | 未闭合部分 |
| --- | --- | --- | --- |
| 开机/SIM/语言/键盘 | 手册 p2；Setup 6 项 | F09 更新；E-MDCD-01/02/15 | 初次配置、SIM2/eSIM切换、语言变更的退出和错误 |
| 实体键/导航/无障碍/电池 | 手册 p2–4、p9；System 6 项 | F01/04/09；电源、音量、离线开关和三导航图标 | 指纹/锁屏恢复、截图、亮度刷新、按 app 优化未闭合 |
| 通话/SMS/MMS | Calling & Messaging 7 项；1.6 Phone/SMS | F01–03；E-MDCD-03–06/22 | 通话打断音乐、接听/拒接全过程、未送达重试 |
| 日历/闹钟/笔记/地图/天气 | Organization 9 项；附件 p1–6 | 目录和变化已读；备份范围见 F10 | app 内完整增改删/返回；地图离线暂停仅发布说明 |
| 联系人/导入 | Contacts 2、导入 6、Center 管理 1 | F01 取联系人；目录 E-MDCD-02 | 去重、覆盖、CSV错误与多来源冲突 |
| 录音 | 录制/收听/删除 3 项；附件 p5 | 仅目录与修复说明；Offline+限制见F05 | 来电中断、保存/未保存、转出与恢复 |
| 文件/阅读/音乐/相机 | 文件 7 项；K1.4发布说明 | F06/07；E-MUD-08–11、E-MDCD-11 | 阅读位置/标记、音乐队列/断点、摄像头返回未闭合 |
| 应用布局/侧载/卸载 | Privacy 3 项；FAQ安装/第三方app | F08；E-MDCD-12–14 | 后台、权限、更新覆盖、崩溃后的单应用修复 |
| 连接/离线/蓝牙 | Connectivity 6 项；Offline+ FAQ | F05；蓝牙入口已读 E-MDCD-21 | 输出切换、耳机断开/来电路由；Wi-Fi/热点/漫游正文未核 |
| 更新/恢复/备份 | Setup更新、System重置、FAQ启动循环、Center备份 | F09/10；E-MDCD-15–20 | 无损回滚、备份合并/覆盖、跨版本和中断 |
| 硬件/法律/其他FAQ | 手册 p5–12；FAQ24条逐项登记 | 手册完整目录；RCS无支持由明确FAQ支持 | 运营商兼容、SAR/性能等非本批操作结论；未读FAQ不据标题断言 |

手册全部 15 个目录项（保留原章节定位）：Get Started 2；Charging and Turning on 2；Offline+ 3；Navigate the Apps 3；Accessibility 4；Mudita Center 5；Important Safety and Legal Information 5；Safety Instructions 5；Frequency Bands and Power Output 7；Charging and Battery Information 9；Regulatory Information 10；Before you use Kompakt 12；Cleaning, Storage and Maintenance 12；Guarantee Information 12；Contact Details 12。（E-MDCD-01）

## 已有任务补证（保持原编号）

| F 编号 | 入口和操作 | 反馈、数据、退出与限制 | 证据 |
| --- | --- | --- | --- |
| F-M012-01 向联系人打电话 | Phone → Contacts → 联系人 → Call；新号码走 Phone → Dial → 输入 → Call。来电阻止：Block 图标 → Block number。Control Center → Settings → Accessibility 可选电源键结束通话。 | 1.6.1修复旧记录误算未接来电、锁屏/主页计数不同。普通接听/拒接入口及占线/无服务未闭合；阻止号码不等同仅拒接一次。媒体暂停/自动续播无证据。 | E-MUD-01/03；E-MDCD-01/03/04/05 |
| F-M012-02 写短信与图片消息 | 联系人：SMS → Pencil → 对象 → 输入 → Send；新号码：SMS → Plus → 号码 → send to:号码 → 输入 → Send；MMS再经 Plus → Image 选图。键盘不出现时点输入光标。 | 同一现行帮助仍有Pencil/Plus区别，不能归因于已知版本。发送不证明送达/已读；草稿、离线重试和图片失败未知。明确无RCS是FAQ结论。 | E-MUD-04/05/06；E-MDCD-06/22 |
| F-M012-03 删除对话 | SMS → Pencil → 勾选会话 → Delete → 再次Delete。 | 二次确认作用于对话；未见撤销、单条删除或附件保留规则。备份恢复也未证明能只找回某条会话。 | E-MUD-07；E-MDCD-06 |
| F-M012-04 调音量与进入控制中心 | 导航图标分返回/主页/Control Center。控制中心铃铛下滑条、物理音量键均调主音量；Settings → Sound有特定用途滑条。铃铛、滑条到底、长按音量减到底或Sound → Silent Mode可静音。 | Sound → Advance管按键/锁屏/充电/触摸提示音。K1.6另加通知振动开关，但不证明其与静音、闹钟的优先级。默认音量记忆、各应用媒体焦点未知。 | E-MUD-01/02；E-MDCD-01/04/07/08 |
| F-M012-05 开启深度离线 | 飞行模式：Control Center → 飞机图标切换；Offline+：左侧开关向下，向上恢复完整功能。 | 飞行模式允许重新开蓝牙。Offline+中GSM/三麦克风硬件停用，相机/Wi-Fi/蓝牙软件停用；不能紧急呼叫。1.6.1不再每次退出都要SIM PIN，仅重启后立即退出的例外保留。播放是否继续、补收短信和重连耗时未测。 | E-MUD-01；E-MDCD-01/03/09/10 |
| F-M012-06 从电脑准备书和音乐 | Center连接 → Your Kompakt → Manage Files → 类型 → Add file → 选择/确认。另一个USB文件可见性分支：接电脑 → Control Center → Settings → Connected Devices → USB Preferences → File Transfer。 | 未接电脑时USB Preferences不显示；No data transfer停止暴露文件。这个USB分支未被认定为Center的必需前置。旧帮助的格式筛选不保证解析成功；容量、重复与断线半成品未知。 | E-MUD-08/09；E-MDCD-11 |
| F-M012-07 从电脑删除文件 | Center相应类型 → 选文件 → Delete → Delete file。 | 删除已安装应用的APK包不会卸载应用（F08）；不能据文件删除推出笔记/播放记录一起消失。未查回收站和单文件恢复。 | E-MUD-10；E-MDCD-12/13/16 |

音频相关只取得两段局部事实：历史 K1.4 描述锁屏播放/暂停及跳曲；当前蓝牙帮助为可发现外设 → Control Center → Settings → Connected Devices → Pair new device → 选设备 → Pair（出现“Bluetooth is on”提示可关闭）。这些均不证明来电抢占后何时恢复、耳机切换保存哪个位置，也不据 Android 底座推断音焦点实现。（E-MUD-11、E-MDCD-21）

K1.6 Phone 另列语音信箱通知：部分支持MWI的网络在应用/最近通话显示，其他网络仍用短信；拨号长按1可呼叫，未配置号码时提示，可到应用设置配置。仅登记版本化分支，不额外计F；运营商状态未实测。（E-MDCD-04 p4）

## 新增独立任务

### F-M012-08 安装应用并区分隐藏、删除安装包与卸载

前提为Kom­pakt系统至少K1.2.0、电脑Center和USB-C。Manage Files → App installers → Add file → 选择APK → 确认传入 → 点击该APK → 接受风险并确认安装。帮助完成步骤允许关闭窗口后使用；实际查看的单帧显示完成标志、Close及可断开设备提示，未观察前面的传入/安装过程。（E-MDCD-12/13）

安装包与应用是两个对象：帮助明确安装后可删APK而不影响应用；Center4.0发布说明给APK“已安装/待安装”状态。隐藏是另一路径：Control Center → Settings → App layout → 开关 → 可Preview → 返回箭头保存退出；不据隐藏推定卸载或清数据。（E-MDCD-12/14/20）

2026-10-03官方FAQ明确设备和Center均无内置卸载侧载应用的选项；恢复出厂可一起移除并清用户数据（F09）。FAQ提到社区第三方管理器，但本批未安装/核查，不能替代官方内置流程。无Google Services、无机内APK loader，应用不一定兼容；安装失败、更新覆盖、权限撤销和后台规则仍未知。K2.0预告不填这些缺口。（E-MDCD-13；E-MUD-12）

### F-M012-09 更新系统及失败后的恢复出厂分支

联网后可从Control Center顶部更新通知 → Update → 如出现许可则勾选/Agree → 自动下载和配置 → Restart phone；手查为Control Center → Settings → About → Software Update。失败帮助依次建议重试、检查稳定联网/移动数据下载许可、重启，再把恢复出厂作为最后手段；仍失败联系支持。没有证明中途断电会自动回滚。（E-MDCD-15）

能进入设置时：Control Center → Settings → System → Factory Reset → Factory Reset → 最终Factory Reset；完成后重启进入设置流程，数据清空但OS版本不变。因此“恢复出厂”不能写成降回旧固件。（E-MDCD-16）

启动循环的独立入口：关机 → Power+音量加到Select Boot Mode → 音量加选Recovery、音量减确认 → 机器人画面按住Power并点一次音量加 → Recovery中音量上下选Wipe data/factory reset、Power确认 → 等待。两层菜单的确认键不同；会清空未备份数据，文档未给保留资料路径或恢复耗时。未执行。（E-MDCD-17）

### F-M012-10 创建加密备份并选择恢复

Center连接后Your Kompakt → Overview → Create backup → 再确认；可设置密码、Confirm passcode后输入，也可Skip password。密码确认后不能更改。恢复：Overview → Restore from backup → 选列表项 → Restore from backup → 如加密则输入密码 → Confirm；无密码则跳过这步。密码遗失无找回/更改方式。Center → Settings → Backup显示电脑本地路径；例示路径不当所有系统固定目录。（E-MDCD-18）

数据范围分版本：产品页Kompakt段列联系人、通话记录、消息、闹钟、笔记；Center4.0.0增加日历事件。产品页把媒体管理另列，并给Pure单列含settings的备份，不能据此将Kompakt称为整机镜像。第三方app及数据、媒体、MMS附件、SIM/eSIM或系统设置是否包含，未得到完整清单。还缺恢复时覆盖/合并、损坏备份、断线回滚、跨版本及最后完成反馈。（E-MDCD-19/20）

## 实际观察及数据状态

只实际查看两张单帧：[手册p3导航/离线开关](../artifacts/mudita-controls-deepening/manual-p03.png)和[官方安装完成图](../artifacts/mudita-controls-deepening/install-complete.png)。第一张是手册插图，第二张是官方帮助中的桌面窗口，均无可确认构建号；不据图内样例容量/应用名作性能或兼容结论。PDF其余页及帮助为文本阅读，不标视觉观察。原件/哈希见新证据批的raw字段，正式图与目录长期保存在artifacts。无连续画面、时间戳、耗时或复现次数。

## 与本项目关系（推断）

实体Offline+能让通信与采集状态可见，但它同时切断电话与麦克风，与R-012常驻唤醒、R-013来电可达存在取舍。G-001若借鉴，应先明确何种状态仍可唤醒/来电，而非只复制开关。

F08的安装包/应用/图标分离，以及F10的有限数据备份，对G-002和Plus的动作命名有启发：AI应区分隐藏、删源文件、卸载、清数据和恢复，提示准确受影响对象。这里没有证明Kompakt具备统一AI读写或自动恢复接口；其Android侧载也不代表Plus可直接执行APK。5–6岁自主读听仍缺误触删除恢复、音频中断及权限控制实证；成人通信/阅读也需这些操作测试。

## 下一项最小核查

固定地区SIM和K1.6.1设备，记录音乐/阅读→来电→接听/拒接→返回、Offline+切回后的补收与播放位置；核短信两个新建入口、草稿和离线发送。之后仅用测试数据验证Center4.0备份类别与恢复覆盖方式。当前均未做；目录中未读项明确保留，不把缺资料写成不支持。

## 来源

'''
newrefs='\n'.join(f"- {e['evidence_id']}：[{e['title']}]({e['url']})；{e['locator']}。" for e in evs)
C.write_text(head+prior+'\n本轮补证（均查阅于2026-10-27；原件/哈希见结构化批次）：\n\n'+newrefs+'\n')
# Formal small interpretation log preserves no entire page text.
(A/'observations.md').write_text('''# Mudita Kompakt 静态观察记录

2026-10-27。hands_on=false，无连续过程或时间测量。

- manual-p03.png：从官方12页PDF第3页渲染，实际view_image。看见左侧开关上/下状态、下方三个导航图标和耳机位置；是手册示意图。E-MDCD-01。
- install-complete.png：官方安装帮助Step9链接的原PNG，实际view_image。桌面完成弹窗有完成标志、关闭按钮及可断开设备提示；未观察安装过程，未核构建号。E-MDCD-12。

目录111行仅表示逐项登记，正文阅读标志与流程覆盖分开。source_type、版本、raw_work_path和SHA由evidence批保存。未取得实机证据。
''')
print(json.dumps({'new_evidence':len(evs),'new_flows':len(new),'old_updates':len(updates),'inventory_rows':len(I),'source_files':len(logs)},ensure_ascii=False))
