# M-040 AntennaPod：订阅、队列、音频中断与可恢复数据

## 范围与方法

初查2026-10-26，深化2026-10-27。对象为Android **AntennaPod 3.12.3**，官方正式发布于2026-10-13，tag提交 **175e5ef8fe3e125a5014d70ef0c57cf85661c71b**，声明versionCode 3120395。未取得/运行发行APK，固定的是发行源码，不声称验证了安装产物或任一Android设备。英文资源为定位依据，中文是分析转述；地区、厂商系统、账号和用户既有配置均未实测。

旧E-ANTP帮助页没有逐页构建边界；旧代码E-ANTP-14～17来自develop `d3e41c6...`，保留为历史。3.12.3明确开启Media3、禁用旧服务，不能把旧服务中的重连逻辑当本版承诺。官方3.12.0发布记录也说明服务重写。[E-APCD-01、04]

覆盖为**D2文档/定点源码重建**；目录和仅声明的设置为D1。没有构建、安装、登录、数据提交、APK/视频下载、实际操作或产品截图观察，`hands_on=false`、`visual_observed=false`。前批队列深化建立F13已有标签浏览；第49批沿缺口补14个固定提交文件，并复读七组旧来源，接通筛选、标签、删除和队列的数据路径。F02/03/05/13沿原编号深化，新增F14局部筛选任务；共14个D2，仍无产品运行证据。

## 功能目录与覆盖

[逐项目录](../artifacts/antennapod-controls-deepening/functional-inventory.csv)列出官方帮助索引全部**47条（8分类、39文章）**及3.12.3全局**11个preferences XML的84个带key节点**；[定点来源记录](../artifacts/antennapod-controls-deepening/source-notes.md)给出代码范围。它们只核定这两个明确集合，**不是全应用功能已分析**：每播客动态设置、播放器全部菜单、转录、统计、扩展/车载等仍有缺口。

[队列深化目录](../batches/antennapod-queue-controls-deepening-inventory.csv)继承上述131项，前批另列27个源码分支；第49批更新12项并增加4个入口/状态分支，共162行。父项、条件及未闭合链分开，不等于162个完整流程。[独立摘记](../artifacts/antennapod-queue-controls-deepening/source-notes.md)区分文档条件、源码请求与未知执行结果。

| 功能域 | 目录/设置定位 | 关联F/E | 已查与未覆盖 |
| --- | --- | --- | --- |
| 入门、Home、订阅来源 | Getting started；Home/Subscribe | F01；E-ANTP-01、E-APCD-02 | 新增入口已有D2；Home布局/首次引导待查。 |
| 订阅管理 | Subscriptions tags；SubscriptionFragment/RemoveFeedDialog | F01、13；E-APCD-13、14；E-APQD-01、02 | 取消/归档/恢复及已有标签浏览已有静态链；第49批已补单/多订阅标签编辑、确认/取消及共同集合；全局重命名/删除与同步仍缺。 |
| 待听队列 | Queue 4篇；Playback/Queue设置；QueueFragment | F02、03、09；E-ANTP-02～04、E-APCD-03；E-APQD-01、04、05 | 补排序回调、锁定、清空确认和批量请求；当前帮助明确不支持多队列，标签不产生独立队列。 |
| 订阅刷新与下载 | Automation 4篇；Downloads、Automatic download XML | F04、10；E-ANTP-05、09、E-APCD-03；E-APQD-03、05、08、09 | 刷新周期/单源排除、手动批量资格与重试请求补读；Tasker、代理、外部存储/权限仍仅目录。默认数量冲突保留。 |
| 清理与状态 | Deletion/Automatic deletion；Auto deletion XML | F01、05；E-ANTP-06、07、E-APCD-14 | 已补本地源警告及物理失败/提示数边界；撤销、恢复与清理预览仍缺。 |
| 播放、章节 | Playback/Auto rewind；Cover/Chapters源码 | F06、07；E-ANTP-08、E-APCD-05、06 | 章节入口/空集/跳转本轮D2；倍速/静音跳过/转录/单集笔记完整任务未查。 |
| 睡眠计时 | 播放菜单/底部表单/计时类 | F08；E-APCD-07～10 | 分钟与集数、事件扣时、结束/停用及部分异常D2；短暂停/休眠/传感器组合未实测。 |
| 中断、外设、通知 | Playback Interruptions；User interface；Session | F09、11；E-APCD-04、10～12、17 | 3.12.3服务、焦点委托、断开偏好和通知命令D2；来电/重连实际表现未知。 |
| 备份/恢复/同步 | General Backup/Sync；Backup & restore XML | F12；E-ANTP-12、13、E-APCD-15、16、18 | DB确认/替换/失败路径D2；自动备份周期、HTML/收藏回导和双端冲突未闭合。 |
| 显示、导航、过滤与手势 | User interface、Swipe XML；ItemFilterDialog/FeedItemFilterQuery | E-APCD-03；E-APQD-06、07 | F14补全部单集/单播客入口、即时写入、空结果和回读范围；原显示设置及订阅筛选仍未补齐，不能共用确认规则。 |
| 错误、数据损坏 | Bugs/errors 8篇 | F10～12；E-ANTP-09～11、18；E-APCD-18 | 原下载/后台排错已有D2；历史库损坏恢复提示与正常退订区分，未执行修复。 |
| 车载、投放、键盘 | Android Auto、Chromecast、Shortcuts帮助；硬件键设置 | E-APCD-02、03 | 仅目录；外设路由、焦点与按钮映射组合未验证。 |
| 统计、项目、平台/权限、播客主 | General其余条目、Podcasters/hosters、About/Notifications XML | E-APCD-02、03 | 统计、广告、F-Droid、Beta、权限、募资/收录/Feed URL等只目录，未提升D2。 |

## 功能与流程

下表“显示/暂停/删除”等均是文档描述或静态回调，未记录实际反馈耗时。

| F 编号 | 任务 | 前置状态 | 操作与资料中的反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M040-01 | 新增来源、取消订阅与归档恢复 | 新增需网络与可用源；已有订阅可管理 | 原帮助：Add Podcast搜索/RSS/OPML。3.12.3订阅菜单 Unsubscribe / archive → Archive 或 Unsubscribe；后者先出现确认按钮，确认后执行。订阅页菜单 Archive → 归档对象 Unsubscribe / restore → Restore。处理时显示进度并隐藏取消，成功或错误均关闭表单（错误记录日志）。 | 新增只建目录；Archive改状态并保留下载/历史，文案称停止找新集；Unsubscribe删该源记录、移出队列、取消下载并清在线源下载，当前媒体发STOP。本地文件夹退订不删底层原文件。归档Restore与删除后的重订阅不同。 | E-ANTP-01；E-APCD-01；E-APCD-13；E-APCD-14 | 重复源合并、私有认证；删除后无逐项撤销证据，重订阅能否保留原历史未知；同步远端处理、批量中途失败回滚未闭合。 |
| F-M040-02 | 把想听内容放进队列 | 已有媒体；批量需已有选择 | 单集长按出现上下文菜单，选Multi-select才启动多选；选择中点击条目/副按钮切换。选中项按列表顺序取出；原Add to queue及下载入队开关保留。 | 批量跳过无媒体/已入队项，发动作后结束选择；提示数来自请求资格计数。 | E-ANTP-02；E-ANTP-06；E-APCD-03；E-APQD-04；E-APQD-05；E-APST-01 | 多选中旋转/数据变化、并发重复入队、DB失败及实际反馈未知。 |
| F-M040-03 | 混排多个节目并保留先后关系 | 已有队列；固定3.12.3 | 排序选择即请求保存，Random关闭Keep Sorted；锁定/Keep Sorted限制移动。清空先确认，取消只关闭；确认后删队列表并发CLEARED事件更新列表。 | 移顶先反转选中列表再逐项插首，保留传入顺序；移底顺序追加。setQueue以事务重建，但SQL异常可仅日志，随后事件不证明落盘成功。清空分支不调用媒体删除/STOP。 | E-ANTP-03；E-ANTP-04；E-APQD-01；E-APQD-04；E-APQD-05；E-APCD-14；E-APST-07；E-APST-08 | 当前播放服务对清空事件的所有反应、撤回、并发/故障注入及实机拖动未测；不承诺清空后一定续播。 |
| F-M040-04 | 准备离线节目：自动资格与手动批量 | 已订阅；自动下载资格/网络条件满足；手动批量已有选择 | 原自动下载入口保留。帮助补充Settings → Downloads → Refresh podcasts可选周期/关闭，单源齿轮Keep Updated关闭后排除自动与Queue/Episodes批量刷新，仍可单独刷新。3.12.3手动批量下载只向有媒体、未下载、非本地源的项发请求。 | 自动下载数量仍有帮助50/XML25/Java回退20边界。Keep Updated关闭排除自动及Queue/Episodes批量刷新但可单独刷新；手动批量下载按传入顺序请求，跳过无媒体/已下载/本地源。 | E-ANTP-05；E-APCD-03；E-APQD-03；E-APQD-05 | 帮助12小时刷新默认未核本版初始化；批量数量是请求资格计数，非下载完成。半文件、自动退避、实际传输并发/次序、断网恢复及初始默认仍未知。 |
| F-M040-05 | 清下载并区分退订与本地原文件删除 | 所选含下载、本地源或下载中媒体 | 任一所选本地源触发警告，确认才执行整组回调，取消不调用；无本地源直接执行。警告说明会删除设备原文件且不能由本应用重新下载。 | 已下载/本地媒体请求删除，下载中请求取消；计数在提交时产生。物理删除失败可仅日志并继续状态路径；删除当前媒体STOP、本地刷新与可选出队分开。 | E-ANTP-06；E-ANTP-07；E-APCD-03；E-APCD-14；E-APQD-05；E-APST-05；E-APST-07 | 实际取消/失败UI、半文件与原文件残留、批量回滚、撤销及恢复未测；数量提示不等于删净。 |
| F-M040-06 | 暂停后恢复并补听上下文 | 先前有播放位置 | 文档说明重新播放会自动回退：暂停超过 1 分钟/1 小时/1 天分别回退 3/10/20 秒，不能自配。 | 恢复位置不是逐毫秒原点，而是按暂停间隔回看少量内容。 | E-ANTP-08 | 阈值正好相等、频繁暂停、强退/断电、越过章节边界的实效。 |
| F-M040-07 | 从播放封面查章并跳转（3.12.3静态） | 已选媒体；章节非空或媒体标记有尚未载入章节 | 播放封面页章按钮 → 章节表。加载进度；点章时若未播放先play，再seek起点、更新当前章；Close退出。外部章节URL存在才有Refresh。封面上一章在当前章起点附近跳前一章，否则回本章起点；末章隐藏下一章。 | 显隐由章节数据/标记决定；上一章窗口按10000×播放速度换算媒体毫秒。空集关闭并提示无章；异常回调仅日志，未证明可见重试。 | E-ANTP-14；E-APCD-05；E-APCD-06 | 无当前媒体/网络章节错误的最终UI与残留进度未知；章节按键、无障碍焦点、跨章实机时间未观察。 |
| F-M040-08 | 设分钟或集数睡眠计时（3.12.3静态） | 播放状态满足源码isRunning条件 | 播放菜单 → Sleep timer；选分钟/集数、输入、Set。0/解析失败及无播放走提示。可Disable或延期（分钟5/10/30，集数1/2/3）。连续播放关闭时集数设置禁用，给Playback设置入口；超队列或剩余时长有提示条件。 | 类型/上次数值/自动时段是偏好；当前剩余值在计时对象。分钟计时由位置事件触发，以System.currentTimeMillis两次事件到达差扣减，间隔>10秒跳过，绝非用position差计算。临近10秒渐弱/可振动，摇动可重置初值；到期pause，集数完结阻止接播；服务销毁停止计时。 | E-ANTP-15；E-ANTP-16；E-APCD-07；E-APCD-08；E-APCD-09；E-APCD-10 | 短暂停、缓冲、未知duration、后台杀进程和时钟变动的真实表现未测；传感器不可用异常未闭合；不得承诺纯墙钟到期。 |
| F-M040-09 | 顺序接播、输出断开与音频焦点变化（3.12.3静态） | 固定3.12.3默认Media3服务；下一项及网络许可满足 | 正式配置启用Media3，旧服务禁用。完成后受集数计时/连续播放控制；无下一项清当前媒体，禁止移动流播时转确认项并停止自动播放。ExoPlayer以speech媒体属性委托Media3自动焦点，按断开偏好处理noisy；焦点临时抑制解除后可回退上下文。 | 停播回调保存位置并排同步播放动作；不能把旧服务的蓝牙重连分支搬来。API31+隐藏耳机/蓝牙重连设置，较低API有设置不等于新服务相同恢复。 | E-ANTP-17；E-APCD-04；E-APCD-10；E-APCD-11；E-APCD-12 | Media3库底层永久/暂时焦点、电话与导航策略、蓝牙恢复及各系统实际通知未操作；源码委托不构成恢复保证。 |
| F-M040-10 | 定位下载/刷新错误并恢复 | 下载日志含失败记录；原源/媒体对象可能已移除 | 旧帮助由错误通知进Download Log，并称长按URL在浏览器打开。3.12.3重试先隐藏按钮，再查原对象：存在才发刷新/下载请求，缺失仅写日志。详情点击URL/技术原因复制，另可复制全部；OK关闭。允许跳转且源存在才显示播客入口，跳转关闭详情及日志表单。 | 3.12.3失败项可请求重试；按钮先隐藏，找不到源/媒体只日志。此前列表同类型/ID有成功则隐藏重试。详情可复制URL/技术原因或整段；对象存在才可转播客。 | E-ANTP-09；E-ANTP-18；E-APQD-08；E-APQD-09 | 日志列表严格时间顺序、自动退避、服务内部许可/取消/重试、失败阻塞未知；提示或按钮消失不是成功。旧帮助长按URL与本版点击复制分开，未人为制造错误。 |
| F-M040-11 | 核对锁屏/通知控制与后台排错的版本适用性 | Android系统和安装版本已知；本案仅固定代码tag | 旧帮助建议通知无法恢复时加Quick Settings tile，另查电池优化；3.12.3配置却禁用旧tile服务，故该建议不直接套用。当前Media3 Session提供快退/快进及可配速度、下一章、跳集、睡眠动作，睡眠按钮随状态切换命令。 | 设置中的高优先通知API26+隐藏、持久控制API30+隐藏；声明的按钮偏好不代表系统一定全部呈现。旧帮助根因未定，不升级成3.12.3已复现问题。 | E-ANTP-10；E-ANTP-11；E-APCD-03；E-APCD-04；E-APCD-17 | 锁屏布局/槽位、厂商电池行为、后台被杀与暂停后重建均未验证；须另做固定Android版本观察。 |
| F-M040-12 | 备份恢复、部分同步与删除后的恢复边界 | 先导出现库备份；同版本迁移建议沿官方帮助 | 3.12.3 Settings → Backup & restore → Database export选文件；导入先显示替换订阅/播放历史警告，可No或取消文件选择。确认选择后等待；成功需OK重启，失败显示错误。OPML勾选追加现有订阅；同步配置沿旧帮助。自动DB备份选择可写文件夹并持久授权，周期执行仅目录。 | SQLite导出保留库数据，不含音频；全局UserPreferences在SharedPreferences，不能以官网all data推成全部设置备份。导入拒绝比当前更高的DB schema，随后替换旧库；非合并。OPML/同步/HTML收藏输出范围不同。归档Restore不等同退订撤销；数据库恢复会覆盖当前库。 | E-ANTP-12；E-ANTP-13；E-APCD-03；E-APCD-15；E-APCD-16；E-APCD-18 | 失败后事务回滚未证明；同步冲突/账户切换未核；历史损库修复帮助不能当正常删除恢复。全局设置迁移、自动备份周期执行与文件权限撤销待验证。 |

| F-M040-13 | 整理订阅标签、按标签浏览并恢复选择 | 单订阅或批量已有选择；固定3.12.3 | 标签菜单打开临时列表；加/删先改显示集合，OK再加入输入框剩余文本并提交，Cancel无提交回调。批量只显示共同标签，保存替换共同集合而保留各自独有标签。 | 每节目分别写入并通知重载；空名/重复名不新增，也不阻止OK提交已有列表。原标签浏览/onPause保存last_tag、归档从root开始及过滤后回退规则保留。 | E-APQD-01；E-APQD-02；E-APCD-13；E-APCD-14；E-APST-04；E-APST-06；E-APST-07；E-APST-14 | 未核全局重命名/删除、跨端同步、并行编辑/部分失败、异常终止与恢复；root仍受现有过滤限制。 |
| F-M040-14 | 筛选全局或单播客单集并恢复可见范围 | 有可进入的列表；区分全局与feed | 全局菜单/筛选提示→选项变化事件→写偏好、页数回1、重载；单播客按钮/既有筛选提示→选项变化→写该feed过滤字段→事件重载。Confirm只关闭，Reset空集合。 | 全局偏好在SharedPreferences，单源在DB；全局空条件仍默认已订阅，收藏扩到所有源状态，单源查询仍只看该feed。空筛选结果提示可清条件。 | E-APST-02；E-APST-03；E-APST-07；E-APST-09；E-APST-10；E-APST-11；E-APST-12；E-APST-13；E-APQD-06；E-APQD-07；E-APCD-03；E-APCD-14 | 关闭不是撤回保证；重启落盘、并行变化、查询失败实际UI、备份/重装恢复未测；订阅筛选为另一控件。 |

## 状态与异常边界

“归档”“删除下载”“取消订阅”“OPML加入”和“数据库替换”操作不同：前者保留订阅数据待恢复；删除下载保留源；取消订阅会清该源记录并触发队列/下载处理；OPML是勾选后追加；数据库导入则覆盖当前库。取消订阅的错误回调也关闭表单，不能把退出当作成功反馈；没有找到删除后逐项回收站或撤销承诺。[E-APCD-13～16、E-ANTP-12]

分钟睡眠计时**不是按音频position差值扣减**。ClockSleepTimer在收到PlaybackPositionEvent时读取`System.currentTimeMillis()`，扣两次事件到达的时间差；差值大于10秒则更新lastTick后返回。Media3服务的位置观察按状态启停，正常周期为每秒、且仅duration>0才发事件。这只能说明事件驱动的实现；短暂停可能仍落入计时差值，缓冲、焦点抑制和休眠的具体结果未运行，不能称“暂停必冻结”或“设置后墙钟到期”。参数偏好保存不代表剩余倒计时持久保存。[E-APCD-08、10]

“临时音频焦点抑制解除后可回退”来自Media3服务回调；真正焦点夺取/回授由依赖库和Android参与处理。断开暂停、重连自动恢复、用户主动暂停是不同任务。本版API31+隐藏重连开关，旧服务代码的同名函数不足以证明Media3行为。通知/锁屏的命令集合也不能代替系统实际布局或后台生存测试。[E-APCD-10～12、17]

备份帮助用“all data”概括数据库恢复，但本tag只导出SQLite，UserPreferences另在SharedPreferences。故本案收窄为库内订阅/单集/队列等状态，音频须重新取得，全局偏好迁移不能保证。导入拒绝更高DB schema，并非仅比较app版本字符串；验证后删除旧库再移动新库，未见足够证据承诺任何失败都有回滚。损库帮助针对历史问题，残留音频可再作为本地文件夹加入不等同恢复RSS更新或已删历史。[E-APCD-15、16、18]


本次队列和下载补证将“发请求”与“处理完成”分开。批量下载按传入列表遍历并调用下载接口，随后累加数量；批量删除对已下载/本地媒体请求删除，对正在下载的未完成项请求取消。数量不是完成回执，也不说明实际传输并发或完成顺序。移出Inbox只将New改为Unplayed，不能当作删媒体。队列的锁定/Keep Sorted限制移动，不是防删除保证；本批已追清空/移动的数据库方法，实际撤回、拖动取消及故障回滚仍未知。[E-APQD-04、05；E-APCD-14；E-APST-07]

下载日志重试的按钮在查库之前隐藏；源/媒体已不存在时只记录日志并返回。失败项之前的列表位置若已有同类型/ID成功项，也不显示重试；本批未读日志排序提供方，不将函数名newerWasSuccessful推成已核实的时间排序。详情URL点击复制与旧帮助的长按浏览器行为分列，未核长按在本版是否仍生效。原对象缺失时可保留日志标题而URL为unknown；提示消失、表单关闭或“下载中”事件均不代表成功。[E-APQD-08、09；E-ANTP-09]

第49批已把ItemFilterDialog接到全局和单播客两个调用方：选择时即请求写入，Confirm仅关闭，Reset先传空集。未找到该链在关闭时补偿原值的回调；这仍是源码结论。全局筛选、单源过滤和标签编辑的保存对象/提交时点不同。FeedItemFilterQuery以AND组合条件，Unplayed包括New、Paused依position>0、Downloaded依下载日期；这些字段不证明实时暂停或文件存在。SubscriptionsFilterDialog是另一未读控件，不套此规则。[E-APST-02、03、07、09～13；E-APQD-06、07]

已有标签的last_tag只在普通订阅页onPause保存；异常终止是否经过该回调未知。标签在过滤后集合中不存在会退root，但root仍受当前过滤条件限制，不能解释为“显示全部订阅”。第49批补标签编辑的OK/Cancel与共同集合算法；每节目分别请求写入，不能推成全批原子提交，也未补齐全局标签管理或跨设备同步。[E-APQD-02；E-APCD-13]

## 对本项目的分析（推断）

对G-001/R-020，Inbox、Queue、Downloads分别表示新到、计划、文件可用，不能被AI合并成“我的内容”。对G-002/R-018，AI执行“清掉不听的”至少应明确归档、删媒体、退订、同步动作与可恢复范围；“晚上停播”还需明确按分钟、按集数和中断规则。状态可读可控的思想有依据可研究，Android实现未在Plus验证。

儿童前台可只暴露已准备的离线集合，家长负责订阅、下载资格和保留规则，这是助手推断，不是新增需求。自动回退、计时渐弱也只是候选体验；没有Plus后台、传感器、功耗和解码测试支持性能承诺。

## 后续最小核查

固定3.12.3安装产物与一个Android版本，使用两个可控RSS源及本地文件夹，观察章节空/非空、暂停超过/不足10秒时计时、电话/导航焦点、拔插耳机、归档恢复/确认退订、DB覆盖及取消。再分别观察筛选关闭/重启、批量标签一项失败、含本地源删除取消、队列清空时当前播放。记录UI、数据库与媒体文件分别变化；本轮没有执行这些任务。先确认API31+重连入口与旧tile是否可用，再决定扩测厂商差异。

## 结构化竞分（2026-10-28）

- 对象/信息架构
  - 订阅（feed）/标签、Inbox（新到）/Queue（待听）/Downloads（文件可用）、单集（item）、过滤条件（全局/单源）、数据库备份（DB）为不同对象（F‑M040‑01/02/03/04/05/12/13/14）。
  - “归档/退订/删除下载/备份导入（替换）/OPML 追加”语义分离（F‑M040‑01/05/12）。
- 关键任务流（已证与限制）
  - 订阅/归档/退订/恢复（D2）；加队列与排序/锁定/清空（D2）；自动/手动下载资格（D2）（F‑M040‑01/02/03/04）。
  - 删除下载与本地源警告（D2）；自动回退（D2）；章节入口（D2）；睡眠计时（分钟/集数）（D2）（F‑M040‑05/06/07/08）。
  - 中断与焦点/断开（D2）；下载/刷新错误定位与重试（D2）；DB 导出/导入与同步（D2）；标签与筛选（D2）（F‑M040‑09/10/11/12/13/14）。
- 失败与恢复边界
  - 多队列不支持；睡眠计时按事件时钟扣减，短暂停/缓冲的实际表现未证；重试按钮隐藏不等于成功（相应分支）。

本批补充记录（2026‑10‑28）：产品为 Android 原生应用，无 Web 端可操作最小链；见 `../artifacts/antennapod-controls-deepening/2026-10-28-browser-operation-audit.md`（E‑APCD‑19）。保持 D2，以官网/帮助/源码静态证据明确边界与未知。
  - DB 导入为覆盖当前库；“all data”不含音频文件与全局偏好（F‑M040‑12）。
- 对 E‑Ink4 Plus 与家庭的启发与风险
  - 启发：将“新到/计划/可用文件/队列/过滤/备份”区分；显式“归档 vs 退订 vs 删文件”。
  - 风险：把数量提示当完成回执；把锁定当作防删除保证；把回退当作逐毫秒断点。

## 证据

原E-ANTP查阅日期2026-10-26；E-APCD及队列补证E-APQD为2026-10-27。新源码URL固定3.12.3提交，原帮助保持版本未指定。原件在work，正式归档为来源笔记和目录；没有视觉观察或连续时间戳。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-ANTP-01 | [Subscribing to a podcast](https://antennapod.org/documentation/getting-started/subscribe) | Add Podcast、RSS/Atom/OPML、来源边界。 |
| E-ANTP-02 | [How to use the queue](https://antennapod.org/documentation/queue/how-to-use) | 长按加入和下载入队。 |
| E-ANTP-03 | [Queue sorting](https://antennapod.org/documentation/queue/sorting) | 双层排序、Smart shuffle、尽量分散。 |
| E-ANTP-04 | [Unlocking the queue](https://antennapod.org/documentation/queue/lock) | Keep Sorted 与 Lock queue。 |
| E-ANTP-05 | [Automatic downloads](https://antennapod.org/documentation/automation/automatic-downloads) | Inbox 资格、开关、网络、数量、筛选。 |
| E-ANTP-06 | [Deletion of episodes](https://antennapod.org/documentation/general/episode-deletion) | 手动删除、是否出队、Downloads。 |
| E-ANTP-07 | [Automatic deletion](https://antennapod.org/documentation/automation/deletion) | 播完与手动已播的差异；清理资格。 |
| E-ANTP-08 | [Automatic rewind](https://antennapod.org/documentation/playback/auto-rewind) | 暂停间隔与回退量。 |
| E-ANTP-09 | [Refresh/download errors](https://antennapod.org/documentation/bugs-first-aid/refresh-download-errors) | Download Log、URL、网络与解析错误。 |
| E-ANTP-10 | [Cannot resume from lock screen](https://antennapod.org/documentation/bugs-first-aid/cannot-resume-playback) | Android 13/14、tile 替代、根因未知。 |
| E-ANTP-11 | [Playback suddenly stops](https://antennapod.org/documentation/bugs-first-aid/playback-stops) | 厂商电池优化排查。 |
| E-ANTP-12 | [Backup](https://antennapod.org/documentation/general/backup) | 数据库、同步、OPML 覆盖和卸载路径。 |
| E-ANTP-13 | [Synchronization](https://antennapod.org/documentation/general/synchronization) | 提供方配置、设备关联、Force sync。 |
| E-ANTP-14 | [ChaptersFragment.java](https://github.com/AntennaPod/AntennaPod/blob/d3e41c6cb37c8e1a060b142686f818999de89859/app/src/main/java/de/danoeh/antennapod/ui/screen/chapter/ChaptersFragment.java) | onCreateDialog/onCreateView/onMediaChanged；静态源码。 |
| E-ANTP-15 | [SleepTimerDialog.java](https://github.com/AntennaPod/AntennaPod/blob/d3e41c6cb37c8e1a060b142686f818999de89859/app/src/main/java/de/danoeh/antennapod/ui/screen/playback/SleepTimerDialog.java) | onCreateView、getSelectedSleepTime、refreshUiState；静态源码。 |
| E-ANTP-16 | [AudioPlayerFragment.java](https://github.com/AntennaPod/AntennaPod/blob/d3e41c6cb37c8e1a060b142686f818999de89859/app/src/main/java/de/danoeh/antennapod/ui/screen/playback/audio/AudioPlayerFragment.java) | onMenuItemClick、sleepTimerUpdate；静态源码。 |
| E-ANTP-17 | [PlaybackService.java](https://github.com/AntennaPod/AntennaPod/blob/d3e41c6cb37c8e1a060b142686f818999de89859/playback/service/src/main/java/de/danoeh/antennapod/playback/service/PlaybackService.java) | getNextInQueue、pauseIfPauseOnDisconnect、unpauseIfPauseOnDisconnect；仅该服务分支。 |
| E-ANTP-18 | [Cannot download an episode](https://antennapod.org/documentation/bugs-first-aid/cannot-stream-download) | 媒体由发布方提供，上游删除边界。 |

| E 编号 | 正式版本首次深化来源 | 定位 |
| --- | --- | --- |
| E-APCD-01 | [AntennaPod official release 3.12.3 and tag identity](https://github.com/AntennaPod/AntennaPod/releases/tag/3.12.3) | release metadata; tag-ref; app/build.gradle 15–16 |
| E-APCD-02 | [Official documentation index](https://antennapod.org/assets/datasets/documentation_index.json) | 47 index entries, no per-page build |
| E-APCD-03 | [3.12.3 global settings declaration inventory](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/ui/preferences/src/main/res/xml) | 11 preferences XML; 84 keyed nodes; referenced strings and visibility flags |
| E-APCD-04 | [3.12.3 selected playback service](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/common.gradle) | common.gradle 15–16; service manifest 20–79; starter 44–60 |
| E-APCD-05 | [3.12.3 chapter entry and previous/next controls](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/playback/audio/CoverFragment.java) | 108–111; 204–273 |
| E-APCD-06 | [3.12.3 chapter loading and selection](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/chapter/ChaptersFragment.java) | 50–87;135–173 |
| E-APCD-07 | [3.12.3 sleep timer UI and service commands](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/playback/SleepTimerDialog.java) | 147–270;315–419;465–476; AudioPlayerFragment 535–536 |
| E-APCD-08 | [3.12.3 clock sleep timer event cadence and reset](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/ClockSleepTimer.java) | 34–54;77–142; SleepTimer/ShakeListener/SleepTimerUpdatedEvent |
| E-APCD-09 | [3.12.3 episode sleep timer](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/EpisodeSleepTimer.java) | 31–65 |
| E-APCD-10 | [3.12.3 Media3 playback interruption, timer and continuation](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/Media3PlaybackService.java) | 339–355;390–428;472–477;504–535;803–879;884–937 |
| E-APCD-11 | [3.12.3 Media3 audio focus and noisy configuration](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/ExoPlayerUtils.java) | 47–69 |
| E-APCD-12 | [3.12.3 playback preferences visibility](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/preferences/PlaybackPreferencesFragment.java) | 38–56; API31 reconnect switches hidden |
| E-APCD-13 | [3.12.3 unsubscribe, archive and restore dialog](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/feed/RemoveFeedDialog.java) | 59–84;119–178;193–206; FeedMenuHandler and SubscriptionFragment archive entry |
| E-APCD-14 | [3.12.3 media/feed deletion and archive persistence](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/storage/database/src/main/java/de/danoeh/antennapod/storage/database/DBWriter.java) | 118–167;177–195;212–249;862–884 |
| E-APCD-15 | [3.12.3 backup and restore UI, confirmation and errors](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/preferences/ImportExportPreferencesFragment.java) | 123–181;189–278;352–389; strings database_import_warning |
| E-APCD-16 | [3.12.3 database export/import scope and schema gate](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/storage/importexport/src/main/java/de/danoeh/antennapod/storage/importexport/DatabaseExporter.java) | 45–105; UserPreferences 148–152 (separate SharedPreferences) |
| E-APCD-17 | [3.12.3 notification/session control commands](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/playback/service/src/main/java/de/danoeh/antennapod/playback/service/internal/MediaLibrarySessionCallback.java) | 113–199 |
| E-APCD-18 | [Official recovery guidance for historical database corruption](https://antennapod.org/documentation/bugs-first-aid/database-error) | Recovery list; pre-2019 damage context |

本次队列深化来源（没有图像观察；源码固定上述提交）：

| E 编号 | 来源 | 实读定位 |
| --- | --- | --- |
| E-APQD-01 | [Official help: Multiple queues](https://antennapod.org/documentation/queue/multiple-queues) | Article body: current multiple-queue limitation |
| E-APQD-02 | [Official help: Subscription tags](https://antennapod.org/documentation/subscriptions/subscription-groups) | Article body: tag entry and no tag-based queues |
| E-APQD-03 | [Official help: Refreshing podcasts](https://antennapod.org/documentation/automation/refreshing-podcasts) | Article body: global interval and per-feed Keep Updated |
| E-APQD-04 | [QueueFragment.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/queue/QueueFragment.java) | 88–137;230–365;419–490;527–728: queue menus, multi-select, ordering, drag write request |
| E-APQD-05 | [EpisodeMultiSelectActionHandler.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/episodeslist/EpisodeMultiSelectActionHandler.java) | 24–251: selected-item action dispatch and count feedback |
| E-APQD-06 | [ItemFilterDialog.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/feed/ItemFilterDialog.java) | 1–122: option callbacks, reset and dismiss |
| E-APQD-07 | [FeedItemFilterQuery.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/storage/database/src/main/java/de/danoeh/antennapod/storage/database/mapper/FeedItemFilterQuery.java) | 1–103: SQL filter predicate generation |
| E-APQD-08 | [DownloadLogAdapter.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/download/DownloadLogAdapter.java) | 28–153: success/error row, retry visibility and dispatch |
| E-APQD-09 | [DownloadLogDetailsDialog.java](https://github.com/AntennaPod/AntennaPod/blob/175e5ef8fe3e125a5014d70ef0c57cf85661c71b/app/src/main/java/de/danoeh/antennapod/ui/screen/download/DownloadLogDetailsDialog.java) | 35–168: detail fields, copying and conditional feed navigation |

E-APCD-13关联的SubscriptionFragment本次只复读128–230、246–300、335–510行；未重新获取，不为同一原件另建E。

## 第49批定点源码依据

E-APST-01～14分别覆盖单集选择、全局/单源筛选、标签编辑、本地删除警告、偏好对象、DB读写和确认分派；[短摘记](../artifacts/antennapod-state-controls/source-notes.md)及[逐文件来源/定位/散列](../artifacts/antennapod-state-controls/source-audit.json)可直接回查。七组旧E仅追加复读范围，不把原件重复算独立来源。新增RP-M040-01～08按取消、保存、批量、空结果和文件范围检索。代码已读不等于产品已运行，14任务仍局部覆盖。
