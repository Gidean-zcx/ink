# M-004 BOOX Note Air5 C：NeoReader、关联笔记与数据边界

## 结构化竞分（2026-10-28）

- 对象/产物边界：
  - 源文件/书库条目、阅读数据（进度/书签/批注）、手写层与“关联笔记”是不同对象；本地备份（.booxc）与云同步亦为不同路径（F‑M004‑06/08/11/12/13）。
  - 导出与上传分离：书内批注/手写的导出与“单书上传源文件（≤200MB说明口径）”分别控制（F‑M004‑06/11）。
- 删除/撤销与恢复：
  - Notes 中 Page‑ 删除不可撤销；图层显隐与清空/删除是不同动作（F‑M004‑09）。
  - 阅读统计仅本机；本机 Local Backup/Restore 与账号/服务器登录无关（F‑M004‑12/17）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发：
  - 以“文件/阅读状态/手写层/关联笔记/导出副本/本地备份/云同步”作独立对象；“已导出/已上传/已通知”不等于可回滚或跨端一致。

## 范围与版本

初查2026-10-26；2026-10-27本地补读阅读与笔记帮助。固定对象为 **Note Air5 C（Android 15），V4.1.2 功能基线**，预装 NeoReader 与 Notes；英文国际版资料，实际设备构建号未知。不是整个 Note 系列，也不包含 Note Air4 C、Palma 2 Pro。官方产品页确认本机 Android 15 与 Smooth 模式的 V4.1.2+ 条件；2025-10-20 官方机型发布文章明确本机使用 V4.1，2025-08-28 官方文章给出该版本增量。[E-BOOX-01～03]

**D2 文档重建，无实机、无连续视觉观察。** 2026-10-26归档的BOOX公开帮助含后续更新，菜单可能比 V4.1 新；下表是该功能基线与截至查阅日帮助的交叉研究，不是某台 V4.1 机器逐页录制。Smooth 是本机 V4.1.2+ 的明确条件功能，不倒填至初始 V4.1；前批读取的固件页已换成 V4.3，也不倒填其 Talk Insights、Dual Notes 等功能。旧 Note Air2 V3.2/3.3 手册及更新记录不作为本机操作依据。

## 功能目录与已查/待查

| 功能域 | 已查范围 | 尚未形成完整路径 |
| --- | --- | --- |
| 系统 | 账户/服务器、下拉双面板、手势/导航球、应用后台冻结、更新 | 首次向导逐屏、指纹/密码失败、休眠策略、第三方来电 |
| 显示 | EinkWise 入口、按 app 配置、NeoReader 刷新作用范围 | 本机 V4.1.2 完整模式名称/参数默认值、真实残影和耗时 |
| 阅读 | 选词/词库、段落批注、目录/书签/返回视图、TTS 入口、关联笔记 | OCR配额耗尽/离线包条件、扫描PDF精度、DRM、朗读定位与后台恢复 |
| Notes | 笔画编辑、页删除、录音入口、识别、导出、账号可见性 | 本机全部画笔/模板参数、隐藏或锁定层的作用范围、冲突合并、录音与手写对齐 |
| 数据 | BOOXDrop、本地备份、源文件与阅读数据同步、第三方云 | 断线重试、同名文件、移路径后的关联、回收站/版本历史 |

前批逐条登记公开 **User Guide (ePaper Tablet PCs)** 的 **14 节、59 篇**，见[目录 CSV](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/functional-inventory.csv)。API 分页已结束，条目数与各节一致；“59 篇”是公开帮助文章数，不是本机完整功能数或已验证任务数。Notes／阅读数据有同名导向文章，保留文章 ID，不人为合并计数。[E-BXCD-01]

第28批复用该原件，完整阅读其中10篇正文；[阅读与笔记目录CSV](../batches/boox-reading-controls-deepening-inventory.csv)保留59篇文章，并列68个正文标题或控件组，共127行。文章与子项有包含关系；这些行不是127个功能或完成任务。正文未重新联网获取，仍是前批采集版本；该批只新取4张官方静态附件并实际查看。

目录完整性仅限这个公开分类；**V4.1.2 专用整册仍未取得**。官方仍要求 Settings → User Manual 或登录对应 send2boox 服务器按机型下载，未登录；搜索找到的商品聚合“手册”和 FCC 单页不当操作手册。[E-BOOX-19；E-BXCD-02] 相机扫描文章只列其他具体型号；InkSpire、Work Profile 的本机适用未核；Talk Insights 等后续能力不倒填到固定基线。下表补公共目录未逐项研究的范围。

| 公开目录节 | 篇数 | 当前覆盖边界 |
| --- | --- | --- |
| General Settings | 11 | 第44批补语言/连接/睡眠/显示/前灯/音量/投屏正文；鼠标资格单列，失败与本机状态仍缺 |
| Interface and Navigation | 2 | F02补桌面/Dock/小组件操作说明，原文限定Tab系列；本机验证未完成 |
| Optimize Speed and Display | 3 | F03补按app颜色、黑白/彩色条件；真实显示、参数默认和恢复仍缺 |
| Apps | 8 | 第44批补浏览器/PushRead/日历局部任务；安装仍入口D1，Work Profile本机资格未知 |
| File Transfer | 3 | F04补手机和第三方云步骤；断线/覆盖/重试未知 |
| Split Screen | 2 | F08；帮助型号例外单列 |
| Reading | 16 | F05–08、11–12、14的局部；本轮F15–17补书库视图、版式/局部控制及统计；OCR错误/恢复和本机逐项布局仍缺 |
| Note-Taking | 4 | F09–10、13；本轮F18–19补标签定位和Text Notes；页/层/对象只按通用正文及静态图限定 |
| Stylus | 2 | 正文已读；EMR配笔资格待核，InkSpire本文限定Tab X C |
| Data Syncing and Backup | 2 | F11–13，仍缺冲突／覆盖恢复实测 |
| Keyboard Cover | 2 | 已读连接/快捷键正文；本机配件资格、输入冲突和状态保留未知 |
| Camera (Smart Scanner) | 1 | 其他型号专属文章，不合并到 Note Air5 C |
| Screenshot and Screen Recording | 2 | 第44批补截图/录屏文档任务与录制设置静态图；没有实际录制 |
| Lab Features | 1 | 后续版本入口，不纳入 V4.1.2 操作结论 |

## 任务流程

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M004-01 | 建立云端身份 | 联网；可接收验证码 | Settings → ONYX Account → Servers；选服务器、邮箱/手机号、验证码后 Login。失败检查号码格式、系统时间/时区和网络。 | 登录同时可注册；跨设备必须保持账号与服务器一致。 | E-BOOX-04；E-BOOX-18 | 首次向导可跳过哪些页、离线设置；不能把 Google 账户等同 ONYX。 |
| F-M004-02 | 从阅读进入全局控制再返回 | 当前 app 开启 | 系统栏右侧打开控制中心、左侧通知中心；Edit 管理快捷项。Settings → System Navigation 配置 Navigation Bar／Bottom Navigation／Side Gestures；隐藏导航栏先底部上滑调出。导航球双击回桌面。 | 控制/通知中心分侧；快捷项可Edit补入；侧边内滑需启用；导航栏隐藏先上滑调出；双击导航球回桌面不等于保存或终止进程。 | E-BOOX-05；E-BXCD-03 | 未取得V4.1.2机型手册；面板关闭动作及选区/阅读页保留、手势与app冲突未观察。 |
| F-M004-03 | 按任务调整显示 | app 或文档已开 | 控制中心 → EinkWise；NeoReader 中央菜单 → Refresh。第三方 app 内 E Ink Center → App Optimization，或桌面长按 app → Optimize；按问题调整 DPI／全刷／方向，支持恢复原 DPI、关闭 Master Switch 或 Reset All。 | 单次全刷/App显示配置/当前文档/全部文档不同；原DPI、关闭Master Switch、单App Reset All及全App重置不同；NeoReader文档帮助Regal/Speed不与通用HD/Speed/Smooth混表。 | E-BOOX-06；E-BOOX-07；E-BOOX-08；E-BXCD-04；E-BXCD-05；E-BXCD-06；E-BXCD-07；E-BXCD-12 | 本机4.1.2完整模式/默认参数/生效及退出反馈，重置关联、卸载后优化数据保留、真实残影/耗电未知。 |
| F-M004-04 | 从电脑、手机或第三方云传入/复制资料 | 电脑和设备同 LAN | 打开 BOOXDrop → 电脑输入设备显示的网址 → 上传；在设备 Receive 找文件。远程 send2boox 则登录同服务器，在 BOOXDrop Online transfer 发送，设备 Push 查看。 | LAN 双向传输与云推送是两条路径；收到文件还需选择 app 打开。 | E-BOOX-09 | 空间不足、取消、重复文件、局域网隔离与失败提示。 |
| F-M004-05 | 查词、加生词与翻译选段 | 可选文字；字典已装 | 长按词 → Word Menu 查释义/加 Vocabulary Builder；拖端点扩选段 → Translate。字典文件解压至 dicts 下独立文件夹，在 Dictionary → Preferred Dictionary 勾选；不识别可重启词典。 | 生词归入词典 app；文字翻译与本地词典查询是不同服务。 | E-BOOX-10；E-BOOX-11 | 弹窗关闭后选区、词典离线完整度、在线引擎配额；不推定所有第三方词典可弹出。 |
| F-M004-06 | 标记并找回文档证据 | 可选文字；本机有笔 | 长按选段标记，Contents → Annotations／Handwriting 管理导出。补充：Progress → Blank Page 可在 PDF 加空白页；可从 Preview 或书签旁入口删除新增页。 | 文本批注与手写分开管理；Progress的Blank Page会嵌入PDF，可在Preview或书签旁入口删除新增页。 | E-BOOX-12；E-BXCD-11 | PDF新增页删除确认/撤销/锚点变化、跨软件对象可编辑性、导出失败和存储不足未观察。 |
| F-M004-07 | 查阅旁页后回到原处 | 已打开文档 | 中央菜单 → Progress，用页码／滑条／章跳转；Prev View／Next View 逐次前后返回。Contents 可回目录／书签，书签图标有独立标记状态。 | Progress内Prev View/Next View逐次往返；书签是独立持久标记，不承诺视图历史跨关书或重启。 | E-BOOX-13；E-BOOX-14；E-BXCD-11 | 关书/系统回收后历史、EPUB重排定位与重复书签未知。 |
| F-M004-08 | 阅读时创建关联笔记 | 支持笔输入的本机；文档打开 | 中央菜单 → Split View → Handwritten/Text Notes 建关联笔记。Current Doc 另为同文档两窗。控制中心 Split Screen → Swap Sides／Exit；系统分屏开 Notes 则独立，返 Home 也是系统分屏退出途径。 | Current Doc两窗标记在刷新后同步；关联笔记与系统独立Notes不同；控制中心换边/退出，系统分屏也可回Home退出；Flexible帮助型号名单未列本机。 | E-BOOX-15；E-BXCD-09；E-BXCD-10 | 关联Notes的写入时刻不能由Current Doc推断；源文件移动/删除、退出后页锚、Flexible本机适用未知。 |
| F-M004-09 | 写笔记、改笔画、保留输出 | Notes 中的普通手写笔记 | Notes书写/擦除/Undo/Redo；Exit保存回列表，More/Save另为手动入口。当前帮助还列Page Manager的侧栏/全屏管理、Layers及Lasso对象工具，细节见下文。 | 全层擦除保留模板层；Page-明确不可撤销；当前套索可跨层选择/移层，但未核本机V4.1.2的全部工具。 | E-BOOX-16；E-BXCD-13；E-BXRD-11/12 | Home/切App/断电写入、.note回导、其他删页/删层/删对象的撤销、隐藏或锁定层选择范围、图层锁效果及新工具适用未知。 |
| F-M004-10 | 手写转文字并保留录音线索 | 有手写内容/麦克风权限 | Notes 的 Text Recognition 可识别后编辑/复制/分享/插入，语言包在 More languages 下载；More → Recorder 录音并加标记，More → Recordings 管理导出。 | 识别文本、墨迹、录音分开；V4.1 Infinite Notes双指拖画布、空白长按Pin及三点改名/删除；Smart Scribe帮助限部分机型。 | E-BOOX-16；E-BOOX-02 | 新套索/Smart Scribe本机适用、原位替换/撤回、录音笔画对齐、来电/锁屏中断、语言包离线条件仍未知。 |
| F-M004-11 | 跨设备同步阅读状态和原书 | 两端同账号/服务器、同文件路径 | Library Settings 开总 Sync；长按书 → Sync 开单书开关；书内上方 Sync 手动同步。V4.1 可在单书 Sync → Upload source file。 | 阅读进度/批注与源书上传分开；单书上限帮助写 ≤200 MB，发布稿写 <200 MB，边界值待测。 | E-BOOX-17；E-BOOX-02 | 两端同时编辑冲突、重命名、路径移动、原书与状态版本不一致。 |
| F-M004-12 | 无云迁移书库及关联数据 | 两端 V4.1+；可传文件 | Library → Library Settings → Backup and Restore → Local Backup and Restore，生成 .booxc；传到另一台后恢复。 | 备份包含本地书及阅读数据；不是整机 app/登录凭据备份。 | E-BOOX-17 | 增量/覆盖策略、恢复确认、加密、损坏包、存储不足。 |
| F-M004-13 | 同步笔记或找回“登出后消失”的笔记 | ONYX 账户与服务器已确定 | Notes → 菜单 → Sync Settings 开全局同步；长按笔记 → Sync 可设单本；点击列表/笔记内 Sync。登出后若笔记不可见，重新登录原账号查看。 | 官方说明登录前笔记会纳入账号，登出后不可见不等于文件已删除；服务器数据独立。 | E-BOOX-20 | 账号删除后回收、未上传笔记去向、其他账号是否留本地副本。 |
| F-M004-14 | 从 TTS/后台问题排查到安全更新 | 选书/相关音频 app 已开 | NeoReader → Progress → TTS；系统 TTS Output 配引擎。按问题分别检查 App Optimization → Others → Stay active，以及 Settings → Apps & Notifications 的权限／通知／App Startup／Freeze。系统更新仍先备份。 | TTS引擎、通知权限、App Startup、Stay active和Freeze是不同控制；冻结阻后台/通知；睡眠连接设置另列；安装/卸载只取得入口D1。 | E-BOOX-14；E-BOOX-21；E-BOOX-22；E-BOOX-23；E-BXCD-06；E-BXCD-07；E-BXCD-08；E-BXCD-14 | 各后台开关优先级、TTS锁屏/蓝牙/焦点中断续播、安装失败/卸载后文件账户和偏好保留/恢复未知。 |

## 阅读与笔记新增任务（2026-10-27）

以下5条仍是**通用官方帮助的局部D2**，实际设备/构建未核。PDF与EPUB帮助明确标V4.1；Text Notes文称V3.3.2引入。Notes页面含2026年编辑内容，没有版本标识的新工具不能倒填至本机V4.1.2。

| F编号 | 目标与前提 | 入口、选择和资料所述结果 | 数据范围与未闭合分支 | 证据 |
| --- | --- | --- | --- | --- |
| F-M004-15 | 书已在设备，切书库视图并找最近文件 | Library书本图标选择Scan或Directory；前者图标为书架，菜单可新增书架/筛选排序；后者按目录路径显示。Scan菜单与Directory第二图标分别切Cover/List；时钟入口显示最近最多12文件。 | 切视图不是移动源文件。New Bookshelf/Manage Library虽在图中，未给命名、移动/删除、扫描范围或回收链；不把“扫描模式”解释成已自动收录所有目录。 | E-BXRD-01/10 |
| F-M004-16 | 已打开文档，按格式调整版式和局部控制 | 固定版式在Style选主题/缩放，Layout再选模式并再次点击细调；双指缩放先在浮动栏Gestures启用，框选缩放另用Marquee。重排文档在Style选主题，More Settings或再点当前主题细调字体/间距等，可复制到其他主题。 | Layout在V4.1仅固定格式出现；不能把视图裁切视为删除PDF内容。EPUB静态Copy to图有For this book only、Cancel/OK；只看到选项，未确认/取消或验证全库生效。具体文件常需试调；保存/还原和重排后锚点未知。 | E-BXRD-02～06；E-BXRD-13 |
| F-M004-17 | 查看或停用本机阅读统计 | Library右上Reading Statistics看每周时间、书量等；菜单Settings → Library settings → Enable reading statistics开关。 | 正文明确仅本设备、不能同步；与F11阅读进度/批注同步分开。计时算法、待机/后台、关停是否清历史、导出/恢复未说明，阅读时长不代表理解或完成。 | E-BXRD-07 |
| F-M004-18 | 为已有手写选区加标签并回到内容 | Notes用Lasso选笔迹→Tag；识别文字可作建议名，也可手输或选最近标签→OK。侧栏Search列本笔记标签，点标签定位；Notes列表菜单Options → Tag Management可增删设备标签。 | 本笔记定位与设备标签管理是不同范围；识别失败、同名合并、跨本/云同步、删标签对笔迹影响及回收未知。 | E-BXRD-09；E-BOOX-16 |
| F-M004-19 | 编辑已有Text Notes并输出 | Undo/Redo、Style/Format/Spacing/Align/List编排；Search搜索或替换；Insert加入网站链接/图片/附件。More → Save手动保存，Export输出TXT/.note。Record录音固定在画布，完成后可回放/删/导出/分享。 | 本篇未给新建入口、Exit保存、导出目标/覆盖/取消、附件上限、.note回导及录音对齐。不能套用手写笔记PNG/PDF/.note导出格式或20MB附件条件。 | E-BXRD-08 |

**F09补充：先区分页、图层和对象。** Page Selector切页并可由Next Page快速新增；Page Manager侧栏便于浏览，全屏可复制、移动或删除页，Thumbnails/Outline/Tag分组。实际查看的示例图是Outline列表，并不是一次删页过程。More → Page-明确不可撤销；资料未说明其他页管理删除是否有同样规则。[E-BOOX-16；E-BXRD-11]

Layers正文说最多**新增5层**，并列显示、隐藏、删除、排序。Eraser的Erase Current Layer清空当前层内容，Erase All Layers清空模板层以外各层的内容；图层显隐与清内容不是同一动作。实际图可见普通层、模板层、锁和垃圾桶，但锁的作用、隐藏/锁定层是否会被全选或擦除未说明。当前Lasso正文列Smart/Freehand/Rectangular、Select All跨层、Move to Layer及剪贴/翻转等；较新的日程/转文入口保持版本未知，不据跨层选择推断一切对象可编辑。[E-BOOX-16；E-BXRD-12]

Insert中的手写笔记附件上限为20MB；可链接文件的具体页或其他笔记/页，长按顶部弯箭头查看笔记跳转历史。链接不是文件复制证明，源文件更名/移动后的有效性仍未说明。Page+的其他笔记/图/文件来源属于当前帮助，复制关系及本机版本未核；FreeMark明确在NeoReader与Notes中禁用，不把它当两者的笔记工具。[E-BOOX-16]

**F16补充：局部控件与显示优化分开。** NeoReader中央菜单→More/Settings → Global Settings可开关状态栏、拖动项目顺序；Settings → Touch Settings设翻页区。Floating Toolbar Settings下General和Handwriting两组分别增删按钮，后者在手写状态出现。这些是阅读器设置，不能套成系统导航球或F03的app DPI优化。退出、持久化与手势冲突尚未实际观察。[E-BXRD-02/03/06]

PDF帮助还给出V4.1的Page/Crop OCR、在线中英每日300页、离线30+语言且无页数限制等条件；本轮只登记这段资料，未核语言包、额度耗尽/离线失败、取消或结果回写，不另计完整OCR任务。其“后台无需等待”“更高准确率”属于官方描述，不作为耗时/识别精度证据。[E-BXRD-04]

## 前批控制路径补强（2026-10-26）

前批不新增F，集中补原编号；下述路径属于**当前通用帮助层面的 D2**，不是 V4.1.2 实机确认。帮助里的设备例图、默认值与型号名单必须连同限制阅读。[E-BXCD-01–14]

**F02：区分面板、返回与保存。** 通知中心 Clear All 是通知列表动作，不是删笔记；侧边内滑返回默认未启用，需要先配置。导航球在控制中心缺失时，可 Edit 后拖入快捷项。文档内返回历史见 F07，退出 Notes 见 F09；双击导航球回 Home，资料没有说明是否同时结束进程或持久保存。关闭面板的确切触摸位置、打断选区后是否保留尚未观察。[E-BXCD-03]

**F03：把显示问题和撤回范围说清楚。** 当前 ePaper PC 帮助默认右下上滑进 EinkWise；另一 E Ink Tablet 分类的同名页写左下。均提醒可自定义，不将两者拼成 Note Air5 C 的统一默认。[E-BXCD-04] 通用刷新页列 HD／Speed／Smooth；NeoReader Refresh 帮助却列 Regal／Speed。记录两种上下文，不把名称硬对齐或声称本机同时具有所有模式。[E-BXCD-05、12]

第三方界面被裁切时，优化帮助建议降低 DPI；过小时提高，也能选原 DPI。**Master Switch 暂停整套优化；单 app Reset All 重置配置；Apps & Notifications → Reset all app optimization 涉及所有 app**，不是同一恢复范围。方向强制跟随系统仍错乱时，帮助要求关闭该选项；Cloud Config 不保证每个 app 都有配置。未取得各开关即时生效、重启或卸载后的保存规则。[E-BXCD-06、07] 对 NeoReader，Apply to All Documents 区分当前文档／全部文档；Full Refresh 的 Deep 选项、触发频率与一次手动 Full Refresh 也分开。静态图中的开关值只是图示状态。[E-BXCD-12]

**F08／09：回读原文与退出保存。** Current Doc 明确两窗口手写、标记、书签在页面刷新后同步；这句话不能外推到“独立 Notes 与原书每笔同步”。关联笔记、另一个文件及两个 app 分屏要分别建模。系统分屏横向为左右窗、纵向为上下窗；未兼容 app 不能强推成功。帮助的 Flexible Split Screen 型号名单只列 Note Air3 C／Note Air3／Tab Ultra C Pro，未列 Note Air5 C，因此该项本机适用仍未知，不能反向断言不支持。[E-BXCD-09、10]

Notes 的 Exit 保存并回笔记列表，More → Save 手动保存，Page- 不可撤销。早期表把“More → Save 或 Exit”并写，前批分开入口。Home、切 app、系统回收与断电时的写入完成点仍没证据；不以“自动保存”概括所有退出方式。[E-BXCD-13]

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

前批只读官方正文及公开API原文，并实际查看四张共用帮助静态图：控制中心、App Optimization 的 Display 页、Apps & Notifications、NeoReader Refresh。**图中型号／固件不明，未识别为 Note Air5 C，不能证明 V4.1.2 的布局或默认值**。全程 hands_on=false，无连续视觉流程、D4 或耗时测量；菜单中的 AI/智能词也不能证明 G-002 全系统访问。

前批静态图与取证摘要见[资料](/home/workspace/reader/docs/research/interaction-library/artifacts/boox-controls-deepening/source-notes.md)。前三图是共用设备示意，第四图可见 Regal／Speed、Normal／Deep、频率与 Apply to All Documents。只记录控件存在，不推断点击效果。正式归档使用相对资料库路径；API 原件在 work，以 raw_work_path/raw_sha256 记录。

以下为推断：对 Plus，最值得复用的是 **内容身份、阅读状态、关联笔记、可恢复输出分开建模**，以及“旁路查阅 → 回原位置”的显式入口。大屏双窗不适合直接缩到小屏，可探索原文与解释/笔记间保留锚点的单页切换。对 5–6 岁，长按、拖选区、两层同步开关可能需要更简单的可见入口；对成人学习，原文引用和独立笔记之间的关系应可查询。R-018 仍需要专门验证听读断点，G-002 需要暴露文件来源、选区、同步开关与删除作用范围，而不是只提供 AI 聊天按钮。

最小后续观察：获取 Note Air5 C 的 V4.1.2 实际构建/机型手册；用同一 PDF/EPUB 完成选段→批注→关联笔记→改名/移文件→同步及恢复，再做 TTS 锁屏/切 app/蓝牙中断。每步记录对象 ID、页锚、文件及回退结果。另选一个布局裁切的第三方 app，比较原 DPI、Master Switch、单 app 重置与全 app 重置；用可丢弃数据观察卸载后内容及配置去留。当前只完成资料补强。

## 系统与内容入口补查（第44批，2026-10-27）

本次补读原目录31篇，详见[逐项来源与7幅静态图](../artifacts/boox-system-content-deepening/source-notes.md)；原127行目录就地更新。下面是**当前通用帮助的局部D2参考**，不是Note Air5 C V4.1.2逐项确认，也不把Tab系列示意图称作本机截图。读取正文不代表每个控件、失败和恢复均已研究。

| F编号 | 任务与前提 | 动作/资料所述结果 | 未闭环 | 证据 |
| --- | --- | --- | --- | --- |
| F-M004-20 | 截取当前画面或长网页（通用帮助参考）；支持该功能的构建、可截图页面；三指需启用 | 控制中心Screenshot/Scrolling Screenshot；缺项Edit拖入；导航球另配置；资料只给触发与限制，未给实际文件和完成提示 | 本机V4.1.2路径、权限/限制反馈、长截图范围、保存/取消未核 | E-BXSD-03；E-BXCD-03 |
| F-M004-21 | 录屏后保存和查看输出（通用帮助参考）；支持录屏的构建，拟录制页面已打开；未实际操作 | 控制中心Screen Recorder；圆形开始、X结束；长按入口先调音源/帧率等；说明存Storage/Movies/Captures为MP4；图片只显示锁屏停止并保存选项，不证明其执行 | 本机构建、实际锁屏保存、权限拒绝/空间不足/取消、音源和文件可播放性未知 | E-BXSD-04 |
| F-M004-22 | 向另一屏幕展示设备内容（通用帮助参考）；按路径满足Windows/Miracast或同LAN/浏览器；第三方路径另需安装授权 | BOOXDrop控制中心→Start Now→接收端打开所示URL；旋转/全屏；图像/正文支持显示路径与限制；不将Miracast所称控制套入BOOXDrop | 设备固件和接收端资格、失败/拒绝、断开重连、延迟及真实控制未验证 | E-BXSD-08 |
| F-M004-23 | 订阅来源并接收稍后读内容（通用帮助参考）；PushRead可用；自备有效源或本地OPML文件 | RSS→Directory→Add subscription；公共源Subscribe或Custom建组加源；菜单Import from OPML；新增的是订阅来源；不证明全文、图片、书籍已下载或离线准备完成 | 本机构建、无效源/重复/部分导入、取消订阅后内容、正文缓存和已读状态未知 | E-BXSD-16 |
| F-M004-24 | 网页阅读中调整显示并查词（通用帮助参考）；NeoBrowser打开可选文字网页；词典可用 | 右上菜单增强显示；长按词→Dictionary→选择词典；资料描述显示/词典弹窗；不证明网页去推荐、保存正文或进度恢复 | 本机版本、全屏退出、查词关闭/返回选区、断网/保存、模式持久化未知 | E-BXSD-15 |
| F-M004-25 | 按日期回看备忘和阅读痕迹（现行通用帮助参考）；Calendar Memo已有对应日期内容；本机现行版本未核 | 选择日期查Memo及Today’s Note/Read，点条目打开；记录依据是最后编辑/阅读日期，不是创建、读完或理解；事件同步不回填固定基线 | V4.1.2逐项适用、保存/删除/恢复、日界线/时区、完成判定未知；日程表单未闭合 | E-BXSD-17；E-BXSD-01 |

**原任务补强。** F02的桌面移出组、移除小组件与卸载对象不同；Tab文档Smart Assistant是快捷信息页，不是AI Assistant。F03区分系统字体/前灯、按app颜色、单文档显示；黑白屏Light Color Filter不能套入彩屏菜单，Regal才有Image Smoothing。[E-BXSD-02/06/11/13/19；E-BXCD-03]

F04补手机双向传输与云复制上传：手机Send to BOOX后设备Receive；反向Save To Phone。第三方云从Storage长按Copy to Cloud Storage选目标并上传，不当自动双向同步。F14保留睡眠与音频的不同条件：Tab帮助静音仅铃声/通知，冻结阻后台及通知；含时钟屏保使自动关机不启用。图中Call滑条不证明蜂窝通话，录屏30fps也不证明电子纸刷新率。[E-BXSD-18/01/07/04；E-BOOX-24；E-BXCD-07/14]

**资格排除与仍待查。** Scan Documents仅列Tab Ultra三款；InkSpire仅Tab X C；Work Profile/AI Assistant仅部分机型；鼠标自定义仅Tab系列；键盘保护套需核配件。它们保留来源以便比较，不据此新增本机任务。NeoReader选区提问不证明跨app执行。后续Talk Insights保留“关闭前备份、关闭可能移除笔记”的参考，排除固定V4.1.2基线；正文更新时间本身不能证明固件归属。[E-BXSD-05/12/14/20–25]

**供本项目讨论。** “静音”“后台运行”“已保存”“已读”必须带对象、范围、前提和结果。可借鉴这些差别设计AI动作确认与状态说明，但不据本文确定系统接口、账户制度、新闻形态或新增已确认需求。

## 证据

下列既有来源采于2026-10-26；本轮复读不作为正文重新获取。01/02/03 为产品/发布资料；其他为官方帮助正文，部分通过官方 Zendesk 公共 API 获取；帮助更新时间已保留在批次 JSONL。03 是官方机型发布文章；本例仍未核验实体设备的固件包。

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

前批E-BXCD-01–14为同一官方来源体系的目录／正文快照和静态取证，不因新编号算独立交叉验证；详细来源、定位与归档见新批 evidence JSONL 和上面的取证摘要。

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

## 本轮正文与静态来源（2026-10-27）

10篇正文来自前批2026-10-26的公开API原件，本轮本地复读；Handwritten Notes沿用E-BOOX-16，9个新文章E由分类原件E-BXCD-01派生，不算新增独立互证。只有以下4张附件重新普通GET，均200并实际查看；图片机型/构建未知、没有连续操作。

| E | 官方来源与定位 | 获取与边界 |
| --- | --- | --- |
| E-BXRD-01 | [Choose a Book Display Mode](https://booxsupport.zendesk.com/hc/en-us/articles/10701497927316-Choose-a-Book-Display-Mode) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-02 | [Customize Status Bar](https://booxsupport.zendesk.com/hc/en-us/articles/10701489683092-Customize-Status-Bar) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-03 | [Set Up Page-Turn Area](https://booxsupport.zendesk.com/hc/en-us/articles/10701488996500-Set-Up-Page-Turn-Area) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-04 | [Adjust PDF and Similar](https://booxsupport.zendesk.com/hc/en-us/articles/10701486858388-Adjust-PDF-and-Similar) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-05 | [Adjust EPUB and Similar](https://booxsupport.zendesk.com/hc/en-us/articles/25400769451540-Adjust-EPUB-and-Similar) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-06 | [Floating Toolbar](https://booxsupport.zendesk.com/hc/en-us/articles/10701491126548-Floating-Toolbar) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-07 | [Reading Dashboard](https://booxsupport.zendesk.com/hc/en-us/articles/10701454503572-Reading-Dashboard) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-08 | [Text Notes](https://booxsupport.zendesk.com/hc/en-us/articles/15196391289108-Text-Notes) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-09 | [Tag System](https://booxsupport.zendesk.com/hc/en-us/articles/10701570125588-Tag-System) | 既有正文；文章更新时间仅帮助元数据，不是本机固件版本。 |
| E-BXRD-10 | [library-scan.jpg 官方帮助静态图](../artifacts/boox-reading-controls-deepening/library-scan.jpg) | Article 10701497927316 image 2；扫描模式菜单可见New Bookshelf/Manage Library/Filter and Sort/List Mode等；未触发操作。 |
| E-BXRD-11 | [notes-page-manager.png 官方帮助静态图](../artifacts/boox-reading-controls-deepening/notes-page-manager.png) | Article 10701578837268 image 10；Outline示例列表与页码、三点菜单可见；没有复制移动删除结果。 |
| E-BXRD-12 | [notes-layers.png 官方帮助静态图](../artifacts/boox-reading-controls-deepening/notes-layers.png) | Article 10701578837268 image 13；普通层/模板层及新增、眼睛、锁、垃圾桶可见；锁行为正文未说明。 |
| E-BXRD-13 | [epub-more-settings.png 官方帮助静态图](../artifacts/boox-reading-controls-deepening/epub-more-settings.png) | Article 25400769451540 image 2；Copy to对话框有For this book only及Cancel/OK；示例Bold选中非默认，未确认或取消。 |

四张实际观察为书库扫描模式菜单、Notes页管理Outline列表、图层面板及EPUB Copy to对话框；没有查看本批其余文章插图。逐图URL/散列见[观察审计](../artifacts/boox-reading-controls-deepening/observed-images.json)，证据范围见[独立摘记](../artifacts/boox-reading-controls-deepening/source-notes.md)。没有设备、账户、安装、购买、同步或外部发送操作，hands_on始终为false。
