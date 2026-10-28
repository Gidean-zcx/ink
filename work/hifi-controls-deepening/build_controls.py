from pathlib import Path
import csv, json, shutil, hashlib
ROOT=Path('/home/workspace/reader')
lib=ROOT/'docs/research/interaction-library'
w=ROOT/'work/hifi-controls-deepening'
a=lib/'artifacts/hifi-controls-deepening'; a.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sh='''# M-063 山灵 M1 Plus：文件、播放页、遥控与版本边界

## 范围与证据方式

查阅日期2026-10-26。固定 **M1 Plus、非 Android 的 MTouch**，英文官网 V2.1 为发布基线；中国站本次可读下载页停在 V1.8，区域页面不同步，不能据此判定设备版本。快速指南 V1.0 是文档修订号。另用两篇明确标注为第一手体验的商业评测补旧界面入口，所用固件均未知；不把评测菜单拼成 V2.1 的完整菜单。M1/M1s、M0 与山灵 Android 产品均排除。[E-SHAN-01～04；E-SHCD-01～05]

本例 D1/D2；未购买、安装、登录、连接设备或测量。实际查看官方指南文件第5、6页静态图，归档第5页；没有连续视觉流程或实机操作。独立评测中的体验是作者的观察，不是本项目实测。账户、地区服务可用性及 Eddict app 构建未知。

## 功能目录与覆盖

官方快速指南完整目录只覆盖安全/规格、部件、控制与接口、升级、常见问题、附件/保修及其译文；它不是 MTouch 高级手册。下表补入已查设置目录，同时把仅有名称和已分析任务分开。页码统一为 **PDF 文件页，从封面起1计数**：中文部件5、控制6、连接7、升级8、故障9；英文部件11、控制12、连接13、升级14、故障15。旧记录少计一页的定位由此修正。[E-SHCD-01]

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

表中 E 简写均保留各自前缀：SHAN 为旧证据，SHCD 为本批。目录出现不提高任务覆盖等级。

## 任务流程

| F 编号 | 任务/前置 | 入口、操作及已知反馈 | 数据/结束与未知 | E 证据 |
| --- | --- | --- | --- | --- |
| F-M063-01 | 电脑文件入库；卡和数据线 | 下拉 USB Storage→复制到卡→断开→Update Library/Scan music now→All Songs。 | 文件与索引分离；空卡、重名、取消和完成提示待查。 | E-SHAN-01；E-SHCD-01 |
| F-M063-02 | 实体播放、关屏与锁键 | 播放键暂停/继续；前后键短按切曲、长按寻位；按电源/锁屏轮关亮屏，旋转调音量。旧评测另列 System Settings/Button lock 的三种熄屏锁定范围。 | 关屏不等于暂停。锁键菜单来自固件未知评测；上一曲/回曲首阈值、当前默认值和解锁后状态未知。 | E-SHAN-01；E-SHCD-01；E-SHCD-03 |
| F-M063-03 | USB DAC 输入；电脑已连接 | 下拉选 USB DAC；Storage、USB AUDIO 为其他角色。 | Windows驱动要求见指南；DAC退出、断线及原曲恢复未知。 | E-SHAN-01 |
| F-M063-04 | 卡内升级；电量充足 | update.bin 解压到卡根目录→System Update/TF card update→确认→重启后 About 核对。 | 不证明升级保留歌单/进度；未执行固件。 | E-SHAN-02 |
| F-M063-05 | OTA 与更新失败 | System Update/Network update；已最新版则停止；卡更新不启动检查解压、根目录及损坏。中国站 V1.8 另列可用 Wi-Fi、卡空闲至少200MB。 | 200MB仅按 V1.8 文档记条件，不无条件推广至 V2.1；断网续传/取消未知。 | E-SHAN-02；E-SHAN-03；E-SHCD-02 |
| F-M063-06 | 无响应恢复 | 电源长按约10秒强制关闭，再正常开机。 | 文档秒数非实测；不是清除数据，写入中断后果未知。 | E-SHAN-01 |
| F-M063-07 | 排序、格式化（D1） | V2.1 记录新增能力。 | 只有更新声明；入口、删除范围、撤销待查。 | E-SHAN-02 |
| F-M063-08 | 手机遥控（蓝牙分支D2）；网络曲源其余D1 | Kajetblog：M1 Settings/Wireless Settings/SyncLink 选 Bluetooth；手机 Eddict 从左缘打开菜单→SyncLink→可用设备选 M1 Plus→浏览设备曲库并控制。作者 Wi-Fi 分支失败改用蓝牙。 | 控制设备不等于复制/备份内容。AirPlay 只定位 Network Services/Wi-Fi、AirPlay 开关；完整发起/退出、DLNA、Tidal账户和离线未查明。 | E-SHAN-02；E-SHAN-04；E-SHCD-04 |
| F-M063-09 | 找回播放页、歌词与曲目信息（D2） | HiFi Oasis：主屏底部 Now Playing 进播放页；封面左滑到歌词，再左滑到信息；其他页面可用竖条图标回播放页（主页例外）。 | 改变视图，未证明更改文件；无歌词/长信息、返回原筛选位置未知。省略号内 AB/速度只有入口，不算完整流程。 | E-SHCD-03 |
| F-M063-10 | 外部 M3U 歌单导入（D2） | 同评测：音频已在卡；引用使用 A:\\ 路径，M3U 放卡根目录 _explaylist_data→Playlist/Import playlist；路径正确时列出导入歌单。 | 建立对现有文件的引用，不等于导入音频。M3U8/相对路径、重复导入、失效项、换卡恢复及版本变化未知；未执行示例脚本。 | E-SHCD-03 |

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

全部查于2026-10-26。旧证据保持可追溯；新记录不修改旧批次。重复托管的同一指南不是独立来源。

| E 编号 | 来源、日期与定位 | 获取/限制 |
| --- | --- | --- |
| E-SHAN-01 | [官方多语快速指南](https://en.shanling.com/catlog/M1_Plus-V1.0.pdf) | 旧文本；本次以 SHCD-01 修正文件页码及旋钮解释。 |
| E-SHAN-02 | [英文下载页 V2.1](https://en.shanling.com/download/118) | 发布日期未明；更新/本地升级。 |
| E-SHAN-03 | [英文固件更新](https://en.shanling.com/article/101223) | V2.1 OTA及失败分支。 |
| E-SHAN-04 | [M1 Plus 官方介绍](https://newen.shanling.com/article-IntroM1Plus.html) | 网络能力仅D1。 |
| E-SHCD-01 | [中国站同型号快速指南](https://shanling.com/uploads/image/M1_Plus-V1.0.pdf)，文件5–9、11–15 | HTTP200下载；PyMuPDF全文及单帧查看；[第5页](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/m1plus-pdf-05.png)。日期/固件未知。 |
| E-SHCD-02 | [中国站 M1 Plus 下载页](https://shanling.com/download/134)，V1.8 | 官方网页正文；含空间条件和旧版更新项，不替代英文V2.1。 |
| E-SHCD-03 | [HiFi Oasis M1 Plus Review](https://www.hifioasis.com/reviews/shanling-m1-plus-review/)，2025-09-11，页面更新2026-05-11 | HiFi-Dawson自述购机；有佣金链接。只读作者正文；MTouch OS and UI、Syncing Music & Playlists。固件未知。 |
| E-SHCD-04 | [Kajetblog M1 Plus 体验](https://kajetblog.com/m1-plus/)，2025-02-03 | 作者かじかじ，标PR:MUSIN；SyncLink/AirPlay。iPhone15Pro，Eddict/固件未知；作者观察，不是本项目。 |
| E-SHCD-05 | [MUSIN V2.0 发布稿](https://prtimes.jp/main/html/rd/p/000000087.000139013.html)，2025-01-20 | 地区代理发布，PR Times托管；Folder Album Cover开关。未读取其返回444的站内链接。 |
'''
ib='''# M-064 iBasso DX180：Mango 播放、音频路由与计时版本变化

## 范围与证据方式

查阅2026-10-26。固定 **DX180、Android 与 Mango Music Player app**。历史63页官方手册的 About 截图显示 Android 13；它不是当前系统升级状态证明。手册修订号、Mango构建和实际设备版本未知。中国官网最新条目为 **1.05.273，2026-08-27**，与英文站V1.05日期相符；当前设置不能全部沿用旧手册。[E-IBA-01～03；E-IBCD-01/02/08]

目录的 Power Off/Reboot/System Switching 对应正文只描述关机/重启，不能推导有独立 Mango OS。未借 DX260/DX340 菜单。D2 为文档重建；仅查看指定官方PDF静态摘页，无连续流程、实机操作、APK/固件执行和耗时测试。镜像和同一PDF的不同页不作独立来源计数。

## 功能目录与覆盖

以下按 **手册印刷页**；PDF文件页=印刷页+1。已阅读全文及选定图页，目录不是当前固件全量验收。前段为包装2–4、特点5–6、规格7–8；操作范围如下。

| 功能域 | 手册章节/后续更新 | F / E 对应 | 缺口与未覆盖 |
| --- | --- | --- | --- |
| 物理键、端口、电源 | 部件9–13；关机26 | F07/12；IBA-01、IBCD-01/06 | 熄屏侧键与旋钮策略不同；当前完整锁键规则、强制重启时进度未知。 |
| Android导航、网络 | Android14；Wi-Fi15；Bluetooth16 | IBCD-01；F05仅接收支线 | 普通蓝牙发射与Bluetooth DAC不能合并；通知/音频焦点、断连重配缺少状态链。 |
| 应用、文件、USB用途 | 安装17、卸载18、复制19–20 | F01；IBCD-01 | 文档列应用商店/APK途径，未安装；权限拒绝、卸载数据保留、当前商店可用性未知。 |
| 系统、音频、升级、版本 | Settings21；Audio22；Update23–24；About25 | F08/12；IBCD-02/06/09 | Backup/Reset仅菜单；音量限制、旋钮设置有入口，未取得所有选项或备份范围。 |
| 播放页、当前队列、信息 | Mango27–29 | F02/09；IBCD-03 | Music Info的Delete对象待查；回到队列/原筛选的精确位置未知。 |
| 分类、排序、目录、搜索 | Music30–36；Recent39；Storage40；Return41；Search42 | F01/09；IBCD-01 | Position定位当前文件，非音频时间书签；搜索空结果、标签冲突、坏文件待查。 |
| 持久歌单 | Playlist37–38；扫描57 | F03；IBCD-01/04 | 重命名/导出/排序有文档；同名覆盖、M3U恢复、条目次序与撤销未知。 |
| 播放策略、打断 | Settings43；Folder Play/Unplug Pause44 | F04；IBCD-07 | 新版本来电、其他app抢占、拔线后重插与重启断点无完整证据。 |
| GEQ/PEQ、平衡/滤波 | EQ45–54；L/R55；Digital Filter56 | F10；IBCD-01/10 | 参数编辑可定位；预设持久化/重置、跨app有效范围、听感效果未验证。 |
| 扫描、过滤、索引 | Media Scan57 | F01/03；IBCD-04 | 已读图；两个Skip Short Duration标签与说明不一致，不能自行纠正文案；完成/取消未给出。 |
| DAC与输出 | USB DAC58；Bluetooth DAC59；端口9–10 | F05/11/12；IBCD-05/06/07 | USB/蓝牙/同轴同时连接的全局优先级、音量继承和退出恢复仍缺。 |
| 显示、锁屏、计时、存储 | Display60；Timer61；System Info62 | F06；IBCD-11/08 | Lock Screen Artwork是显示设置，不等于实体键锁。旧计时已发生版本变化。 |
| 旧册以外能力 | 官网1.02.181、1.04.237、1.05.273 | F06/08；IBCD-08 | 应用固定、在线歌词、ReplayGain/局域网等只列更新域；入口/权限/失败不齐，不新增D2。 |

E 简写：IBA为旧证据，IBCD为本批。目录中未配 F 的项目保留为已定位、未完成任务分析。

## 任务流程

| F 编号 | 任务/前置 | 操作与文档/图示反馈 | 数据、返回和剩余缺口 | E 证据 |
| --- | --- | --- | --- | --- |
| F-M064-01 | 导入、扫描、查找 | USB复制→Mango Settings/Media Scan，图示可选内部/SD及Start；Music分类/搜索找曲，All Music的Position定位当前文件。 | 图中有28%示意，非本轮进度测量。短时长/小文件过滤、M3U等见扫描页；空库、完成、取消、重复导入未知。 | E-IBA-01；E-IBA-02；E-IBCD-04 |
| F-M064-02 | 移出当前队列 | 播放页右下 Now Playing→歌曲左滑→Delete。 | 仅此段确定移出当前列表；不是存储文件删除保证，当前项移除后续播/撤销未知。 | E-IBA-01 |
| F-M064-03 | 创建/管理/导出歌单 | New Playlist填名/说明；歌曲长按加入；歌单长按重命名、导出或删除，可按标题/创建/更新排序。 | 导出内部 iBasso/mango player；Scan M3U Playlist只是扫描选项，不能当已验证恢复链。格式、覆盖及音频/进度备份未知。 | E-IBA-01；E-IBCD-04 |
| F-M064-04 | 连续播放与拔线暂停 | Folder Play需顺序/循环及完整扫描；目录选曲后跨文件夹续播；Unplug Pause控制拔耳机暂停。 | 重插、蓝牙断连、app/通知打断与原进度未知。 | E-IBA-01 |
| F-M064-05 | 手机作为音源 | 播放页Settings→Bluetooth DAC→手机配对/播放。 | 接收章节列AAC/SBC；发射规格不能套用。退出、来电、音量继承仍缺。 | E-IBA-02；E-IBCD-07 |
| F-M064-06 | 计时与版本核对 | **历史手册** Sleep Timer以时/分设定，操作后重计、到时关机。**1.04.237**新增系统Settings内定时关机，Mango计时行为变更；**1.05.273**为定时关机增加“始终有效”。 | 历史D2仍留档；现版只知设置域及变化，未证明当前操作会重计。原文疑点见下节；两种计时相互关系、结束动作/反馈须现版确认。 | E-IBA-02；E-IBCD-08 |
| F-M064-07 | 实体控制/无响应 | 播放暂停、前后键长按寻位；电源短按关屏，冻结时约10秒重置。 | Audio Settings有Volume Control Knob；1.02.181提到关闭侧键的修复，但不提供完整设置链。强制重启前最后保存点未知。 | E-IBA-01；E-IBCD-06；E-IBCD-08 |
| F-M064-08 | 更新并核对版本（D2文档） | 旧册OTA：Wi-Fi→Check Update→Download→Update Now；图中安装确认为INSTALL/CANCEL。中国站给本地路径：Settings/System/System Update/Local Upgrade选卡根包；菜单不显时调整字体。之后About DX180查Build number。 | 海外RAR先解压一次、百度网盘ZIP不解压，不能一概解压。当前发布1.05.273；未执行更新。校验、保留范围、失败回退与当前界面差异仍未知。 | E-IBA-03；E-IBA-01；E-IBCD-08；E-IBCD-09；E-IBCD-02 |
| F-M064-09 | 播放视图与曲目信息（D2） | 封面左右滑切曲；左上Music，右上Settings；Music子页底部缩略图回播放。Music Info展开底部信息，点上半部收回。 | 视图返回与切曲分别处理；Info还有加歌单/Delete入口，后者删除范围未知。 | E-IBCD-01；E-IBCD-03 |
| F-M064-10 | 编辑/停用EQ滤波器（D2） | Settings/Equalizer→启用GEQ或PEQ→点响应曲线。GEQ滑杆；PEQ滑选滤波器，编辑类型/Fc/Gain/Q，可单独On/Off。 | 参数/开关作用域有文档；保存、重置、跨模式持久化未知。不能从参数界面推导音质收益。 | E-IBCD-01；E-IBCD-10 |
| F-M064-11 | 进入/退出USB DAC（D2） | Settings/USB DAC；图示Enter DAC Mode含Charging mode、CANCEL/CONFIRM；进入页有Back To DAP Mode。 | 取消不进入；返回按钮只证明模式出口，不证明原曲自动继续。电脑输出选择、断线/后台干扰及音量待查；驱动条件限手册平台说明。 | E-IBCD-05；E-IBCD-07 |
| F-M064-12 | 模拟输出与同轴分支（D2文档） | 系统Audio Settings的Output；手册区分PO接耳机、LO接外部放大器。接入同轴时手册说关闭phone/line输出。 | 这是文档路由分支；PO/LO实际切换反馈、拔同轴恢复及与USB/蓝牙同时连接未知。 | E-IBCD-01；E-IBCD-06 |

## 重要修正与版本分支

旧 F06 的“操作重新计时、到时关机”只能归于未标修订的手册印刷页61，不能再代表1.05.273。中国官网 **1.04.237（2025-07-04）第1、2项**把系统定时关机与 Mango 变化分列；第2项原文为“**MangoPlayer中的定时关机更改为定时暂时播放。**”其中“暂时播放”疑有笔误，本例不无条件改写成已证实“暂停播放”。**1.05.273（2026-08-27）第5项**新增“始终有效”选项，也没有交代它对每种操作或后台事件的规则。现版计时需同时记录位置、版本、选项和结束动作。根代理已独立获取并核对上述原文。[E-IBCD-08]

另外两项静态观察纠正了此前缺口：扫描图印刷57的两行都写 Skip Short Duration，说明分别涉及30秒及500k，保留原标签冲突；印刷25可确认历史截图Android13，不能继续写“手册Android主版本未确认”。印刷22的旋钮设置与印刷60的锁屏封面是不同设置；没有看到选项不能推定熄屏全键锁。[E-IBCD-02/04/06/11]

| 分支/对象 | 已知与边界 | 下一证据 |
| --- | --- | --- |
| 文件/当前列表/持久歌单删除 | 同名Delete不足以证明同一数据范围；只有F02的队列移除含义明确 | 三个入口各自的确认、取消、当前项与恢复。 |
| 扫描排除与重新入库 | 页面提供位置/过滤/M3U等设置 | 忽略文件后的反馈、重新扫描去重、文件移动和旧引用修复。 |
| 切源与打断 | DAC输入、模拟输出、同轴是不同角色 | 来电/通知/应用抢占、同时连接、重插和锁屏后的音量/进度。 |
| 维护与恢复 | 版本可核对，官网提供升级入口；仅列Backup/Reset不足以判断备份 | 歌单/数据库/第三方离线文件的保存范围、回退与恢复失败。 |
| 长内容 | Recent Play、Position、Folder Play均不能证明逐书时间书签 | 章节、AB、速度、跨书断点、强制关机后的时间点。 |

## 项目关系与后续

以下为推断：对G-002/R-021，AI需要报告对象与版本，不应把“列表移除”答成“文件已删”，或把旧版计时重置规则答成现版事实。R-018的持续故事进度需独立于Recent Play和设备出口。对5–6岁儿童，关屏、暂停、计时结束和锁键应有可识别状态；成人可保留参数编辑入口。这是交互建议，既不承诺Plus兼容Android/Mango，也未新增需求。

最小后续为固定1.05.273和Mango构建，先核对系统与Mango两处计时，再完成导入→扫描→队列/歌单→拔线/切源→重启并比较文件、列表和时间点；只需针对这些缺口观察，不以更多目录条目代替任务验证。

## 证据

全部查于2026-10-26。官方PDF原件在 `/home/workspace/reader/work/hifi-extension-research/DX180userguide.pdf`，63页；本轮用既有PyMuPDF提取并看选定摘页。以下不同图页共用一份手册，不扩大引用预算或构成连续操作。

| E 编号 | 来源/定位 | 获取与版本边界 |
| --- | --- | --- |
| E-IBA-01 | [官方手册](https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf) | 旧证据；本轮沿用此前HTTP200原件，当前web打开超时未当作新内容。 |
| E-IBA-02 | [厂家手册镜像](https://www.manualslib.com/manual/3477221/Ibasso-Audio-Dx180.html) | 同源手册，只用正文；印刷61为历史计时，非当前版本。 |
| E-IBA-03 | [英文固件目录](https://ibasso.com/?p=3371) | V1.05，2026-08-27；与中文完整编号交叉定位。 |
| E-IBCD-01 | [官方手册全文](https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf) | 本地PDF提取；目录/页码核对，修订未知。 |
| E-IBCD-02 | 同手册印刷25（文件26） | [About静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-25.png)：历史Android13。 |
| E-IBCD-03 | 同手册印刷28（文件29） | [播放页静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-28.png)。 |
| E-IBCD-04 | 同手册印刷57（文件58） | [扫描静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-57.png)：示意状态非实际扫描。 |
| E-IBCD-05 | 同手册印刷58（文件59） | [USB DAC静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-58.png)：同页多截图非连续观察。 |
| E-IBCD-06 | 同手册印刷22（文件23） | [Audio Settings静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-22.png)。 |
| E-IBCD-07 | 同手册印刷43（文件44） | [Mango Settings静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-43.png)。 |
| E-IBCD-08 | [中国官网DX180固件记录](https://ibasso.cn/support/dx180firmware) | 1.02.181、1.04.237、1.05.273及底部本地升级说明；网页正文，最高2026-08-27。官网身份由品牌页脚/支持导航核对。 |
| E-IBCD-09 | 同手册印刷23（文件24） | [OTA静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-23.png)：示范旧构建；文字Update Now与图INSTALL保留差异。 |
| E-IBCD-10 | 同手册印刷49（文件50），配合45–54正文 | [PEQ参数摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-49.png)：实际看的是说明页，不称已观察参数操作。 |
| E-IBCD-11 | 同手册印刷60（文件61） | [Display静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/hifi-controls-deepening/dx180-printed-60.png)。 |
'''
(lib/'cases/M-063-shanling.md').write_text(sh)
(lib/'cases/M-064-ibasso.md').write_text(ib)
E=[]
def ev(eid,pid,title,url,locator,note,source_type='official_manual',version=None,updated=None,artifact=None,access='opened_text',supersedes=None):
 art=None;digest=None
 if artifact:
  shutil.copy2(w/artifact,a/artifact);art='artifacts/hifi-controls-deepening/'+artifact;digest=sha(a/artifact)
 E.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=source_type,access_mode=access,version_scope=version or ('M1 Plus MTouch; guide revision V1.0, firmware unknown' if pid=='M-063' else 'DX180 historical Android13/Mango manual; document revision and device firmware unknown'),app_build=None,firmware_version=None,source_updated_on=updated,locator=locator,note=note,hands_on=False,visual_observed=bool(artifact),archived_artifact=art,sha256=digest,capture_state='local_pdf_page_render_actually_viewed' if artifact else 'bibliography_and_short_note_only',supersedes=supersedes))
ev('E-SHCD-01','M-063','M1 Plus官方指南：部件与完整目录复核','https://shanling.com/uploads/image/M1_Plus-V1.0.pdf','文件5部件/6控制/7连接/8升级/9故障；英文11–15','HTTP200；24页；PDF SHA256 '+sha(w/'M1_Plus-V1.0.pdf')+'。实际查看文件5/6，归档5；同一旋钮兼电源/锁屏，修正旧独立键误读；旧手册同源而非独立证据。',artifact='m1plus-pdf-05.png',access='official_pdf_text_and_selected_static_pages',supersedes='E-SHAN-01')
ev('E-SHCD-02','M-063','中国站M1 Plus V1.8下载与OTA条件','https://shanling.com/download/134','V1.8更新项；OTA在线升级；说明书链接','中国站本次页V1.8；可用Wi-Fi、TF卡空闲200MB条件限此文档；英文V2.1另列，不拼版本。','official_release_notes','M1 Plus V1.8 Chinese release page; English V2.1 is separate baseline')
ev('E-SHCD-03','M-063','HiFi Oasis M1 Plus第一手评测：UI与歌单','https://www.hifioasis.com/reviews/shanling-m1-plus-review/','MTouch OS and UI；Syncing Music & Playlists','作者HiFi-Dawson自述购机；有佣金链接。发布2025-09-11、页面更新2026-05-11；只读正文，未观察其图/视频。固件未知，不据此构造当前完整界面。','firsthand_commercial_review','M1 Plus reviewed device; firmware unknown; not first-hand by this project','2026-05-11')
ev('E-SHCD-04','M-063','Kajetblog M1 Plus：SyncLink与AirPlay入口','https://kajetblog.com/m1-plus/','SyncLink；AirPlay操作段','作者かじかじ；PR:MUSIN，iPhone15Pro；作者Wi-Fi遥控失败改蓝牙。只读作者正文，无本项目操作或视频下载；不推出当前普遍失败。','firsthand_sponsored_review','M1 Plus; firmware/Eddict build unknown; iPhone15Pro author environment','2025-02-03')
ev('E-SHCD-05','M-063','MUSIN发布M1 Plus V2.0更新','https://prtimes.jp/main/html/rd/p/000000087.000139013.html','M1 Plus V2.0：Folder Album Cover开关','官方地区代理MUSIN发布、PR Times托管；仅作版本化菜单补证，不是制造商独立手册。其站内链接返回444后停止。','official_distributor_release_notes','M1 Plus V2.0 Japan distributor release','2025-01-20')
pdf='https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf'
ev('E-IBCD-01','M-064','DX180完整手册目录及正文复核',pdf,'63文件页；印刷1目录、9–62操作','沿用此前HTTP200原件；SHA256 '+sha(ROOT/'work/hifi-extension-research/DX180userguide.pdf')+'。PyMuPDF全文，截图另分条；无缺字计数不当完整提取证明。本次web打开超时，未从错误页推导内容。',access='existing_official_pdf_extracted_text_read')
pages=[('02',25,'About设备页与Android13','仅证明历史手册示例Android13，不代表当前升级状态。'),('03',28,'Mango播放页布局','布局静态观察；左右切曲/返回行为另依27、29正文。'),('04',57,'Media Scan扫描选项','两个Skip Short Duration标签分别配30秒/500k说明；保留文案冲突，28%是印刷示例。'),('05',58,'USB DAC确认与退出入口','实际查看同页图示CANCEL/CONFIRM、Charging mode、Back To DAP Mode；非连续操作。'),('06',22,'系统Audio Settings','实际图中Output/PO及旋钮、音量限制等入口；未见全旋钮选项，不推断熄屏全键锁。'),('07',43,'Mango Settings功能目录','USB DAC、Bluetooth DAC等入口；不是当前固件所有设置已验收。'),('09',23,'OTA示例状态与安装确认','旧手册图为示范构建，INSTALL/CANCEL与正文Update Now保留措辞差异；没有执行更新。'),('10',49,'PEQ参数说明摘页','实际查看的是参数文字页；编辑步骤依45–54正文，没有观察操作或验证持久化。'),('11',60,'Display显示与锁屏封面','Lock Screen Artwork属于显示设置；不等同实体键锁。')]
for num,p,title,note in pages:
 ev('E-IBCD-'+num,'M-064',title,pdf,f'印刷{p}；PDF文件{p+1}；相关正文见案例',note,artifact=f'dx180-printed-{p:02d}.png',access='existing_official_pdf_page_render_actually_viewed')
ev('E-IBCD-08','M-064','DX180中国官网完整固件变更及本地升级','https://ibasso.cn/support/dx180firmware','1.04.237第1–2项；1.05.273第5项；1.02.181及底部OTA升级方法','保留原文“定时暂时播放”疑误，不无条件改为暂停。历史手册重计规则不能推广到新版本。根代理独立HTTP200归档work/hifi-controls-deepening/root-dx180-firmware.html，SHA256 '+sha(w/'root-dx180-firmware.html')+'；只读未升级。','official_release_notes','DX180 1.02.181 (2024-09-27), 1.04.237 (2025-07-04), 1.05.273 (2026-08-27); separate from historical manual','2026-08-27')
E.sort(key=lambda x:x['evidence_id'])
(lib/'batches/hifi-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
def row(fid,task,eids,branches,gaps,visual=False,depth='D2'):
 pid='M-'+fid.split('-')[1][1:]
 return dict(zip(fields,[fid,pid,task,depth,eids,branches,gaps,'static_reference_only' if visual else 'not_observed','false','cases/M-063-shanling.md' if pid=='M-063' else 'cases/M-064-ibasso.md','2026-10-26']))
new=[row('F-M063-09','找回播放页、歌词与曲目信息','E-SHCD-03','评测记载Now Playing和歌词/信息切换、返回图标；固件未知。','缺歌词、长信息、返回筛选位置；AB/速度仅入口。'),row('F-M063-10','外部M3U歌单导入','E-SHCD-03','音频先在卡、A:\\引用、_explaylist_data及Playlist/Import playlist；作者观察。','相对路径/M3U8、重复导入、失效项和换卡；非当前固件实测。'),row('F-M064-09','播放视图与曲目信息','E-IBCD-01;E-IBCD-03','封面手势切曲；Music/Settings入口；缩略图返回、Info展开/收回。','Info Delete对象、原筛选/滚动位置、当前构建差异。',True),row('F-M064-10','编辑与停用EQ滤波器','E-IBCD-01;E-IBCD-10','GEQ/PEQ从响应曲线进入；PEQ逐滤波器编辑及On/Off。','保存/重置、模式间持久化、跨app范围及音质未验证。',True),row('F-M064-11','进入与退出USB DAC','E-IBCD-05;E-IBCD-07','图示确认/取消、Charging mode和Back To DAP Mode；仅静态。','电脑输出选择、断线、后台干扰、音量与原曲恢复。',True),row('F-M064-12','模拟输出与同轴分支','E-IBCD-01;E-IBCD-06','Output设置；PO耳机/LO外放；手册说明同轴接入关闭phone/line输出。','实际切换反馈、拔同轴恢复、USB/蓝牙同时接入及音量。',True)]
with (lib/'batches/hifi-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fields);wr.writeheader();wr.writerows(new)
old=[row('F-M063-01','电脑文件进入曲库','E-SHAN-01;E-SHCD-01','文件传入后仍需扫描；新版补证修正指南页码。','自动扫描、空卡、重名、路径移动、取消/完成提示。'),row('F-M063-02','实体播放、关屏与锁键','E-SHAN-01;E-SHCD-01;E-SHCD-03','静态图确认同一旋钮兼电源/锁屏；评测提供熄屏锁音量/按键/两者目录。','当前固件锁键默认/完整链、回曲首阈值、耳机线控。',True),row('F-M063-05','在线更新与失败恢复','E-SHAN-02;E-SHAN-03;E-SHCD-02','已最新停止；本地包异常检查；V1.8文档另列TF卡空闲200MB。','空间条件未无条件推广V2.1；断网续传、取消、校验失败。'),row('F-M063-08','手机遥控与网络曲源','E-SHAN-02;E-SHAN-04;E-SHCD-04','D2仅作者蓝牙SyncLink分支；AirPlay入口/DLNA/Tidal其余D1，Wi-Fi失败非普遍结论。','固件/Eddict构建、权限、退出/中断、云服务账户与离线；遥控非传文件。'),row('F-M064-01','导入、扫描并找回文件','E-IBA-01;E-IBA-02;E-IBCD-04','扫描静态图补位置/Start/过滤/M3U；Position定位文件非时间点。','扫描标签冲突保留；授权、完成/取消、重复与路径修复。',True),row('F-M064-03','创建、管理和导出歌单','E-IBA-01;E-IBCD-04','补重命名/删除/排序；输出iBasso/mango player；M3U扫描选项不等于恢复完成。','格式、相对路径、覆盖、撤销与导入恢复；非音频/进度备份。',True),row('F-M064-05','用手机作为音源','E-IBA-02;E-IBCD-07','Settings的Bluetooth DAC入口得到静态补证；接收AAC/SBC。','退出后原曲、手机来电与音量继承；不套发射codec。',True),row('F-M064-06','计时与版本核对','E-IBA-02;E-IBCD-08','历史手册D2为操作重计/到时关机；1.04.237分列系统定时关机和Mango变化，原文暂时播放疑误；1.05.273新增始终有效。','当前Mango结束动作、两计时关系、始终有效含义/完整交互未知；不得概括现版操作重计。'),row('F-M064-07','实体控制与无响应','E-IBA-01;E-IBCD-06;E-IBCD-08','正常关屏与强制重置分开；旋钮设置及历史侧键修复有目录，未确认全键锁。','完整熄屏锁键链、第三方app拦截、最后保存点及未写入数据。',True),row('F-M064-08','更新并核对版本','E-IBA-03;E-IBA-01;E-IBCD-08;E-IBCD-09;E-IBCD-02','D2文档OTA/本地路径；海外RAR解压一次、百度ZIP不解压；历史图与当前1.05.273分开。','校验、升级保留、失败回退、Mango独立版本和当前UI差异；未执行升级。',True)]
updates=[]
base={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
for x in old:
 prev=base[x['flow_id']]
 updates.append(dict(flow_id=x['flow_id'],changes={k:{'old':prev[k],'new':v} for k,v in x.items() if prev.get(k)!=v},replacement_row=x))
(w/'old-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
(w/'scope-manifest.json').write_text(json.dumps({'modified_cases':['cases/M-063-shanling.md','cases/M-064-ibasso.md'],'new_batch_files':['batches/hifi-controls-deepening-evidence.jsonl','batches/hifi-controls-deepening-coverage.csv'],'artifacts':[x['archived_artifact'] for x in E if x['archived_artifact']],'new_evidence':len(E),'new_flows':len(new),'old_flow_updates':len(updates)},ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'new_evidence':len(E),'new_flows':len(new),'old_flow_updates':len(updates),'artifacts':sum(x['visual_observed'] for x in E)},ensure_ascii=False))
