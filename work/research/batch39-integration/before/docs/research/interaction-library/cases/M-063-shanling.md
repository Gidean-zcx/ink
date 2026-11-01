# M-063 山灵 M1 Plus：文件、播放页、遥控与版本边界

## 范围与证据方式

初查2026-10-26，本批补证2026-10-27。固定 **M1 Plus、非 Android 的 MTouch**，英文官网 V2.1 为发布基线；中国站本次可读下载页停在 V1.8，区域页面不同步，不能据此判定设备版本。快速指南 V1.0 是文档修订号。另用两篇明确标注为第一手体验的商业评测补旧界面入口，HiFi Oasis 作者自报升级至 V2.1，Kajetblog 未注明固件；截图未逐一核版，不把两篇菜单拼成同版完整菜单。M1/M1s、M0 与山灵 Android 产品均排除。[E-SHAN-01～04；E-SHCD-01～05]

本例 D1/D2；未购买、安装、登录、连接设备或测量。实际查看官方指南文件第5、6页静态图，归档第5页；没有连续视觉流程或实机操作。独立评测中的体验是作者的观察，不是本项目实测。账户、地区服务可用性及实际安装的 Eddict app 构建未知。本批另取得官方 Eddict 使用说明书V1.2（16页），是伴随App共用手册，不是M1 Plus高级手册；下载页当前Android2.3.7/2026-09-11与手册修订号分开。实际查看其中p12/15静态页，示例设备不是M1 Plus。[E-SLCD-01～03]

## 功能目录与覆盖

官方快速指南章节覆盖安全/环保、部件、控制与接口、升级、常见问题、附件（含保修卡）及其译文；它不是 MTouch 高级手册。下表补入已查设置目录，同时把仅有名称和已分析任务分开。页码统一为 **PDF 文件页，从封面起1计数**：中文部件5、控制6、连接7、升级8、故障9；英文部件11、控制12、连接13–14、升级14–15、故障15；日文另在23页列技术证明入口。旧记录少计一页的定位由此修正。[E-SHCD-01]

| 功能域 | 目录/来源定位 | 已关联 F / E | 缺口与未覆盖 |
| --- | --- | --- | --- |
| 主屏、快捷菜单、返回 | 指南7；HiFi Oasis 的 MTouch OS and UI | F01/03/09；SHAN-01、SHCD-03 | 主屏图标显隐/重排与重置只有评测目录；误触、返回栈未完整核验。 |
| 文件夹、标签、搜索、扫描 | 指南7；评测 UI / Syncing Music & Playlists | F01/10；SHCD-01/03 | Artist/Album Artist 切换有旧界面描述；搜索空结果、坏文件、扫描过滤/去重仍缺。 |
| 播放页、歌词、信息、AB/速度 | 评测 UI 的播放页及省略号菜单 | F02/09；SHCD-03 | AB、变速仅知入口，不知设点、范围、退出与保存；不能记作完整精听流程。 |
| 当前列表、歌单、收藏、删除 | 评测 Syncing Music & Playlists | F10；SHCD-03 | 已补外部歌单导入；当前队列编辑、批量删除对象、确认/撤销仍未知。 |
| 断点与播放策略 | 评测 Playback Settings：Memory playback、Play mode | SHCD-03；无新增 F | 旧界面列 Off/Track/Position；跨文件/关机恢复的保存时点、颗粒度未证实。 |
| 按键、锁屏、音量 | 指南5–6；评测 System Settings/Button lock | F02/06；SHCD-01/03 | 评测列熄屏锁音量、锁按键、两者都锁；当前固件默认值及耳机线控待查。 |
| EQ、增益、音量、封面 | 评测 Playback Settings；MUSIN V2.0 更新 | SHCD-03/05；无新增 F | Playback Setting→Folder Album Cover 的开关有更新依据；EQ保存、默认/最大音量冲突未闭环。 |
| 定时、待机、关机 | 评测 System Settings/Idle | SHCD-03；无新增 F | Sleep Timer、Auto Standby、Auto Shutdown 有目录；计时范围、操作是否重计、结束动作未知。 |
| 蓝牙、SyncLink、网络曲源 | Kajetblog 的 SyncLink/AirPlay；官方产品页 | F08；SHCD-04、SHAN-04 | 蓝牙遥控有作者操作链；Wi-Fi遥控失败是单次报告；DLNA、Tidal登录/离线及双向蓝牙全链仍缺。 |
| USB、模拟/同轴输出 | 指南5、7；V1.8 修复说明 | F03；SHCD-01/02 | USB三角色可定位；输出优先级、PO/LO切换、DAC退出和音量继承未查清。 |
| 显示、语言及系统设置 | 指南6氛围灯；评测 System Settings | SHCD-01/03；无新增 F | 只做目录索引；主题、显示睡眠、输入与无障碍的当前完整选项未核定。 |
| 更新、格式化、复位、备份 | 指南8–9；中/英文更新页 | F04–07；SHCD-01/02、SHAN-02/03 | 格式化确认、删除恢复、设置/歌单备份、恢复出厂范围仍缺。 |

表中E简写保留各自前缀：SHAN为初批，SHCD为上轮，SLCD为本次。目录出现不提高任务覆盖等级。

本批[逐条台账](/home/workspace/reader/docs/research/interaction-library/batches/shanling-controls-deepening-inventory.csv)有**114行**：M1快速指南22个章节/说明域、11个部件标签、5个FAQ；作者自报V2.1的评测目录39项；共用Eddict手册21个章节域、12个App本地设置、下载页4个范围项。译文合并定位，父级/标签单列便于检索；114不是功能完成数。官方M1指南、商业评测、伴随App目录分别标注，不合成一份“当前M1全菜单”。本批没有新增F，只补F01/08/10。

| 本轮优先问题 | 补到的依据 | 仍不能下的结论 |
| --- | --- | --- |
| 断点与计时 | M1评测列Memory playback的Off/Track/Position，以及Sleep Timer/Auto-standby/Auto-Shutdown；Eddict p9/15有同类App设置。 | **Eddict p15明确播放设置只作用于App本地播放，对连接的山灵设备无影响。**不能在手机设定后声称已设置M1计时/断点；设备保存时点、断电粒度、计时重置和结束动作仍未知。 |
| 歌单/文件管理 | 原评测补到指定文件夹扫描、歌单导出能力和创建顺序的描述。 | 仍无官方M1队列编辑/删除确认链；App可加队列不等于M1具备同样队列。导出再导入不是本项目验证过的无损排序/恢复办法。 |
| SyncLink | 官方共用App手册补控制端、设备搜索、连接成功反馈、iOS只做控制端；英文提必要时Music Scan。 | 这些是App侧补证，M1设备入口仍靠既有型号评测；重新扫描作用对象、权限拒绝、配对失败/重试、退出/后台后恢复未知。 |
| 网络服务与传歌 | 共用手册把Wi-Fi传歌、SyncLink、DLNA及投屏分开；英文提供App接收页的电脑浏览器步骤。 | 不能把手机接收地址/目录套成M1 Wi-Fi传歌流程；不能将streamer投屏示例、手机NAS/云下载迁移为M1原生能力。 |

上述M1菜单名称来自E-SHCD-03商业评测；App范围来自E-SLCD-02 p2、5–6、9、12–15。只是菜单或声明的条目保持D1/目录，不为数量新建流程。

## 任务流程

| F 编号 | 任务/前置 | 入口、操作及已知反馈 | 数据/结束与未知 | E 证据 |
| --- | --- | --- | --- | --- |
| F-M063-01 | 电脑文件入库；卡和数据线 | 下拉 USB Storage→复制到卡→断开→Update Library/Scan music now→All Songs。 | 文件与索引分离；评测另描述可选指定文件夹扫描，不能因此补出过滤/去重。空卡、重名、取消和完成提示待查。 | E-SHAN-01；E-SHCD-01；E-SHCD-03 |
| F-M063-02 | 实体播放、关屏与锁键 | 播放键暂停/继续；前后键短按切曲、长按寻位；按电源/锁屏轮关亮屏，旋转调音量。作者自报V2.1的评测另列 System Settings/Button lock 的三种熄屏锁定范围。 | 关屏不等于暂停。锁键菜单来自作者自报V2.1的评测，非本项目实测；上一曲/回曲首阈值、当前默认值和解锁后状态未知。 | E-SHAN-01；E-SHCD-01；E-SHCD-03 |
| F-M063-03 | USB DAC 输入；电脑已连接 | 下拉选 USB DAC；Storage、USB AUDIO 为其他角色。 | Windows驱动要求见指南；DAC退出、断线及原曲恢复未知。 | E-SHAN-01 |
| F-M063-04 | 卡内升级；电量充足 | update.bin 解压到卡根目录→System Update/TF card update→确认→重启后 About 核对。 | 不证明升级保留歌单/进度；未执行固件。 | E-SHAN-02 |
| F-M063-05 | OTA 与更新失败 | System Update/Network update；已最新版则停止；卡更新不启动检查解压、根目录及损坏。中国站 V1.8 另列可用 Wi-Fi、卡空闲至少200MB。 | 200MB仅按 V1.8 文档记条件，不无条件推广至 V2.1；断网续传/取消未知。 | E-SHAN-02；E-SHAN-03；E-SHCD-02 |
| F-M063-06 | 无响应恢复 | 电源长按约10秒强制关闭，再正常开机。 | 文档秒数非实测；不是清除数据，写入中断后果未知。 | E-SHAN-01 |
| F-M063-07 | 排序、格式化（D1） | V2.1 记录新增能力。 | 只有更新声明；入口、删除范围、撤销待查。 | E-SHAN-02 |
| F-M063-08 | 手机遥控（蓝牙分支D2）；网络曲源其余D1 | Kajetblog：M1 Settings/Wireless Settings/SyncLink 选 Bluetooth；手机 Eddict 从左缘打开菜单→SyncLink→可用设备选 M1 Plus→浏览设备曲库并控制。作者 Wi-Fi 分支失败改用蓝牙。 | 控制设备不等于复制/备份内容。共用App手册补控制端搜索与连接反馈，且播放设置不改变M1设置；具体条件/冲突见下节。AirPlay、DLNA、Tidal账户和离线仍未闭合。 | E-SHAN-02；E-SHAN-04；E-SHCD-04；E-SLCD-01～03 |
| F-M063-09 | 找回播放页、歌词与曲目信息（D2） | HiFi Oasis：主屏底部 Now Playing 进播放页；封面左滑到歌词，再左滑到信息；其他页面可用竖条图标回播放页（主页例外）。 | 改变视图，未证明更改文件；无歌词/长信息、返回原筛选位置未知。省略号内 AB/速度只有入口，不算完整流程。 | E-SHCD-03 |
| F-M063-10 | 外部 M3U 歌单导入（D2） | 同评测：音频已在卡；引用使用 A:\ 路径，M3U 放卡根目录 _explaylist_data→Playlist/Import playlist；路径正确时列出导入歌单。 | 建立对现有文件的引用，不等于导入音频。作者另说可导出，歌单依创建次序、后添在末尾；不能当当前固件排序保证。导出步骤/文件名、M3U8/相对路径、重导冲突、失效项和换卡仍未知；未执行示例脚本。 | E-SHCD-03 |

## SyncLink补证：App与设备分层

官方Eddict手册V1.2中文p6：手机入口为设备控制→SyncLink，允许蓝牙提示，打开控制端后自动搜索，选设备连接，文档描述显示连接成功，然后控制设备本地音乐；iOS只能做控制端。英文p12另说设备先启用SyncLink，并可能需Music Scan才能正确显示曲库。**这是共用操作说明，本批没有在M1 Plus上确认成功反馈、菜单版本或扫描对象。**原F08的M1入口仍来自Kajetblog型号评测，不能升级成已实测或型号专用官方全链。[E-SLCD-02；E-SHCD-04]

三项范围差异保留：

- 手册p2明确非单一型号；官网App下载页虽更新到Android2.3.7，其SyncLink名单仍未列M1 Plus。旧M1产品介绍又明确支持Eddict/SyncLink，故缺项是资料覆盖差异，不能据此说不支持。没有把2.3.7当作手册截图构建，也未推定iOS版本。[E-SLCD-01/02；E-SHAN-04]
- 英文p12要求双方蓝牙和Wi-Fi打开，中文p6先讲蓝牙，再称Wi-Fi模式同理；旧M1评测报告两无线不能同时打开。本文不把共用英文前提直接作为M1必做步骤，也不据评测报告解释硬件原因。[E-SLCD-02；E-SHCD-03]
- 中文p5“电脑传歌”段与手机段内容重复；英文p12给出同网后电脑浏览器打开App所示地址的步骤。它没有给M1设备自己的网页入口/保存目录。DLNA选择媒体渲染设备与SyncLink控制设备本地音乐不同；p13无线投屏明确以streamer示例说明，不能补成M1镜像操作。[E-SLCD-02]

本次实际查看[App手册p12](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p12.png)和[p15](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p15.png)：前者展示Controller开关及已连接/已配对/可用分组，示例连接SCD3.3；后者同时显示App计时/Resume等设置和“对连接设备无影响”的正文，下方USB授权示例是UA7。**都不是M1 Plus界面观察**，不能用来证明该机配对、权限或DAC连接成功。完整范围摘记见[本批短摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/source-notes.md)。

## 修正、分支与剩余未知

**撤回旧“中文有独立电源键、英文说音量轮”的冲突判断。**实际查看中文部件图第5页，电源/锁屏与音量标签指向同一旋钮组；第6页用“电源键”称其按压动作。不能从称谓差异推断独立按键。根代理已独立查看该图。上一曲回曲首的触发阈值仍待查。[E-SHCD-01]

| 分支 | 能确定的范围 | 缺少的恢复证据 |
| --- | --- | --- |
| 手机找不到/连不上 | 一篇评测报告 Wi-Fi遥控失败、改蓝牙成功 | 配对权限、错误文字、重试与清除配对；不能据单次失败称不支持。 |
| 断点/自动停止 | 旧评测列出记忆模式及三个 Idle 项 | 保存粒度、跨故事恢复、重启行为、计时重置和结束状态。 |
| 路径变化或删除 | 外部歌单依赖指向音频的路径（研究分析） | 失效条目提示、重新链接、取消/撤销及文件实际删除范围。 |
| 网络、插拔或切输出 | 手册区分 USB 角色，评测给出部分遥控入口 | 同时连接优先级、音量保存、手机来电、原播放源恢复。 |

商业评测报告的 Wi-Fi/蓝牙互斥等行为仍需固定固件复核，不作为当前设备保证。网络服务账户、订阅、缓存、授权到期和退出清理均未闭环；未知不等于不支持。没有取得 M1 Plus 当前完整高级手册，不以其他山灵型号补齐。

## 项目分析与下一项核查

以下为推断：G-002/R-021 需区分文件、扫描结果、歌单引用、播放视图和出口；AI 应能说清“导入歌单”是否也复制了音频。R-018 所需的逐故事进度不能仅用 Memory playback 名称承诺。锁屏、锁键、暂停和关机是不同状态，儿童自主听故事时尤其需要可辨认的状态反馈；成人编辑歌单则需保留路径和撤销信息。这些是候选交互启发，不构成 Plus 的硬件/固件能力或新增需求。

下一最小验证：固定 V2.1 与 Eddict 构建，以两个长音频和一份含失效项的歌单检查导入→查找→切故事→关屏锁键→重启→恢复，单独观察计时。需记录屏幕、按键、真实文件、队列和时间点；不因本轮目录更完整而提高为实测。

## 证据

旧证据查于2026-10-26；本批2026-10-27本地复读M1指南、HTTP200复抓中国V1.8页及HiFi Oasis正文，建议追加到旧E。英文/download/118和两篇旧Eddict/MTouch帮助普通GET返回403后停止，未用别的域名/接口绕过。新取得Eddict官网页和手册均HTTP200；没有下载App/固件。重复托管及派生静态页不是独立来源。

| E 编号 | 来源、日期与定位 | 获取/限制 |
| --- | --- | --- |
| E-SHAN-01 | [官方多语快速指南](https://en.shanling.com/catlog/M1_Plus-V1.0.pdf) | 旧文本；本次以 SHCD-01 修正文件页码及旋钮解释。 |
| E-SHAN-02 | [英文下载页 V2.1](https://en.shanling.com/download/118) | 发布日期未明；更新/本地升级。 |
| E-SHAN-03 | [英文固件更新](https://en.shanling.com/article/101223) | V2.1 OTA及失败分支。 |
| E-SHAN-04 | [M1 Plus 官方介绍](https://newen.shanling.com/article-IntroM1Plus.html) | 网络能力仅D1。 |
| E-SHCD-01 | [中国站同型号快速指南](https://shanling.com/uploads/image/M1_Plus-V1.0.pdf)，文件5–9、11–15 | HTTP200下载；PyMuPDF全文及单帧查看；[第5页](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/m1plus-pdf-05.png)。日期/固件未知。 |
| E-SHCD-02 | [中国站 M1 Plus 下载页](https://shanling.com/download/134)，V1.8 | 官方网页正文；含空间条件和旧版更新项，不替代英文V2.1。 |
| E-SHCD-03 | [HiFi Oasis M1 Plus Review](https://www.hifioasis.com/reviews/shanling-m1-plus-review/)，2025-09-11，页面更新2026-05-11 | HiFi-Dawson自述购机；有佣金链接。只读作者正文；MTouch OS and UI、Syncing Music & Playlists；Overall User Experience 自报已升级V2.1，截图未逐项核版。 |
| E-SHCD-04 | [Kajetblog M1 Plus 体验](https://kajetblog.com/m1-plus/)，2025-02-03 | 作者かじかじ，标PR:MUSIN；SyncLink/AirPlay。iPhone15Pro，Eddict/固件未知；作者观察，不是本项目。 |
| E-SHCD-05 | [MUSIN V2.0 发布稿](https://prtimes.jp/main/html/rd/p/000000087.000139013.html)，2025-01-20 | 地区代理发布，PR Times托管；Folder Album Cover开关。未读取其返回444的站内链接。 |
| E-SLCD-01 | [Eddict官方下载页](https://shanling.com/download/76)，Android2.3.7，页面发布日期2026-09-11 | 官方App版本/功能及机型名单；名单未列M1不推出不支持；只有正文读取。 |
| E-SLCD-02 | [Eddict官方使用说明书](https://shanling.com/uploads/image/EddictPlayerAPP.pdf)，16页、末页文档V1.2 | 官网说明书链接；日期/对应App构建未标，全文文本已读；p2/15限定范围。 |
| E-SLCD-03 | 同一Eddict PDF p12/15静态摘页，derived_from E-SLCD-02 | 两张实际view_image；其他机型示例，不重复计算独立来源，无连续操作。 |
