# M-020 Anki：资料制卡、学习调度与数据恢复

查阅：2026-10-26。范围：Anki **桌面端**现行手册；不把流程套用到 AnkiDroid/AnkiMobile。未固定桌面构建、调度参数与插件。**D2 文档重建；无实际操作、无连续视觉观察**。

本轮由官方文档索引定位 Desktop Manual，并下载对应官方 Markdown 正文。网页读取器对部分页面返回壳或不支持 text/markdown，改用公开 HTTP 只读获取，逐文件记录状态及 SHA-256；不是执行文档中的程序。文档混有最低版本、当前规则和历史选项，下面保留相应条件。没有因桌面支持某项就认定 Plus 可运行 Anki。

## 功能目录与覆盖

目录基线为 E-AND-01 的 Desktop Manual、Card Templates、Importing；移动端、开发者文档是另一个范围。表中引用任务仅覆盖其具体步骤，未核定每个设置项的全部组合。

| 功能域 | 已关联任务/材料 | 仍未闭合的主要部分 |
| --- | --- | --- |
| 档案与账户 | F-M020-03、F-M020-20；Profiles、Syncing、Backups | 登录/退出所有分支，多人同时使用与跨平台恢复 |
| 导入与共享包更新 | F-M020-04、F-M020-05；Importing 两章 | 编码错误提示、半导入回退、同名媒体冲突、全量结果日志 |
| 笔记、字段与标签 | F-M020-06、F-M020-10；Adding/Editing、Browsing | 所有字段属性、输入法、批量修改后的撤销边界 |
| 卡片模板与生成 | F-M020-07～F-M020-09；Templates、Editing | CSS/JS 全部能力、模板错误恢复、不同客户端渲染一致性 |
| 日常回忆与评分 | F-M020-01、F-M020-02；Studying | 学习中误评分撤销的完整链、退出时保存粒度 |
| 每日负荷与调度 | F-M020-11、F-M020-13、F-M020-14；Deck Options | 子牌组/预设组合、迁移后队列、实验参数实际效果 |
| 临时练习与困难材料 | F-M020-12、F-M020-16；Filtered Decks、Leeches | 重复额外练习的历史记录、取消后提示、逐题失败恢复 |
| 自动推进、声音与专注 | F-M020-15、F-M020-22；Deck Options、Preferences | 自动动作每种配置、语音与手动答题冲突、暂停续接 |
| 搜索与组织 | F-M020-10；Browsing、Searching | 搜索语言全部操作符、排序组合与大库性能 |
| 统计与可解释历史 | F-M020-17；Stats | 每张图完整交互、跨时区日界、人工改期后的全部指标 |
| 媒体与文件 | F-M020-18；Media；Files 目录 | 各平台格式、删除保留期、文件损坏及修复结果 |
| 导出与恢复 | F-M020-03、F-M020-19；Exporting、Backups | 导出中断、旧客户端恢复、插件配置是否在包内 |
| 扩展与故障定位 | F-M020-21；Add-ons、Troubleshooting | 每个插件兼容与数据迁移、卸载后残留、更新完整链 |
| 专题与运维 | 索引另列 Math、平台安装、自建同步、开发者文档 | 尚未逐功能分析；不是已证明不支持，也未部署服务器 |

## 流程与状态

| 流程 | 用户操作与反馈 | 数据/结束 | 证据 |
| --- | --- | --- | --- |
| F-M020-01 开始一轮并自评 | 选牌组查看新卡/学习中/复习计数 → Study Now → 尝试回忆 → Show Answer → Again/Hard/Good/Easy；按钮显示预计下次间隔。 | Again 表示未答出，Hard 表示答出但困难。评分影响调度；可用的当日卡片结束后显示完成。自评并非自动判定掌握。 | E-ANK-01 |
| F-M020-02 暂缓、停用与删除 | 学习中进入 More：Bury 暂埋；Suspend 停用；Delete Note 删除笔记。 | 暂埋通常次日恢复或手动恢复；停用要手动解除。删除笔记影响由该笔记生成的相关卡片。三者不能统称“跳过”。 | E-ANK-01 |
| F-M020-03 同步与恢复 | 首次 Sync 登录后可遇到上传本地/下载远端选择；恢复备份从切换档案界面 Open Backup 进入。要带媒体完整导出，选择 .colpkg 并包含媒体。 | 普通同步合并学习/笔记变动；有些结构变化需单向同步。自动备份含文本和调度、不含图片音频；恢复会丢失备份之后的修改。 | E-ANK-02、03 |

## 深化任务：从资料进入到持续学习

| 流程 | 前置、入口与操作 | 可确认的数据结果和分支 | 证据 |
| --- | --- | --- | --- |
| F-M020-04 文本题库导入 | UTF-8 分隔文本 → File / Import → 核对分隔符预览、字段/标签映射和目标牌组。 | 首字段与匹配范围决定更新/忽略/新增；更新原笔记保留调度及原牌组。缺列按空值，多列可能不导入；不能只核文件行数。 | E-AND-02、03 |
| F-M020-05 导入新版共享牌组 | Import 选 .apkg；23.10+ 可选择是否带入学习进度，以及更新既有笔记/类型的策略。 | 默认较新内容更新；可覆盖或不更新。类型合并会涉及完整同步；不要把作者的学习历史当成自己的记录。 | E-AND-02、04 |
| F-M020-06 制作笔记及派生卡 | Add → 选择笔记类型/牌组 → 填字段与标签 → Add；历史入口找回刚添加内容。 | 添加的是笔记，再按模板生成卡；首字段重复检查受笔记类型限制。 | E-AND-05 |
| F-M020-07 改模板与处理空卡 | 编辑页 Cards → 正面/背面/样式；右侧预览。清空生成条件后，用 Tools / Empty Cards 检查再删除空卡。 | 模板作用于同类型材料；删除字段内容不会立即删除原卡。Deck Override 可改变生成目的地。 | E-AND-06、07 |
| F-M020-08 制作遮挡练习 | Cloze 类型中选文本点遮挡；图片遮挡（23.10+）从 Add / Type 选 Image Occlusion，载图、画区域、选模式、Add。 | 同编号文字共用一张卡；图片可全遮猜一处或只遮一处。删形状后仍需 Empty Cards 清理对应卡。 | E-AND-05、07 |
| F-M020-09 输入答案并自评 | 选 Basic (type in the answer)，或模板添加类型比较；学习时输入，Enter/显示答案后比较差异。 | 差异展示不替代自评；只支持单行、每卡一次比较。预览框和 AnkiWeb 不显示该输入框。 | E-AND-08 |
| F-M020-10 检索后批量整理 | Browse 中切换卡片/笔记模式、输入查询、选记录；Notes 菜单改标签/类型，Cards 菜单移牌组。 | 操作对象不同；删除笔记连带卡片。变更笔记类型不改变调度，但涉及单向同步。Edit / Undo 有最近操作撤销入口。 | E-AND-09、10 |
| F-M020-11 重排或重新开始 | 浏览器选卡 → Cards / Set Due Date、Reset 或 Reposition。 | 改到期、回新卡队列、改新卡顺序是三种动作；Reset 保留复习历史，2.1.50+另有计数器选项。 | E-AND-09 |
| F-M020-12 额外练一组 | 牌组 Custom Study，或 Tools / Create Filtered Deck → 查询/数量/顺序 → Build；Empty/删除过滤组让卡回原组。 | 默认会重排复习；关闭 Reschedule 才是预览。暂停、暂埋或已在另一过滤组的卡不会被纳入；删除过滤组不删卡。 | E-AND-11 |
| F-M020-13 调整每日任务 | 牌组 Options 中选择预设、当前牌组或仅今天的限制。 | 共享预设会影响其他牌组；达到上限可能仍有到期卡未展示。 | E-AND-12 |
| F-M020-14 启用及调整 FSRS | 所有使用客户端须兼容；Deck Options 启用 FSRS → Optimize → 选择目标保持率。 | 启用是全局的；默认改变参数不立即重排。开启重排才改变现有到期日并可能增加当日任务。 | E-AND-12 |
| F-M020-15 自动推进 | 23.12+先设置问题/答案非零停留时间，再从学习页 More / Auto Advance 启动。 | 显示计时、记录用时与自动动作分别配置；停止/重进及与语音争用仍缺操作证据。 | E-AND-12 |
| F-M020-16 处理反复遗忘材料 | 复习卡多次失败达到 leech 阈值后产生标签，默认还会停用；浏览器找该标签，编辑材料后可解除停用。 | 文档默认阈值8次；只算复习阶段失败，阈值与停用策略可改。自动标记不等于已诊断遗忘原因。 | E-AND-13、09 |
| F-M020-17 查看一题或整体历史 | 学习页 I / Cards / Info 看卡片历史；主界面 Stats 选择牌组、全集或搜索范围与时间段。 | 一题多次回答可算多次 review；删笔记后的历史仍计入全集统计，但不计入原牌组。人工改期记录与正常评分不同。 | E-AND-14 |
| F-M020-18 检查缺失附件 | Tools / Check Media → 查看未引用文件、缺失引用 → 标记缺失笔记或清理闲置文件；工具另有恢复入口。 | 不扫描模板；手工加入媒体后还需核文件名。正文对回收位置的说法需按版本复核，未证明保留期限。 | E-AND-15 |
| F-M020-19 导出并理解恢复范围 | File / Export → 选文本、.apkg 或 .colpkg，分别核媒体、调度和兼容选项。 | .apkg 导入合并，.colpkg 导入替换当前卡片；已有媒体不会随集合替换自动清除。导出成功不证明可完整恢复所有设置。 | E-AND-16 |
| F-M020-20 切换家庭成员档案 | File / Switch Profile → 新建/命名/打开档案；各档案关联各自 AnkiWeb 账户。 | 学习集合与程序设置分档案，插件共享；多个档案连同一云账户会覆盖数据。不能把档案切换当成同账户多成员同步。 | E-AND-17 |
| F-M020-21 扩展故障与安全启动 | Tools / Add-ons 找扩展；故障时按 Shift 启动至安全模式提示，再禁用扩展排查；Tools / Check Database 检查集合。 | 扩展可随主程序升级失效；文档没有保证删除扩展后恢复其全部数据影响。 | E-AND-18、19 |
| F-M020-22 调整专注与节奏 | Windows/Linux 从 Tools / Preferences，Mac 从 Anki / Preferences；设置界面简化、减少动效、Timebox、回答时中断音频等。 | Timebox 周期显示学习量，不是已证实的强制停课；关闭添加窗口未添卡时，不保存刚选的新默认牌组。 | E-AND-20 |

这些任务均只重建官方可定位步骤；错误弹窗、连续时序、触控操作和全部撤销均未实测。D2 不是全部分支完成标记。

## 分支与恢复边界

- 已在两端产生不同资料，首次单向同步不能不看方向就确认；文档另有合并处理。媒体同步规则独立于集合的单向替换。
- 两端复习同一卡片时，手册区分保留的复习历史与最后决定调度的状态，不能简单说“所有冲突自动无损解决”。
- 打开自动备份后，自动同步/备份暂时关闭供检查，之后关闭再打开才恢复。恢复流程需要检查内容，而不只是出现“完成”提示。
- 答错后误按 Good 如何撤销、连续多步撤销范围、后台退出时保存粒度，本轮未核实。不能因为软件通常有 Undo 就补写其效果。
- 完成一天的计划只代表当前调度任务结束，不能直接代表词汇或数学能力达标。
- 新旧包、文本导入、集合恢复的覆盖规则不同；不能把所有“导入”当作只新增不修改。AI 更新题库时需要查已有对象和导入策略，不能只返回新增文件名。
- “输入答案”和“自动判分”并不等价；此处仍要求使用者自评。若用于儿童或考试训练，本项目是否增加自动评分需要另外研究，不能借 Anki 的输入框作依据。
- 现行 Deck Options 简短教程仍提到 CMRR，但同页专节明确其于25.07移除；不能把旧计算入口放进当前操作链。新 Help Me Decide 标为实验功能，且当前安装构建未知。本例不推荐具体参数或承诺保持率。
- Check Media 文档同时写工具内恢复/清空入口和操作系统回收站；本轮不裁定真实目录或清理期限。源代码或固定构建实操才能进一步确认。

## 对本项目的意义（推断）

关联 G-001、G-002、R-016、H-004/005：内容对象、生成的练习、一次作答、长期调度要分别理解；AI 执行“今天先别练这个”时，应能区分一次跳过、暂缓、永久停用和删除。服务器同步与可回滚备份必须分别研究，媒体不能被漏算。这里未确定使用 Anki 的调度算法或文件格式。

对成人的启发是把临时加练、长期计划与今天的可用时间分开；对5–6岁儿童，桌面的预设/模板/过滤语法不宜直接变成日常学习入口，此为界面研究建议。资料→笔记→多张卡→每次作答→计划→统计的对象关系，使 AI 可以解释“内容更新了但学习记录保留了”“题没出现是被暂埋了”；实际采用前仍需定义本系统自己的数据规则。

## 后续观察脚本

准备隔离的测试档案与含图片/音频的三张卡：观察各评分后的队列、误评恢复、暂埋后跨日、停用后重开；分别恢复自动备份与含媒体导出文件。两端冲突只在测试数据上演练，记录覆盖警告、实际损失和返回步骤。尚未执行。

## 证据

- E-ANK-01：[Studying](https://docs.ankiweb.net/manual/studying)，牌组学习、评分与 More 操作。
- E-ANK-02：[Syncing](https://docs.ankiweb.net/manual/syncing)，首次同步、合并、单向同步与媒体。
- E-ANK-03：[Backups](https://docs.ankiweb.net/manual/backups)，自动备份、恢复与含媒体导出。

## 本轮新增证据

全部查阅2026-10-26；以下官方 Markdown 通过公开 HTTP 获取正文，不是只有搜索摘要。原件及状态/哈希在临时 `work/anki-deepening/fetch-log.json`，正式库留章节和短摘记，不归档整套手册。图片链接及演示视频未观看。

| E 编号 | 官方来源 | 章节定位 |
| --- | --- | --- |
| E-AND-01 | [文档索引](https://docs.ankiweb.net/llms.txt) | Desktop Manual、Card Templates、Importing；范围目录 |
| E-AND-02 | [Importing](https://docs.ankiweb.net/manual/importing/intro.md) | File / Import 与文件类型 |
| E-AND-03 | [Text Files](https://docs.ankiweb.net/manual/importing/text-files.md) | 格式、字段映射、Duplicates and Updating、File Headers |
| E-AND-04 | [Packaged Decks](https://docs.ankiweb.net/manual/importing/packaged-decks.md) | Scheduling、Updating、23.10+ |
| E-AND-05 | [Adding/Editing](https://docs.ankiweb.net/manual/editing.md) | Adding、Duplicate Check、Cloze、Image Occlusion |
| E-AND-06 | [Card Templates](https://docs.ankiweb.net/manual/templates/intro.md) | Templates Screen、Deck Override |
| E-AND-07 | [Card Generation](https://docs.ankiweb.net/manual/templates/generation.md) | Reverse、Generation and Deletion、条件生成 |
| E-AND-08 | [Field Replacements](https://docs.ankiweb.net/manual/templates/fields.md) | Checking Your Answer，输入比较及限制 |
| E-AND-09 | [Browsing](https://docs.ankiweb.net/manual/browsing.md) | Table Modes、Menus and Actions、Find and Replace |
| E-AND-10 | [Searching](https://docs.ankiweb.net/manual/searching.md) | 组合查询、字段/标签/状态、添加/修改时间 |
| E-AND-11 | [Filtered Decks](https://docs.ankiweb.net/manual/filtered-decks.md) | Custom Study、Home Decks、Creating Manually、Rescheduling |
| E-AND-12 | [Deck Options](https://docs.ankiweb.net/manual/deck-options.md) | Presets、Daily Limits、Timers、Auto Advance、FSRS；同页保留历史选项 |
| E-AND-13 | [Leeches](https://docs.ankiweb.net/manual/leeches.md) | 失败计数、标签/停用、编辑或等待 |
| E-AND-14 | [Stats](https://docs.ankiweb.net/manual/stats.md) | Card Info、选择统计范围、Today；删除历史口径 |
| E-AND-15 | [Media](https://docs.ankiweb.net/manual/media.md) | Checking Media、Manually Adding Media；回收位置用词需核版本 |
| E-AND-16 | [Exporting](https://docs.ankiweb.net/manual/exporting.md) | File / Export；文本、.apkg、.colpkg 及媒体/调度选项 |
| E-AND-17 | [Profiles](https://docs.ankiweb.net/manual/profiles.md) | Switch Profile、账户对应、跨档案插件共享 |
| E-AND-18 | [Add-ons](https://docs.ankiweb.net/manual/addons.md) | 管理入口、删除、升级兼容边界 |
| E-AND-19 | [Troubleshooting](https://docs.ankiweb.net/manual/troubleshooting.md) | 安全启动、版本检查、Check Database；周期保存不保证零丢失 |
| E-AND-20 | [Preferences](https://docs.ankiweb.net/manual/preferences.md) | Distractions、Scheduler、Review、Editing、Syncing |

下一步仍需固定构建核查误评分撤销、自动推进的退出与持久状态、模板清理及媒体恢复，并扩展移动端平台差异。上述目录与22项任务使研究更完整，但不能声称每种模板、插件、设置组合和异常均已完成。
