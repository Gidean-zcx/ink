# M-039 Audible：听书、片段标记、进度找回与离线内容

## 范围

查阅：2026-10-26。以 **美国市场 Audible Android/iOS app** 的英文官方帮助为主，构建号未知；网页 Cloud Player 单独注明，不套用 app 独有功能。Help 页面有前端 JavaScript 错误提示，但下方文章正文可以读取；这不是 app 实际报错。文章日期主要为 2026-09-03/09、10-01/13。需要 Amazon 账号；免费/已购内容与会员包含内容的可访问资格分别处理，不把订阅权限当作本地文件所有权。

**D2 文档重建，无实机/连续视觉观察。**覆盖书库、下载、章节、速度、片段/书签、队列、计时、进度同步、误定位恢复、移除与播放故障；车载、外接音频路由、Apple Watch、Kids Profile 权限和外部音频导入未展开。完整 app 功能目录未核定。

## 任务流程

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M039-01 | 找到并继续一本书 | 登录正确账号/市场，书已在 Library | Library 可按未开始/进行中/下载/完成筛选；选标题播放。已开始内容可在 mini player 或锁屏点 Play。 | 书库成员、下载状态、听读状态不同；“进行中”不证明已经离线可用。 | E-AUD-01；E-AUD-09 | 筛选保持、空库引导、两个设备同时播放的冲突顺序。 |
| F-M039-02 | 下载后无网收听 | 稳定网络、足够空间、具访问资格 | Library → All Titles → 标题三点或长按 → Download；播客先进入 Podcasts 的具体单集。完成后可离线听。当前帮助注明 app 逐本下载，没有批量下载。 | 下载到本设备；不推定缓存可移给别的播放器。 | E-AUD-02 | 进度/取消/断点续传、离线授权检查周期、下载不全时可播放范围。 |
| F-M039-03 | 按章跳转和调速度 | 当前标题已打开 | 播放器 Chapters → 选章节/单集；Speed 改本书速度。Profile → Settings → Player → Narration Speed → Default 改新听内容的默认值。 | 单书速度跨 app 保存；默认速度可按设备不同。音频章节可能与纸书/电子书不一致。 | E-AUD-05；E-AUD-08 | 跳转前位置撤销、章节改版映射、调速对计时显示的影响。 |
| F-M039-04 | 标记值得重听的一段并加笔记 | App 中有声书播放页 | +Clip 先保存 30 秒；“Clip Saved”横幅 Edit，拖起止手柄，输入笔记 Save。也可 Clips & Notes → 三点 → Edit Clip。 | 片段限定 5–44 秒且同时产生书签；不是音频导出。列表可只播 Clip 或 Start in Player 从此继续；删除需再次确认。 | E-AUD-03 | 默认 30 秒取当前点前后何种范围、离线编辑同步、笔记冲突/导出。 |
| F-M039-05 | 播客书签与回到原文 | App 播客单集或 Cloud Player | App 单集 +Bookmark，提示横幅 Add note 后保存；Cloud Player 可加书签。Go to Bookmark 返回该时间点。 | 片段与书签有不同入口；网页不能创建/管理音频片段。 | E-AUD-03 | 删除片段与关联书签是否同删、跨端时间点精度、取消编辑的保留范围。 |
| F-M039-06 | 排好下一本并控制自动延续 | App Library 中有候选标题 | 三点/长按 → Play next；当前播放页 Chapters → Up next → 单项菜单可上下/置顶底或 Remove from queue。Profile → Player → Continuous listening 的 Up next titles 控制自动接播。 | 用户队列和 Recommended titles 预览开关不同；预览只在没有排队下一本时出现。 | E-AUD-06 | 队列重启/跨端保存、重复加入、末本离线失败时是否跳过。 |
| F-M039-07 | 睡眠计时与延期 | App 正在播放 | Timer 选预设/自定义；运行中可点计时图标或封面倒计时改新时长，覆盖旧计时。结束有提醒；若开启 Shake to extend，可在声音渐弱起 30 秒内摇动延长相同时间。 | 到时停止音频；摇动开关在 Player → Timer。iOS 17+ Live Activity 可显示剩余时间，取决于权限。 | E-AUD-04 | 换书/暂停/系统杀后台后计时规则、误摇动、闹钟/通话优先级。 |
| F-M039-08 | 找回误拖/睡着后的位置 | 该书已有收听活动 | 播放页封面下 Listen Log → 选历史段回放；该日志记录书内会话起止，与 Profile → Activity & Stats → Listening History 的标题历史不同。 | 日志可用于找回有意收听起点；官方不提供日志导出，亦可清历史。 | E-AUD-07 | 日志保留范围、跨设备汇总、清除确认/恢复；日志不是理解程度证明。 |
| F-M039-09 | 换设备续听与纠正同步 | 同账号、多设备联网 | app 自动同步听到的位置；Profile → Settings → Player → Sync listening position 管理。不同步时官方建议关闭再开启。 | 同步的是进度，不等于下载文件/整个设置镜像。 | E-AUD-01 | 离线两端各听后谁胜出、回退是否需确认、账号切换如何处理本机进度。 |
| F-M039-10 | 清空间、移出书库与永久删除 | 明确要操作的对象 | Library 三点 → Remove download 清本机音频；Select 可批量。Remove from Library 是另一动作。永久删除需 Amazon 的 Manage Your Content and Devices → Content → Delete 并明确确认。 | 清下载不移出书库，可重下；永久删除后所有设备都不能从书库访问。 | E-AUD-09 | 片段/笔记在移除后的保留、删除撤销、会员退订/目录轮换对离线文件影响。 |
| F-M039-11 | 缺章、播放停顿或找不到书 | 可区分单本问题/全库问题 | 缺章或缺内容可 Remove download 后重下，官方称保留原收听位置；改下载质量后需重下。锁屏/后台不播可检查系统电池限制。书库缺书先下拉刷新或 Settings → Library → Refresh Library，再核对市场、过滤和归档。 | 更新/刷新/重下载/换市场各作用不同；会员包含目录可能轮换。 | E-AUD-10；E-AUD-11 | 来电结束后实际自动恢复、具体系统版本、下载失败重试和错误码；没有实测。 |

## 分支与尚未核实

“播放页有章节”不证明能与同名电子书直接对齐。帮助明确有声书可能拆分长章、组合短章或用分段代替原作章节；本项目读听切换需要内容标识和定位映射，不能只用第几章。[E-AUD-05]

片段、书签、当前进度和 Listen Log 分别回答“值得再听哪里”“定位点在哪”“现在听到哪”“最近从哪开始”。F-M039-04/05 的文字共同来源于一份帮助，属于同一证据族，不计作两次独立验证。没有借播放日志推测用户真正听懂、醒着或专注。

网页 Cloud Player 不支持片段管理、app timer；它可有章节/书签/速度。手机锁屏有恢复入口仍不足以证明任意耳机按键、蓝牙断连或来电后都自动续播。该类中断是后续实测目标。不同国家市场、不同会员资格、Kids Profile 与成人账号不混用；本案例未替用户登录/购买。

## 对本项目的分析（推断）

对 G-001，区分“我主动排的下一本”和“系统推荐试听”有助于研究结束感与自动消费；这不是要求本项目采用推荐流。对 R-018/G-002，AI 应能撤回一次误定位并说明恢复依据，也应读到计时器、书签、下载、授权和进度同步各自状态。若说“记下刚才那段”，最好产生可返回的内容锚点及笔记，而非只在聊天里总结。

对 5–6 岁自主听书，家长选好有限队列、清楚的暂停和到时停可以作为候选；适龄、资格和可用内容仍需单独研究。Plus 上实现片段/日志概念有条件可行，但 DRM 内容、Audible app 本身和 Android 后台机制不能直接移植。

最小后续观察：固定一台 Android 与一台 iOS、同一免费/已购标题，记录“下载完成→飞行模式→片段编辑→误拖→日志找回→联网同步”；另以电话/闹钟、耳机拔插、强退逐个打断并核对时间点，不合并多个变量。

## 证据

均于 2026-10-26 读取官方美国英文帮助正文；页面前端错误未阻断文章读取。发布日期见定位。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-AUD-01 | [Listen in the app](https://help.audible.com/s/article/listen-in-the-app) | 2026-10-13；Library、mini player、锁屏、进度同步。 |
| E-AUD-02 | [Download titles](https://help.audible.com/s/article/download-titles) | 2026-09-03；app 与播客下载、完成后离线、无批量下载。 |
| E-AUD-03 | [Manage clips & notes](https://help.audible.com/s/article/manage-clips-bookmarks) | 2026-09-09；片段/书签、笔记、删除与返回。 |
| E-AUD-04 | [Use timer](https://help.audible.com/s/article/use-timer) | 2026-09-03；计时覆盖、摇动延期、iOS Live Activity。 |
| E-AUD-05 | [Browse chapters and episodes](https://help.audible.com/s/article/browse-chapters-and-episodes) | 2026-09-03；章节入口与编号不一致原因。 |
| E-AUD-06 | [Manage auto-play settings](https://help.audible.com/s/article/manage-your-auto-play-settings?language=en_US) | 2026-09-03；Up next 排序与推荐预览条件。 |
| E-AUD-07 | [View listening log](https://help.audible.com/s/article/view-listening-log?language=en_US) | 2026-09-03；Listen Log、Listening History、不可导出。 |
| E-AUD-08 | [Set narration speed](https://help.audible.com/s/article/set-narration-speed?language=en_US) | 2026-09-03；单书速度与每设备默认值。 |
| E-AUD-09 | [Manage your Library](https://help.audible.com/s/article/manage-your-library) | 2026-09-09；筛选、移出、清下载、永久删除分层。 |
| E-AUD-10 | [Audio quality issues](https://help.audible.com/s/article/title-won-t-play-has-poor-audio-quality) | 2026-09-09；重下载保进度、质量、系统电池限制。 |
| E-AUD-11 | [Missing title issues](https://help.audible.com/s/article/title-is-missing?language=en_US) | 2026-10-01；刷新、marketplace、归档与目录轮换。 |
