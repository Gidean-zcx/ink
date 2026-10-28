# M-040 AntennaPod：订阅收件箱、待听队列、下载与可恢复进度

## 范围与方法

查阅：2026-10-26。对象为 **Android AntennaPod**。官方文档自称描述当前功能，但未给每页适用 app 构建；不把它当作固定发行版。为补章节、计时和外设变化，额外静态阅读官方仓库 `develop` 快照 **d3e41c6cb37c8e1a060b142686f818999de89859** 的具体文件。快照存在新旧播放服务分支，代码路径与已发布版本可见行为不能等同。没有构建/安装/运行 app，也没有私有账号、实机和连续视觉观察。

深度是 **D2 文档重建 + 局部源码核查**。已查订阅、自动下载、Inbox/Queue/Downloads 的关系、排序与锁定、清理、续播回退、章节/计时条件、错误日志、锁屏恢复、备份/同步。功能目录仍不完整：逐集时间点标记/笔记、全文稿、Cast 和车载仅有线索，未完成任务流程；不能据未查判定不支持。

## 功能与流程

| F 编号 | 任务 | 前置状态 | 操作与资料中的反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M040-01 | 新增播客来源 | 有网络、可访问的订阅源 | 底部溢出菜单 → + Add Podcast，可搜索/发现或填 RSS；也可导入 OPML。官方支持 RSS/Atom 等入口，网页频道 URL 不等于可播放媒体源。 | 新增订阅是获得目录/更新来源，并非自动取得所有历史音频。 | E-ANTP-01 | 搜索无结果、重复源合并、私有 feed 认证、取消订阅时下载处理。 |
| F-M040-02 | 把想听内容放进队列 | 发现具体单集 | 长按单集 → Add to queue；文档称下载也会自动入队。队列用于计划待听，查看全部已下载应进 Downloads。 | 手动排队、已下载、已播放是不同状态。 | E-ANTP-02；E-ANTP-06 | 默认值/可配置差异、重复添加、临时播非队列单集、退出后队列。 |
| F-M040-03 | 混排多个节目并保留先后关系 | 队列含多个订阅 | 顶部 … → Sort → Smart shuffle，可升降序；若想按节目聚合且各自按日期，先 Date 再 Podcast title。不能拖时检查 Keep Sorted 与 Lock queue。 | Smart shuffle 尽量分散单一订阅，不保证严格轮流；排序锁与拖动锁是两个原因。 | E-ANTP-03；E-ANTP-04 | 当前播放项如何固定、排序撤销、海量不均衡内容的实际可读性。 |
| F-M040-04 | 自动准备下一批离线节目 | 已订阅；下载资格满足 | Settings → Downloads → Automatic download，或单播客齿轮 → Include in auto downloads。刷新发现的新集先进入 Inbox；下载后入 Queue 并从 Inbox 移出。 | 下载受 Wi-Fi/移动设置、数量上限、单播客筛选影响；不是“订阅后全部下载”。 | E-ANTP-05 | 半文件、失败重试、断网恢复、下载顺序；通知和进度未观察。 |
| F-M040-05 | 听完清空间或手动清下载 | 下载文件已存在 | Queue/播客列表长按 → Delete，Downloads 可点垃圾桶；Delete removes from Queue 决定是否连同出队。Automatic deletion 可在实际播完或下一批自动下载需空间时清理。 | 手动标记 played 不触发“播完自动删”；空间清理有收藏/在队列等资格条件，必须读取当前配置。 | E-ANTP-06；E-ANTP-07 | 删除确认/撤销、正在播的文件、存储损坏、清理候选预览。 |
| F-M040-06 | 暂停后恢复并补听上下文 | 先前有播放位置 | 文档说明重新播放会自动回退：暂停超过 1 分钟/1 小时/1 天分别回退 3/10/20 秒，不能自配。 | 恢复位置不是逐毫秒原点，而是按暂停间隔回看少量内容。 | E-ANTP-08 | 阈值正好相等、频繁暂停、强退/断电、越过章节边界的实效。 |
| F-M040-07 | 查章并跳到起点（源码范围） | 媒体有章节数据；已进入章节对话框 | ChaptersFragment 加载时显示进度，点章调用播放并 seekTo 起点、更新选中项；空章节集合关闭对话框并显示无章节提示。可 Close；外部章节 URL 存在时有 Refresh。 | 当前章与播放位置变化，章节来自内容而非 app 凭空生成。 | E-ANTP-14 | 上层入口及其显示条件、错误加载文案、当前发行版分支；本行不是运行观察。 |
| F-M040-08 | 设睡眠结束条件（源码范围） | 播放器媒体可用；具体服务分支匹配 | 播放页菜单打开 SleepTimerDialog；选择分钟或集数、输入、设定/停用，可延期。无媒体或输入 0/解析失败有提示路径。未启用连续播放时，集数模式禁用相应输入并提供播放设置入口。 | 计时类型、上次输入、自动启用时段分别保存；范围超过队列/剩余时长有提示逻辑。 | E-ANTP-15；E-ANTP-16 | 发行版本、后台持久化、Shake 重设与实际暂停动作、系统杀进程后的结果。 |
| F-M040-09 | 顺序播下一个及外设变化（源码范围） | 有下一队列项；当前播放服务路径成立 | PlaybackService 根据连续播放和计时状态决定接播。下一集无本地文件且不许流播时有通知并停止路径；音频输出变为 noisy 时按配置暂停，重连恢复受原先暂停原因/外设类型/系统版本约束。 | 用户主动暂停不能简单等同外设中断。新旧服务并存，未验证默认构建，不能承诺所有 Android 上自动恢复。 | E-ANTP-17 | Media3 路径完整行为、来电音频焦点、耳机/蓝牙实机与锁屏。 |
| F-M040-10 | 定位下载/刷新错误并恢复 | 收到下载有错误通知 | 点通知进 Download Log → 具体失败项 → 长按 URL 在浏览器打开。只少量源失败先查发布方；网络连接失败与 RSS 解析错误分别处理，可复制错误提供排查。 | 可以追溯失败对象和源 URL；上游删除媒体后，AntennaPod 不能替用户重新取得。 | E-ANTP-09；E-ANTP-18 | 自动重试退避、失败队列是否阻塞后续、取消重试；未人为制造错误。 |
| F-M040-11 | 锁屏/后台停止后找回控制 | Android 系统限制可能存在 | 官方记录 Android 13/14 暂停较久后锁屏/通知可能无法恢复，建议下拉快捷设置，编辑并加 AntennaPod tile。锁屏后突然停止另需检查该机电池优化例外。 | 控件失效和播放服务被系统结束是不同问题；官方对前者根因尚不确定。 | E-ANTP-10；E-ANTP-11 | 发生率、受影响厂商和最新系统；本项目没有复现，不能写普遍缺陷。 |
| F-M040-12 | 换机/多端保存订阅和进度 | 明确要完整迁移还是同步部分数据 | 同版本数据库导出/导入后仍需重下音频；OPML 经 Settings → Import/Export 导出，导入时勾订阅加入现有库。同步在 Settings → Synchronization 选服务、授权并连设备。 | 数据库、OPML、gPodder/Nextcloud 同步三种覆盖不同；同步不包含收藏/队列/设置，OPML 主要只有订阅。 | E-ANTP-12；E-ANTP-13 | 双端离线冲突、数据库覆盖确认、版本迁移、失败回滚和同步敏感 URL。 |

## 可追踪的数据状态和异常

`Inbox → 下载 → Queue` 是自动下载文档给出的业务流，不是界面标签同义词。如果把新集动作改成不入 Inbox，自动下载就没有合格对象；达到数量上限应查 Downloads 的数量，单看 Queue 可能误判。文档默认上限为 50 集，但案例没有验证安装默认配置。[E-ANTP-05]

移出队列、删除下载、标记已播、取消订阅应该分别研究。现有证据足够证明前三者存在独立条件，却不足以补全取消订阅的级联删除语义。自动清理既看规则又看候选状态；用“清掉听完的”做 AI 指令时必须明确是否保留收藏、下载和待听成员。[E-ANTP-06、E-ANTP-07]

备份文档建议 OPML 存到 app 目录之外以免卸载一起删除。OPML 导入是勾选后加入现有订阅；不能把它称作整机状态恢复。同步文档还区分 gPodder 网页预建/关联设备与在 app 创建设备：后一种需要 Force sync 才上传过去已播状态。公开 gpodder.net 可能过载，不能把网络失败直接解释成用户进度丢失。[E-ANTP-12、E-ANTP-13]

代码核查保留了一条具体反例：旧 PlaybackService 的耳机重连逻辑在 Android API 31+ 有停止服务分支，且新 Media3 服务另在别处。这说明只看一个 `resume` 函数不能宣布“蓝牙重连可靠续播”。F-M040-09 是用于指导下一轮观察的路径分析，不代表稳定版全覆盖。[E-ANTP-17]

## 对本项目的分析（推断）

对个人新闻/学习 G-001/R-020，AntennaPod 的“先收件、再选择、有限待听”比单纯无限追加更值得研究。Smart shuffle 解决的是来源堆积而非内容价值筛选，不能用混排代替高质量判断。对 G-002/R-018，AI 需同时读下载资格、文件可用性、队列位置、自动清理条件、断点和同步范围，才能完成“明天路上继续上次节目”。

5–6 岁儿童未必应直接使用 RSS 管理和多个集合，但家长后台可以先准备，孩子前台只看到明确可离线的内容。Plus 上复用这些状态思想有条件可行；此 Android app 的运行环境、后台策略、Cast 和解码性能并未在 Plus 验证。自动回退可作为恢复上下文候选，不自动成为需求。

下一项最小验证：固定发行版与 Android 版本，用两个可控 RSS 源、少量长短音频及一个失效 URL，记录下载/入队/排序/清理、离线接播和三种备份。章节与计时需单独对照发布版本；源代码快照须能映射到实际构建才提高结论强度。

## 证据

均于 2026-10-26 获取。官方网页未给逐页版本/日期；源码经 GitHub API 固定提交后读取 raw 文件，未运行。

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
