from pathlib import Path
import csv,json,hashlib,shutil
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/pocketbook-controls-deepening';a=lib/'artifacts/pocketbook-controls-deepening';a.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
case='''# M-003 PocketBook InkPad Color 3：笔记、读听与同步对象

## 范围与证据方式

查阅2026-10-26。严格固定 **PB743K3 / InkPad Color 3、官方6.10.2854路径的95页英文手册**，不套InkPad Color、InkPad 3、Eo或手机Reader app界面。原件哈希与旧E-PBK-01一致；PDF元数据日期2025-12-26只是文件时间。本次另取得同型号 **6.10.2854 WW发布说明，2026-01-05**，实际安装构建仍未知。[E-PBK-01；E-PBCD-01/07]

本例D1/D2，无实机操作、连续视觉观察、登录、发送邮件或固件执行。已实际查看新摘页27、33、55、62、68，并重看旧13/32；均是手册静态示意，没有点击结果或耗时测量。通用FAQ仅作有限互证，不以它的重启秒数、SD卡或其他型号步骤替代本册。

## 全目录逐项清点

手册印刷页与PDF文件页相同。第2–4页目录的每一项均登记在[功能目录CSV](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/functional-inventory.csv)，含页码、已关联F/E及缺口；不是全功能已验证声明。以下按用途汇总，逐项名称以CSV为准。

| 功能域 | 手册定位 | 本轮覆盖与待查 |
| --- | --- | --- |
| 外观、安全、规格、充电 | 5–11 | 型号/章节定位；不据参数宣称续航、刷新或耐久实测。 |
| 电源、任务管理、主页、通知、手势、键盘 | 12–16 | F01；任务管理与普通锁屏可定位；所有后台保留/强制关闭影响未测。 |
| 前光、网络、蓝牙 | 17–20 | F09；缺网入口、搜索耳机与配对可查；真实切源及音量继承未知。 |
| Library/Store/Browser/Notes | 21–27 | F03；商店、浏览器属于已读目录/部分规则；购买和账号条件未操作。 |
| 日历、计算、图片、相框、RSS | 28–30 | F08补RSS全链；相框列表、保存和删除另有章节，未新增任务。 |
| Music Player/Audiobooks/Libby | 30–39 | F07；音乐与有声书不同。Libby电子书本地下载、下载失败时webview、在线有声书不可合并成本地音频保证。 |
| 游戏、Scribble、Coloring | 40–41 | 单独清点各应用；Scribble可保存My Pictures并编辑图形笔记，不能据此推断所有导出笔记可回导编辑。 |
| Dropbox、Send-to-PocketBook | 41–45、70–71 | F06；Dropbox只访问其应用目录，云同步不是整机备份。 |
| 阅读导航、目录、书签、词典与笔记 | 46–54 | F02/03；标记、注释、译文和位置不是同一对象；计页期间部分功能不可用。 |
| Voice、文本/PDF版式、链接与加载 | 55–62 | F05；PDF重排不能保证保留所有表格/图片，未验证TTS读取顺序；USB与Adobe DRM另列，未执行授权。 |
| 书库分类、Cloud | 63–68、71–72 | F04；补三种落地状态、同步触发、错误入口及删除差异。 |
| PIN、Accessibility、Personalize | 72–84 | PIN恢复、屏幕阅读器、手势、刷新、用户档案逐项定位；不因目录存在称已验证儿童隔离或无障碍全链。 |
| 语言、时间、电源、维护、About、更新与错误 | 84–90 | 设置恢复/内部存储格式化、设备重启/关闭应用有不同范围；未执行恢复或磁盘检查。 |
| 支持/合规 | 91–95 | 目录清点；不展开法律认证判断。 |

同版本发布说明还列Libby、PIN、无障碍、LCP PDF/音频、FLAC等变化；其中FLAC没有明确归到哪一个播放器操作链，不能按手册音乐段仅写`.mp3`推定不支持，也不能据RN宣告两个播放器均已逐格式验证。[E-PBCD-07]

## 任务流程

原F05的“两种听读概览”收束为文本TTS；真人文件的独立控制在F07，不重复统计同一操作。原F03/04直接补编辑、导出和同步删除，不另造重复任务。

| F编号 | 任务/前置 | 操作、反馈与结束 | 数据状态与未核实 | E证据 |
| --- | --- | --- | --- | --- |
| F-M003-01 | 从主页进入系统控制 | 顶缘下拉/点通知图标，也可用Menu键；任务管理由通知栏或长按Home进入。主页13图示近期/商店及Library、Audiobooks、Store、Settings、Apps。 | 21页文字却列默认Library、Audiobooks、Store、Notes；图文默认项不一致。可自定义不等于已确认当前默认。手势触发和后台应用状态未知。 | E-PBK-01；E-PBK-04；E-PBCD-01 |
| F-M003-02 | 连续查词并留资料 | 阅读菜单/Dictionary→点词；保存译文及语境后从目录Notes或页边图标访问；长按可改译文/语境、删相应对象；Exit退出词典模式。 | 退出前触摸语义不同；设置可选择保存译文/语境的方式。词典缺词、下载失败、改后与原文定位关系未知。 | E-PBK-01；E-PBCD-01 |
| F-M003-03 | 编辑本地注释并导出 | Notes按类型/排序找条目；长按/Edit comment→修改/Save。导出可多选→Export notes→目标路径/Export；默认在Library的Notes目录。 | 可确认本地评论再编辑；不代表已导出文件会跟随变化。当前手册/FAQ未给扩展名、导入入口和回导后可编辑范围；同名覆盖、取消、删除恢复未知。 | E-PBK-01；E-PBK-02；E-PBCD-02 |
| F-M003-04 | Cloud同步、离线状态与删除 | Settings/Accounts and Synchronization/PocketBook Cloud登录；打开/关闭书、改收藏/合集触发同步，也可点通知栏Sync。书库长按菜单区分Delete本机和Remove from Cloud；同步图标可打开各服务状态。 | 虚线框表示仅云端；脱网只可读本地已有内容。本机删除后云条目下次再现，不等于已证明自动重下文件；云移除手册称不可恢复。失败先查网络，正常则稍后重试；冲突合并/撤销未知。 | E-PBK-01；E-PBCD-05；E-PBCD-06 |
| F-M003-05 | 从文本开启、控制与退出TTS | 已打开书→阅读菜单/Voice；面板选择语音、Play/Pause、速度、音量、最小化和Exit。当前朗读片段有进度指示、随听自动翻页。Download New Voices→选择语音包/Download。 | 最小化与退出是两个入口；未观察最小化后的实际播放、重开位置或其他app抢音。语音包断网/空间不足、DRM/PDF排版的朗读顺序待测。 | E-PBK-01；E-PBK-02；E-PBCD-04 |
| F-M003-06 | 邮件传书与接收核对 | Send-to-PocketBook向导完成激活后使用`@pbsync.com`设备地址；可信发件人附件可联网自动收取，或Settings/Accounts and Synchronization/Send-to-PocketBook/Receive files now。 | 本册已给出白名单及手动接收。默认目录在43/44页称SendToPb/Send-to-PocketBook，实际应在Download folder核对；未发送。附件到邮箱不等于成功落地；拒收/重名/重试未知。 | E-PBK-02；E-PBCD-01 |
| F-M003-07 | 播放真人有声书并调整控制（D2） | Audiobooks选择本地文件/目录书后播放；Playback Control分别调速度和音调。33页图有Now Playing、Contents、Bookmarks及Play/Pause、音量、Bookmark入口。 | 文档支持目录书及部分容器，不等于TTS。书签创建提示/命名/回跳、快退步长、章节对应、计时及跨书/重启恢复未给全链，图示入口不单列为完成任务。 | E-PBK-01；E-PBK-02；E-PBK-05；E-PBCD-03 |
| F-M003-08 | 添加/更新RSS并离线回看（D2） | RSS setup/Add RSS feed→URL；菜单Update时显示下载状态，更新写入`.hrss`。Read the latest news在浏览器打开最新下载，News Archive选旧更新；源菜单可改名/URL或Delete。 | 只有已下载更新可离线回看；删除源是否同时清档、URL失效/认证/断网/重复更新、下载取消和RSS返回点未说明。 | E-PBCD-01 |
| F-M003-09 | 为听读连接蓝牙输出（D2） | 外设配对模式；通知栏启用BT，首次可确认配对许可。启动音频/TTS时的列表选耳机，或Settings/Bluetooth/ON→列表；未找到用Find BT device。 | 手册说下次开机自动连接，未实测。6.10.2854 RN修复音乐断连及重启蓝牙后错误外放；不保证所有外设/中断恢复。USB/扬声器优先级、断线音量/位置未知。 | E-PBCD-01；E-PBCD-07 |

## 本地笔记、导出与再次编辑的边界

| 对象 | 本型号材料能确定 | 尚未确定 |
| --- | --- | --- |
| Notes应用中的本地评论 | 27页长按/Edit comment与Save；Rename quote、Delete comment、Delete note是不同入口 | 删除确认/撤销与误删恢复。 |
| 词典译文/语境笔记 | 52页可分别手改、删除译文/语境或删整个笔记 | 原书改版后定位、与Cloud同步的冲突规则。 |
| 图形笔记 | 41页说Scribble可编辑图形笔记 | 是否能重新编辑任意导出文件，不能据此推导。 |
| 导出结果 | 27页给目标路径、默认Notes目录；25页笼统提及export/import | **格式仍未知**；导入操作、书籍匹配、往返编辑和外部修改兼容性均未闭合。 |

已检索官方Notes/HTML材料。命中的旧InkPad 3 / PB740 5.20.1010发布记录属于其他型号/年代，不能作为PB743K3 6.10.2854输出格式证明；Eo的PDF/PNG说明同样排除。本轮没有生成或导入真实笔记文件，不把历史HTML线索写成当前已验证格式。

## 同步、音频与错误分支

| 分支 | 官方依据/实际观察 | 未闭合部分 |
| --- | --- | --- |
| 同步正在进行/失败/完成 | 62页动画同步图标可打开服务状态；出错变错误状态，无错误完成后图标消失。图中仅是Dropbox示例。 | 不把单个服务图示当所有Cloud下载完成；消息码、重试次数及部分成功未给出。 |
| 进度同步范围 | 66页列PDF、EPUB、FB2可同步阅读位置；文件存储支持范围更广 | TTS音频片段、有声书位置及所有笔记类型不能自动纳入这一范围。 |
| Cloud删除 | 68页明确本机Delete与Remove from Cloud不同；后者不可恢复 | 删除确认、其他设备何时消失、局部副本及回收站均未说明。 |
| 音乐删除 | 32页上半部正文说从列表移除，菜单图却写Remove from the device | 仍保留矛盾；这是Music Player段落，不等于已确定Audiobooks或TTS删除范围。 |
| 输出重连 | 6.10.2854 RN记载断连/错误外放修复 | 历史缺陷与修复声明不是实测恢复保证，也不是当前必现故障。 |
| 书打不开/无法保存 | 89–90页含损坏/受保护、编码、存储满/文件系统损坏等排错 | 未执行格式转换、磁盘检查或删文件；“当前位置不可保存”不能被UI继续显示误当已持久化。 |
| 重启/应用冻结 | 本册12页设备重启10–12秒；89页另列关闭冻结应用；通用FAQ给30秒 | 不拼成唯一操作；本例按型号手册保留动作对象及来源差异，均未执行。 |

本册还有需要保留的图文差异：13页主页静态图底栏含Settings，21页默认应用文字含Notes；59页传文件称microUSB，而9/10页型号接口及88页更新说明为Type-C。不能借这些旧措辞改写PB743K3硬件规格。手册导出说明未给格式，不应把缺少说明误写成“不支持导入”。

## 项目分析与下一项核查

以下为推断：G-002/R-021需要把本地笔记、导出文件、云条目、落地副本和播放位置分别建模；仅看到“导出成功”不足以答复之后还可编辑，看到书封也不足以答复可离线。RSS的下载状态、最新快照和历史归档是不同对象，适合G-001下的可追溯阅读研究。

R-018需要分别验证文本TTS与真人有声书断点。对5–6岁自主听读，最小化、退出、换书和外放切换应有明确反馈；对成人学习，译文/语境与可导出的评论有价值，但格式和回导未知前不能承诺学习资料全链。以上是建议，不构成Plus已有能力、设备外部AI接口或新增需求。

下一最小观察：固定6.10.2854创建高亮/评论/译文/图形四类笔记，导出后检查扩展名及再导入；再分别对TTS、有声书执行暂停→换书→锁屏→重启→耳机重连。RSS和Cloud只需补URL失败、断网与删除后另一端状态。记录真实文件和提示，不以更多目录条目代替验证。

## 来源与静态观察

全部查于2026-10-26。新摘页与旧手册同源，不按页数增加独立来源或连续流程等级。

| E编号 | 来源/定位 | 获取方式与限制 |
| --- | --- | --- |
| E-PBK-01 | [PB743K3 / 6.10.2854官方手册](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf) | 原95页本地文件，哈希核对；无重新下载或执行固件。 |
| E-PBK-02 | [官方设备FAQ](https://pocketbook.ch/en-ch/faq) | 本次重读Notes、TTS、Audiobooks、Cloud和邮件条目；通用跨型号，不证明导出格式。未访问视频。 |
| E-PBK-03 | [型号规格与应用目录](https://pocketbook.ch/en-ch/catalog/color-e-readers/pocketbook-inkpad-color-3-ch) | 本次重读；不作流程或性能测量。 |
| E-PBK-04 | 同册13页[旧静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/M003-manual-p13.png) | 本次重看；保留与21页文字默认项差异。 |
| E-PBK-05 | 同册32页[旧静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/M003-manual-p32.png) | 本次重看；上部音乐删除图文冲突，下部有声书速度/音调。 |
| E-PBCD-01 | 同官方手册，目录2–4、操作11–90 | 既有PyMuPDF解析；逐项目录另存CSV；未把全部法律/合规内容宣称为已分析。 |
| E-PBCD-02 | 同册27页[笔记摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p27.png) | 实看静态菜单/说明；没有导出格式选择画面。 |
| E-PBCD-03 | 同册33页[有声书摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p33.png) | 实看Now Playing/Contents/Bookmarks及控制；图上时码不是观察时间戳。 |
| E-PBCD-04 | 同册55页[TTS摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p55.png) | 实看语音、速度、音量、最小化与Exit标签；未触发按钮。 |
| E-PBCD-05 | 同册62页[同步状态摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p62.png) | 实看图示及文字；其中服务示例为Dropbox。 |
| E-PBCD-06 | 同册68页[Cloud状态摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p68.png) | 实看三种存储状态；删除行为另依页内正文，不是实际删除观察。 |
| E-PBCD-07 | [同型号6.10.2854发布说明](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/rn/ww/RN_PB_InkPad_Color_3_EN.pdf)，2026-01-05 | 5文件页；封面版本日期；印刷3/文件4的蓝牙与高亮修复。web正文和HTTP200原件，未看作操作或性能验证。 |
'''
(lib/'cases/M-003-pocketbook.md').write_text(case)
url='https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf'
E=[]
def ev(n,title,url,locator,note,artifact=None,source_type='official_manual',updated=None):
 art=None;digest=None
 if artifact:
  shutil.copy2(w/artifact,a/artifact);art='artifacts/pocketbook-controls-deepening/'+artifact;digest=sha(a/artifact)
 E.append(dict(evidence_id=f'E-PBCD-{n:02d}',product_id='M-003',title=title,url=url,checked_on='2026-10-26',source_type=source_type,access_mode='local_pdf_page_render_actually_viewed' if artifact else 'official_pdf_text_read',version_scope='PB743K3 InkPad Color 3; 6.10.2854 manual and matching WW release notes; actual installed build unknown',app_build=None,firmware_version='6.10.2854 document baseline; installed version unknown',source_updated_on=updated,locator=locator,note=note,hands_on=False,visual_observed=bool(artifact),archived_artifact=art,sha256=digest,capture_state='static_page_excerpt_archived' if artifact else 'bibliography_and_short_note_only',supersedes=None))
ev(1,'PB743K3完整目录及任务正文复核',url,'95页；目录2–4；操作11–90；文件页=印刷页','沿用work/research/interaction-depth/pocketbook-6.10.2854.pdf；SHA256 '+sha(root/'work/research/interaction-depth/pocketbook-6.10.2854.pdf')+'。PDF元数据2025-12-26不是固件发布日期；目录逐项清点，法律合规仅目录。保留13/21主页与59页线缆措辞差异。')
for n,page,title,note in [(2,27,'Notes编辑与导出','可见本地评论编辑/保存及目标路径；无文件扩展名或回导编辑证明。'),(3,33,'Audiobooks播放页','Now Playing/Contents/Bookmarks及控制图；书签保存/回跳全链未知，显示时码非采样时间。'),(4,55,'TTS控制与语音包','最小化和Exit分开；实际看图不证明后台或退出恢复行为。'),(5,62,'网络同步状态','文字描述进行/错误/完成状态；静态截图只示范Dropbox，不能当Cloud本次执行。'),(6,68,'Cloud三种文件状态与删除','图示本地/云端/两者；本机删除后重新出现不推定自动重新下载文件。')]:
 ev(n,title,url,f'印刷及PDF第{page}页',note+' 与主手册同一来源，无连续视觉流程。',artifact=f'pb743k3-p{page:02d}.png')
rn='https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/rn/ww/RN_PB_InkPad_Color_3_EN.pdf'
ev(7,'PB743K3 6.10.2854 WW发布说明',rn,'5文件页；封面日期；印刷1–4；蓝牙修复在印刷3/文件4','HTTP200原件work/pocketbook-controls-deepening/RN_PB743K3_6.10.2854_EN.pdf，SHA256 '+sha(w/'RN_PB743K3_6.10.2854_EN.pdf')+'。修复声明不代表本项目验证；FLAC支持未归因到特定播放器全链；未执行固件。',source_type='official_release_notes',updated='2026-01-05')
(lib/'batches/pocketbook-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
def row(n,task,eids,branches,gaps,visual=False):
 return dict(zip(fields,[f'F-M003-{n:02d}','M-003',task,'D2',eids,branches,gaps,'static_reference_only' if visual else 'not_observed','false','cases/M-003-pocketbook.md','2026-10-26']))
new=[row(7,'播放真人有声书并调整控制','E-PBK-01;E-PBK-02;E-PBK-05;E-PBCD-03','独立Audiobooks应用；播放/暂停、音量、速度/音调；Contents/Bookmarks仅可见入口。','书签完整创建/回跳、快退步长、章节映射、计时、跨书/重启恢复；非TTS或连续观察。',True),row(8,'添加与更新RSS并离线回看','E-PBCD-01','RSS setup/Add RSS feed；更新状态、.hrss快照、浏览器离线/News Archive；可改名URL及删源。','认证/失效URL、断网、取消、重复更新、删源是否清档、返回点。'),row(9,'为听读连接蓝牙输出','E-PBCD-01;E-PBCD-07','音频/TTS启动列表或Settings/Bluetooth配对；Find BT device；手册说明下次开机连接；同版本RN列断连/错误外放修复。','未实测外设重连、USB/扬声器优先级、断线音量/进度；修复非全场景保证。')]
with (lib/'batches/pocketbook-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fields);wr.writeheader();wr.writerows(new)
old=[row(1,'从主页进入系统控制','E-PBK-01;E-PBK-04;E-PBCD-01','补Menu键通知栏、长按Home任务管理；13图底栏Settings与21默认文字Notes不一致。','当前默认布局、手势触发、后台/关闭应用行为及实际安装版本。',True),row(2,'连续查词并留资料','E-PBK-01;E-PBCD-01','词典模式需Exit；译文/语境可分别保存、编辑、删除，目录Notes或页边入口。','缺词/词典下载失败、改后定位与同步冲突；未实测。'),row(3,'编辑本地注释并导出','E-PBK-01;E-PBK-02;E-PBCD-02','本地Edit comment/Save；多选Export notes至指定路径，默认Library/Notes。','当前格式和回导编辑仍未知；导出文件不保证随本地修改更新；同名覆盖、取消、恢复。',True),row(4,'Cloud同步、离线状态与删除','E-PBK-01;E-PBCD-05;E-PBCD-06','虚线仅云端；登录/开关书/收藏合集及手动同步；本机Delete后条目再现、Remove from Cloud不可恢复；服务状态与网络重试。','条目再现不保证自动重下载；位置仅PDF/EPUB/FB2；冲突/删除确认/他端副本/撤销未知。',True),row(5,'从文本开启、控制与退出TTS','E-PBK-01;E-PBK-02;E-PBCD-04','阅读菜单Voice；语音、速度、音量、Play/Pause、最小化/Exit；自动翻页及语音包下载。真人有声书移至独立F07。','最小化后台、退出/换书/重启位置、抢音、语音包失败、DRM/PDF顺序；未实测。',True),row(6,'从邮件传入文件','E-PBK-02;E-PBCD-01','型号手册补激活向导、白名单、联网自动接收/Receive files now及Download folder。','默认SendToPb/Send-to-PocketBook命名差异；未发送；拒收/重名/重试与落地成功待核。')]
base={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
u=[dict(flow_id=x['flow_id'],changes={k:{'old':base[x['flow_id']].get(k),'new':v} for k,v in x.items() if base[x['flow_id']].get(k)!=v},replacement_row=x) for x in old]
(w/'old-coverage-updates.json').write_text(json.dumps(u,ensure_ascii=False,indent=2)+'\n')
# Each item in the manual's three-page table of contents, including parent headings.
items=[('Safety Precautions',5),('Appearance',9),('Technical Specifications',10),('Getting Started',11),('Charging Battery',11),('Device management',12),('Main menu overview',13),('Gestures',15),('Working with On-Screen Keyboard',16),('Brightness',17),('Connecting to the Internet',18),('Configuring Bluetooth pairing',19),('Applications',21),('Library [Applications]',22),('Store',22),('Browser',24),('Notes',25),('General',28),('Calendar & Clock',28),('Calculator',28),('Gallery',28),('Photo Frame',28),('RSS News',30),('Music Player',30),('Audiobooks',32),('PocketBook x Libby',34),('Games',40),('Klondike',40),('Sudoku',40),('Chess',40),('Scribble',40),('Coloring',41),('Services',41),('Dropbox PocketBook',41),('Send-to-PocketBook',43),('Reading Books',46),('Gestures in Reading Mode',47),('Reading Menu',48),('Adjust SMARTlight and frontlight Brightness in reading mode',49),('Contents',49),('Bookmarks',50),('Dictionary',50),('Note',53),('Voice',55),('Settings [Reading Books]',56),('Following Links',59),('Loading Files',59),('Library [Reading Books]',63),('PocketBook Cloud',65),('Settings [system]',69),('Wi-Fi',69),('Bluetooth',69),('Accounts and Synchronization',70),('PIN code',72),('Accessibility',74),('Personalize',78),('Languages',84),('Date/Time',84),('Saving Power',84),('Maintenance',85),('About Device',85),('Firmware Update',87),('Troubleshooting',89),('Customer Support',91),('Compliance with International Standards',92)]
mapf={'Device management':'01','Main menu overview':'01','Applications':'01','Notes':'03','RSS News':'08','Audiobooks':'07','Configuring Bluetooth pairing':'09','Bluetooth':'09','Send-to-PocketBook':'06','Dictionary':'02','Note':'03','Voice':'05','PocketBook Cloud':'04','Accounts and Synchronization':'04;06','Loading Files':'04;06'}
gaps={'Notes':'扩展名、import入口和回导可编辑性未给出；本地评论编辑与导出分开。','RSS News':'失败URL、认证、取消/断网、重复更新、删源与档案范围未知。','Audiobooks':'Contents/Bookmarks只有部分入口；时间书签全链、章节和恢复未知。','Music Player':'31控制页已读；32移出列表与Remove from the device图文冲突。','PocketBook Cloud':'同步进度仅PDF/EPUB/FB2；删除条目再现不推定自动重下载；他端/冲突未知。','PocketBook x Libby':'34–39已读；卡登录/借阅/到期条件，仅目录与规则，不新增流程。','PIN code':'72–74已读；恢复依邮件，未设置/验证，不借此推定离线恢复。','Accessibility':'74–78已读；Screen Reader改变触摸语义；Menu三击入口和Power三击快捷项对象分开，未操作。','Personalize':'78–84已读；前光/Logos/LED/触摸滑动拖动/全刷/User Profiles；配置隔离范围未实测。','Maintenance':'85：Factory Reset保留书/图与内部格式化不同；均未执行。','About Device':'85–86：Memory格式化删除非系统数据；软件状态/更新；未操作。','Firmware Update':'87–88已读；文件缺失/损坏/版本错误只是文档，未下载或刷机。','Troubleshooting':'89–90已读；损坏/编码/满存储影响保存等；未磁盘检查/转换/删除。','Store':'22–23已读账户/下载目录；未购买或登录。','Browser':'24–25菜单/退出已读；RSS复用浏览器，未访问真实订阅。','Scribble':'40–41保存My Pictures/可编辑图形笔记；不能外推导出回导。','Dropbox PocketBook':'41–42、71：目录范围与自动/手动同步已读；权限和删除冲突未验证。','Main menu overview':'13图含Settings，21默认文字含Notes；不自行统一。','Send-to-PocketBook':'43/44默认目录名不一致，实际Download folder待核；未发送。','Voice':'55控制与自动翻页明确；后台/退出恢复、格式顺序未知。'}
inv=[]
for title,page in items:
 fs=';'.join('F-M003-'+x for x in mapf.get(title,'').split(';') if x)
 status='D2对应任务/仍有缺口' if fs else ('目录定位；正文未作任务分析' if title in ['Safety Precautions','Appearance','Technical Specifications','Charging Battery','Customer Support','Compliance with International Standards'] else '已读章节/功能目录；未单列完整任务')
 inv.append(dict(manual_item=title,printed_page=page,pdf_page=page,checked_scope=status,flow_ids=fs,evidence_ids='E-PBCD-01',gap=gaps.get(title,'完整操作、确认/取消、错误、数据保留与恢复未逐项闭环。')))
with (a/'functional-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,list(inv[0]));wr.writeheader();wr.writerows(inv)
(w/'scope-manifest.json').write_text(json.dumps(dict(modified_cases=['cases/M-003-pocketbook.md'],new_batch_files=['batches/pocketbook-controls-deepening-evidence.jsonl','batches/pocketbook-controls-deepening-coverage.csv'],artifacts=[x['archived_artifact'] for x in E if x['archived_artifact']],inventory='artifacts/pocketbook-controls-deepening/functional-inventory.csv',inventory_items=len(inv),new_evidence=len(E),new_flows=len(new),old_flow_updates=len(u)),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(evidence=len(E),new_flows=len(new),old_flow_updates=len(u),static_artifacts=sum(x['visual_observed'] for x in E),inventory_items=len(inv))))
