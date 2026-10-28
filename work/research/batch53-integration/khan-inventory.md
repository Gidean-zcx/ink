# Batch 53：M-030 Khan Academy Kids 本地材料盘点

日期：2026-10-27。仅核对现有案例、coverage、功能目录和本地材料；未联网，未重试已知 403 路线，未操作 App，未改正式文件。唯一写入是本报告。

## 当前覆盖状态

- 当前案例为 `docs/research/interaction-library/cases/M-030-khan-kids.md`，正式 `coverage.csv` 有 F-M030-01～16，共 16 条，全部 D2、`hands_on=false`。
- 原八栏目目录 `khan-controls-deepening-inventory.csv` 只登记 Books、Videos、Create、Offline、Letters、Reading、Math、Logic+；八个栏目不是八个完整交互。
- 扩展目录 `khan-library-controls-deepening-inventory.csv` 有 130 行，其中 112 条帮助入口含重复列示，另有 18 个正文支持的功能节点。目录行不能直接升级为任务。
- `catalog.csv` 的 M-030 行仍是 `indexed_on=2026-10-26`，而案例和覆盖已在 2026-10-27 扩到作品、账户、家长控制、声音、教师报告及成人纸面激励。其 `next_gap` 对阅读/提示仍有效，但作为当前完整摘要已过时。

## 六类目标的证据状态

| 目标 | 正文已读但缺实际操作 | 仍未知 |
| --- | --- | --- |
| 儿童自主选书/听读 | Books 有 Read to Me / Read by Myself；Reading 有提问、点图和部分选择题；Early Readers“先听后读”是成人建议。证据见 E-KHK-01、E-KHCD-03、E-KLCD-11，F01。 | 同一本书模式切换、音频暂停、逐字点读、退出回位、收藏书籍均无操作证据。“收藏”没有独立官方帮助正文或既有任务。 |
| 收藏/录音/画作 | Create 正文已拆到背景、贴纸、绘制、撤销、录音、停止、回放、绿色勾保存 Gallery；作品导出另有 Enable export 与逐件箭头。见 F07/F08、E-KHCD-01/02/03。 | 未保存退出、录音权限拒绝、中断来电、旧作品再编辑、Gallery 收藏/排序、同步与重装恢复、导出音频/动画格式未知。没有真正未读且直接讲这些控件的已知文章。 |
| 奖励 | 设计正文和抽样帧支持首次正确粒子、累计成功后角色礼物及角色房间入口；F05。纸面贴纸/证书是成人活动 F15，不是 App 自动奖励。 | 错后答对是否奖励、阈值、重复刷取、领奖关闭、礼物收藏与删除未知。角色介绍文章只见目录。 |
| 家长报告 | 家庭 Library 彩色勾号与教师 Class Reports 必须分开。教师正文已读：Assignments/All Progress、展开多次尝试、Email 导出 ZIP/HTML；F02/F13。 | 家庭版详细报告、计算口径、发送失败、权限、同步冲突与回导未知。Module 4 仍只见目录。 |
| 离线 | 正文已读：Kodi’s Suitcase 与 Create 可离线，首次建账号需联网；F03。重装帮助和中文指南只给进度保留声明，未实测。 | 资源准备/更新/删除、离线书籍范围、下载失败、离线新进度何时上传、冲突、作品恢复未知。网络错误文章只见目录且曾遇栏目 GET 403。 |
| 中断恢复 | Learning Path 文档明确中断后回 App 按 Play 开始下一课；F06。这不是原题恢复。 | 后台、强退、断网、重启、跨设备、Books/Library/Create 的恢复单位，以及未完成课如何再现未知。重启设备文章只见目录。 |

## 最多六个真正未读的官方入口/章节

以下均没有正文核读记录。能给 article URL 的直接给 URL；目录未展开 article URL 的，给确切 section URL、网页行和链接编号。已知 403 项仅记录，不建议重复普通 GET。

1. **Three ways to play / 教师视频入口**：`https://sites.google.com/khanacademy.org/khanacademykids/getting-students-into-the-app/three-ways-to-play`。案例来源定位 E-KHD-04 明确“仍未观看”。可核 Learning Path、Library、Assignments 三种进入方式及儿童自主选择边界。
2. **Module 4: Viewing student progress**：`https://khankids.zendesk.com/hc/en-us/sections/360008453772-Class-and-Teacher-Resources`，本地 `khan-library-controls-deepening-inventory.csv` KLC-I041，`web L18, link 12`。状态 `index_only_section_fetch_blocked`；正文未读、article URL 未解析，普通 GET 403 路线不得重试。
3. **✨ NEW: Teacher Dashboard on the web**：同一教师 section，KLC-I034，`web L11, link 5`。状态 `index_only_section_fetch_blocked`。可补报告入口、跨网页/App 状态和中断恢复，但不能从标题推断功能。
4. **How do I resolve the internet connection error?**：`https://khankids.zendesk.com/hc/en-us/sections/360001481891-Home-and-Parent-Resources`，KLC-I018，`web L25, link 19`。状态 `index_only_section_fetch_blocked`；正文与 article URL 未取得。与离线/同步失败直接相关。
5. **How do I restart my device?**：`https://khankids.zendesk.com/hc/en-us/sections/360001481891-Home-and-Parent-Resources?page=2`，KLC-I105，`web L12, link 4`。状态 `index_only`。可核故障恢复建议，但不能代替 App 内断点证据。
6. **Learn more about the characters inside Khan Academy Kids**：`https://khankids.zendesk.com/hc/en-us/sections/46978591932315-News-and-Announcements`，KLC-I090，`web L13, link 9`。状态 `index_only`。只可能补角色房间/奖励对象背景；若正文没有操作步骤，应保持背景资料，不新增流程。

未将 Spanish books、作品展、手工周等标题列入六项：它们主要是内容或活动入口，不能直接闭合自主读听、Gallery 保存或 App 奖励交互。也未把 77 个 `index_only` 全部称为全局未读。

## 本地材料定位

- 正式案例：`docs/research/interaction-library/cases/M-030-khan-kids.md`
- 正式覆盖：`docs/research/interaction-library/coverage.csv` 的 16 条 M-030 行
- 八栏目目录：`docs/research/interaction-library/batches/khan-controls-deepening-inventory.csv`
- 130 行帮助/控件台账：`docs/research/interaction-library/batches/khan-library-controls-deepening-inventory.csv`
- 已保存正文：`work/khan-library-controls-deepening/text/articles-*.txt`
- 已保存目录：`work/khan-library-controls-deepening/text/sections-*.txt`
- 403 范围记录：`docs/research/interaction-library/artifacts/khan-remaining-source-attempt/access-audit.json` 与 `source-notes.md`
- 阅读/奖励设计原 HTML：`work/khan-controls-deepening/11-design.html`
- 三份成人活动 PDF：`work/research/batch37-khan/pdf/goals.pdf`、`tracker.pdf`、`certificate.pdf`；均已读完七页或相应单页，不是未读候选。

## 执行记录

所有命令均通过 `python3` 执行，退出码均为 0：

1. 读取 M-030 案例及 `catalog.csv` 对应行。
2. 枚举路径含 `khan`、`khcd`、`klcd`、`khat` 的本地文件及大小。
3. 读取两个目录 CSV，按 book/read/listen/audio/record/create/gallery/art/reward/progress/report/offline/interrupt/resume 等词筛选。
4. 读取正式 `coverage.csv` 的 16 条 M-030 行，并筛选 `index_only` / `index_only_section_fetch_blocked` 的相关入口。
5. 写入并校验本报告。

未知：六个未读入口中只有 E-KHD-04 保留直接 URL；其余五项当前台账只保留 section URL 与链接编号，没有 article URL。禁止联网条件下不能可靠补出 article ID。所有六类功能均无本项目实机操作证据。

状态变化：只新建 `work/research/batch53-integration/khan-inventory.md`；正式案例、coverage、目录、证据与已有材料均未修改。
