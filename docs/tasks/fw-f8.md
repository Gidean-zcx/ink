# fw-f8：今日屏放什么、长按图标（框架 F8）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/fw-f8/REPORT.md。

## 要做的

预算 300 行产品代码（docs/system/app-framework-design.md 第 464 行 F8）：

- `TodayEdit`：“今日屏放什么”。最上面总是正在读的书；下面放四张，从应用提供的卡片（`app.cards` 的来源）里选、排顺序。按成员保存，重启后还在。
- `IconMenu`：在“全部”页长按一个图标，弹出这个应用的快捷动作（例如“接着听”）、“放到今日屏”和“关闭”。快捷动作来自应用已经声明的能力或卡片动作，不为菜单另写逻辑。
- 今日屏按保存的选择和顺序显示；没有选过时保持现在的默认。

`IconSheet` 是图标和形状的样式说明，不是页面，不做。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 TodayEdit、IconMenu；规格 docs/product/system-ui-spec.md 第 1 节的数。
- 今日屏和“全部”页：system/ui/shell/navigation.c（`apps_walk`、`tile`、今日屏的卡片）；卡片来源：system/ui/app_registry.c（`app.cards`）和 system/include/ink/sys/appreg.h（`ink_card_t`）。
- 按成员保存：已有的成员数据存储（system/services/data 的按成员集合），不要新增全局设置项。

## 边界

- 长按只在“全部”页的图标上；短按行为不变。
- 卡片内容只用应用给的，不改各应用的卡片。
- 同时在做的有 settings-b（修复中）、kid-2、transfer-pages、remote-app、activity-card、storage-usage；activity-card 也改 navigation.c 的主屏和控制中心，你只动今日屏和“全部”页；共用文件只在末尾加函数，现有函数里只加最少的接线调用。

## 验收

1. 在 `TodayEdit` 选四张并排序后，今日屏按这个顺序显示，重启后还在；换成员看到的是各自的选择（测试）。
2. 长按“听”出现 `IconMenu`，“接着听”走的就是应用自己的那个能力；“放到今日屏”后今日屏多了这张（测试）。
3. 短按图标照旧打开应用（测试，反例：长按处理吞掉短按，测试失败）。
4. 两页都有 AI 快照，按钮和 AI 走同一个能力；孩子的今日屏不受大人的选择影响（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行；截图留在构建目录。
