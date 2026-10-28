# Batch 52：M-021 Quizlet 本地材料盘点

日期：2026-10-27。仅核查当前案例、覆盖、目录、证据和已保存文本；未联网、未登录、未安装或操作 Quizlet，未修改正式资料。唯一写入是本报告。

## 当前对象与材料状态

- `docs/research/interaction-library/catalog.csv` 将 M-021 登记为 **Quizlet**，类别为“记忆卡片与资料生成练习”。
- 当前案例 `docs/research/interaction-library/cases/M-021-quizlet.md` 已有 F-M021-01～17；正式 `coverage.csv` 也是 17 条，全部 D2、`hands_on=false`、`not_observed`。
- 与本批三类问题直接相关的是 F01～13；F14～17 是游戏模式，不应因同属 Quizlet 而混进内容导入、个人进度或恢复任务。
- `quizlet-controls-deepening-inventory.csv` 有 68 行，是 2026-10-27 当时三个帮助目录的分层快照；父级、目录标题与已读文章混列，不能按 68 项生成 68 个产品任务。
- 本地没有官方 HTML、PDF 或 App 包。`quizlet-response-01.txt`～`05.txt` 是网页工具响应文本；`parsed/*.txt` 是从响应中拆出的正文/目录，不是源站 HTML，也没有图片观察。

## 已保存且已经核读的正文

以下主题已有本地文章正文，当前缺口来自正文没有给出完整分支，并非“尚未打开文章”：

| 主题 | 精确官方帮助链接 | 本地正文 | 已知与未知 |
| --- | --- | --- | --- |
| 文本导入 | `https://help.quizlet.com/hc/en-us/articles/360029977151-Creating-sets-by-importing-content` | `work/quizlet-controls-deepening/parsed/articles-360029977151.txt` | 已知分隔符、导入、语言与创建；空行、重复、异常分隔、失败回退未知。案例证据表写 en-AU，`source-map.json` 保存 en-US，同 article ID；是地区 URL 口径差异，不足以判断功能版本不同。 |
| 草稿恢复 | `https://help.quizlet.com/hc/en-us/articles/360029768912-Editing-draft-sets` | `parsed/articles-360029768912.txt` | 已知自动保存、In Progress、断网警告、登出前未发布草稿会丢失；强退、系统杀进程、设备重启后的确切保存时点未知。 |
| Study Guide | `https://help.quizlet.com/hc/en-us/articles/18312306436365-Studying-with-Study-Guides` | `parsed/articles-18312306436365.txt` | 已知生成和可见性边界；正文/生成题逐项校订、源更新、重生成覆盖、删除级联未知。 |
| Learn/Study path | `https://help.quizlet.com/hc/en-us/articles/360048314692-Setting-up-a-study-path` 与 `https://help.quizlet.com/hc/en-us/articles/360048806511-Resetting-study-path-progress` | `parsed/articles-360048314692.txt`、`parsed/articles-360048806511.txt` | 已知开始学习后才保存、重置清除 Learn 进度；普通 Learn 中断队列、重置撤销、与 Progress/SRS 的清除范围未知，Android 支持范围在两页之间不一致。 |
| 错题与误判 | `https://help.quizlet.com/hc/en-us/articles/360030990531-Studying-with-Write-mode`、`https://help.quizlet.com/hc/en-us/articles/360030645752-Studying-with-Spell-mode`、`https://help.quizlet.com/hc/en-us/articles/40011154960653-Studying-with-Answer-Streaks` | 前两篇有 `parsed/*.txt`；Answer Streaks 仅旧证据正文短记，本批目录中有标题 | 已知 Write/Spell 的重试与完成口径及 I was correct；纠正是否回写长期调度、跨模式错题集合和历史审计未知。 |
| Progress 与星标 | `https://help.quizlet.com/hc/en-us/articles/360048803491-Using-Progress-for-targeted-studying`、`https://help.quizlet.com/hc/en-us/articles/360031172312-Studying-most-missed-terms-with-stars` | 对应两份 `parsed/*.txt` | 已知跨活动对错统计、游戏不计、分组/手动星标定向复习；Mastered 阈值、误判重算、源集编辑后的映射、星标与 SRS 互通未知。 |
| Flashcards 会话 | `https://help.quizlet.com/hc/en-ca/articles/360030988091-Studying-with-Flashcards` | 旧 E-QZLT-13 有正文短记；当前 `parsed/` 无独立文件 | 已知手机 X 保存本设备会话位置且不跨设备；强退/重启、模式切换后的同一张卡恢复未知。 |
| 普通 Test / AI Practice Test | `https://help.quizlet.com/hc/en-us/articles/360030642972-Studying-with-Test`、`https://help.quizlet.com/hc/en-us/articles/25946589648013-Studying-with-Practice-Tests` | 对应 `parsed/*.txt` | 普通 Test 明确未完成离开不保存；AI Practice Test 的计时到零、暂停/续做没有闭合，不能套普通 Test 规则。 |
| 离线与同步 | `https://help.quizlet.com/hc/en-ca/articles/360030565412-Studying-offline-with-Quizlet-mobile-apps` | 旧 E-QZLT-10 有正文短记；当前 `parsed/` 无独立文件 | 已知最近 8 集缓存、单集下载、重连刷新；精确同步字段、冲突、账户切换和订阅变化后的缓存未知。 |

## 仅目录登记、尚未核读正文的相关入口

当前本地目录响应保留标题和链接编号，但没有把下列文章的目标 URL 展开保存。能够精确给出的官方入口是目录页，而不是文章直链；不能根据标题臆造 article ID。

| 目录标题 | 精确已保存官方入口 | 本地位置 | 与本批关系 |
| --- | --- | --- | --- |
| Create flashcard sets directly in Claude | `https://help.quizlet.com/hc/en-us/sections/360004698771-Creating-editing-and-saving-sets` | `parsed/sections-360004698771.txt` 链接 9；inventory order 2 | 可能补外部 AI 创建后的对象归属、编辑和失败边界；正文未读。 |
| Creating flashcard sets | 同上 | 链接 15；inventory order 8 | 可补手工创建基线和模式切换前的保存规则；正文未读。 |
| Creating a set by scanning a document | 同上 | 链接 19；inventory order 12 | 可补图片/文档导入、OCR 校订和失败；正文未读。 |
| Trouble copying and saving a set | 同上 | 链接 25；inventory order 18 | 与保存失败/恢复直接相关；正文未读。 |
| Combining flashcard sets | 同上 | 链接 26；inventory order 19 | 可补合并后重复、来源、进度映射；正文未读。 |
| Create flashcard sets directly in ChatGPT | `https://help.quizlet.com/hc/en-us/sections/360005938691-Studying` | `parsed/sections-360005938691.txt` 链接 14；inventory order 44 | 可能补 AI 生成入口和回写对象；正文未读。 |
| Studying on Quizlet | 同上 | 链接 20；inventory order 50 | 可能是模式总览，只能先判断是否有状态模型增量；正文未读。 |
| Using error checking options (UK) | 同上 | 链接 33；inventory order 64 | 可能补 US grading 之外的评分/误判差异；正文未读。 |

这 8 项是与当前重点相邻的目录条目，不代表目录外所有 Quizlet 页面，也不代表全局未读集合。

## 旧条目状态审计

- **案例与正式覆盖未过期到需要回退**：F01～17 与 `coverage.csv` 数量一致，且明确没有实际操作。现有“D2 部分”仍符合证据强度。
- **catalog 的 `next_gap` 已语义过期**：它只写“生成失败、来源校验、指南校订/更新/删除联动”，没有反映后来已登记的草稿恢复、Learn/Write/Spell、Progress、Test、离线同步与模式恢复缺口。它仍能表示案例未完成，但不能作为 M-021 当前完整下一步摘要。
- **68 行目录台账是部分时间切片**：其 F01～13 状态仍可用；后来新增的 F14～17 游戏证据不在该台账。对此不能反推游戏未读，也不能据台账宣称产品目录完整。
- **旧 E-QZLT-01～13 与新 E-QZCD 重叠但未全部本地化**：部分旧条目只有案例中的正文短记，没有独立 `parsed/*.txt`，例如 Smart Assist、Learn、Spaced Repetition、Flashcards、offline。它们不是“仅目录”，但复查性弱于已保存逐篇正文。
- **动态官方帮助没有版本号**：所有条目只绑定 2026-10-26/27 查阅日和地区语言路径；“当前仍适用”没有构建级证明。状态不是已证伪，而是缺少版本重验。
- **模式状态不可互相补写**：普通 Test 的“未完成离开不保存”、Flashcards 的本机会话、Learn path 的跨端进度、离线总页的同步、Progress 的跨活动统计分别成立，不能合并成统一恢复承诺。

## 最多 8 个可执行下一步

1. 打开并保存目录中的 **Trouble copying and saving a set** 正文，先闭合保存失败、重试、重复副本与恢复；入口为创建/编辑目录链接 25。
2. 打开并保存 **Creating a set by scanning a document** 正文，核输入格式、OCR 预览校订、取消、失败和重新扫描；入口链接 19。
3. 打开并保存 **Combining flashcard sets** 正文，核重复卡、来源集保留、合并后的进度/星标映射及撤销；入口链接 26。
4. 打开并保存 **Create flashcard sets directly in ChatGPT** 正文，与已读 Smart Assist 分开记录生成对象、发布前校订、失败和权限；入口为学习目录链接 14。
5. 复取并本地化 `Studying with Learn` 正文：`https://help.quizlet.com/hc/en-us/articles/360030986971-Studying-with-Learn`，逐项核普通 Learn 退出、跨日、切题型和重新进入后的队列。
6. 复取并本地化 `Studying with Flashcards`：`https://help.quizlet.com/hc/en-ca/articles/360030988091-Studying-with-Flashcards`，核 X 退出、强退、重启、切 Learn/Test 后的卡位与排序状态。
7. 复取并本地化 `Studying offline with Quizlet mobile apps`：`https://help.quizlet.com/hc/en-ca/articles/360030565412-Studying-offline-with-Quizlet-mobile-apps`，列出卡组内容、会话位置、答题统计、草稿和下载状态中哪些明确同步。
8. 打开并保存 **Using error checking options (UK)** 正文，和 US grading 对照误判纠正、既有成绩、Progress 与调度影响；入口为学习目录链接 33。

## 执行记录

全部命令通过 `python3` 执行，退出码均为 0：

1. 读取 `catalog.csv` 与 `cases/M-021-quizlet.md`。
2. 递归枚举路径含 `quizlet`/`qz` 的本地文件及大小。
3. 读取 68 行 `quizlet-controls-deepening-inventory.csv`，按创建/导入/进度/错题/模式/恢复关键词筛选。
4. 读取三个 `sections-*-titles.json` 和对应 `parsed/sections-*.txt`，核标题与目录链接编号。
5. 检索全部本地 Quizlet 文本，核已保存标题、文章正文和 `source-map.json` 中的精确 URL。
6. 读取正式及批次 coverage，确认正式 17 个 F；读取 E-QZ 证据的来源类型、保存路径和查阅日。
7. 写入并校验本报告。

未知：目录响应没有保存 8 个未读文章中前四项及 UK error checking 的目标 article URL，只保存目录页和链接编号；在禁止联网的条件下不能可靠恢复其精确 article ID。未观察任何账号、App 构建、生成、答题、断网或跨设备行为。

状态变化：只新建 `work/research/batch52-integration/quizlet-inventory.md`；正式案例、目录、覆盖、证据及现有原始响应均未修改。
