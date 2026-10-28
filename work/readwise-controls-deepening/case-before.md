# M-015 Readwise Reader：读听、数据恢复与全库 AI

查阅：2026-10-26。范围：官方现行 FAQ，区分 web 与移动端；未固定 app 构建、操作系统和套餐。**D2 文档重建；无亲自操作或连续视觉证据**。完整功能清单未核定。

## 流程与状态

| 流程 | 操作与反馈 | 状态/分支 | 证据 |
| --- | --- | --- | --- |
| F-M015-01 订阅与处理 Feed | web 从 Manage feeds 添加；移动端从 Feed 菜单添加，也可导入 OPML。阅读条目、标记已见或刷新。 | 普通滚动不会自动标记 seen；移动端专用卡片视图会。源只给部分正文时，可报告 partial feed content，让后续条目改从文章 URL 解析。 | E-RDW-01 |
| F-M015-02 从正文听读并返回 | 移动端点顶部播放，从屏幕中部附近段落开始；长篇 EPUB 入口在底部菜单。暂停保留控制条与位置；停止会隐藏控制条，继续需重新定位。 | 听着手动滚动后，可让屏幕回到正在播放处，或让音频跳到屏幕所处段落，两者方向不同。按需生成语音，文档明确不支持离线 TTS。 | E-RDW-02 |
| F-M015-03 高亮、标签与笔记 | web 可用方向键聚焦段落，再用 H/T/N 操作；自动高亮可关闭。批注可以从原网页带入 Reader。 | 页面与解析文本对应失败时会提示；批注仍可在 Notebook 找到。文档标签和高亮标签不互相继承。 | E-RDW-03 |
| F-M015-04 准备离线内容 | 移动端 Account → Offline documents → 开启分区并等待下载进度。 | Feed 默认关闭；关闭分区会移除缓存，单独打开过的文档仍可能缓存。 | E-RDD-01 |
| F-M015-05 浏览后返回阅读位置 | web 右下返回箭头；移动端 Return to X%。 | 最远阅读进度与最后位置分开。移动端点返回条的 X 会把进度改为当前位置。 | E-RDD-01 |
| F-M015-06 删除后恢复 | 删除进入 Trash；在原删除设备恢复，连同批注回到 Library。 | web 左栏；移动端单篇恢复文档写 Search，批量恢复写 View，入口存在文本差异。跨设备回收站不通用。 | E-RDD-01 |
| F-M015-07 搜索范围变化 | web 放大镜或 `/`；移动端 Search。Better Search 默认联网搜全 Library，断网改搜已同步本机内容。 | Feed 正文未索引；移入 Library 后可索引。标题过滤不能代替全文搜索；过滤视图与全文搜索目前分开。 | E-RDD-04 |
| F-M015-08 全库问答与动作 | web/桌面开启 Global Ghostreader → 左栏问答；引用可定位原段。设置中逐工具选择允许、逐次确认或禁用。 | 读工具默认允许，写工具默认确认；确认卡 Remember 会保存该工具的决定。 | E-RDD-02 |
| F-M015-09 技能、上下文与会话 | 偏好中编辑技能名称、描述、指令和作用范围；文内侧栏可展开同一会话；聊天菜单管理历史。 | 全局界面限 web/桌面。手机清除过的聊天仍可能出现在网页历史；清除与删除须区分。 | E-RDD-02 |
| F-M015-10 临时解释或存入笔记 | 移动端选词 → Quick Lookup → Define/Lookup/Translate；后两者分别解释上下文和翻译。 | 需联网及开启 Ghostreader；默认只展示。Copy 复制结果，Highlight 存选区并把结果附为笔记；点外部关闭不保存。 | E-RDD-03 |
| F-M015-11 导出不同数据对象 | Notebook → Export 输出笔记；账户 Export account data 分别选择 CSV、OPML、完整文件/文章 ZIP。 | 完整包异步生成后邮件给下载地址；CSV、订阅清单和内容包不能互相替代。未证明可重建全部账户状态。 | E-RDD-05 |
| F-M015-12 建立可重复使用的筛选 | web Manage views → Add → 输入查询 → Filter → Save 并命名；移动端 Search 的 `+` → Show → Save。 | RSS 文件夹另在 Feed 添加，选择订阅源后确认；标签也分文档和高亮两类。过滤语法错误与重命名冲突未核。 | E-RDD-06 |

## 功能目录与覆盖

以官方文档导航和已打开章节为目录依据；下表不是全量设置清单，也不将导航标题当成已体验。

| 功能域 | 已登记任务 | 仍未深入的主要内容 |
| --- | --- | --- |
| 收集与订阅 | F-M015-01、F-M015-12 | 邮件订阅、批量迁移、浏览器保存失败、解析修复完整链 |
| 阅读与定位 | F-M015-02、F-M015-05 | EPUB/PDF/视频各自导航，版式、手势、快捷动作冲突 |
| 批注与组织 | F-M015-03、F-M015-12 | 批量修改、标签合并、锚点修复、Action Tags |
| 查找与离线 | F-M015-04、F-M015-07 | 容量不足、索引延迟、多人/多端冲突、后台下载被杀 |
| AI | F-M015-08～F-M015-10 | 全部工具清单、长任务部分成功、撤回、额度耗尽、错误引用修正 |
| 数据与集成 | F-M015-06、F-M015-11 | 导出包重新导入，第三方笔记应用各自同步/删除，账号退出保留 |
| 阅读节奏与设备适配 | 文档导航有 Daily Digest、E-ink、Appearance | 尚无足够流程；不以存在页面推断推荐效果、刷新体验 |

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

新增证据均为2026-10-26实际打开正文，未观看图片或连续演示，未登录操作。第三方集成、全部同步/删除规则仍未穷尽，不能用已补任务数表示产品完成率。
