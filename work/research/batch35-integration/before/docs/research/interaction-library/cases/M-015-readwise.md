# M-015 Readwise Reader：读听、数据恢复与全库 AI

查阅：2026-10-26；2026-10-27补公开目录、内容生命周期及五张静态图。范围：官方现行 FAQ，区分 web 与移动端；未固定 app 构建、操作系统和套餐。**主要为D2文档及选定静态图；外部AI用途保留D1。无亲自操作或连续视觉证据**。完整功能清单未核定。

## 流程与状态

| 流程 | 操作与反馈 | 状态/分支 | 证据 |
| --- | --- | --- | --- |
| F-M015-01 订阅与处理 Feed | web 从 Manage feeds 添加；移动端从 Feed 菜单添加，也可导入 OPML。阅读条目、标记已见或刷新。 | 普通滚动不会自动标记 seen；移动端专用卡片视图会。源只给部分正文时，可报告 partial feed content，让后续条目改从文章 URL 解析。 | E-RDW-01 |
| F-M015-02 听读与双向定位 | 暂停/停止、滚动分离、离线 长文模式顶端改为Aa，More/Listen using text-to-speech进入听读；不是播放能力被移除。 | 强退/重启/换设备、加载失败 | E-RDW-02;E-RWCD-14 |
| F-M015-03 批注与标签 | 网页列表T/文内Shift+T标文档，选高亮T标高亮；Enter确认关闭，Ctrl/Cmd+Enter确认留菜单。Action Tag置于note影响Readwise：.h1标题、.c1/.c2合并；旧锚点失败仍从Notebook查回。 | 标签不互相继承；合并/删除、锚点修复、离线冲突、Action Tag更新回滚未核；不是全部高亮自动制题。 | E-RDW-03;E-RDD-06;E-RWCD-03 |
| F-M015-04 准备离线内容 | 移动端 Account → Offline documents → 开启分区并等待下载进度。 | Feed 默认关闭；关闭分区会移除缓存，单独打开过的文档仍可能缓存。 | E-RDD-01 |
| F-M015-05 浏览后返回阅读位置 | 最远进度与最后位置分开；网页箭头/手机Return返回，手机X重设当前位置。网页Shift+R重设可能暂显灰色0%，持续阅读后更新；关闭Return indicator只隐藏按钮仍跟踪。 默认视图可选Currently reading以进入最近文档，非精确音频断点证明。 | 实际阅读判定阈值、刷新/跨端并发、音频与文本锚点、重设失败及误关按钮恢复未核。 | E-RDD-01;E-RWCD-14 |
| F-M015-06 删除后恢复 | More/Delete或网页D删除；原设备Trash恢复批注。网页Z可逐步撤销近几项、iOS系统摇动或部分toast可撤销；批量恢复Restore all，移动Search/View措辞差异保留。 | 撤销深度/时限/重启、Android通用Undo、永久清空、换设备恢复、更新文章后旧批注与版本冲突未知；即时撤销不等同备份。 | E-RDD-01 |
| F-M015-07 搜索范围变化 | web 放大镜或 `/`；移动端 Search。Better Search 默认联网搜全 Library，断网改搜已同步本机内容。 | Feed 正文未索引；移入 Library 后可索引。标题过滤不能代替全文搜索；过滤视图与全文搜索目前分开。 | E-RDD-04 |
| F-M015-08 全库问答与动作 | Global网页/桌面启用后问答带段落引用；读写工具分组、每工具允许/确认/禁用。确认卡Remember加Approve/Deny将该工具后续权限改为允许/禁用；静态卡未展开详情。 | 文档只举工具类别不是完整工具清单；拒绝后对话、部分成功/取消/重试、并发状态、失败回滚与移动全库能力未核。 | E-RDD-02;E-RWCD-13 |
| F-M015-09 技能与会话 | 技能编辑名称/描述/指令/作用域/模型，斜杠触发；文内会话可展开同聊天，菜单新建/改名/复制/删除/检索历史。静态空表Save灰显；移动清除聊天仍可能在网页历史。 | 未填表；保存校验、技能加载与模型上下文成本、触发歧义、删除与跨端历史一致性未知；空表灰显不证明具体必填规则。 | E-RDD-02;E-RWCD-13 |
| F-M015-10 临时解释或存入笔记 | 移动端选词 → Quick Lookup → Define/Lookup/Translate；后两者分别解释上下文和翻译。 | 需联网及开启 Ghostreader；默认只展示。Copy 复制结果，Highlight 存选区并把结果附为笔记；点外部关闭不保存。 | E-RDD-03 |
| F-M015-11 导出不同数据对象 | Notebook笔记、账户CSV/OPML/内容ZIP分开；完整包邮件异步。网页Shift+D导出带批注PDF；Print with annotations调用浏览器打印，未实际输出。 | 完整账户回导、ZIP进度/失败、文字视图高亮在PDF出口中的位置、外部PDF批注往返和第三方删改同步未核。 | E-RDD-05;E-RWCD-08;E-RWCD-11 |
| F-M015-12 保存筛选与分组 | 命名保存查询；可从标签/作者/源建立视图；按Location或Seen分栏、启计数及在Home配置排序。移动Feed文件夹选源Next创建，之后Done确认增减；视图是查询而非内容副本。 | 计数范围、空/错误查询、重名/删视图、移出结果后的当前阅读状态、跨端排序和批量恢复未知。 | E-RDD-06;E-RWCD-12 |

## 功能目录与覆盖

以官方文档导航和已打开章节为目录依据；下表不是全量设置清单，也不将导航标题当成已体验。

| 功能域 | 已登记任务 | 仍未深入的主要内容 |
| --- | --- | --- |
| 收集与订阅 | F-M015-01/12/17/18/22 | 补分享/邮件/更新快照；批量迁移、保存失败、重试与完整恢复仍缺 |
| 阅读与定位 | F-M015-02/05/14/15/16/20/21 | 补分页/布局/媒体文本条件；实际手势冲突、重排定位和恢复仍缺 |
| 批注与组织 | F-M015-03/12/19 | 补标签确认、Action Tags及问答卡入口；同步后的学习/修改/删除链仍缺 |
| 查找与离线 | F-M015-04、F-M015-07 | 容量不足、索引延迟、多人/多端冲突、后台下载被杀 |
| AI | F-M015-08～F-M015-10 | 全部工具清单、长任务部分成功、撤回、额度耗尽、错误引用修正 |
| 数据与集成 | F-M015-06、F-M015-11 | 导出包重新导入，第三方笔记应用各自同步/删除，账号退出保留 |
| 阅读节奏与设备适配 | F-M015-13～16，文档及选定静态图 | Digest选择规则、儿童适配、刷新与触控体验未测 |

目录依据：E-RDD-01、E-RDD-02、E-RDD-04～E-RDD-06；F 范围符号只作本案例内导航。

## 分支与未知

- Feed 的 seen 不等于文章读完。切换卡片视图改变标记行为，不能用一种入口的行为解释全部入口。
- RSS 解析补救不保证可获得付费墙后内容；刷新周期受订阅规模与是否打开影响，不能承诺每条新闻固定几分钟到达。
- F-M015-02 的“暂停记位置”是文档所述当前播放行为；断电、强制结束、换设备后如何恢复未验证。PDF 听读会转到文本视图，原始版面位置如何对应待观察。
- “返回音频所在段落”和“从当前段落播放”需要不同动作语义。新段落语音未就绪时可能等待；失败提示、取消和重试规则未核实。
- 批注定位失败有可找回入口，但修复锚点、重复导入与内容更新的处理尚未知。
- 本轮新增章节仍为未标构建的现行文档；不能证明某台设备已经收到相同界面。原资料中只列 Ghostreader 功能名的浅层描述，由本轮具体任务补充，不扩大为“全部 app 行为都可由它操作”。
- AI 许可、技能说明与执行结果是不同研究对象：目前只重建前两者的部分路径，没有操作验证取消、失败重试或写入回滚。工具描述可以帮助发现能力，但渐进式披露的实际上下文成本尚无证据。
- 本例显示应单独验证“端上能看见”“检索可找到”“AI 能读取”“可以修改”。四者的覆盖不应由其中一项代替。此为研究结论，不是已经确定的平台协议。

## 对本项目的意义（推断）

关联 R-018、R-020、R-021：应区分文章已出现、已看过、读完、已掌握；也应区分阅读位置、播放位置、用户临时浏览位置。AI 接到“继续”“回到刚才”时需要这些上下文。不能由“正文已下载”推断“语音也离线可用”。以上为需要讨论的数据语义，尚未选定实现。

## 后续观察脚本

固定移动端版本，打开一篇长文和 EPUB：播放 → 滚远 → 返回播放位置 → 跳至当前段落 → 暂停/停止 → 退出重开 → 离线重试；记录每步文字位置与音频位置。另用全文/摘要 RSS 各一条，比较普通列表与卡片视图的 seen 变化。所有步骤尚未执行。

## 证据

- E-RDW-01：[Feed FAQ](https://docs.readwise.io/reader/docs/faqs/feed)，添加、seen、部分正文与刷新。
- E-RDW-02：[TTS FAQ](https://docs.readwise.io/reader/docs/faqs/text-to-speech)，移动/网页入口、暂停/停止、滚动分离与离线限制。
- E-RDW-03：[批注、标签与笔记 FAQ](https://docs.readwise.io/reader/docs/faqs/highlights-tags-notes)，快捷操作、锚点失败、标签层级。
- E-RDD-01：[Basics](https://docs.readwise.io/reader/docs/faqs)，Offline documents、reading progress、Trash 章节；同页的移动端回收站入口文字有差异，保留待核。
- E-RDD-02：[Global Ghostreader](https://docs.readwise.io/reader/guides/ghostreader/global)，Start a conversation、Run skills、tools、Manage conversations 及移动端历史 FAQ。
- E-RDD-03：[Quick Lookup](https://docs.readwise.io/reader/guides/ghostreader/quick-lookup)，三种解释、保存/复制/关闭与禁用入口。
- E-RDD-04：[Searching](https://docs.readwise.io/reader/docs/faqs/searching)，服务端/离线搜索、Feed 索引边界、全文搜索与过滤视图区别。
- E-RDD-05：[Exporting](https://docs.readwise.io/reader/docs/faqs/exporting)，Notebook 与 Account 的不同数据出口、异步下载。
- E-RDD-06：[Organizing Content](https://docs.readwise.io/reader/docs/organizing-content)，Tags、Filtered Views、RSS Folders。

前一批上述证据为2026-10-26正文读取，当时未观看图片或连续演示，未登录操作。第三方集成、全部同步/删除规则仍未穷尽，不能用已补任务数表示产品完成率。

## 2026-10-27：内容生命周期、阅读节奏与可见控制

本批19篇官方正文、五张静态图；没有登录或实际App操作。目录分帮助侧栏、正文h2/h3和单帧控件，不是完整菜单或完成率。参见[逐条目录](../batches/readwise-controls-deepening-inventory.csv)及[原件定位与静态审计](../artifacts/readwise-controls-deepening/source-notes.md)。

| F编号/目标 | 文档所述操作与状态 | 未核范围 | E依据 |
| --- | --- | --- | --- |
| F-M015-13 打开每日摘要并控制提醒 | 手机Home顶部；网页Home/Configure勾选Today's Daily Digest，显示同组文档但无相同滚动形式。Account关闭Enable Daily Digest亦关闭邮件；iOS徽标查看后清除、最多1。 | 来源为Feed与Saved for Later；选择规则、去重、当日结束/补读、离线与掌握效果未给，不把消徽标当读完。 | E-RWCD-02 |
| F-M015-14 控制归档后的推进与列表定位 | 默认移动文档后返回原列表；网页Command Palette Toggle auto-advance或手机设置可改为自动下一篇。Bump to top/快捷键B仅在Date moved排序生效；侧栏自定义与固定四栏目区分。 | 末篇、批量动作、失效过滤项、撤销后当前页及跨端设置未知；Customize swipes只给入口，没有完整手势表。 | E-RWCD-04 |
| F-M015-15 配置分页、长文布局与AI工具栏 | Appearance调整字形/主题；分页默认按文档类型设置且同设备保留，EPUB默认分页/长文。移动端Toolbar layout分别选短/长Modern或Classic，Modern需先启用Ghostreader；横屏平板双栏有独立条件。 | 正文纵向分页说明与平板横向选项分开；字距重排锚点、跨端设置、误触和关闭AI后的布局回退未知。 | E-RWCD-06 |
| F-M015-16 设置墨水屏模式与恢复入口 | Account Settings/E-ink Mode总开关启用推荐项；关总开关后可分别调动画、页动画、对比及音量键翻页。BOOX冻结提示指向系统Freeze Settings关闭Reader冻结。 音量键功能限Android；连续滚动模式跳一屏，iOS无此选项。 | 只为兼容Android设备的帮助；不是Plus驱动或60fps证据。BOOX型号/固件、内存回收、耗电与异常重启恢复未核。 | E-RWCD-05;E-RWCD-14 |
| F-M015-17 从浏览器或移动分享保存文章 | 扩展图标/Alt+R保存已渲染内容，条中可加标签/备注及移动位置；手机系统分享后可Read Now、删除、标签/备注、Later/Archive，Shortlist配置按钮不同。 | iOS Safari已登录源站与其他App/Android仅URL分开；权限拒绝、网络失败、解析完整性及重试未知，静态Saved提示不是实际成功。 | E-RWCD-01;E-RWCD-07 |
| F-M015-18 识别重复保存并更新文章快照 | 同URL再保存去重并置顶/绿点；不同参数URL可能建第二份。保存内容不自动刷新；文档要求删旧后再存更新，会连同Reader/Readwise批注删除。 | 不保证内容语义去重；Trash恢复旧版与新版本并存、批注迁移、引用修复及删除失败未知。不能用重存同URL保证更新。 | E-RWCD-07 |
| F-M015-19 将阅读高亮制成问答卡 | 在高亮note以.qa [QUESTION?][ANSWER]写问答，Enter保存；Shift+Enter可换行追加普通注释。作用对象是同步到Readwise的mastery card。 | 不是Reader内复习实操；错误语法反馈、生成时机、重复/改题/删卡联动、评分调度与离线同步未知。 | E-RWCD-03 |
| F-M015-20 标注PDF并在原版与文字视图间查回 | 网页区域截图图标→拖选→Notebook存图；Text view与原PDF高亮互不叠显，Notebook仍可查。可Shift+D导出带批注PDF；既有外部PDF高亮不识别为Reader高亮。 | 截图文字识别/AI读取、图文锚点、跨页选择、笔迹/格式往返、导入失败和批注回写未知；没有实际文件操作。 | E-RWCD-08 |
| F-M015-21 从视频字幕定位并切换文本版本 | 保存YouTube链接后字幕与播放相连；网页关autoscroll可自由浏览，Ctrl/Cmd+Enter回播放段。切增强文本可能不叠显旧高亮；换字幕语言建立新文档。 | 可用语言受原字幕限制；高亮留Notebook不代表新旧锚点修好。字幕缺失/改版、准确性、离线和音频焦点未核，未播放视频。 | E-RWCD-10 |
| F-M015-22 接收邮件内容并控制发件人 | Library/Feed地址分开；可个性化，原随机地址仍有效、旧个性化地址会失效。Reader Unsubscribe屏蔽发件人含转发，恢复需支持；原样/文本偏好按发件人跨端保存。 | 外部名单退订需邮件原链接，不能把本地屏蔽当退订成功；现有订阅总表不可得，丢信/撤销/自助解封及数据删除规则未核，未发信。 | E-RWCD-09 |
| F-M015-23 发布带批注链接并撤回访问 | 网页More/Enable public link→蓝链接复制；手机Share document/Share with annotations。蓝链接Disable后确认撤回；shared:true可查已分享文档。 | 某些用户上传PDF/EPUB不适用。后续批注更新、接收者已保存副本、撤回缓存与旧链接重启未知；没有实际发布。 | E-RWCD-11 |
| F-M015-24 分享主题内容包并显式更新 | 网页过滤视图菜单Enable public link创建Bundle；接收者Open in Reader建立视图及内容。新增文章不自动传播，Refresh public link显式增加到公开包及此前已将该Bundle添加到Reader的用户账号。 | 删除/改题/移出视图、重复导入、接收者批注冲突、断网部分成功与撤回后本地副本未知；不把Bundle当连续订阅或备份。 | E-RWCD-11 |
| F-M015-25 切换内容分流配置并确认位置迁移 | 网页Library下拉/Edit Library locations选择Triage、Shortlist或Classic→Update；手机Account选配置。若导致位置移动先确认。新保存去向及可用位置随配置变化；Triage的Shortlist标签视图不等于Shortlist配置的位置。 | 已有文档逐位置映射、取消/回滚、跨端并发、筛选视图和AI动作参数变化未核；未实际切换。 | E-RWCD-15 |
| F-M015-26 外部AI推荐与整理阅读清单（声明范围） | 官方给时间/主题/来源及跨领域推荐提示和标签/移动用途；新MCP包括Reader文档与Readwise高亮，旧版范围较小；全网搜索和定期运行依赖外部AI。 | 只有能力/提示示例；工具schema、授权范围、实际查询/写入反馈、索引时延、部分成功与恢复均未核。推荐页删除/可逆概括不作完整动作保证。 | E-RWCD-16;E-RWCD-17 |

新增来源按短摘记同名章节定位；原始URL、日期、HTML/正文SHA、静态图SHA与派生关系在E记录。旧Basics、Global、Organizing沿原E追加本轮原件，不另算第二来源。全库AI工具只取得文档列举类别，不是完整工具协议；静态技能表单说明描述有助选择，但没有验证按需加载过程或成本。

对G-001/G-002的具体启发仍是研究推断：Daily Digest徽标表示当日查看而非学会；更新文章可能影响批注，需确认原文版本；邮件退订与屏蔽发件人含义不同；公开内容包的更新会影响接收者；可见高亮与当前文本锚点是不同状态。分页、工具栏和墨水屏模式只证明App级配置，不证明E-Ink4 Plus适配或60fps。没有把这些机制自动升级为需求。

下一证据：固定App构建后核保存失败/重复与文章更新、分页跨页高亮、动作后自动推进的末篇/撤销、Digest结束反馈、问答卡生成后练习、分享/包的删改传播及离线冲突。其余未读指南、完整AI工具与原生设置仍保留目录缺口。

补充边界：F-M015-26仅D1外部AI用途与提示示例；原生Global的权限流程不能代替外部MCP授权。官方列Reader16项、Readwise6项用途，未获得schema或调用结果。长文工作流补最近文档默认入口及Android音量键条件；实际断点仍未知。Mastery页面转登录后停止，未运行练习。
