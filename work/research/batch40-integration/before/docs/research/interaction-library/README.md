# 竞品交互资料库

建立：2026-10-26。阶段：竞品研究。服务 G-001 信息/学习收益、G-002 全局 AI 能力；不构成本产品已认可的交互或系统设计。

- [第三十九期：AI预览与数据传递](/home/workspace/reader/docs/briefs/039-ai-preview-and-transfer.md)：历史AiPaper的事件/邮件确认、第三方云周期和人工跨设备传递；[交互依据表](reference-points.csv)新增按条件/结果检索的16条示例，非全库完成表。
- [第三十八期：读听切换与恢复](/home/workspace/reader/docs/briefs/038-reading-audio-and-recovery.md)：样刊网页实际缩放、听读、关闭和重启，成功与未取得结果分别保留。
- [第三十七期：成人引导与系统结果](/home/workspace/reader/docs/briefs/037-adult-guidance-and-system-controls.md)：区分纸面激励、教师任务和App反馈；补Kompakt闹钟/地图/截图等操作与结果边界。
- [第三十六期：控件与状态依据](/home/workspace/reader/docs/briefs/036-controls-and-state-evidence.md)：Readwise官方渲染器合成样本、X2剩余手册任务及墨案历史控件；区分按钮、状态和完成证据。
- [第三十五期：AI工作流与阅读证据](/home/workspace/reader/docs/briefs/035-ai-workflows-and-reading-evidence.md)：外部AI逐篇筛选、订阅批次、测验与画像；小猿历史订正/听书链，保留版本和运行证据缺口。
- [第三十四期：内容对象与学习材料](/home/workspace/reader/docs/briefs/034-content-objects-and-study-materials.md)：Anki批量编辑的影响范围、公式准备、题库修订分发与自托管同步；补读原目录剩余30页。
- [第三十三期：家庭听书与语音控制](/home/workspace/reader/docs/briefs/033-family-audio-and-voice-controls.md)：儿童档案与位置、前台语音范围、RSS导入与绑卡、取盘后的后台下载。

## 先说明当前研究做到了哪里

用户指出此前研究偏表面，要求深入功能交互，并留下正式设计时可反复查阅的依据。这个判断成立：原有主矩阵主要是产品和功能发现，混合了宣传介绍、帮助文档、API/手册与少量论文。**没有逐一体验全部产品，也没有完整拆解每个功能。** 43 组名称不是 43 个完整交互研究案例；来源链接数量也不是研究深度。

资料库把产品索引与后续专题统一登记，维护任务流程、覆盖台账、证据记录和横向比较。首批七个案例之后，已继续扩展阅读系统、中文背词、电子词典、儿童互动、音频、新闻和系统 AI。每个案例列功能域、已知路径、异常/恢复与未完成项。未研究的功能显示缺口，不写成“不支持”。

**当前准确数量与逐产品入口统一见 [案例与覆盖索引](case-index.md)**，由已登记批次生成；不把正在编写而未登记证据的草稿计为入库案例。52 组是原主目录基线，此前已点名的 13 组扩展对象现登记为 M-053～M-065，范围详见 [目标与覆盖审计](analysis-status.md)。完整功能总数未知，所有案例仍是部分覆盖。第六期简报中的 7 案例/19 流程/20 来源保留为历史批次，不代表当前总量。

当前没有手机/电子纸实机或已登录的原生App操作环境；文档、源码和静态图研究之外，已通过临时浏览器操作The Week Junior公开样刊、驾考宝典公开练习／模拟页、TIME for Kids单篇听读／搜索、Duolingo匿名法语课程、Tolino公开图书馆选择器及Britannica公开Community Helpers活动。环境、动作日志、截图及失败见[样刊审计](artifacts/week-junior-controls-deepening/browser-operation-audit.md)、[练习审计](artifacts/jiakao-controls-deepening/browser-operation-audit.md)、[听读审计](artifacts/time-controls-deepening/browser-operation-audit.md)、[课程审计](artifacts/duolingo-controls-deepening/browser-operation-audit.md)、[选馆审计](artifacts/tolino-controls-deepening/browser-operation-audit.md)及[儿童互动审计](artifacts/britannica-controls-deepening/browser-operation-audit.md)。数量统一见覆盖索引；D4包括尝试后未完成的分支，不是通过率。PDF解析库与研究浏览器均装在work/，没有创建产品工具链；这些网页结果不能证明触摸手感、时延、刷新残影或原生App行为。

第33批见[第三十三期：家庭听书与语音控制](/home/workspace/reader/docs/briefs/033-family-audio-and-voice-controls.md)：Audible补儿童档案、语音、内容关联及不同播放端；Yoto补RSS/音频流绑定，Tonie补对象与后台下载。新增内容全是文档依据。上一批[学习游戏与完成结果](/home/workspace/reader/docs/briefs/032-learning-games-and-completion.md)保留[Duolingo Chess匿名课实际操作](artifacts/duolingo-chess-interactions/browser-operation-audit.md)，目标设置保存仍未证；全部案例仍部分覆盖。

最新[第三十四期：内容对象与学习材料](/home/workspace/reader/docs/briefs/034-content-objects-and-study-materials.md)补Anki原目录剩余30页，新增公式、分享和自托管同步任务，并深化批量修改范围。只有文档依据；55页正文读取完成不代表全部交互已验证。

## 从哪里查

| 入口 | 用途 |
| --- | --- |
| [按设计问题查证据](design-reference-guide.md) | 读听、学习、新闻、AI动作、恢复与显示的使用入口；说明怎样引用F/E及判断证据是否足够。 |
| [案例与覆盖索引](case-index.md) | 所有已入库案例、每组任务数与深度；保留尚无案例的对象。 |
| [产品目录 catalog.csv](/home/workspace/reader/docs/research/interaction-library/catalog.csv) | 保留 M-001～M-043，给后续专题产品补编号；记录类别、旧资料、当前案例和下一项缺口。产品系列仍需按型号/平台/版本拆分。 |
| [任务覆盖 coverage.csv](/home/workspace/reader/docs/research/interaction-library/coverage.csv) | 看具体任务哪些已有文档依据，哪些缺图像、实测或异常结果；不是全产品完成率。 |
| [逐条功能与帮助目录](inventories/README.md) | 对能取得完整入口列表的资料逐项登记，分开“发现入口”与“研究了操作”；尚未取得的目录仍标未知。 |
| [证据索引 evidence.jsonl](/home/workspace/reader/docs/research/interaction-library/evidence.jsonl) | 用稳定 E 编号查来源 URL、章节、查阅时间、平台/版本范围、获取方式与简短摘记。 |
| [横向比较 comparison.md](/home/workspace/reader/docs/research/interaction-library/comparison.md) | 按学习、读听、新闻、AI执行、数据恢复、内容关联、系统交互七个问题域反查案例。 |
| [待核查清单 backlog.md](/home/workspace/reader/docs/research/interaction-library/backlog.md) | 逐产品的后续范围、未解决冲突、实测脚本与材料缺口。 |
| [记录模板 template.md](/home/workspace/reader/docs/research/interaction-library/template.md) | 后续新增产品、功能与实测时采用的统一字段。 |
| [字段与读取约定](data-dictionary.md) | F/E、版本、路径、观察/操作字段及当前校验范围；说明自由描述字段和未自动检查的内容。 |
| [批次与路径约定](batches/README.md) | 分批取证如何合并、图片如何长期保存，以及主索引的字段和路径含义。 |

最初七个案例仍保留，新增案例从完整索引进入：

- [M-002 Kobo Libra Colour：内容、阅读、手写、听书与恢复](/home/workspace/reader/docs/research/interaction-library/cases/M-002-kobo.md)
- [M-015 Readwise Reader：订阅、读听与定位](/home/workspace/reader/docs/research/interaction-library/cases/M-015-readwise.md)
- [M-020 Anki：资料制卡、学习调度与数据恢复](/home/workspace/reader/docs/research/interaction-library/cases/M-020-anki.md)
- [M-030 Khan Kids：阅读入口、进阶与中断](/home/workspace/reader/docs/research/interaction-library/cases/M-030-khan-kids.md)
- [M-035 HiBy：全局快捷操作与播放](/home/workspace/reader/docs/research/interaction-library/cases/M-035-hiby.md)
- [M-037 Yoto：离线准备与睡眠计时](/home/workspace/reader/docs/research/interaction-library/cases/M-037-yoto.md)
- [M-044 Inoreader：筛选、去重与规则](/home/workspace/reader/docs/research/interaction-library/cases/M-044-inoreader.md)

此前续补[reMarkable的日历/分享副本与恢复](cases/M-005-remarkable.md)、[欧路的取词/索引/解释页](cases/M-026-eudic.md)、[Sony的播放范围/介质/输出状态](cases/M-036-sony.md)，见[第十二期简报](/home/workspace/reader/docs/briefs/012-data-objects-and-state-boundaries.md)。三项均保留文档条件及实际操作缺口。

此前续补[Duolingo主学习应用与实际网页任务](cases/M-031-duolingo-adventures.md)、[墨墨内容/API结果](cases/M-017-maimemo.md)、[Tonie游戏恢复和家庭权限](cases/M-038-tonie.md)，见[第十一期简报](/home/workspace/reader/docs/briefs/011-learning-actions-and-recovery.md)。

上一批新增[Inoreader的来源、新闻修订与规则试跑](cases/M-044-inoreader.md)、[Quizlet各模式的完成与恢复](cases/M-021-quizlet.md)、[Audible离线和读听条件](cases/M-039-audible.md)。同一任务补回原编号，帮助正文、官方单帧与实际操作分开；测试按钮、下载完成和同步声明不替代结果核查。

例如正式设计“听书时问 AI 后继续”，先看横向比较，再进入 Reader/Yoto/HiBy 案例，读取其 F 流程和 E 证据；最后查看缺口，确定还需实测什么。搜索关键词和编号即可跨文件定位，避免把同一事实复制进越来越多简报。

## 两个维度分别标注

**覆盖深度**描述研究粒度：D0 待收集；D1 功能发现；D2 任务、步骤与分支重建；D3 有连续视觉证据的流程分析；D4 有可复现的实际操作记录。深度按任务标记，不因一个流程达到 D2 就把整台设备写成 D2 完成。

**证据取得方式**另记：官方宣传、官方操作文档、官方索引文本、视频/截图观察、源码核查、亲自操作、第三方报告。源码不能证明视觉体验；演示不能证明全部错误路径；帮助页也不保证当前版本完全一致。本轮案例主要为 D2＋官方操作材料；早期Yoto及部分Kobo/Khan Kids条目采用官方搜索索引正文；Yoto本轮已取得部分公开HTML内嵌正文，作为additional_captures追加，保留最初取得方式。多数记录的hands_on仍为false。公开网页操作按具体任务记true，数量以覆盖索引为准，同时强制写明operation_method=automated_public_browser及审计路径；这表示代理驱动真实网页，不是人在目标硬件上的手感试用。

未知型号、版本、地区、套餐、内容来源须写“未明确”。产品发布时间、网页更新时间、查阅时间、实际测试版本四者分开。文档冲突保留两条证据与适用范围，不能挑一句最符合期待的话当结论。

## 每个功能研究到什么程度才够用

至少回答：用户目标与前置条件、入口、操作/反馈、状态与数据变化、成功结束、取消/返回、误操作恢复、空内容/失败/断网、中断后恢复、跨设备/更新、权限/付费边界。不同功能不适用的项要写理由，未查到则保留未知。

学习与游戏额外检查题目呈现、提示顺序、判分、失败重试、奖励触发、难度调整、重复刷奖励与长期进度。新闻额外检查选择依据、去重、已读含义、刷新/更正和视角覆盖。音频额外检查播放对象、时间/文字位置、队列、计时归属与音频打断。电子纸设备额外记录手势、按键、刷新反馈和不同显示模式；没有实测就不填响应毫秒数。

每个案例末尾的“本项目启发”必须明确属于推断，并列出不确定条件。AI 能读取/操作哪些对象也是本项目需求映射，不能倒推竞品已经提供该 API。

## 证据保存与长期维护

- 正式库保存短摘记、章节定位和出处，不大量搬运整本手册。直接引用原文时保持必要且短；更多内容回到原始来源。
- 原始网页抓取、解析与临时资料放在 work/。早期Yoto请求只得到页面壳，最初依据为索引文本；2026-10-27已从公开分类HTML取得选定文章的内嵌正文，并登记原件、文章定位及追加采集。不能仅凭HTTP 200或文件哈希判断读到正文。
- 获得可用截图时保留来源、设备/平台/版本、截图日期、截图区域与相关 F/E 编号。视频记录发布者、链接、时间段、所见动作和无法判断之处；没有观看就不填写时间戳。
- 已取得的静态图/手册摘页放在本目录 artifacts/，登记来源、页码和 SHA-256。原始整本手册或网页临时件留 work/；不能把临时文件存在误称为长期归档。静态图数不等于连续操作录像数。
- 对实际看过但没有必要整幅归档的营销图，保留正式观察笔记、原图 URL 与临时原件哈希；使用 `visual_audit_path` 标出记录，不能填成 `archived_artifact` 或声称原图已保存到正式库。
- 变化通过新证据记录 `supersedes`，旧记录保留；设计引用稳定 E/F 编号。受版本变化影响的结论，在实际采用前重新核对。
- catalog.csv 的 seed_topics 是早期功能线索；每个产品的完整功能数量目前未知。coverage.csv 只统计明确登记的研究任务，不宣称“全功能覆盖百分比”。

机器可读索引约定：CSV 为 UTF-8；多值 evidence_ids/source_urls 用分号分隔。catalog 的 legacy_document 相对项目根目录，case 路径相对本资料库。JSONL 每行一条证据；null 表示未知或没有取得，不表示该产品没有此能力。source_type 描述材料性质，access_mode 描述本轮怎样取得；app_build、firmware_version、source_updated_on 分开记录。index_reported_date 仅为搜索索引显示日期，不能替代正文核定的更新时间。

## 后续设计怎样引用

设计决策应能反查：目标/需求编号 → 比较的问题 → 产品的具体 F 流程 → 支持该行为的 E 证据 → 已知限制/反例 → 本项目的取舍 → 待验证指标。当前只整理研究依据与候选问题，不提前决定采用哪个流程、技术栈或界面。

前批入口：[第十五期简报](../../briefs/015-exploration-undo-and-stopping.md)。DragonBox、Anki和HiBy补任务结构、撤销/恢复和自动停止条件；同一任务补回原编号，本轮没有新增D4。[第十四期](../../briefs/014-news-learning-offline-evidence.md)记录新闻选择、家庭绑定与学习打卡的状态差异。

前批入口：[第十六期简报](../../briefs/016-visibility-editability-and-recovery.md)。Sony DPT、QUADERNO和MatePad Paper补文件副本、批注与录音位置、删除/同步/恢复及系统状态；官方目录与当前实机分开，来源日期和地区条件保留。

前批入口：[第十七期简报](../../briefs/017-action-targets-and-lifecycle.md)。联想Smart Paper、山灵M1 Plus和iBasso DX180补控件/目录、应用生命周期、计时与控制对象；通用App、型号设备和版本声明分别限定。

前批入口：[第十八期简报](../../briefs/018-reading-controls-and-source-completeness.md)。TIME补词卡与目录的实际网页操作，SR35补EQ/校准及逐条目录，有道2.0保留未能取得的学习闭环；详见[词卡操作审计](artifacts/time-vocabulary-deepening/browser-operation-audit.md)。

前批入口：[第十九期简报](../../briefs/019-reading-queues-and-ai-actions.md)。Reader补阅读队列、内容版本、分享与原生/外部AI范围；Kobo补合集、页书签及显示/音频控件；不背单词核版本措辞与入口证据。静态图和文档步骤不当实际执行。

前批入口：[第二十期简报](../../briefs/020-child-feedback-and-resumable-state.md)。Britannica公开活动补错答、回看、讲解、四题回顾与重载；微信读书分历史AI引用、Web帮助和Skill数据口径；Meebook M7补配置与型号日志。文档、有限实际操作和仍缺原件的范围分别保留。

前批入口：[第二十一期简报](../../briefs/021-reading-feedback-and-data-reset.md)。Espresso补历史完成链与机构控件；头条拆推荐控制、历史兴趣和个人副本；汉王N10 Pro二代补作者使用报告中的按键与入口。没有新增实操，历史版本、政策声明和设备未知分开。

前批入口：[第二十二期简报](../../briefs/022-ai-actions-and-resume-conditions.md)。Gemini补通信、Live与自定义接入的文档链；Palma 2补通用手册及固定型号材料的范围；掌阅区分手机与墨水屏版。目录、静态资料和运行结果分别标注，全部仍部分覆盖。

前批入口：[第二十三期简报](../../briefs/023-learning-completion-and-records.md)。百词斩补50题公开帮助及版本冲突，墨案补Plus短说明书/历史文件路径，Bigme分系统显示与阅读排版；实际动作结果仍待核。目录和状态差异补回原案例，不按新增行数计算完成率。

前批入口：[第二十四期简报](../../briefs/024-controls-scope-and-evidence.md)。抖音补两篇完整图像指南和功能图，海信补机内引导/设置照片，百词斩单词机核帮助型号范围。文档步骤、静态控件与实际结果仍分开；所有案例部分覆盖。

前批入口：[第二十五期简报](../../briefs/025-practice-feedback-and-recovery.md)。小猿手机软件13类历史练习逐项补控件/错误/结束；AiPaper旧手册逐页目录与编辑/恢复；小猿硬件资料/账号范围核查。静态图、正文报告与实际操作分开，全部案例仍部分覆盖。

前批入口：[第二十六期简报](../../briefs/026-audio-controls-and-research-reuse.md)。Kindle补声音/AI/家长限制与静态控件，Kompakt补核心App对象与保存，Khan补帮助目录/档案/音乐及报告。新增按设计问题检索入口；全部仍部分覆盖，没有新增产品实操。

前批入口：[第二十七期简报](../../briefs/027-learning-states-and-evidence-reuse.md)。讯飞四教程补抽样画面，PocketBook补应用/档案/维护，Supernote补笔记编辑与日历条件。抽帧仍非连续过程或产品实操；全部部分覆盖。

最新读听取证见[第三十八期简报](../../briefs/038-reading-audio-and-recovery.md)和[实际操作审计](artifacts/week-reader-recovery/browser-operation-audit.md)。在[设计参考指南](design-reference-guide.md)可按“手动保存、关窗口、关播放器、浏览器重启、空上下文”查具体证据。任务F、证据E和截图各有不同计数单位，当前数量及合成组件边界统一见[案例索引](case-index.md)。
