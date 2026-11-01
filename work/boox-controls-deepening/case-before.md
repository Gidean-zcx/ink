# M-004 BOOX Note Air5 C：NeoReader、关联笔记与数据边界

## 范围与版本

查阅：2026-10-26。固定对象为 **Note Air5 C（Android 15），V4.1.2 功能基线**，预装 NeoReader 与 Notes；英文国际版资料，实际设备构建号未知。不是整个 Note 系列，也不包含 Note Air4 C、Palma 2 Pro。官方产品页确认本机 Android 15 与 Smooth 模式的 V4.1.2+ 条件；2025-10-20 官方机型发布文章明确本机使用 V4.1，2025-08-28 官方文章给出该版本增量。[E-BOOX-01～03]

**D2 文档重建，无实机、无连续视觉观察。** BOOX 当前公开帮助已更新，菜单可能比 V4.1 新；下表是该功能基线与截至查阅日帮助的交叉研究，不是某台 V4.1 机器逐页录制。Smooth 是本机 V4.1.2+ 的明确条件功能，不倒填至初始 V4.1；最新固件页已换成 V4.3，也不倒填其 Talk Insights、Dual Notes 等功能。旧 Note Air2 V3.2/3.3 手册及更新记录不作为本机操作依据。

## 功能目录与已查/待查

| 功能域 | 已查范围 | 尚未形成完整路径 |
| --- | --- | --- |
| 系统 | 账户/服务器、下拉双面板、手势/导航球、应用后台冻结、更新 | 首次向导逐屏、指纹/密码失败、休眠策略、第三方来电 |
| 显示 | EinkWise 入口、按 app 配置、NeoReader 刷新作用范围 | 本机 V4.1.2 完整模式名称/参数默认值、真实残影和耗时 |
| 阅读 | 选词/词库、段落批注、目录/书签/返回视图、TTS 入口、关联笔记 | OCR 配额与失败、扫描 PDF 精度、DRM、朗读定位与后台恢复 |
| Notes | 笔画编辑、页删除、录音入口、识别、导出、账号可见性 | 所有画笔/模板/图层参数、冲突合并、录音时间戳与手写对齐 |
| 数据 | BOOXDrop、本地备份、源文件与阅读数据同步、第三方云 | 断线重试、同名文件、移路径后的关联、回收站/版本历史 |

目录覆盖主要任务域，**未核定该构建的完整功能总数**。手册入口需本机 Settings → User Manual 或 send2boox 登录，本轮未登录取得机型专用最新手册。[E-BOOX-19]

## 任务流程

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M004-01 | 建立云端身份 | 联网；可接收验证码 | Settings → ONYX Account → Servers；选服务器、邮箱/手机号、验证码后 Login。失败检查号码格式、系统时间/时区和网络。 | 登录同时可注册；跨设备必须保持账号与服务器一致。 | E-BOOX-04；E-BOOX-18 | 首次向导可跳过哪些页、离线设置；不能把 Google 账户等同 ONYX。 |
| F-M004-02 | 从阅读进入全局控制再返回 | 当前 app 开启 | 右上区域下拉/点击打开控制中心，左上打开通知中心；前者 Edit 可改快捷项。导航球点开/收起、双击返桌面；Settings → System Navigation 设置底部/侧边手势。 | 系统面板与应用页面是不同上下文；隐藏导航栏时先底部上滑调出。 | E-BOOX-05 | 当前本机固件是否同路径；返回 app 后页码、选区及滚动位置是否保留。 |
| F-M004-03 | 按任务调整显示 | app 或文档已开 | 控制中心 → EinkWise → 选显示主题/刷新；必要时用 Full Refresh。NeoReader 点中心 → Refresh 可调全刷频率及 Apply to All Documents。 | app 配置、当前文档配置、全局范围分别处理；一次全刷不等于改变后续策略。 | E-BOOX-06；E-BOOX-07；E-BOOX-08 | 当前帮助模式已变；本机 V4.1.2 模式实值、主题覆盖关系、耗电/残影未测。 |
| F-M004-04 | 将电脑资料送入设备 | 电脑和设备同 LAN | 打开 BOOXDrop → 电脑输入设备显示的网址 → 上传；在设备 Receive 找文件。远程 send2boox 则登录同服务器，在 BOOXDrop Online transfer 发送，设备 Push 查看。 | LAN 双向传输与云推送是两条路径；收到文件还需选择 app 打开。 | E-BOOX-09 | 空间不足、取消、重复文件、局域网隔离与失败提示。 |
| F-M004-05 | 查词、加生词与翻译选段 | 可选文字；字典已装 | 长按词 → Word Menu 查释义/加 Vocabulary Builder；拖端点扩选段 → Translate。字典文件解压至 dicts 下独立文件夹，在 Dictionary → Preferred Dictionary 勾选；不识别可重启词典。 | 生词归入词典 app；文字翻译与本地词典查询是不同服务。 | E-BOOX-10；E-BOOX-11 | 弹窗关闭后选区、词典离线完整度、在线引擎配额；不推定所有第三方词典可弹出。 |
| F-M004-06 | 标记并找回文档证据 | 可选文字；本机有笔 | 长按选段 → 标注工具栏 → 高亮/下划线/批注；Contents → Annotations 管理/导出。手写标记在 Contents → Handwriting，可导出 PNG/PDF。 | 文本批注与手写墨迹分开管理；文档级导出设置在 Settings → Annotation Export Settings。 | E-BOOX-12 | 外部 PDF 软件如何识别注释、擦除/撤销、导出是否保留可编辑对象。 |
| F-M004-07 | 查阅旁页后回到原处 | 已打开文档 | 点中心 → Contents 跳目录/书签；右上书签图标空心变实心。Progress 可输入页码/跳章；Prev View、Next View 沿已看视图前后返回。 | 书签是持久标记；视图历史是暂时导航，不必靠记页码返回。 | E-BOOX-13；E-BOOX-14 | 重排 EPUB 后锚点、关书后历史、重复添加书签。 |
| F-M004-08 | 阅读时创建关联笔记 | 支持笔输入的本机；文档打开 | 中央菜单 → Split View → Handwritten Note；创建与文档同名的关联笔记。若走控制中心 Split Screen 再开 Notes，则是独立笔记；控制中心可 Swap Sides/Exit。 | 一文档只有一个关联笔记，改笔记名不解除关联；也能在 Notes 编辑。 | E-BOOX-15 | 移动/复制/删除源文件后关联；两窗口手写同步的实际时刻。 |
| F-M004-09 | 写笔记、改笔画、保留输出 | Notes 中的普通手写笔记 | 选笔书写，橡皮可按像素/整笔画/圈区作用；Undo/Redo 纠错；More → Save 或 Exit 保存；Share and Export 输出 PNG/PDF/.note。 | 普通导出与原笔记对象不同；More → Page- 删除当前页，帮助明确不可撤销。 | E-BOOX-16 | .note 再导入保真、删除整本恢复、退出前断电、不同擦除模式的实际反馈。 |
| F-M004-10 | 手写转文字并保留录音线索 | 有手写内容/麦克风权限 | Notes 的 Text Recognition 可识别后编辑/复制/分享/插入，语言包在 More languages 下载；More → Recorder 录音并加标记，More → Recordings 管理导出。 | 识别文本、墨迹与录音属于不同数据；V4.1 Infinite Notes 是另一种画布，可用 Pin 定位。 | E-BOOX-16；E-BOOX-02 | 是否每次原位替换、录音与笔画逐时对齐、来电/锁屏中断、识别离线条件。 |
| F-M004-11 | 跨设备同步阅读状态和原书 | 两端同账号/服务器、同文件路径 | Library Settings 开总 Sync；长按书 → Sync 开单书开关；书内上方 Sync 手动同步。V4.1 可在单书 Sync → Upload source file。 | 阅读进度/批注与源书上传分开；单书上限帮助写 ≤200 MB，发布稿写 <200 MB，边界值待测。 | E-BOOX-17；E-BOOX-02 | 两端同时编辑冲突、重命名、路径移动、原书与状态版本不一致。 |
| F-M004-12 | 无云迁移书库及关联数据 | 两端 V4.1+；可传文件 | Library → Library Settings → Backup and Restore → Local Backup and Restore，生成 .booxc；传到另一台后恢复。 | 备份包含本地书及阅读数据；不是整机 app/登录凭据备份。 | E-BOOX-17 | 增量/覆盖策略、恢复确认、加密、损坏包、存储不足。 |
| F-M004-13 | 同步笔记或找回“登出后消失”的笔记 | ONYX 账户与服务器已确定 | Notes → 菜单 → Sync Settings 开全局同步；长按笔记 → Sync 可设单本；点击列表/笔记内 Sync。登出后若笔记不可见，重新登录原账号查看。 | 官方说明登录前笔记会纳入账号，登出后不可见不等于文件已删除；服务器数据独立。 | E-BOOX-20 | 账号删除后回收、未上传笔记去向、其他账号是否留本地副本。 |
| F-M004-14 | 从 TTS/后台问题排查到安全更新 | 选书/相关音频 app 已开 | NeoReader → Progress → TTS；可在系统 TTS Output 配引擎/语言包。Apps & Notifications 的 Freeze 会阻止后台运行及通知；System Settings → System Update 检查更新，更新前备份。 | “支持 TTS”不能证明锁屏续播；恢复出厂设置默认清除存储，更新和恢复不是同一动作。 | E-BOOX-14；E-BOOX-21；E-BOOX-22；E-BOOX-23 | 焦点抢占、蓝牙断开、语音通话、自动恢复句子、首次引擎下载失败。 |

## 分支、错误与版本冲突

| 条件 | 已有依据/可走路径 | 仍需核实 |
| --- | --- | --- |
| 同账号却不同步 | 先核服务器、路径、全局与单书开关；不要直接重复导入制造更多对象。 | 文件身份是否还依赖内容校验、冲突优先级。 |
| 分屏笔记改名后仍随书出现 | 关联不因改名解除；要独立笔记，入口应改用系统分屏。 | 删除文档时是否连带笔记，原路径移动的效果。 |
| 更新后菜单/刷新模式不同 | 当前通用帮助含 V4.1+ 和新机型条件；具体设备 changelog 才能确认变更。 | 本机 V4.1.2 配置迁移及默认值；不能用星级表当性能测量。 |
| 第三方云文件 | Library 云图标绑定账户，点文件下载后生成以账号命名的书架；Storage 中 Copy to Cloud Storage 再粘贴上传。 | 文档复制不是自动双向合并，绑定多账号不等于共享同一身份。[E-BOOX-24] |
| 暂停、取消、删除、断网 | 笔画 Undo 不覆盖 Page-；服务器切换不是数据搬家；缺网不等于本地书删除。 | 空库、取消导入、满盘、断电写入、冲突和系统回收站均无实测。 |

## 实际观察与项目分析

只读官方正文及 Zendesk 公共 API 原文；没有操作设备、测耗时、观看连续视频或把产品图算成操作记录。菜单中的 AI/智能词不能证明 G-002 的全系统数据和动作可访问。

以下为推断：对 Plus，最值得复用的是 **内容身份、阅读状态、关联笔记、可恢复输出分开建模**，以及“旁路查阅 → 回原位置”的显式入口。大屏双窗不适合直接缩到小屏，可探索原文与解释/笔记间保留锚点的单页切换。对 5–6 岁，长按、拖选区、两层同步开关可能需要更简单的可见入口；对成人学习，原文引用和独立笔记之间的关系应可查询。R-018 仍需要专门验证听读断点，G-002 需要暴露文件来源、选区、同步开关与删除作用范围，而不是只提供 AI 聊天按钮。

最小后续观察：获取 Note Air5 C 的 V4.1.2 实际构建/机型手册；用同一 PDF/EPUB 完成选段→批注→关联笔记→改名/移文件→同步及恢复，再做 TTS 锁屏/切 app/蓝牙中断。每步记录对象 ID、页锚、文件及回退结果。

## 证据

下列均查于 2026-10-26。01/02/03 为产品/发布资料；其他为官方帮助正文，部分通过官方 Zendesk 公共 API 获取；帮助更新时间已保留在批次 JSONL。03 是官方机型发布文章；本例仍未核验实体设备的固件包。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-BOOX-01 | [Note Air5 C](https://shop.boox.com/products/noteair5c) | 机型、Android 15、笔与平板式界面。 |
| E-BOOX-02 | [V4.1 官方增量](https://shop.boox.com/blogs/news/boox-firmware-v4-1-overview-of-new-features-and-upgrades) | 2025-08-28；EinkWise、源文件/本地备份、Infinite Notes。 |
| E-BOOX-03 | [Note Air5 C 官方发布](https://shop.boox.com/blogs/news/introducing-note-air5-c-the-10-3-color-epaper-notebook-for-light-productivity) | 2025-10-20；本机 Android 15、V4.1、Notes 与分屏；V4.1.2 Smooth 条件另见01。 |
| E-BOOX-04 | [ONYX Account](https://booxsupport.zendesk.com/hc/en-us/articles/10701174893972-ONYX-Account-Sign-up-and-Deletion) | 注册、服务器、账号删除。 |
| E-BOOX-05 | [Navigate Your Device](https://booxsupport.zendesk.com/hc/en-us/articles/25213043150996-Navigate-Your-Device) | 控制/通知中心、导航栏/手势/导航球。 |
| E-BOOX-06 | [EinkWise](https://booxsupport.zendesk.com/hc/en-us/articles/10701257505044-EinkWise) | 三类入口、机型差异。 |
| E-BOOX-07 | [Refresh Modes](https://booxsupport.zendesk.com/hc/en-us/articles/10701257029780-Refresh-Modes) | 按 app 模式；Smooth 的机型条件。 |
| E-BOOX-08 | [NeoReader Refresh Settings](https://booxsupport.zendesk.com/hc/en-us/articles/8569340209684-Refresh-Settings) | Refresh 标签、当前文档/全部文档。 |
| E-BOOX-09 | [Transfer with Your Computer](https://booxsupport.zendesk.com/hc/en-us/articles/10701399613076-Transfer-with-Your-Computer) | LAN Receive 与在线 Push。 |
| E-BOOX-10 | [Translation and Dictionary](https://booxsupport.zendesk.com/hc/en-us/articles/8569333726740-Translation-and-Dictionary) | Word Menu、翻译、Vocabulary Builder。 |
| E-BOOX-11 | [安装字典](https://booxsupport.zendesk.com/hc/en-us/articles/8339387602708-How-could-I-install-a-dictionary-on-my-BOOX-device) | dicts、Preferred Dictionary、未检出恢复。 |
| E-BOOX-12 | [Take Notes on Books](https://booxsupport.zendesk.com/hc/en-us/articles/8569296110100-Take-Notes-on-Books) | 批注/手写两类导出。 |
| E-BOOX-13 | [TOC & Bookmarks](https://booxsupport.zendesk.com/hc/en-us/articles/10701496119060-TOC-Bookmarks) | 目录、书签图标、批注索引。 |
| E-BOOX-14 | [Book Progress](https://booxsupport.zendesk.com/hc/en-us/articles/10701495414804-Book-Progress) | Prev/Next View、跳页/章、TTS。 |
| E-BOOX-15 | [Split View](https://booxsupport.zendesk.com/hc/en-us/articles/8569296893844-Split-View) | 关联笔记、改名、换边/退出、机型限制。 |
| E-BOOX-16 | [Handwritten Notes](https://booxsupport.zendesk.com/hc/en-us/articles/10701578837268-Handwritten-Notes) | Eraser、Exit、Text Recognition、Recorder、More、Infinite Notes；其余新功能不倒填4.1。 |
| E-BOOX-17 | [Reading Data Syncing and Backup](https://booxsupport.zendesk.com/hc/en-us/articles/25401838386196-Reading-Data-Syncing-and-Backup) | 账号/路径、开关、200MB、.booxc。 |
| E-BOOX-18 | [登录验证码失败](https://booxsupport.zendesk.com/hc/en-us/articles/34964921849492-Troubleshooting-Login-Issues-with-Onyx-Account) | 格式、时间和网络。 |
| E-BOOX-19 | [User Manual Download](https://booxsupport.zendesk.com/hc/en-us/articles/8569203926804-User-Manual-Download) | 本机或登录网页取得专用手册。 |
| E-BOOX-20 | [Note Syncing](https://booxsupport.zendesk.com/hc/en-us/articles/25401889318548-Note-Syncing) | 同步开关与登出可见性。 |
| E-BOOX-21 | [TTS 引擎配置](https://booxsupport.zendesk.com/hc/en-us/articles/360031058312-How-could-I-download-new-TTS-engine-on-BOOX-device-and-use-it-as-the-default-TTS-on-our-device) | 引擎、TTS Output、语言包。 |
| E-BOOX-22 | [App Management Settings](https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings) | 自启动、冻结、权限、通知、app显示设置。 |
| E-BOOX-23 | [Firmware Updates](https://booxsupport.zendesk.com/hc/en-us/articles/10701135807124-Firmware-Updates) | 分批更新、备份、恢复出厂的删除范围。 |
| E-BOOX-24 | [Integrated Third-Party Cloud Storage](https://booxsupport.zendesk.com/hc/en-us/articles/10701398359572-Integrated-Third-Party-Cloud-Storage) | 绑定、下载入书架、显式上传。 |
