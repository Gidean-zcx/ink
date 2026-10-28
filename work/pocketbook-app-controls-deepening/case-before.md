# M-003 PocketBook InkPad Color 3：笔记、读听与同步对象

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
