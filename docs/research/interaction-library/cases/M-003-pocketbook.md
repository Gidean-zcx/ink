# M-003 PocketBook InkPad Color 3：阅读、音频与内容状态

## 范围与证据方式

初查2026-10-26；2026-10-27分批复读既有手册补App、设置、阅读与音乐。严格固定 **PB743K3 / InkPad Color 3、官方6.10.2854路径的95页英文手册**，不套InkPad Color、InkPad 3、Eo或手机Reader app界面。原件哈希与旧E-PBK-01一致；PDF元数据日期2025-12-26只是文件时间。前批已取得同型号 **6.10.2854 WW发布说明，2026-01-05**，实际安装构建仍未知。本轮没有重新判定最新固件或联网下载。[E-PBK-01；E-PBCD-01/07]

本例26条任务最高D2，无实机操作、连续视觉观察、登录、发送邮件或固件执行。此前实际查看13、27、29、32、33、37、41、55、62、68、76、77；第45批复读48页正文，另实际看16、31、48、49、57、80、82共7页。全部为静态手册，没有点击结果或耗时测量。本批范围和图文疑点见[独立摘记](../artifacts/pocketbook-reading-media/source-notes.md)；同源摘页不算独立互证。通用FAQ只作此前有限互证，不以其重启秒数、SD卡或其他型号步骤替代本册。

## 结构化竞分（2026-10-28）

- 对象边界（导出/本地副本/云条目/阅读点/音频点）：
  - 导出：Notes 导出为设备上的文件（默认 Library/Notes）；未给“回导可编辑”的保证；Scribble 的 My Pictures 与 Notes 导出对象不同（F‑M003‑03/14；E‑PBK‑01/02）。
  - 本地副本 vs 云条目：PocketBook Cloud 的 Delete/Remove from Cloud 区分本机删除与云端移除；Dropbox 同步目录为特定路径，不等于整机备份（F‑M003‑04/24；E‑PBK‑01/E‑PBCD‑05/06）。
  - 阅读点：阅读目录/书签/跳页与 Page appearance 的设置分开；退出/重启持久化未证（F‑M003‑21/22；E‑PBK‑01）。
  - 音频点：Audiobooks 与 TTS 控制面板不同；书签/目录/速度等在文档中分列，断点/睡眠/重启恢复未证（F‑M003‑05/07；E‑PBK‑01/05）。
- 关键任务流（样例）：
  - “批注→导出”：Notes 列表多选 → Export notes → 设备路径；随后用 USB 取回；不等价“云端可回导编辑”。
  - “同步→离线”：Accounts and Synchronization → PocketBook Cloud 登录 → 修改后 Sync；断网时仅本地已落地对象可读。
- 失败与恢复（文档证据）：
  - 同步图标状态与错误：62页图示说明服务状态；失败后不等于所有内容可回滚（E‑PBCD‑05）。
  - 音乐删除图文不一致：列表移除 vs Remove from the device，保留矛盾，不据此外推其他应用（E‑PBK‑05）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发：
  - 明确“导出件/手写图/本地书库/云账户条目/阅读点/音频点”的不同持久层级；不可把“导出成功/云同步已开”当作“学习资料已可回导编辑/全端恢复”。

## 全目录逐项清点

手册印刷页与PDF文件页相同。第2–4页的65项原目录完整保留在[当前功能目录CSV](../batches/pocketbook-app-controls-deepening-inventory.csv)，此前为26项补正文；第45批在同一目录就地更新27项，保留原名与页码，不新建重复目录。父章节与子项有包含关系，不是新增功能数。其余条目保留前次状态；最初[目录快照](../artifacts/pocketbook-controls-deepening/functional-inventory.csv)仅供追溯，当前判断用更新后的CSV。目录完整、正文读过与操作验证分别记录。

| 功能域 | 手册定位 | 当前覆盖与待查 |
| --- | --- | --- |
| 外观、安全、规格、充电 | 5–11 | 充电与USB模式文字补读；安全/规格仍以章节和型号核对为主，不据参数宣称续航、刷新或耐久实测。 |
| 电源、任务管理、主页、通知、手势、键盘 | 12–17、81–84 | F01补触摸映射确认、键盘输入/布局；Cancel、输入保存和后台关闭影响仍未验证。 |
| 前光、网络、蓝牙 | 17–20、69、78–79 | F09蓝牙；F18网络与隐藏SSID；F25自动/手动与时段调光；真实连通、切源及恢复未知。 |
| Library/Store/Browser/Notes | 21–27、59、63–65 | F03笔记；F19购买/下载、F20网页/外链返回、F21书库查找；未购买、清数据或操作账户。 |
| 日历、计算、图片、相框、RSS | 28–30 | F08保留RSS文档流程；F15补相框列表与播放。Gallery/日历控件已读，计算器只有功能范围，不造完整任务。 |
| Music Player/Audiobooks/Libby | 30–39 | F07本地Audiobooks、F10馆借、F17音乐列表与播放分开；最小化/退出、删除和断点仍缺实证。 |
| 游戏、Scribble、Coloring | 40–41 | F14补绘图/撤销/保存疑点；Sudoku数字/候选标记、Chess模式/10难度只有局部控件或功能声明。Coloring自动保存不套Scribble。 |
| Dropbox、Send-to-PocketBook | 41–45、70–71 | F06邮件；F24指定目录同步及Autosync触发；不扩大为全Dropbox或整机备份。 |
| 阅读导航、目录、书签、词典与笔记 | 46–54 | F02/03查词和笔记；F21跳页、层级目录、书签删除确认；计页期间部分功能不可用，搜索仅入口。 |
| Voice、文本/PDF版式、链接与加载 | 55–62 | F05 TTS；F20外链返回；F22页码单位/排版/引擎；F23 USB与ACSM授权/下载。PDF重排可能缺图表，未验证朗读顺序。 |
| 书库分类、Cloud | 63–68、71–72 | F21分类/排序/视图及菜单；F04云端/本地状态与删除。标为已读不证明理解，整理菜单不当已完成全链。 |
| PIN、Accessibility、Personalize | 72–84 | F11 PIN、F12档案、F13读屏；F25调光、F26开/关机图；F01触摸映射。普通睡眠图、隔离、持久化及刷新性能未知。 |
| 语言、时间、电源、维护、About、更新与错误 | 84–90 | F16区分配置密码、重置和格式化；版本/更新及错误已读，操作回执、取消、断电恢复仍缺；没有执行恢复、检查磁盘或刷机。 |
| 支持/合规 | 91–95 | 目录清点；不展开法律认证判断。 |

同版本发布说明还列Libby、PIN、无障碍、LCP PDF/音频、FLAC等变化；其中FLAC没有明确归到哪一个播放器操作链，不能按手册音乐段仅写`.mp3`推定不支持，也不能据RN宣告两个播放器均已逐格式验证。[E-PBCD-07]

## 任务流程

原F05的“两种听读概览”收束为文本TTS；真人文件的独立控制在F07，不重复统计同一操作。原F03/04直接补编辑、导出和同步删除，不另造重复任务。

| F编号 | 任务/前置 | 操作、反馈与结束 | 数据状态与未核实 | E证据 |
| --- | --- | --- | --- | --- |
| F-M003-01 | 从主页进入系统控制 | 顶缘下拉/点通知图标或Menu键；任务管理由通知栏或长按Home。通知面板Settings进入配置，总则称退出节应用变化。Personalize可配置阅读触摸/滑动/边缘拖动；Key mapping可新增HID蓝牙按键并分配动作。 | 13图底栏含Settings，21默认文字含Notes，差异保留。配置能力不证明默认动作/适配，保存失败和后台保留未知；读屏的触摸语义另见F13。 | E-PBK-01；E-PBK-04；E-PBCD-01 |
| F-M003-02 | 连续查词并留资料 | 阅读菜单/Dictionary→点词；保存译文及语境后从目录Notes或页边图标访问；长按可改译文/语境、删相应对象；Exit退出词典模式。 | 退出前触摸语义不同；设置可选择保存译文/语境的方式。词典缺词、下载失败、改后与原文定位关系未知。 | E-PBK-01；E-PBCD-01 |
| F-M003-03 | 编辑本地注释并导出 | Notes按类型/排序找条目；长按/Edit comment→修改/Save。导出可多选→Export notes→目标路径/Export；默认在Library的Notes目录。图形注释可由Scribble编辑，独立绘图见F14。 | 已导出文件不保证跟随修改；扩展名、导入入口/回导编辑仍未知。Scribble的My Pictures路径及New/保存同形图标不能填补Notes导出格式；同名覆盖、取消、恢复未核。 | E-PBK-01；E-PBK-02；E-PBCD-02；E-PBACD-03 |
| F-M003-04 | Cloud同步、离线状态与删除 | Settings/Accounts and Synchronization/PocketBook Cloud登录；打开/关闭书、改收藏/合集触发同步，也可点通知栏Sync。书库长按菜单区分Delete本机和Remove from Cloud；同步图标可打开各服务状态。 | 虚线框表示仅云端；脱网只可读本地已有内容。本机删除后云条目下次再现，不等于已证明自动重下文件；云移除手册称不可恢复。失败先查网络，正常则稍后重试；冲突合并/撤销未知。 | E-PBK-01；E-PBCD-05；E-PBCD-06 |
| F-M003-05 | 从文本开启、控制与退出TTS | 已打开书→阅读菜单/Voice；面板选择语音、Play/Pause、速度、音量、最小化和Exit。当前朗读片段有进度指示、随听自动翻页。Download New Voices→选择语音包/Download。 | 最小化与退出是两个入口；未观察最小化后的实际播放、重开位置或其他app抢音。语音包断网/空间不足、DRM/PDF排版的朗读顺序待测。 | E-PBK-01；E-PBK-02；E-PBCD-04 |
| F-M003-06 | 邮件传书与接收核对 | Send-to-PocketBook向导完成激活后使用`@pbsync.com`设备地址；可信发件人附件可联网自动收取，或Settings/Accounts and Synchronization/Send-to-PocketBook/Receive files now。 | 本册已给出白名单及手动接收。默认目录在43/44页称SendToPb/Send-to-PocketBook，实际应在Download folder核对；未发送。附件到邮箱不等于成功落地；拒收/重名/重试未知。 | E-PBK-02；E-PBCD-01 |
| F-M003-07 | 播放真人有声书并调整控制（D2） | Audiobooks选择本地文件/目录书后播放；Playback Control分别调速度和音调。33页图有Now Playing、Contents、Bookmarks及Play/Pause、音量、Bookmark入口。 | 文档支持目录书及部分容器，不等于TTS。书签创建提示/命名/回跳、快退步长、章节对应、计时及跨书/重启恢复未给全链，图示入口不单列为完成任务。 | E-PBK-01；E-PBK-02；E-PBK-05；E-PBCD-03 |
| F-M003-08 | 添加/更新RSS并离线回看（D2） | RSS setup/Add RSS feed→URL；菜单Update时显示下载状态，更新写入`.hrss`。Read the latest news在浏览器打开最新下载，News Archive选旧更新；源菜单可改名/URL或Delete。 | 只有已下载更新可离线回看；删除源是否同时清档、URL失效/认证/断网/重复更新、下载取消和RSS返回点未说明。 | E-PBCD-01 |
| F-M003-09 | 为听读连接蓝牙输出（D2） | 外设配对模式；通知栏启用BT，首次可确认配对许可。启动音频/TTS时的列表选耳机，或Settings/Bluetooth/ON→列表；未找到用Find BT device。 | 手册说下次开机自动连接，未实测。6.10.2854 RN修复音乐断连及重启蓝牙后错误外放；不保证所有外设/中断恢复。USB/扬声器优先级、断线音量/位置未知。 | E-PBCD-01；E-PBCD-07 |

## App与设置的局部任务补充

以下均为固定型号手册重建；没有登录图书馆/邮箱或实际修改设备。图标示意不证明执行结果。

| F编号 | 任务与前提 | 入口、操作与文档结果 | 退出、失败与对象边界 | E/页码 |
| --- | --- | --- | --- | --- |
| F-M003-10 | Libby借阅、预约与归还；需网络及有效馆卡 | PocketBook x Libby按馆名/邮编/城市找馆，可换国家；Sign in with my card后用另一设备扫码、外部浏览器链接/代码或本机登录。Preview Library只能看内容。可用副本Borrow并选借期；无副本Place Hold；书进入shelf，Manage Loan → Return Early可提前还，到期自动还。 | 借阅电子书通常下载并出现于主页/书库；不能下载时webview需联网，借阅音频只说明在线听。菜单可换馆/管理卡/移除馆，但授权失败、还书确认、期限后文件/笔记和移除馆影响未说明；37图不显示还书结果。 | E-PBK-01，34–39；E-PBACD-02 |
| F-M003-11 | 设置PIN及恢复访问 | Settings → Device PIN Code → Turn PIN code on，先指定并邮件确认恢复地址，再设PIN。错码显示Wrong PIN，输错3次出现Forgot PIN，点击后请求含恢复链接的邮件。Change PIN可修改；Request PIN列Always、开机、解锁、连接PC、断开PC及Settings。关闭走Turn PIN code off → Turn off确认。 | 可用Store账户或个人邮件。恢复需访问邮箱；失去邮箱则手册指向客服并需序列号和/或原邮件。不是离线恢复保证；重设界面、限速、邮件失败和资料保留未核。与85页配置密码分开。 | E-PBK-01，72–74 |
| F-M003-12 | 创建、切换及整理用户档案 | Settings → Personalize → User Profiles → New Profile，输入名称确认；点档案名 → Activate，活动项有勾号。菜单还可设头像、复制参数、改名/删除。文档列每档案保存书籍状态、最近书、书签、笔记、收藏和预设网络。 | 不证明图书文件、Cloud账户、密码或应用权限隔离；没有删除确认、资料回收或切号同步规则。不能把配置档案直接称为儿童账户隔离。 | E-PBK-01，83–84 |
| F-M003-13 | 启用Screen Reader并按焦点导航 | Settings → Accessibility或Menu三击进入；Use Screen Reader开关，Voice配置声音/语速/音量。Power三击的快捷入口须先开Screen Reader Shortcut。章节手势为单点/拖动聚焦、双点激活、左右滑换焦点、上下滑改滑块、双点保持进退上下文；两指向右返回/关弹窗、两指上下滚动。 | 76图读屏off、快捷on仅示例，不能当默认；三击Power是否也负责关闭、不同应用覆盖、音频中断和误触恢复未核。UI字大/粗体/OpenDyslexic另列，不据字体设置推断已启动读屏。 | E-PBK-01，74–78；E-PBACD-04/05 |
| F-M003-14 | Scribble绘图、撤销与保存 | Scribble工具栏选颜色、文字框/字体、Pencil或Eraser；Back的文字定义为撤销上次操作，另有New和Delete。手册称图像保存到根目录My Pictures，并可编辑图形注释。 | 41页New条目与保存句使用同形纸张图标，不能确定是两个按钮或新建兼保存；不补造自动保存。格式、覆盖、退出取消、撤销层数、误删及导出回导未知。 | E-PBK-01，40–41；E-PBACD-03 |
| F-M003-15 | 建立相框照片列表并播放；图片已在设备 | Applications → Photo Frame → New Playlist → 名称/Continue → 选图/Create Playlist；列表＋或长按Add photos to playlist增图。Play开始；列表长按可改名/删，照片长按菜单Delete用于从列表移除。齿轮或播放时点中部进入间隔、创建日期、当前时间、Shuffle和Display mode设置。 | 手册未说明Delete对底层图片文件的影响；停止/返回、锁屏/关机、中断进度和设定范围未给全链。Gallery的Power-off Logo是独立用途，不把播放列表当屏保。 | E-PBK-01，28–30；E-PBACD-01 |
| F-M003-16 | 区分设置重置、格式化与维护 | Settings → Maintenance中Protect Configurations by Password保护进入Settings，修改/移除需再输密码；Factory Reset说明清用户设置而保留书/图片。Settings → About Device → Memory → Format Internal Memory则删系统文件以外数据；手册要求用设备此项而非电脑格式化。 | 缺确认/取消/完成回执、笔记/档案细项、断电/错误与恢复规则。69页退出节应用设置的总则不能证明Reset/Format等到退出才执行。更新/排错另见目录，不据建议保证修复。 | E-PBK-01，69、85–90 |

游戏的证据仍较薄：Sudoku给选空格、底部数字/候选标记；Chess仅朋友/设备及10难度；Klondike仅名称说明。未取得判错、提示、局内退出或完成保存链，未各自增计F。Coloring称自动保存，但没有路径或恢复方法；不能转移给Scribble。[E-PBK-01，40–41]

## 阅读、音乐与内容准备补充（第45批）

以下仍是同册D2文档重建。每行有目标、前提、动作/所述结果与未闭合分支；没有实际购买、授权、复制、同步或删除。系统键盘与手势补回原F01，不把每个控件增加为一条功能。

| F编号与任务 | 前提与入口 | 操作与资料所述结果 | 状态边界、退出及缺口 | 来源/印刷页 |
| --- | --- | --- | --- | --- |
| F-M003-17 建立音乐列表并控制播放 | 本地音乐文件，Music Player | 应用菜单加文件/文件夹，重排/保存/清空列表；播放面板切循环/正常/随机、前后曲、播放暂停及音量。长按可看信息/加队列/移除；31图分别标Exit与Collapse。 | 队列/歌单/原文件不混同；32页移出列表正文与Remove from the device菜单矛盾；后台、重开断点、删除确认及FLAC播放器归属未核。 | E-PBK-01；E-PBRC-01；E-PBK-05；E-PBCD-07；30–32 |
| F-M003-18 选择或配置Wi-Fi网络 | 需要网络的应用或通知栏/Settings → Wi-Fi | 已知网络可自动连接，未知显示选择；安全网络输入密码。More含DHCP/手动/代理。隐藏网络输入SSID、安全方式及凭据，返回该节Connect；Scanning是扫描状态。 | 网络列表/配置不等于互联网可用；错密、门户、取消、超时、重连及凭据保存未核。 | E-PBK-01；18–19、69 |
| F-M003-19 从商店取得并下载图书 | 网络、Store账户及相应购买条件 | 商店授权后选书/Buy进入支付选项；已购内容列于My Purchases，可稍后下载。文档称下载完成后出现于Library/Downloads和Recent Events；个人区可改语言/商店或退出。 | 未登录付款；购买权益不当文件落地。取消、支付/下载失败、退款、地区/DRM与再下载条件未知。 | E-PBK-01；22–23 |
| F-M003-20 浏览网页并从书中外链返回 | Browser或阅读中的外部链接，网页需网络 | 地址栏输入/Enter，链接/拖动滚动/缩放；菜单含历史、下载、收藏及清历史/cookie、JS/图片设置。阅读外链打开浏览器后，菜单Exit回阅读；书内链接模式另从左上退出。 | 未实际打开或清除；返回书中精确位置、空白错误、下载取消/保存及隐私数据范围未知。 | E-PBK-01；24–25、59 |
| F-M003-21 在书库找书、定位并整理页书签 | 最近书或Library中已有可读书 | 分组/排序/视图找书；打开后菜单滑条或数字跳页，目录含章节/书签/笔记且层级可展开。右上折角加书签，再点并确认删除。重开回停处为文档描述；计页中省略号且部分功能暂不可用。 | 搜索仅见入口；标为已读不证明完成。误删撤销、跳页错误、重排后的定位、退出/重启持久化及源文件变化未核。 | E-PBK-01；E-PBRC-01；46–50、63–65 |
| F-M003-22 调整排版、计页和阅读引擎 | 阅读菜单 → Settings或书库长按Open with | 文本可设字号/行距/边距/字体等；内置原书页码可一页多屏，关掉后按屏计数并在首次/更改时重算。PDF/DjVu可整页/滚动/分栏/裁边，PDF另可Reflow及图像调整；可切阅读引擎。 | Reflow仅PDF，表格/图像可能不显示；页数不等于学习量，裁边不当删除原文。未测格式准确性、引擎切换后的笔记定位、设置恢复或耗时。 | E-PBK-01；E-PBRC-01；56–59 |
| F-M003-23 通过USB或ACSM准备可读图书 | USB路径需开机/电脑；ACSM路径另需网络与Adobe授权 | 连电脑选PC link，出现可移动存储后复制并安全移除。ACSM拷入打开，按授权表输入凭据，取得整书到Digital Edition目录；电脑ADE也需授权。删除Adobe授权后保护文档不可用。 | ACSM不是整书；手册microUSB与本型号Type-C、移除授权两路径差异保留。旧Adobe网页步骤不当现行指南；未实操，复制错误/空间不足/重复与撤销未知。 | E-PBK-01；59–61、71、85 |
| F-M003-24 设置Dropbox目录并同步文件 | Dropbox账户与Wi-Fi | Accounts and Synchronization → Dropbox登录；创建本机同步目录及云端应用目录，只能访问后者。文件加入对应目录后联网同步；可改/新建Sync Folder及Synchronize Now。授权时Autosync自动开，可关。 | 不是全Dropbox或整机备份，也不等于PocketBook Cloud阅读位置同步。删除传播、冲突、部分失败、换目录/退出账户及重装恢复未核。 | E-PBK-01；41–42、71 |
| F-M003-25 切换前光自动/手动并定时调节 | 通知面板、阅读边缘或Personalize → Frontlight | 通知滑条/开关及Menu长按；SMARTlight按时间自动需时钟准确。阅读边缘调节遇自动模式先询问关闭；时段图选时间并设亮度/色温，退出确认后应用，可激活/编辑/复制/改名/删。 | 询问仅正文；边缘可改绑音量/字号，当前映射未核。拒绝询问、跨模式覆盖、重启恢复及亮度/续航测量缺失。 | E-PBK-01；E-PBRC-01；17–18、49、78–79、83 |
| F-M003-26 分别配置开机图与关机图 | Personalize → Logos；自定义图在设备 | Boot Logo或Power-off Logo选默认/封面/自定义/随机；自定义选图后Set，随机最多10图。Current Page只在Boot列表，关机另有none；Gallery也可设图。 | 手册screensaver措辞不扩大到普通睡眠；未看实际启动/关机、图片失效、选择取消、循环顺序或配置保存。 | E-PBK-01；E-PBRC-01；80–81 |

原F01补充：阅读点按区选择动作后须勾号确认，82图另有Cancel但没有取消效果。四方向滑动、两侧拖动可配置，拖动可改前光/色温/语音音量/字号；当前映射不能由默认示意推断。屏幕键盘长按特殊字符、globe切布局，系统最多3布局；16页文字说空格显示所选语言，图中却为space，差异保留。输入的取消、草稿、重启恢复均未验证。[E-PBK-01；E-PBRC-01，16–17、81–84]

原F04的Cloud继续限定文件及阅读位置的文档范围；F24的Dropbox文件目录和Autosync规则不能移作Cloud的删除/进度或整机备份依据。F23移除Adobe授权后“不可用”也不等于删除本地文件。商店购买、链接材料、下载文件、已打开与已读状态必须分别核对，这是本项目G-002相关的候选分析问题，不是已选定接口方案。

本批七页静态摘页作为同一派生记录E-PBRC-01保存，详见[逐页观察](../artifacts/pocketbook-reading-media/observed-pages.json)。16页键盘、31页音乐、48页阅读菜单、49页调光、57页排版计页、80页Logo、82页触摸映射均来自同一手册；其中49页的自动调光询问仅为文字规则，80页主要为正文嵌图标，不能说看过完整弹窗或选择流程。

## 本地笔记、导出与再次编辑的边界

| 对象 | 本型号材料能确定 | 尚未确定 |
| --- | --- | --- |
| Notes应用中的本地评论 | 27页长按/Edit comment与Save；Rename quote、Delete comment、Delete note是不同入口 | 删除确认/撤销与误删恢复。 |
| 词典译文/语境笔记 | 52页可分别手改、删除译文/语境或删整个笔记 | 原书改版后定位、与Cloud同步的冲突规则。 |
| 图形笔记 | 41页说Scribble可编辑图形笔记；Back为撤销，保存指向My Pictures | New与保存同形图标保留；不能据此推导任意导出文件回导或自动保存。 |
| 导出结果 | 27页给目标路径、默认Notes目录；25页笼统提及export/import | **格式仍未知**；导入操作、书籍匹配、往返编辑和外部修改兼容性均未闭合。 |

前批已检索官方Notes/HTML材料。命中的旧InkPad 3 / PB740 5.20.1010发布记录属于其他型号/年代，不能作为PB743K3 6.10.2854输出格式证明；Eo的PDF/PNG说明同样排除。本轮没有生成或导入真实笔记文件，不把历史HTML线索写成当前已验证格式。

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

88页更新错误表仍使用card及SWUDATE.BIN等措辞，不能据此为本型号补存储卡操作；87页自动检查路径与86页设置名称也按原文定位，不合成已验证界面。84页锁屏和关机只给空闲时长，不能推定播放/读屏期间是否触发；本轮未下载更新或执行任何排错。

## 项目分析与下一项核查

以下为推断：G-002/R-021需要把本地笔记、导出文件、云条目、落地副本和播放位置分别建模；仅看到“导出成功”不足以答复之后还可编辑，看到书封也不足以答复可离线。RSS的下载状态、最新快照和历史归档是不同对象，适合G-001下的可追溯阅读研究。

R-018需要分别验证文本TTS与真人有声书断点。对5–6岁自主听读，最小化、退出、换书和外放切换应有明确反馈；对成人学习，译文/语境与可导出的评论有价值，但格式和回导未知前不能承诺学习资料全链。以上是建议，不构成Plus已有能力、设备外部AI接口或新增需求。

下一最小观察：固定6.10.2854创建高亮/评论/译文/图形四类笔记，导出后检查扩展名及再导入；再分别对TTS、有声书执行暂停→换书→锁屏→重启→耳机重连。RSS和Cloud只需补URL失败、断网与删除后另一端状态。记录真实文件和提示，不以更多目录条目代替验证。

## 来源与静态观察

下表为前批2026-10-26取得的资料；2026-10-27本地复读追加于原手册E-PBK-01。静态摘页均同源，不按页数增加独立来源或连续流程等级。

| E编号 | 来源/定位 | 获取方式与限制 |
| --- | --- | --- |
| E-PBK-01 | [PB743K3 / 6.10.2854官方手册](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf) | 原95页本地文件，哈希核对；无重新下载或执行固件。 |
| E-PBK-02 | [官方设备FAQ](https://pocketbook.ch/en-ch/faq) | 前批重读Notes、TTS、Audiobooks、Cloud和邮件条目；通用跨型号，不证明导出格式。未访问视频。 |
| E-PBK-03 | [型号规格与应用目录](https://pocketbook.ch/en-ch/catalog/color-e-readers/pocketbook-inkpad-color-3-ch) | 前批重读；不作流程或性能测量。 |
| E-PBK-04 | 同册13页[旧静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/M003-manual-p13.png) | 前批重看；保留与21页文字默认项差异。 |
| E-PBK-05 | 同册32页[旧静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/M003-manual-p32.png) | 前批重看；上部音乐删除图文冲突，下部有声书速度/音调。 |
| E-PBCD-01 | 同官方手册，目录2–4、操作11–90 | 既有PyMuPDF解析；逐项目录另存CSV；未把全部法律/合规内容宣称为已分析。 |
| E-PBCD-02 | 同册27页[笔记摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p27.png) | 实看静态菜单/说明；没有导出格式选择画面。 |
| E-PBCD-03 | 同册33页[有声书摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p33.png) | 实看Now Playing/Contents/Bookmarks及控制；图上时码不是观察时间戳。 |
| E-PBCD-04 | 同册55页[TTS摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p55.png) | 实看语音、速度、音量、最小化与Exit标签；未触发按钮。 |
| E-PBCD-05 | 同册62页[同步状态摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p62.png) | 实看图示及文字；其中服务示例为Dropbox。 |
| E-PBCD-06 | 同册68页[Cloud状态摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/pocketbook-controls-deepening/pb743k3-p68.png) | 实看三种存储状态；删除行为另依页内正文，不是实际删除观察。 |
| E-PBCD-07 | [同型号6.10.2854发布说明](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/rn/ww/RN_PB_InkPad_Color_3_EN.pdf)，2026-01-05 | 5文件页；封面版本日期；印刷3/文件4的蓝牙与高亮修复。web正文和HTTP200原件，未看作操作或性能验证。 |

此前App与设置批次的静态观察（2026-10-27）：

| E | 同册页与摘页 | 观察边界 |
| --- | --- | --- |
| E-PBACD-01 | [相框列表与播放设置](../artifacts/pocketbook-app-controls-deepening/manual-p29.png)；PDF/printed page 29 | ＋、Play及齿轮图标；删除来自列表正文，未观察结果。 |
| E-PBACD-02 | [Libby书目及借还规则](../artifacts/pocketbook-app-controls-deepening/manual-p37.png)；PDF/printed page 37 | 静态图有Download/Place Hold；Return Early仅正文；没有执行借还。 |
| E-PBACD-03 | [Scribble撤销/保存与Coloring边界](../artifacts/pocketbook-app-controls-deepening/manual-p41.png)；PDF/printed page 41 | Back为撤销；New与保存句同形纸张图标保留歧义。 |
| E-PBACD-04 | [Screen Reader开关与物理快捷键](../artifacts/pocketbook-app-controls-deepening/manual-p76.png)；PDF/printed page 76 | 读屏off/快捷on是示例；Power三击需启用快捷，不当默认。 |
| E-PBACD-05 | [无障碍焦点、激活及上下文手势](../artifacts/pocketbook-app-controls-deepening/manual-p77.png)；PDF/printed page 77 | 单点/双点/滑动/双点保持分开；未实际触发。 |

完整范围见[独立摘记](../artifacts/pocketbook-app-controls-deepening/source-notes.md)和[逐页观察记录](../artifacts/pocketbook-app-controls-deepening/observed-pages.json)。该次仅查看这5页图像；第45批另补上文所列7页。正文阅读不升级为连续视觉证据。
