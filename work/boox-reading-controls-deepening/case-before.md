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

本轮逐条登记公开 **User Guide (ePaper Tablet PCs)** 的 **14 节、59 篇**，见[目录 CSV](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/functional-inventory.csv)。API 分页已结束，条目数与各节一致；“59 篇”是公开帮助文章数，不是本机完整功能数或已验证任务数。Notes／阅读数据有同名导向文章，保留文章 ID，不人为合并计数。[E-BXCD-01]

目录完整性仅限这个公开分类；**V4.1.2 专用整册仍未取得**。官方仍要求 Settings → User Manual 或登录对应 send2boox 服务器按机型下载，未登录；搜索找到的商品聚合“手册”和 FCC 单页不当操作手册。[E-BOOX-19；E-BXCD-02] 相机扫描文章只列其他具体型号；InkSpire、Work Profile 的本机适用未核；Talk Insights 等后续能力不倒填到固定基线。下表补公共目录未逐项研究的范围。

| 公开目录节 | 篇数 | 当前覆盖边界 |
| --- | --- | --- |
| General Settings | 11 | 账户／更新已查；语言输入、连接、睡眠、显示、前灯、音量、鼠标、投屏等仅部分条件或目录 |
| Interface and Navigation | 2 | F02；桌面布局、Dock／小组件未完整研究 |
| Optimize Speed and Display | 3 | F03；对比度／颜色完整参数未研究 |
| Apps | 8 | 优化／管理条件见 F03、14；安装卸载仅入口 D1；浏览器、PushRead、Calendar Memo、Work Profile 未形成完整任务 |
| File Transfer | 3 | F04 及原第三方云分支；手机传输、异常重试未完整 |
| Split Screen | 2 | F08；帮助型号例外单列 |
| Reading | 16 | F05–08、11–12、14 的局部；版式、翻页区、状态栏、PDF/EPUB排版、浮动栏、Dashboard等仍有目录级缺口 |
| Note-Taking | 4 | F09–10、13；Text Notes、Tag、全部新工具未完整研究 |
| Stylus | 2 | 校准／InkSpire仅目录及适用性核查缺口 |
| Data Syncing and Backup | 2 | F11–13，仍缺冲突／覆盖恢复实测 |
| Keyboard Cover | 2 | 配件连接及快捷键未研究，不能从平板功能推配件条件 |
| Camera (Smart Scanner) | 1 | 其他型号专属文章，不合并到 Note Air5 C |
| Screenshot and Screen Recording | 2 | 未实际录屏；本轮只读静态图 |
| Lab Features | 1 | 后续版本入口，不纳入 V4.1.2 操作结论 |

## 任务流程

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M004-01 | 建立云端身份 | 联网；可接收验证码 | Settings → ONYX Account → Servers；选服务器、邮箱/手机号、验证码后 Login。失败检查号码格式、系统时间/时区和网络。 | 登录同时可注册；跨设备必须保持账号与服务器一致。 | E-BOOX-04；E-BOOX-18 | 首次向导可跳过哪些页、离线设置；不能把 Google 账户等同 ONYX。 |
| F-M004-02 | 从阅读进入全局控制再返回 | 当前 app 开启 | 系统栏右侧打开控制中心、左侧通知中心；Edit 管理快捷项。Settings → System Navigation 配置 Navigation Bar／Bottom Navigation／Side Gestures；隐藏导航栏先底部上滑调出。导航球双击回桌面。 | 控制/通知中心分侧；快捷项可Edit补入；侧边内滑需启用；导航栏隐藏先上滑调出；双击导航球回桌面不等于保存或终止进程。 | E-BOOX-05；E-BXCD-03 | 未取得V4.1.2机型手册；面板关闭动作及选区/阅读页保留、手势与app冲突未观察。 |
| F-M004-03 | 按任务调整显示 | app 或文档已开 | 控制中心 → EinkWise；NeoReader 中央菜单 → Refresh。第三方 app 内 E Ink Center → App Optimization，或桌面长按 app → Optimize；按问题调整 DPI／全刷／方向，支持恢复原 DPI、关闭 Master Switch 或 Reset All。 | 单次全刷/App显示配置/当前文档/全部文档不同；原DPI、关闭Master Switch、单App Reset All及全App重置不同；NeoReader文档帮助Regal/Speed不与通用HD/Speed/Smooth混表。 | E-BOOX-06；E-BOOX-07；E-BOOX-08；E-BXCD-04；E-BXCD-05；E-BXCD-06；E-BXCD-07；E-BXCD-12 | 本机4.1.2完整模式/默认参数/生效及退出反馈，重置关联、卸载后优化数据保留、真实残影/耗电未知。 |
| F-M004-04 | 将电脑资料送入设备 | 电脑和设备同 LAN | 打开 BOOXDrop → 电脑输入设备显示的网址 → 上传；在设备 Receive 找文件。远程 send2boox 则登录同服务器，在 BOOXDrop Online transfer 发送，设备 Push 查看。 | LAN 双向传输与云推送是两条路径；收到文件还需选择 app 打开。 | E-BOOX-09 | 空间不足、取消、重复文件、局域网隔离与失败提示。 |
| F-M004-05 | 查词、加生词与翻译选段 | 可选文字；字典已装 | 长按词 → Word Menu 查释义/加 Vocabulary Builder；拖端点扩选段 → Translate。字典文件解压至 dicts 下独立文件夹，在 Dictionary → Preferred Dictionary 勾选；不识别可重启词典。 | 生词归入词典 app；文字翻译与本地词典查询是不同服务。 | E-BOOX-10；E-BOOX-11 | 弹窗关闭后选区、词典离线完整度、在线引擎配额；不推定所有第三方词典可弹出。 |
| F-M004-06 | 标记并找回文档证据 | 可选文字；本机有笔 | 长按选段标记，Contents → Annotations／Handwriting 管理导出。补充：Progress → Blank Page 可在 PDF 加空白页；可从 Preview 或书签旁入口删除新增页。 | 文本批注与手写分开管理；Progress的Blank Page会嵌入PDF，可在Preview或书签旁入口删除新增页。 | E-BOOX-12；E-BXCD-11 | PDF新增页删除确认/撤销/锚点变化、跨软件对象可编辑性、导出失败和存储不足未观察。 |
| F-M004-07 | 查阅旁页后回到原处 | 已打开文档 | 中央菜单 → Progress，用页码／滑条／章跳转；Prev View／Next View 逐次前后返回。Contents 可回目录／书签，书签图标有独立标记状态。 | Progress内Prev View/Next View逐次往返；书签是独立持久标记，不承诺视图历史跨关书或重启。 | E-BOOX-13；E-BOOX-14；E-BXCD-11 | 关书/系统回收后历史、EPUB重排定位与重复书签未知。 |
| F-M004-08 | 阅读时创建关联笔记 | 支持笔输入的本机；文档打开 | 中央菜单 → Split View → Handwritten/Text Notes 建关联笔记。Current Doc 另为同文档两窗。控制中心 Split Screen → Swap Sides／Exit；系统分屏开 Notes 则独立，返 Home 也是系统分屏退出途径。 | Current Doc两窗标记在刷新后同步；关联笔记与系统独立Notes不同；控制中心换边/退出，系统分屏也可回Home退出；Flexible帮助型号名单未列本机。 | E-BOOX-15；E-BXCD-09；E-BXCD-10 | 关联Notes的写入时刻不能由Current Doc推断；源文件移动/删除、退出后页锚、Flexible本机适用未知。 |
| F-M004-09 | 写笔记、改笔画、保留输出 | Notes 中的普通手写笔记 | Notes 书写／擦除／Undo/Redo。Exit 保存后回 Notepad View；More → Save 是另一个手动保存入口。Share and Export 输出 PNG/PDF/.note；More → Page- 删除当前页。 | Exit保存并返回Notepad View；More内Save手动保存，两入口不合并；Page-帮助称不能撤销。 | E-BOOX-16；E-BXCD-13 | Home/切App/系统回收是否持久保存、断电写入、.note往返、删整本恢复未知。 |
| F-M004-10 | 手写转文字并保留录音线索 | 有手写内容/麦克风权限 | Notes 的 Text Recognition 可识别后编辑/复制/分享/插入，语言包在 More languages 下载；More → Recorder 录音并加标记，More → Recordings 管理导出。 | 识别文本、墨迹与录音属于不同数据；V4.1 Infinite Notes 是另一种画布，可用 Pin 定位。 | E-BOOX-16；E-BOOX-02 | 是否每次原位替换、录音与笔画逐时对齐、来电/锁屏中断、识别离线条件。 |
| F-M004-11 | 跨设备同步阅读状态和原书 | 两端同账号/服务器、同文件路径 | Library Settings 开总 Sync；长按书 → Sync 开单书开关；书内上方 Sync 手动同步。V4.1 可在单书 Sync → Upload source file。 | 阅读进度/批注与源书上传分开；单书上限帮助写 ≤200 MB，发布稿写 <200 MB，边界值待测。 | E-BOOX-17；E-BOOX-02 | 两端同时编辑冲突、重命名、路径移动、原书与状态版本不一致。 |
| F-M004-12 | 无云迁移书库及关联数据 | 两端 V4.1+；可传文件 | Library → Library Settings → Backup and Restore → Local Backup and Restore，生成 .booxc；传到另一台后恢复。 | 备份包含本地书及阅读数据；不是整机 app/登录凭据备份。 | E-BOOX-17 | 增量/覆盖策略、恢复确认、加密、损坏包、存储不足。 |
| F-M004-13 | 同步笔记或找回“登出后消失”的笔记 | ONYX 账户与服务器已确定 | Notes → 菜单 → Sync Settings 开全局同步；长按笔记 → Sync 可设单本；点击列表/笔记内 Sync。登出后若笔记不可见，重新登录原账号查看。 | 官方说明登录前笔记会纳入账号，登出后不可见不等于文件已删除；服务器数据独立。 | E-BOOX-20 | 账号删除后回收、未上传笔记去向、其他账号是否留本地副本。 |
| F-M004-14 | 从 TTS/后台问题排查到安全更新 | 选书/相关音频 app 已开 | NeoReader → Progress → TTS；系统 TTS Output 配引擎。按问题分别检查 App Optimization → Others → Stay active，以及 Settings → Apps & Notifications 的权限／通知／App Startup／Freeze。系统更新仍先备份。 | TTS引擎、通知权限、App Startup、Stay active和Freeze是不同控制；冻结阻后台/通知；睡眠连接设置另列；安装/卸载只取得入口D1。 | E-BOOX-14；E-BOOX-21；E-BOOX-22；E-BOOX-23；E-BXCD-06；E-BXCD-07；E-BXCD-08；E-BXCD-14 | 各后台开关优先级、TTS锁屏/蓝牙/焦点中断续播、安装失败/卸载后文件账户和偏好保留/恢复未知。 |

## 本轮控制路径补强

本轮不新增 F，集中补原编号；下述路径属于**当前通用帮助层面的 D2**，不是 V4.1.2 实机确认。帮助里的设备例图、默认值与型号名单必须连同限制阅读。[E-BXCD-01–14]

**F02：区分面板、返回与保存。** 通知中心 Clear All 是通知列表动作，不是删笔记；侧边内滑返回默认未启用，需要先配置。导航球在控制中心缺失时，可 Edit 后拖入快捷项。文档内返回历史见 F07，退出 Notes 见 F09；双击导航球回 Home，资料没有说明是否同时结束进程或持久保存。关闭面板的确切触摸位置、打断选区后是否保留尚未观察。[E-BXCD-03]

**F03：把显示问题和撤回范围说清楚。** 当前 ePaper PC 帮助默认右下上滑进 EinkWise；另一 E Ink Tablet 分类的同名页写左下。均提醒可自定义，不将两者拼成 Note Air5 C 的统一默认。[E-BXCD-04] 通用刷新页列 HD／Speed／Smooth；NeoReader Refresh 帮助却列 Regal／Speed。记录两种上下文，不把名称硬对齐或声称本机同时具有所有模式。[E-BXCD-05、12]

第三方界面被裁切时，优化帮助建议降低 DPI；过小时提高，也能选原 DPI。**Master Switch 暂停整套优化；单 app Reset All 重置配置；Apps & Notifications → Reset all app optimization 涉及所有 app**，不是同一恢复范围。方向强制跟随系统仍错乱时，帮助要求关闭该选项；Cloud Config 不保证每个 app 都有配置。未取得各开关即时生效、重启或卸载后的保存规则。[E-BXCD-06、07] 对 NeoReader，Apply to All Documents 区分当前文档／全部文档；Full Refresh 的 Deep 选项、触发频率与一次手动 Full Refresh 也分开。静态图中的开关值只是图示状态。[E-BXCD-12]

**F08／09：回读原文与退出保存。** Current Doc 明确两窗口手写、标记、书签在页面刷新后同步；这句话不能外推到“独立 Notes 与原书每笔同步”。关联笔记、另一个文件及两个 app 分屏要分别建模。系统分屏横向为左右窗、纵向为上下窗；未兼容 app 不能强推成功。帮助的 Flexible Split Screen 型号名单只列 Note Air3 C／Note Air3／Tab Ultra C Pro，未列 Note Air5 C，因此该项本机适用仍未知，不能反向断言不支持。[E-BXCD-09、10]

Notes 的 Exit 保存并回笔记列表，More → Save 手动保存，Page- 不可撤销。旧表把“More → Save 或 Exit”并写，本轮分开入口。Home、切 app、系统回收与断电时的写入完成点仍没证据；不以“自动保存”概括所有退出方式。[E-BXCD-13]

**F14：后台问题不是一个开关。** 权限、通知、自启动、优化中的 Stay active、Freeze 与睡眠连接策略分别存在；冻结阻止后台操作／通知。没有文档给出这些开关冲突时的优先级，也不能由 Stay active 推断 TTS 在锁屏、失去音频焦点或蓝牙断开后自动续播。Sleep and Battery 帮助属于通用 Tab 系列，列睡眠连接延迟设置，但 Note Air5 C V4.1.2 的入口与联网时序未核。[E-BXCD-06、07、14]

安装／卸载与数据保留仅达到以下范围，**不新计一条完整 D2 流程**：[E-BXCD-07、08]

| 生命周期节点 | 可核查入口／声明 | 尚缺的状态与失败分支 |
| --- | --- | --- |
| 安装 | 官方列 Google Play 和浏览器／电脑传入 APK；产品页确认本机内置 Google Play | 来源许可、安装确认、被拒／签名或版本冲突、安装后首启；没有下载 APK 或登录 |
| 权限与通知 | Settings → Apps & Notifications 内分别管理 | 拒绝后应用提示、重新授予后的恢复、后台通知到达均未观察 |
| 文件默认打开 | 同页 Default app | 取消默认、仅一次／总是选择、原文件与关联数据如何跟随未说明 |
| 卸载 | 同页 Application 可管理或卸载 | 确认文案、书籍下载／账号／偏好／优化配置去留、重装恢复均未知 |
| 数据保护 | F12 的 .booxc 是书库及阅读数据；F13 是 Notes 同步 | 不能把两者称为所有第三方 app 的卸载恢复包 |

## 分支、错误与版本冲突

| 条件 | 已有依据/可走路径 | 仍需核实 |
| --- | --- | --- |
| 同账号却不同步 | 先核服务器、路径、全局与单书开关；不要直接重复导入制造更多对象。 | 文件身份是否还依赖内容校验、冲突优先级。 |
| 分屏笔记改名后仍随书出现 | 关联不因改名解除；要独立笔记，入口应改用系统分屏。 | 删除文档时是否连带笔记，原路径移动的效果。 |
| 更新后菜单/刷新模式不同 | 当前通用帮助含 V4.1+ 和新机型条件；具体设备 changelog 才能确认变更。 | 本机 V4.1.2 配置迁移及默认值；不能用星级表当性能测量。 |
| 第三方云文件 | Library 云图标绑定账户，点文件下载后生成以账号命名的书架；Storage 中 Copy to Cloud Storage 再粘贴上传。 | 文档复制不是自动双向合并，绑定多账号不等于共享同一身份。[E-BOOX-24] |
| 暂停、取消、删除、断网 | 笔画 Undo 不覆盖 Page-；服务器切换不是数据搬家；缺网不等于本地书删除。 | 空库、取消导入、满盘、断电写入、冲突和系统回收站均无实测。 |

## 实际观察与项目分析

只读官方正文及公开 API 原文，并实际查看四张共用帮助静态图：控制中心、App Optimization 的 Display 页、Apps & Notifications、NeoReader Refresh。**图中型号／固件不明，未识别为 Note Air5 C，不能证明 V4.1.2 的布局或默认值**。全程 hands_on=false，无连续视觉流程、D4 或耗时测量；菜单中的 AI/智能词也不能证明 G-002 全系统访问。

静态图与取证摘要见[本批资料](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/source-notes.md)。前三图是共用设备示意，第四图可见 Regal／Speed、Normal／Deep、频率与 Apply to All Documents。只记录控件存在，不推断点击效果。正式归档使用相对资料库路径；API 原件在 work，以 raw_work_path/raw_sha256 记录。

以下为推断：对 Plus，最值得复用的是 **内容身份、阅读状态、关联笔记、可恢复输出分开建模**，以及“旁路查阅 → 回原位置”的显式入口。大屏双窗不适合直接缩到小屏，可探索原文与解释/笔记间保留锚点的单页切换。对 5–6 岁，长按、拖选区、两层同步开关可能需要更简单的可见入口；对成人学习，原文引用和独立笔记之间的关系应可查询。R-018 仍需要专门验证听读断点，G-002 需要暴露文件来源、选区、同步开关与删除作用范围，而不是只提供 AI 聊天按钮。

最小后续观察：获取 Note Air5 C 的 V4.1.2 实际构建/机型手册；用同一 PDF/EPUB 完成选段→批注→关联笔记→改名/移文件→同步及恢复，再做 TTS 锁屏/切 app/蓝牙中断。每步记录对象 ID、页锚、文件及回退结果。另选一个布局裁切的第三方 app，比较原 DPI、Master Switch、单 app 重置与全 app 重置；用可丢弃数据观察卸载后内容及配置去留。当前只完成资料补强。

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

本轮 E-BXCD-01–14 为同一官方来源体系的目录／正文快照和静态取证，不因新编号算独立交叉验证；详细来源、定位与归档见新批 evidence JSONL 和上面的取证摘要。

| 新 E | 官方来源／定位 |
| --- | --- |
| E-BXCD-01 | [公开指南目录](https://booxsupport.zendesk.com/hc/en-us/categories/38115205868180-User-Guide-ePaper-Tablet-PCs)，14节59篇，按API逐条登记。 |
| E-BXCD-02 | [手册下载](https://booxsupport.zendesk.com/hc/en-us/articles/8569203926804-User-Manual-Download)，机内／登录网站两入口。 |
| E-BXCD-03 | [Navigate Your Device](https://booxsupport.zendesk.com/hc/en-us/articles/25213043150996-Navigate-Your-Device)，控制／通知中心、导航栏、手势及导航球。 |
| E-BXCD-04 | [EinkWise](https://booxsupport.zendesk.com/hc/en-us/articles/10701257505044-EinkWise)，本分类默认右下；另一分类[同名页](https://booxsupport.zendesk.com/hc/en-us/articles/10990010331156-EinkWise)写左下，只用已有公开快照比较。 |
| E-BXCD-05 | [Refresh Modes](https://booxsupport.zendesk.com/hc/en-us/articles/10701257029780-Refresh-Modes)，当前通用模式／全刷。 |
| E-BXCD-06 | [App Optimization](https://booxsupport.zendesk.com/hc/en-us/articles/10701299044372-App-Optimization)，DPI／Master Switch／Reset All／Others。 |
| E-BXCD-07 | [App Management](https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings)，权限／通知／启动／冻结／全app重置。 |
| E-BXCD-08 | [安装渠道](https://booxsupport.zendesk.com/hc/en-us/articles/10701308914964-Download-and-Install-Apps)，仅渠道声明。 |
| E-BXCD-09 | [系统分屏](https://booxsupport.zendesk.com/hc/en-us/articles/10701452686484-System-Split-Screen)，API正文；web读取一次Internal Error后未重试。 |
| E-BXCD-10 | [阅读器分屏](https://booxsupport.zendesk.com/hc/en-us/articles/10701493183764-Split-View)，Current Doc、关联Notes与退出。 |
| E-BXCD-11 | [Book Progress](https://booxsupport.zendesk.com/hc/en-us/articles/10701495414804-Book-Progress)，视图历史／PDF空白页。 |
| E-BXCD-12 | [NeoReader刷新](https://booxsupport.zendesk.com/hc/en-us/articles/10701492644372-Refresh-Settings)，当前／全部文档范围。 |
| E-BXCD-13 | [Handwritten Notes](https://booxsupport.zendesk.com/hc/en-us/articles/10701578837268-Handwritten-Notes)，Exit及More的Save/Page-，不整页倒填版本。 |
| E-BXCD-14 | [Sleep and Battery](https://booxsupport.zendesk.com/hc/en-us/articles/10701171762452-Sleep-and-Battery)，通用睡眠连接条件。 |
