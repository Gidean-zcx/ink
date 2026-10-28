# M-012 Mudita Kompakt：通信、离线开关与系统恢复

查阅：2026-10-27；旧批为 2026-10-26。本例只研究 **Mudita Kompakt**，不套用 Mudita Pure 的桌面短信/设置备份。英语文档；地区、SIM、运营商和安装构建均未实际选定。无登录、安装、拨号、发送、刷机或设备操作；`hands_on=false`。27 条任务最高 D2；两张官方静态参考图不算连续操作。此前核心App补18篇帮助；本次再读剩余46篇，未新增图像观察。

## 型号和版本边界

- 系统基线取已发布 **MuditaOS K 1.6.1**：官方 1.6 条目解释 1.6.0 曾限量推送后暂停，附件实际说明 1.6.1；条目修改于 2026-10-13。6 页附件仍题为 K 1.6，标注变化日期 2026-08-24，两日期不合并。（E-MDCD-03/04）
- 12 页官方手册未标 OS；PDF 创建元数据 2026-01-27 只作文件线索。10 月帮助说明具体路径，但没有证明全部截图/菜单来自 1.6.1。手册整册目录和现行支持目录均已登记，不宣称已取得一本覆盖每个 app 的完整操作手册。（E-MDCD-01/02）
- 桌面版本参照 **Mudita Center 4.0.0** 已发布说明；现行帮助及安装截图的确切构建未知。K 1.2.0 是侧载加入的系统门槛；Center 产品页仍以 3.1.0 介绍侧载，并不把那段旧文案当桌面当前版本。（E-MDCD-13/19/20）
- K 1.4 的锁屏音乐/文件夹等改进保留为历史 D1；K 2.0 IFA 预告不当成已交付卸载或同步能力。（E-MUD-11/12）
- 本轮18篇帮助的已保存HTML均标2026-10-03修改，未标OS/Center构建；不据这些日期把菜单统一认定为1.6.1。Notes、Contacts的web缓存曾显示2025-08-08，正式日期采用本轮原件。没有重新判定最新系统版本。（E-MACD-01—18）

## 功能目录与覆盖

[逐条功能目录及当前阅读状态](../batches/mudita-app-controls-deepening-inventory.csv)保留111个稳定条目；原46项index_only已逐篇读正文，现80项body_read、1项prior_body_read、30项text_read。没有仅据标题断言的剩余行，但这只是已有来源目录，尚未核全机功能，也未刷新全部官网目录。综合/拆分帮助的重复路径合并在同一任务；硬件/市场FAQ不计操作任务。原[目录快照](../artifacts/mudita-controls-deepening/function-inventory.csv)作为历史保留。

| 功能域 | 官方目录/页码 | 已关联任务或证据 | 未闭合部分 |
| --- | --- | --- | --- |
| 开机/SIM/语言/键盘 | 手册 p2；Setup 6 项 | F09 更新；E-MDCD-01/02/15 | 初次配置、SIM2/eSIM切换、语言变更的退出和错误 |
| 实体键/导航/无障碍/电池 | 手册 p2–4、p9；System 6 项 | F01/04/09/16；另已读Display → Screen Timeout → 选时长（E-MACD-16），未独立计F | 超时选项/默认值、保存返回、指纹失败、截图文字见F25，亮度刷新与按App优化未闭合 |
| 通话/SMS/MMS | Calling & Messaging 7 项；1.6 Phone/SMS | F01–03；E-MDCD-03–06/22 | 通话打断音乐、接听/拒接全过程、未送达重试 |
| 日历/闹钟/笔记/地图/天气 | Organization 9 项；附件 p1–6 | Notes见F13；本次F20–23补日历/闹钟/地图/天气，备份范围F10 | 笔记退出草稿/回收；各App错误/返回、地图暂停下载后的恢复仍未核 |
| 联系人/导入 | Contacts 2、导入 6、Center 管理 1 | F11机内/Center增删；F12综合帮助三路导入；F01阻止/解封 | 拆分六页已读；去重、覆盖、字段错误与持续同步仍未验证 |
| 录音 | 录制/收听/删除 3 项；附件 p5 | F14录制/停止/命名保存、回放、单删/多删；Offline+限制F05 | 暂停续录、保存前离开、来电/锁屏、关闭画面后播放、转出与恢复 |
| 文件/阅读/音乐/相机 | 文件 7 项；K1.4发布说明 | F06/07；E-MUD-08–11、E-MDCD-11 | 阅读位置/标记、音乐队列/断点、摄像头返回未闭合 |
| 应用布局/侧载/卸载 | Privacy 3 项；FAQ安装/第三方app | F08安装生命周期；F15定位总开关/逐App；F16指纹 | 定位等级生效优先级、其他权限、后台、更新覆盖、崩溃修复 |
| 连接/离线/蓝牙 | Connectivity 6 项；Offline+ FAQ | F05；蓝牙入口已读 E-MDCD-21 | 输出切换、耳机断开/来电路由；Wi-Fi/热点/漫游正文见F26；断连恢复仍未核 |
| 更新/恢复/备份 | Setup更新、System重置、FAQ启动循环、Center备份 | F09/10；E-MDCD-15–20 | 无损回滚、备份合并/覆盖、跨版本和中断 |
| 硬件/法律/其他FAQ | 手册 p5–12；FAQ24条逐项登记 | 手册完整目录；RCS无支持由明确FAQ支持 | 运营商兼容、SAR/性能等非本批操作结论；全部原列FAQ正文已读，仍不据厂商安全/性能声明推定本项目实测 |

手册全部 15 个目录项（保留原章节定位）：Get Started 2；Charging and Turning on 2；Offline+ 3；Navigate the Apps 3；Accessibility 4；Mudita Center 5；Important Safety and Legal Information 5；Safety Instructions 5；Frequency Bands and Power Output 7；Charging and Battery Information 9；Regulatory Information 10；Before you use Kompakt 12；Cleaning, Storage and Maintenance 12；Guarantee Information 12；Contact Details 12。（E-MDCD-01）

## 已有任务补证（保持原编号）

| F 编号 | 入口和操作 | 反馈、数据、退出与限制 | 证据 |
| --- | --- | --- | --- |
| F-M012-01 向联系人打电话 | Phone → Contacts → 联系人 → Call；新号码走Phone → Dial → 输入 → Call。来电Block → Block number。Phone → Recents → App settings → Call Blocking可开隐藏/非联系人号码阻止；Blocked Numbers → Add to blocked或＋ → 号码 → Block。解封帮助另从Phone → Contacts → App settings → Call Blocking → Blocked Numbers点号码旁减号。 | 两类总开关都涉及来电与消息；非联系人规则只接收联系人中的号码。不等于删除联系人，也不说明解封补收。普通接听/拒接、SIM选择和音频中断仍未闭合。电源键结束通话仍需Control Center → Settings → Accessibility设置；K1.6.1计数修复只作版本记录。 | E-MUD-01/03；E-MDCD-01/03/04/05；E-MACD-01/10 |
| F-M012-02 写短信与图片消息 | SMS联系人用Pencil，新号码用Plus → 号码 → Send to确认 → 输入 → Send；新号码MMS在确认号码后Image → 选图 → Send。联系人MMS也用Plus。键盘不出现时点输入光标。 | 四个拆页与原汇总重复；Pencil/Plus差异仍在，未证明版本原因。Send不证明送达/已读或创建联系人；取消草稿、离线重试、附件失败仍未知。RCS限制保留。 | E-MUD-04/05/06；E-MDCD-06/22；E-MACD-02—05 |
| F-M012-03 删除对话 | SMS → Pencil → 勾选会话 → Delete → 再次Delete。 | 二次确认作用于对话；未见撤销、单条删除或附件保留规则。备份恢复也未证明能只找回某条会话。 | E-MUD-07；E-MDCD-06 |
| F-M012-04 调音量与进入控制中心 | 导航图标分返回/主页/Control Center。控制中心铃铛下滑条、物理音量键均调主音量；Settings → Sound有特定用途滑条。铃铛、滑条到底、长按音量减到底或Sound → Silent Mode可静音。 | Sound → Advance管按键/锁屏/充电/触摸提示音。K1.6另加通知振动开关，但不证明其与静音、闹钟的优先级。默认音量记忆、各应用媒体焦点未知。 | E-MUD-01/02；E-MDCD-01/04/07/08 |
| F-M012-05 开启深度离线 | 飞行模式：Control Center → 飞机图标切换；Offline+：左侧开关向下，向上恢复完整功能。 | 飞行模式允许重新开蓝牙。Offline+中GSM/三麦克风硬件停用，相机/Wi-Fi/蓝牙软件停用；不能紧急呼叫。1.6.1不再每次退出都要SIM PIN，仅重启后立即退出的例外保留。播放是否继续、补收短信和重连耗时未测。 | E-MUD-01；E-MDCD-01/03/09/10 |
| F-M012-06 从电脑准备书、音乐和照片并传出文件 | Center连接 → Your Kompakt → Manage Files → Music等类型 → Add file → 选择/确认。USB文件可见性另走接电脑 → Control Center → Settings → Connected Devices → USB Preferences → File Transfer。 | 不支持的格式不能选择/上传，帮助未给完整音频格式表；可选不等于解码/入库成功。未接电脑时USB Preferences不显示；No data transfer关闭访问，不当Center必需前置。容量、重复、断线半成品与断点未知。 | E-MUD-08/09；E-MDCD-11；E-MACD-12 |
| F-M012-07 从电脑删除文件 | 新帮助写Your Kompakt → Manage（Music/Photos/E-books）→ 勾选文件；顶部减号样式复选框可全选 → Delete → Delete file。类别文案与传入页Manage Files分别保留。 | 全选框不是解封号码的减号。删APK不卸载应用（F08）；笔记/播放记录依赖、回收站、单文件恢复与断线结果仍未知。 | E-MUD-10；E-MDCD-12/13/16；E-MACD-13 |

音乐控制与蓝牙输出另有两段局部事实：历史 K1.4 描述锁屏播放/暂停及跳曲；当前蓝牙帮助为可发现外设 → Control Center → Settings → Connected Devices → Pair new device → 选设备 → Pair（出现“Bluetooth is on”提示可关闭）。这些均不证明来电抢占后何时恢复、耳机切换保存哪个位置，也不据 Android 底座推断音焦点实现；Recorder保存/回听见F14。（E-MUD-11、E-MDCD-21）

K1.6 Phone 另列语音信箱通知：部分支持MWI的网络在应用/最近通话显示，其他网络仍用短信；拨号长按1可呼叫，未配置号码时提示，可到应用设置配置。仅登记版本化分支，不额外计F；运营商状态未实测。（E-MDCD-04 p4）

## 新增独立任务

### F-M012-08 安装应用并区分隐藏、删除安装包与卸载

前提为Kompakt系统至少K1.2.0、电脑Center和USB-C。Manage Files → App installers → Add file → 选择APK → 确认传入 → 点击该APK → 接受风险并确认安装。帮助完成步骤允许关闭窗口后使用；实际查看的单帧显示完成标志、Close及可断开设备提示，未观察前面的传入/安装过程。（E-MDCD-12/13）

安装包与应用是两个对象：帮助明确安装后可删APK而不影响应用；Center4.0发布说明给APK“已安装/待安装”状态。隐藏是另一路径：Control Center → Settings → App layout → 开关 → 可Preview → 返回箭头保存退出；不据隐藏推定卸载或清数据。（E-MDCD-12/14/20）

2026-10-03官方FAQ明确设备和Center均无内置卸载侧载应用的选项；恢复出厂可一起移除并清用户数据（F09）。FAQ提到社区第三方管理器，但未安装/核查，不能替代官方内置流程。无Google Services、无机内APK loader，应用不一定兼容。定位已有全局/逐App入口（F15），第37批补到等级名称；其他权限撤销、更新覆盖和后台规则仍未知。K2.0预告不填这些缺口。（E-MDCD-13；E-MUD-12；E-MACD-15；E-MKRD-89）

### F-M012-09 更新系统及失败后的恢复出厂分支

联网后可从Control Center顶部更新通知 → Update → 如出现许可则勾选/Agree → 自动下载和配置 → Restart phone；手查为Control Center → Settings → About → Software Update。失败帮助依次建议重试、检查稳定联网/移动数据下载许可、重启，再把恢复出厂作为最后手段；仍失败联系支持。没有证明中途断电会自动回滚。（E-MDCD-15）

能进入设置时：Control Center → Settings → System → Factory Reset → Factory Reset → 最终Factory Reset；完成后重启进入设置流程，数据清空但OS版本不变。因此“恢复出厂”不能写成降回旧固件。（E-MDCD-16）

启动循环的独立入口：关机 → Power+音量加到Select Boot Mode → 音量加选Recovery、音量减确认 → 机器人画面按住Power并点一次音量加 → Recovery中音量上下选Wipe data/factory reset、Power确认 → 等待。两层菜单的确认键不同；会清空未备份数据，文档未给保留资料路径或恢复耗时。未执行。（E-MDCD-17）

忘记PIN或屏幕图案的FAQ也给相同Recovery清空流程，并明确未备份数据丢失；这不是无损找回凭据，也不能据指纹功能推定可绕过忘口令。SIM/eSIM清理细项、清空后备份覆盖及失败恢复仍未知。（E-MACD-17）

### F-M012-10 创建加密备份并选择恢复

Center连接后Your Kompakt → Overview → Create backup → 再确认；可设置密码、Confirm passcode后输入，也可Skip password。密码确认后不能更改。恢复：Overview → Restore from backup → 选列表项 → Restore from backup → 如加密则输入密码 → Confirm；无密码则跳过这步。密码遗失无找回/更改方式。Center → Settings → Backup显示电脑本地路径；例示路径不当所有系统固定目录。（E-MDCD-18）

数据范围分版本：产品页Kompakt段列联系人、通话记录、消息、闹钟、笔记；Center4.0.0增加日历事件。产品页把媒体管理另列，并给Pure单列含settings的备份，不能据此将Kompakt称为整机镜像。第三方app及数据、媒体、MMS附件、SIM/eSIM或系统设置是否包含，未得到完整清单。还缺恢复时覆盖/合并、损坏备份、断线回滚、跨版本及最后完成反馈。（E-MDCD-19/20）

## 核心App补充任务：文档可确定的路径

以下F11–16均为局部D2，无实际操作或已核构建；各自的取消/失败和数据恢复缺口保留。

### F-M012-11 创建及删除联系人（机内/Center）

机内Phone → Contacts → Add contact → 填信息 → Create。删除选择联系人 → Pencil → Bin → Delete contact；阻止/解封是另一组动作，不能当删除。解封段的中间一步仍写“want to block”，标题与末键是Unblock contact，保留此文案疑点。（E-MACD-10）

电脑Center连接后Your Kompakt → Contacts → 悬停/勾选目标，可多选或用顶部减号样式复选框全选 → Delete → Delete。没有取得编辑字段的保存流程、存储在SIM/机内的选择、删除传播、回收或单联系人恢复规则。（E-MACD-11/18）

### F-M012-12 选择联系人来源并导入Kompakt

Center已连接：Your Kompakt → Contacts → Import contacts。文件路线选Upload CSV or vCard → 电脑文件；Google/Outlook路线选择对应来源并登录，再选部分或全部联系人 → Import selected contacts。帮助要求导入中不要断开手机；本轮只读该文，没有进入登录。综合页也列iCloud/Google/Outlook生成来源文件的步骤，属于电脑/外部账户预备工作，不是Kompakt机内导出。（E-MACD-18）

导入选择不等于持续同步；授权范围、字段映射、重复合并/覆盖、断线重试和完成反馈未说明。不能据此推断从设备删除联系人会删除云端原件。

### F-M012-13 创建便笺并批量删除

Apps → Notes → Plus；首条可用Create new note。输入标题（帮助上限54字符）→ 键盘Enter → 正文 → Done。删除走Notes → Edit → 勾选一条/多条 → Delete → 再Delete。Done是文档创建路径的结束步骤；没有证明按返回也会保存。退出草稿、修改已有笔记、超长标题反馈、撤销/回收和恢复位置未知。（E-MACD-06）

K1.6附件p4另说新增可勾选的Task list，但没有其创建/完成步骤；不把普通Notes正文流程补成任务清单闭环。（E-MDCD-04）

### F-M012-14 录音保存、回听及删除

Apps → Recorder → Record开始；Pause暂停；Done停止录制 → New recording输入标题 → 再Done保存。帮助没有给暂停后的恢复按钮，也没有解释未命名/保存前离开时如何处理。（E-MACD-07）

回听：Recorder → Menu → 选录音 → Play；Done的原文作用是关闭录音画面，不能据此认定停止播放。单删从选中录音 → Bin → Delete；批删从Menu → Pencil/Edit → 勾选 → Delete → 再Delete。没有回收站、单项找回、导出格式或备份包含录音的证据。（E-MACD-08/09）

K1.6附件p5只列删除提示翻译、save and leave弹窗排版修复，不能补造离开弹窗的按钮/保存规则。来电、锁屏、Offline+和耳机中断后状态仍未知。（E-MDCD-04）

### F-M012-15 选择全局或逐App定位共享范围

Control Center → Settings → Location Sharing：用总开关控制全部App，或选择某App再选共享等级。初读帮助没有列等级名称；第37批GPS说明补到Share、While using app、Don’t share，且要求总开关及App许可都打开。总开关与单项生效优先级、保存/返回反馈、精确定位及其他权限仍未核。权限打开、GPS取得位置和App显示更新需分别判断，不扩大到麦克风/存储权限。（E-MACD-15；E-MKRD-89）

### F-M012-16 添加和删除解锁指纹

Control Center → Settings → Security → Manage Fingerprints → 输passcode/Enter。添加走Plus并按屏幕提示，每次震动后换手指接触部位；电源键含指纹传感器。删除在同一认证入口点目标旁Bin → Delete。帮助未给录入最终完成标志、失败次数/上限、删除最后一个后的行为。忘口令清空流程另见F09，不从指纹功能推出无损解锁。（E-MACD-14）

## 实际观察及数据状态

此前只实际查看两张单帧：[手册p3导航/离线开关](../artifacts/mudita-controls-deepening/manual-p03.png)和[官方安装完成图](../artifacts/mudita-controls-deepening/install-complete.png)。第一张是手册插图，第二张是官方帮助中的桌面窗口，均无可确认构建号；不据图内样例容量/应用名作性能或兼容结论。PDF其余页及帮助为文本阅读，不标视觉观察。核心App续补未新增看图；原件/散列在E的raw字段，独立范围说明见[短摘记](../artifacts/mudita-app-controls-deepening/source-notes.md)。无连续画面、时间戳、耗时或复现次数。

## 与本项目关系（推断）

实体Offline+能让通信与采集状态可见，但它同时切断电话与麦克风，与R-012常驻唤醒、R-013来电可达存在取舍。G-001若借鉴，应先明确何种状态仍可唤醒/来电，而非只复制开关。

F08的安装包/应用/图标分离，以及F10的有限数据备份，对G-002和Plus的动作命名有启发：AI应区分隐藏、删源文件、卸载、清数据和恢复，提示准确受影响对象。这里没有证明Kompakt具备统一AI读写或自动恢复接口；其Android侧载也不代表Plus可直接执行APK。5–6岁自主读听仍缺误触删除恢复、音频中断及权限控制实证；成人通信/阅读也需这些操作测试。

## 下一项最小核查

固定地区SIM和K1.6.1设备，记录音乐/阅读→来电→接听/拒接→返回、Offline+切回后的补收与播放位置；核短信两个新建入口、草稿和离线发送。之后仅用测试数据验证Center4.0备份类别与恢复覆盖方式。当前均未做；原目录正文已读，完整交互和未验证分支仍保留，不把缺资料写成不支持。

## 来源


- E-MUD-01：[官方英文简明手册](https://assets.ctfassets.net/isxmxtc67n72/3biBEzYwk8UjSapgvMB461/b6ade0485fba2625382f44aca0fd0b51/ENG_Mudita_Kompakt_Manual_Online_04.pdf)，第 2–4 页，来自官方 Quick Start 下载链接。
- E-MUD-02：[基础设置](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/basic-settings-and-customization/)。
- E-MUD-03：[拨打电话](https://mudita.com/products/phones/mudita-kompakt/quick-start/calling/)。
- E-MUD-04：[联系人短信](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/)。
- E-MUD-05：[新号码短信](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/send-sms-to-new-number/)，官方索引正文。
- E-MUD-06：[MMS 附件流程](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/messaging/send-mms-to-contact/)，官方索引正文。
- E-MUD-07：[删除会话](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/delete-conversations/)。
- E-MUD-08：[传入电子书](https://support.mudita.com/en/support/solutions/articles/77000596832-how-to-transfer-e-books-to-kompakt-)，更新 2026-09-17。
- E-MUD-09：[传入音乐](https://mudita.com/products/phones/mudita-kompakt/quick-start/managing-files/transfer-music-to-kompakt/)，官方索引正文。
- E-MUD-10：[删除文件](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/managing-files/delete-files-from-kompakt/)，官方索引正文。
- E-MUD-11：[1.4 发布说明](https://mudita.com/about/media/muditaos-k-1-4-0-released-with-many-improvements-for-kompakt/)。
- E-MUD-12：[2.0 预告](https://mudita.com/community/blog/muditaos-k-2-0-at-ifa-2026-a-first-look-at-whats-coming/?tags=e+ink+technology%2Cmudita+kompakt%2Cmudita+backstage)，不作为已发布操作依据。
- E-MUD-13：[官方对 Center 侧载的说明](https://mudita.com/community/blog/why-mudita-kompakt-is-considered-a-flexible-minimalist-phone/)，功能发现，未验证所有兼容性。
- E-MUD-14：[1.6 官方发布条目](https://support.mudita.com/en/support/solutions/articles/77000598634-muditaos-k-1-6)，旧批仅登记条目；本轮 E-MDCD-03/04 已读取正文与英文附件。

本轮补证（均查阅于2026-10-27；原件/哈希见结构化批次）：

- E-MDCD-01：[官方 Kompakt 英文手册：整册目录与实体导航](https://assets.ctfassets.net/isxmxtc67n72/3biBEzYwk8UjSapgvMB461/b6ade0485fba2625382f44aca0fd0b51/ENG_Mudita_Kompakt_Manual_Online_04.pdf)；TOC p4; pp2–5; p9; observed illustration p3。
- E-MDCD-02：[官方 Kompakt 指南、FAQ、Center 目录快照](https://support.mudita.com/en/support/solutions/folders/77000329481)；10 how-to folders / 55 articles; FAQ 24; Center 2。
- E-MDCD-03：[MuditaOS K 1.6：1.6.1 发布范围与修复](https://support.mudita.com/en/support/solutions/articles/77000598634-muditaos-k-1-6)；Why 1.6.1; key fixes。
- E-MDCD-04：[K 1.6 英文发布附件（入口说明对应1.6.1）](https://support.mudita.com/helpdesk/attachments/77279297918)；pp1–6; 15 section headings; Phone p4; Maps p3。
- E-MDCD-05：[拨号、联系人与来电阻止](https://support.mudita.com/en/support/solutions/articles/77000579064-how-to-make-calls-and-manage-emergency-number-on-mudita-kompakt)；Call a contact / new number / block incoming call。
- E-MDCD-06：[短信、图片消息与会话删除](https://support.mudita.com/en/support/solutions/articles/77000579754-how-to-send-sms-and-mms-messages-on-mudita-kompakt)；SMS contact/new number; MMS; delete conversations。
- E-MDCD-07：[主音量、功能音量与提示音](https://support.mudita.com/en/support/solutions/articles/77000579749-how-to-manage-volume-levels-on-mudita-kompakt)；main volume; silent; Sound > Advance。
- E-MDCD-08：[静音：控制中心与设置入口](https://support.mudita.com/en/support/solutions/articles/77000596914-how-to-put-kompakt-in-silent-mode-)；Method 1 / Method 2。
- E-MDCD-09：[Offline+ 与飞行模式开关](https://support.mudita.com/en/support/solutions/articles/77000579070-how-to-use-offline-and-airplane-mode-on-mudita-kompakt)；Airplane / Offline+ / full function。
- E-MDCD-10：[Offline+ 硬件/软件停用边界](https://support.mudita.com/en/support/solutions/articles/77000577366-is-mudita-kompakt-offline-more-secure-than-airplane-mode-)；What is the difference?。
- E-MDCD-11：[USB 文件不可见及禁用数据传输](https://support.mudita.com/en/support/solutions/articles/77000580051-how-to-enable-and-disable-usb-file-transfer-on-mudita-kompakt)；File Transfer / No data transfer。
- E-MDCD-12：[Center 安装第三方 APK 与完成提示](https://support.mudita.com/en/support/solutions/articles/77000595285-installing-third-party-apps-via-mudita-center)；Steps1–10; observed Step9 image。
- E-MDCD-13：[第三方应用前提及当前卸载限制](https://support.mudita.com/en/support/solutions/articles/77000577356-does-mudita-kompakt-support-third-party-apps-)；How to Sideload / Uninstalling。
- E-MDCD-14：[隐藏/显示应用图标与保存退出](https://support.mudita.com/en/support/solutions/articles/77000579751-how-to-unhide-or-hide-app-icons-on-mudita-kompakt)；App layout / Preview / Back arrow。
- E-MDCD-15：[系统升级入口与失败处理](https://support.mudita.com/en/support/solutions/articles/77000579066-how-to-do-system-updates-on-mudita-kompakt)；update notification / manual check / fails。
- E-MDCD-16：[设置内恢复出厂](https://support.mudita.com/en/support/solutions/articles/77000579068-how-to-do-a-factory-reset-on-mudita-kompakt)；Warning; reset steps。
- E-MDCD-17：[启动循环：Recovery 恢复出厂](https://support.mudita.com/en/support/solutions/articles/77000586371-what-to-do-when-the-mudita-kompakt-is-stuck-in-a-loop-when-it-starts-up-)；Factory reset procedure。
- E-MDCD-18：[Center 创建/恢复备份及密码](https://support.mudita.com/en/support/solutions/articles/77000579156-how-to-create-and-restore-mudita-kompakt-data-backups-with-mudita-center)；create / restore / find backup file。
- E-MDCD-19：[Mudita Center 的 Kompakt 备份范围](https://mudita.com/products/software-apps/mudita-center/)；Mudita Kompakt > Backup & Restore; separate media section。
- E-MDCD-20：[Mudita Center 4.0.0 发布说明](https://support.mudita.com/en/support/solutions/articles/77000594959-mudita-center-4-0-0)；backup and restore; APK statuses。
- E-MDCD-21：[蓝牙配对入口](https://support.mudita.com/en/support/solutions/articles/77000579073-how-to-connect-devices-using-bluetooth-to-mudita-kompakt)；Connect a device。
- E-MDCD-22：[Kompakt 的 RCS 边界](https://support.mudita.com/en/support/solutions/articles/77000597865-does-mudita-kompakt-support-rcs-messaging-)；FAQ answer。

核心App补证（2026-10-27；18个此前只列标题的URL，均为官方帮助正文，未看图）：

| E | 来源 | 定位与范围 |
| --- | --- | --- |
| E-MACD-01 | [阻止未知/隐藏号码及解封](https://support.mudita.com/en/support/solutions/articles/77000573196-how-to-manage-blocked-numbers-on-mudita-kompakt) | Hidden/unrecognised; manual blocked list; incoming call; unblock；构建未标，正文修改2026-10-03。 |
| E-MACD-02 | [联系人短信拆分帮助](https://support.mudita.com/en/support/solutions/articles/77000596862-how-to-send-sms-messages-to-a-contact-) | Steps 1–5; keyboard note；构建未标，正文修改2026-10-03。 |
| E-MACD-03 | [新号码短信拆分帮助](https://support.mudita.com/en/support/solutions/articles/77000596863-how-to-send-sms-messages-to-a-new-number-) | Steps 1–6; keyboard note；构建未标，正文修改2026-10-03。 |
| E-MACD-04 | [联系人MMS拆分帮助](https://support.mudita.com/en/support/solutions/articles/77000596867-how-to-send-an-mms-message-to-a-contact-) | Steps 1–6；构建未标，正文修改2026-10-03。 |
| E-MACD-05 | [新号码MMS拆分帮助](https://support.mudita.com/en/support/solutions/articles/77000596869-how-to-send-an-mms-message-to-a-new-number-) | Steps 1–7；构建未标，正文修改2026-10-03。 |
| E-MACD-06 | [Notes新建与多选删除](https://support.mudita.com/en/support/solutions/articles/77000579951-how-to-create-and-delete-notes-on-mudita-kompakt) | How to create notes; How to delete notes；构建未标，正文修改2026-10-03。 |
| E-MACD-07 | [Recorder录制、停止、命名保存](https://support.mudita.com/en/support/solutions/articles/77000596747-how-to-record-a-voice-recording-) | Steps 1–7；构建未标，正文修改2026-10-03。 |
| E-MACD-08 | [Recorder回放与关闭画面](https://support.mudita.com/en/support/solutions/articles/77000596769-how-to-listen-to-a-voice-recording) | Steps 1–6；构建未标，正文修改2026-10-03。 |
| E-MACD-09 | [Recorder单个及批量删除](https://support.mudita.com/en/support/solutions/articles/77000596772-how-to-delete-voice-recordings-) | Single recording; many recordings；构建未标，正文修改2026-10-03。 |
| E-MACD-10 | [机内联系人创建、删除及阻止](https://support.mudita.com/en/support/solutions/articles/77000579753-how-to-manage-contacts-on-mudita-kompakt) | Add/delete/block/unblock contacts；构建未标，正文修改2026-10-03。 |
| E-MACD-11 | [Center批量删除联系人](https://support.mudita.com/en/support/solutions/articles/77000596849-how-to-delete-contacts-using-mudita-center-) | Steps 1–7; select-all tip；构建未标，正文修改2026-10-03。 |
| E-MACD-12 | [Center传入音乐及格式筛选](https://support.mudita.com/en/support/solutions/articles/77000596828-how-to-transfer-music-to-kompakt-) | Steps 1–6; Important；构建未标，正文修改2026-10-03。 |
| E-MACD-13 | [Center文件多选/全选删除](https://support.mudita.com/en/support/solutions/articles/77000596842-how-to-delete-files-from-kompakt-) | Steps 1–7；构建未标，正文修改2026-10-03。 |
| E-MACD-14 | [指纹添加与删除的口令前置](https://support.mudita.com/en/support/solutions/articles/77000579069-how-to-add-and-delete-fingerprints-on-mudita-kompakt) | How to add fingerprints; How to delete fingerprints；构建未标，正文修改2026-10-03。 |
| E-MACD-15 | [全局与逐App定位共享](https://support.mudita.com/en/support/solutions/articles/77000579756-how-to-manage-location-sharing-on-mudita-kompakt) | All location sharing; individual apps；构建未标，正文修改2026-10-03。 |
| E-MACD-16 | [屏幕休眠超时入口](https://support.mudita.com/en/support/solutions/articles/77000579752-how-to-change-the-screen-time-out-delay-on-mudita-kompakt) | Screen timeout steps 1–5；构建未标，正文修改2026-10-03。 |
| E-MACD-17 | [忘记PIN/图案后的有损恢复](https://support.mudita.com/en/support/solutions/articles/77000586370-can-i-access-the-mudita-kompakt-if-i-forgot-my-pin-or-screen-pattern-) | Factory reset procedure；构建未标，正文修改2026-10-03。 |
| E-MACD-18 | [Center联系人导入与管理综合帮助](https://support.mudita.com/en/support/solutions/articles/77000579155-how-to-manage-mudita-kompakt-contacts-with-mudita-center) | CSV/vCard; Google; Outlook; source exports; delete；构建未标，正文修改2026-10-03。 |

## 第37批补证：现有任务的数据与作用范围

- F-M012-06：照片传入经Center Photos；USB传出照片在DCIM、截图在Pictures，须启用MTP，Mac可能需额外工具。 传输、相册登记和双向同步分开，未测。 依据：E-MKRD-50、E-MKRD-51、E-MKRD-87。
- F-M012-08：第三方消息应用FAQ仍为首发范围，无应用商店；是否可侧载另受兼容条件约束。 修改日期不证明最新固件或指定APK运行。 依据：E-MKRD-74。
- F-M012-09：root相关FAQ表示厂商不提供MuditaOS K ROM；此陈述不等于已验证刷机或回刷。 没有安装其他系统或取得回滚原件。 依据：E-MKRD-79。
- F-M012-12：拆分六页已读：Outlook/Google登录选联系人，或电脑先导出CSV/vCard再上传；选择部分或全部后Import selected contacts。 第三方网站步骤仅厂商转述，导入不等于持续同步；未核去重、坏行、覆盖和失败回执。 依据：E-MKRD-38、E-MKRD-39、E-MKRD-40、E-MKRD-41、E-MKRD-42、E-MKRD-43。
- F-M012-14：FAQ没有内置语音转文字承诺，录音保存与转写分开。 FAQ未标构建，未核侧载转写。 依据：E-MKRD-78。
- F-M012-15：GPS帮助明确总开关与逐App许可均须打开，后者有Share/While using app/Don’t share；定位及App显示可分别滞后。授权状态不代表已定位或App显示已刷新。依据：E-MKRD-89。

## 本次新增文档任务

| 任务 | 前提/入口 | 操作与文档预期结果 | 未闭合分支 | 依据 |
| --- | --- | --- | --- | --- |
| F-M012-17 完成首次设置 | 尚未完成设置的Kompakt | 电源开机→语言/Wi-Fi→六位密码两次→按振动录指纹→可跳过教程→若有更新则更新并重启。 | 无网/取消/密码或指纹失败、更新中断与原数据迁移未知。 | E-MKRD-17 |
| F-M012-18 更改系统语言并添加键盘布局 | 进入Control Center的Settings | System→Language and Keyboard；System Language选语言后Confirm；Keyboard Settings另选Layout和Language，经Back回上级后Add。 | 默认输入法切换、移除布局、取消与重启保留未知。 | E-MKRD-19 |
| F-M012-19 配置SIM/eSIM并确认模式切换 | 相应SIM或eSIM激活资料；eSIM安装需网络 | SIM Cards中扫码/手输激活码→接受策略→Add；立即启用或两次确认Switch mode会重启。About可查卡槽号码；eSIM与SIM2互斥而可保留SD。 | 两篇帮助对先安装或先切模式存在顺序疑点；激活失败、卡默认选择与配置删除未知。 | E-MKRD-18、E-MKRD-20、E-MKRD-21、E-MKRD-76、E-MKRD-85 |
| F-M012-20 从主页日期创建日历事件 | 主页日期或已取消隐藏的Calendar入口 | 切月→选日→＋→填标题与起止时间→Create。 | 创建回执、提醒/重复、跨日校验、编辑删除与同步恢复未知。 | E-MKRD-29 |
| F-M012-21 创建、修改、删除和贪睡闹钟 | 主页时钟或可见Alarm入口 | 首个Create an Alarm/后续＋→时间及名称、铃声、重复→Create；点时间编辑后Save；铅笔→删除；响铃手动Snooze固定五分钟。 | 删除只在提示时确认；隐藏图标入口表述不一；关闭响铃、贪睡上限、离线/重启唤醒与持久状态未知。 | E-MKRD-30、E-MKRD-34、E-MKRD-35、E-MKRD-36、E-MKRD-37 |
| F-M012-22 下载地区地图供离线查看并删除地图包 | 首次联网取得地区包 | Maps→Manage Maps→搜索或All Maps选地区→Download；文档承诺下载后离线搜索/查看；Edit→目标删除→Delete Map。 | 下载进度/校验、暂停/断网续传、存储不足、版本更新与路线导航未给出。 | E-MKRD-32 |
| F-M012-23 选择天气地点并处理定位等待 | Weather控件；自动位置另需系统与App许可 | Menu→铅笔→默认地点→搜地点或Use current location→Done；权限开启后仍可能等GPS和App刷新。 | 许可成功不等于已定位/天气已更新；缓存时间、无位置回退、手动刷新和错误状态未知。 | E-MKRD-33、E-MKRD-89 |
| F-M012-24 拍照并在相册查看 | Apps→Camera | 取景→拍照；Image图标→选照片。设备灰阶显示而传至电脑可见彩色；官方建议拍摄时多保持静止。 | 快门即时反馈、抖动/保存失败、删除恢复、拍摄时长未观察；视频限制仅FAQ范围。 | E-MKRD-55、E-MKRD-75 |
| F-M012-25 截取当前画面并找到截图 | 设备当前画面 | 电源加音量减；文档称自动保存且无屏幕提示；到相册查找，USB传出时截图目录为Pictures。 | 未按键实测；组合键识别、锁屏/权限、失败回执、文件命名与重复截图未知。 | E-MKRD-55、E-MKRD-62、E-MKRD-86、E-MKRD-50 |
| F-M012-26 连接或共享网络并控制蜂窝数据 | Settings→Network and Internet；对应网络可用 | Wi-Fi选网输密码Connect；共享当前网先验PIN/指纹。热点设名称/至少八位密码/安全项后开关启停，控制中心也可开关；蜂窝数据和漫游分别开关。 | 错误密码、认证取消、路由/并发、切网后下载、后台重连和实际连通结果未核。 | E-MKRD-66、E-MKRD-68、E-MKRD-69、E-MKRD-70、E-MKRD-93 |
| F-M012-27 查看电池状态并设置充电上限 | Settings→Battery→Battery Information | 查看容量、循环、制造及初用信息；Stop charging at 80%开关在80%/100%之间选上限。 | 设置回执与实际充电终止分开，阈值滞后、重启保留和充电方式适用范围未知。 | E-MKRD-61 |

详见[46篇逐源摘记与原件日期](../artifacts/mudita-remaining-controls/source-notes.md)。SIM/eSIM先安装还是先切模式的疑点、Alarm隐藏图标入口差异均保留，不能自行补成统一成功路径。截图无即时反馈、地图下载完成后离线可用、GPS许可/定位/天气更新是不同状态，值得作为G-002执行结果的比较问题；这些仍是文档依据，并非本项目已定方案。

FAQ中的无闪烁措辞及标准引用没有给出波形、残影或帧率测量，不能用于兑现R-011的60fps目标。射频、续航、SAR、耐用性均属Kompakt厂商声明，不是Plus硬件验证或儿童使用效果结论。
