# M-064 iBasso DX180：Mango 播放、音频路由与计时版本变化

## 范围与证据方式

查阅2026-10-26；补查2026-10-27。固定 **DX180、Android 与 Mango Music Player app**。历史63页官方手册的 About 截图显示 Android 13；它不是当前系统升级状态证明。手册修订号、Mango构建和实际设备版本未知。2026-10-27中国官网页首条目为 **1.05.273，2026-08-27**，与英文站V1.05日期相符；当前设置不能全部沿用旧手册。[E-IBA-01～03；E-IBCD-01/02/08]

目录的 Power Off/Reboot/System Switching 对应正文只描述关机/重启，不能推导有独立 Mango OS。未借 DX260/DX340 菜单。D2 为文档重建；仅查看指定官方PDF静态摘页，无连续流程、实机操作、APK/固件执行和耗时测试。镜像和同一PDF的不同页不作独立来源计数。

## 功能目录与覆盖

以下按 **手册印刷页**；PDF文件页=印刷页+1。已阅读全文及选定图页，目录不是当前固件全量验收。前段为包装2–4、特点5–6、规格7–8；操作范围如下。

| 功能域 | 手册章节/后续更新 | F / E 对应 | 缺口与未覆盖 |
| --- | --- | --- | --- |
| 物理键、端口、电源 | 部件9–13；关机26 | F07/12；IBA-01、IBCD-01/06 | 熄屏侧键与旋钮策略不同；当前完整锁键规则、强制重启时进度未知。 |
| Android导航、网络 | Android14；Wi-Fi15；Bluetooth16 | IBCD-01；F05仅接收支线 | 普通蓝牙发射与Bluetooth DAC不能合并；通知/音频焦点、断连重配缺少状态链。 |
| 应用、文件、USB用途 | 安装17、卸载18、复制19–20 | F01/13–15；IBCD-01 | 本轮补安装、卸载和对外供电文档链；权限拒绝、卸载数据保留、当前商店可用性及USB退出未知。 |
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

2026-10-27新增 [逐项台账](../batches/ibasso-controls-deepening-inventory.csv)：原目录13项（含父子项）、正文功能域41项、实际查看的静态菜单37项、中文版本声明37项及日本区域解释1项。五类合计129行有重叠，**不是129个功能完成或独立任务**。正文域以印刷页逐项关联；菜单项只表示看见入口。原PDF元数据生成/修改日均为2024-06-13、时区不同，不能充当发布日期或修订号。未取得比这份历史手册更新的完整指南。

## 任务流程

| F 编号 | 任务/前置 | 操作与文档/图示反馈 | 数据、返回和剩余缺口 | E 证据 |
| --- | --- | --- | --- | --- |
| F-M064-01 | 导入、扫描、查找 | USB复制（历史Mac需MTP工具）→Mango Settings/Media Scan；Music左向右开分类/文件与底部搜索，关键词范围曲目/专辑/艺术家。All Music可按Title/Album/Artist/Time/Folder排序，Position定位当前文件；目录Return逐级返回。 | 图中28%是印刷示例；短时长/小文件过滤文案冲突仍留。Time含义、ID3编辑、空结果/清搜索历史、完成/取消、重复和路径修复未知。 | E-IBA-01；E-IBA-02；E-IBCD-04；E-IBCD-01 |
| F-M064-02 | 移出当前队列 | 播放页右下 Now Playing→歌曲左滑→Delete。 | 仅此段确定移出当前列表；不是存储文件删除保证，当前项移除后续播/撤销未知。 | E-IBA-01 |
| F-M064-03 | 创建/管理/导出歌单 | New Playlist填名/说明；歌曲长按加入；歌单长按重命名、导出或删除，可按标题/创建/更新排序。 | 导出内部 iBasso/mango player；Scan M3U Playlist只是扫描选项，不能当已验证恢复链。格式、覆盖及音频/进度备份未知。 | E-IBA-01；E-IBCD-04 |
| F-M064-04 | 连续播放与拔线暂停 | Folder Play需顺序/循环及完整扫描；目录选曲后跨文件夹续播；Unplug Pause控制拔耳机暂停。 | 重插、蓝牙断连、app/通知打断与原进度未知。 | E-IBA-01 |
| F-M064-05 | 手机作为音源 | 播放页Settings→Bluetooth DAC→手机配对/播放。 | 接收章节列AAC/SBC；发射规格不能套用。退出、来电、音量继承仍缺。 | E-IBA-02；E-IBCD-07 |
| F-M064-06 | 计时与版本核对 | **历史手册** Sleep Timer以时/分设定，操作后重计、到时关机。**1.04.237**新增系统Settings内定时关机，Mango计时行为变更；**1.05.273**为定时关机增加“始终有效”。 | 历史D2仍留档；现版只知设置域及变化，未证明当前操作会重计。原文疑点见下节；日本MUSIN v1.04另明确Mango定时停止播放，只作区域解释。两计时关系、当前结束反馈/取消/重计仍缺。 | E-IBA-02；E-IBCD-08；E-IBDC-01 |
| F-M064-07 | 实体控制/无响应 | 播放暂停、前后键长按寻位；电源短按关屏，冻结时约10秒重置。 | Audio Settings有Volume Control Knob；1.02.181提到关闭侧键的修复，但不提供完整设置链。强制重启前最后保存点未知。 | E-IBA-01；E-IBCD-06；E-IBCD-08 |
| F-M064-08 | 更新并核对版本（D2文档） | 旧册OTA：Wi-Fi→Check Update→Download→Update Now；图中安装确认为INSTALL/CANCEL。中国站给本地路径：Settings/System/System Update/Local Upgrade选卡根包；菜单不显时调整字体。之后About DX180查Build number。 | 海外RAR先解压一次、百度网盘ZIP不解压，不能一概解压。当前发布1.05.273；未执行更新。校验、保留范围、失败回退与当前界面差异仍未知。 | E-IBA-03；E-IBA-01；E-IBCD-08；E-IBCD-09；E-IBCD-02 |
| F-M064-09 | 播放视图与曲目信息（D2） | 封面左右滑切曲；左上Music，右上Settings；Music子页底部缩略图回播放。Music Info展开底部信息，点上半部收回。 | 视图返回与切曲分别处理；Info Delete范围未知。Recent Play仅列50首最近曲目，年份标签只读ID3显示；均不证明时间断点、清历史或ID3编辑。 | E-IBCD-01；E-IBCD-03 |
| F-M064-10 | 编辑/停用EQ滤波器（D2） | Settings/Equalizer→启用GEQ或PEQ→点响应曲线。GEQ滑杆；PEQ滑选滤波器，编辑类型/Fc/Gain/Q，可单独On/Off。 | GEQ五预设/自定义及10段；PEQ每设置最多6滤波器、8类型，Fc 33–16000Hz、Gain −20–20dB、Q 0.3–20。低通/高通/全通/Band/Notch类型的Gain不生效。越界反馈、保存/重置/持久化未证；不推音质收益。 | E-IBCD-01；E-IBCD-10 |
| F-M064-11 | 进入/退出USB DAC（D2） | Settings/USB DAC；图示Enter DAC Mode含Charging mode、CANCEL/CONFIRM；进入页有Back To DAP Mode。 | 取消入口可见但未触发；返回按钮只证明模式出口，不证明原曲自动继续。电脑输出选择、断线/后台干扰及音量待查；驱动条件限手册平台说明。 | E-IBCD-05；E-IBCD-07 |
| F-M064-12 | 模拟输出与同轴分支（D2文档） | 系统Audio Settings的Output；手册区分PO接耳机、LO接外部放大器。接入同轴时手册说关闭phone/line输出。 | 这是文档路由分支；PO/LO实际切换反馈、拔同轴恢复及与USB/蓝牙同时连接未知。 | E-IBCD-01；E-IBCD-06 |

## 本轮新增的应用与USB任务

以下只重建目标型号手册的 D2；没有安装、卸载、连接硬件或使用下载服务。三项均沿用旧 E-IBCD-01，新静态采集提交追加元数据，不把同一PDF重新编号。

| F 编号 | 前置、入口与动作 | 文档终点、分支与未知 |
| --- | --- | --- |
| F-M064-13 安装应用 | 印刷17给两条历史路径：打开预装 Google Play/Aptoide 检索后下载/安装；或桌面浏览器进入应用网站下载，再回桌面下载管理点击已下载应用安装。 | 文档到安装动作；图只有浏览器和空下载列表示例，无成功、授权或失败画面。当前商店可用性、登录要求、权限拒绝、取消、包兼容性和运行反馈未知。 |
| F-M064-14 卸载指定应用 | 印刷18：桌面 Settings→Apps→选择目标应用→Uninstall。静态并排图到 App info，另见 Open/Force stop、权限及存储入口。 | 文字把终点描述为删除App，图没有确认或完成状态。不能据此保证删除外部音频、歌单、账户、离线内容；预装限制、误删/取消和重装恢复未证。强停/清存储仅菜单，未造操作链。 |
| F-M064-15 为外接设备供电 | 印刷20：连接对端→下拉通知栏底部 USB 项→选连接用途→启用为所连设备充电。 | 文档描述设备开始获得供电；该页纯文字，未观察USB选项或充电反馈。关闭/拔线、低电量规则、设备兼容、电流、与文件传输/DAC并行关系未知。不能把“连接USB”当固定音频角色。 |

本轮实际查看印刷17、18、20、21、34、42（文件18、19、21、22、35、43）六页：分别为安装、卸载、供电纯文字、系统菜单、Mango侧栏、搜索页。它们不是连续操作。21页Backup/Reset仅可见入口，42页搜索历史/删除样式图标不证明清除规则；未依据通用Android补菜单或恢复流程。具体图注和散列见 [短摘记](../artifacts/ibasso-controls-deepening/source-notes.md)。

## 重要修正与版本分支

旧 F06 的“操作重新计时、到时关机”只能归于未标修订的手册印刷页61，不能再代表1.05.273。中国官网 **1.04.237（2025-07-04）第1、2项**把系统定时关机与 Mango 变化分列；第2项原文为“**MangoPlayer中的定时关机更改为定时暂时播放。**”其中“暂时播放”疑有笔误，本例不无条件改写成已证实“暂停播放”。**1.05.273（2026-08-27）第5项**新增“始终有效”选项，也没有交代它对每种操作或后台事件的规则。现版计时需同时记录位置、版本、选项和结束动作。根代理已独立获取并核对上述原文。[E-IBCD-08]

2026-10-27补查日本MUSIN的2025-07-22一手公告：DX180 **v1.04** 段明确将Mango休眠计时改为“按计时停止播放”，并分列系统关机计时。iBasso经销名录将MUSIN列为日本Distributor；此为有身份依据的区域说明，不是中国站笔误的直接更正，也没有补出1.05的控制链。该稿DX340段的Mango OS不能套到DX180。中文v1.0（2024-06-26）标题与英文V1.01同日但写法不同，台账保留原名；1.02的应用固定/在线歌词/艺术家二级专辑页，以及1.05的ReplayGain/局域网/降级仍仅D1声明。[E-IBDC-01；E-IBCD-08；E-IBA-03]

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

旧材料查于2026-10-26，本轮复读及补查2026-10-27。官方PDF原件在 `/home/workspace/reader/work/hifi-extension-research/DX180userguide.pdf`，63页；本轮用既有PyMuPDF提取并看选定摘页。以下不同图页共用一份手册，不扩大引用预算或构成连续操作。

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


| 本轮新来源 | 定位与取得边界 |
| --- | --- |
| E-IBDC-01 [MUSIN在PR TIMES发布的DX180 v1.04公告](https://prtimes.jp/main/html/rd/p/000000144.000139013.html) | 2025-07-22 17:00，DX180(v1.04)第1–2条；本地原HTML可核。品牌[经销名录](https://ibasso.com/where-to-buy/)仅用于确认MUSIN关系。相同机构站内新闻经web正文可读，但直接HTTP断开、无该URL本地原件；下游支持页444后停止。 |

本轮只新增1E/3F；原F01/06/09/10共14字段补证建议及E-IBCD-01/08、E-IBA-03的追加采集建议在 `work/ibasso-controls-deepening/`，由根合并旧批。未改旧批与主表。固件HTML、PDF元数据和图页原件SHA均登记；未下载APK/固件或执行外部产品动作。
