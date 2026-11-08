# ui-cards：服务卡片（应用框架 F5）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/ui-cards/REPORT.md。

## 基线

你的分支建在 ui-input-host（F2）的分支上，它又建在 ui-semantics（F1）上：都还没合入主线，都已经自己验收完。只在上面加；它们做的部分不改，除非是修 bug（写进报告）。

## 要做的

docs/system/app-framework-design.md §9 的 F5，预算 250 行产品代码：今日页（Today）和 AI 读同一份 `app.cards`；按成员过滤；页面看得见时才更新，而且有节流；点卡片走同一个打开能力；不重做应用注册。

## 从哪里看起

- app-framework-design.md：§9 的 F5 一行和讲服务卡片的章节（自己找）。
- 应用目录和注册在 system/ui/app_registry.c；今日页在 system/ui/ 下（搜 Today）。
- F1 的接口：ui-semantics 加的文件。

## 边界

- 只做卡片的数据、成员过滤、更新节流和打开。不重画今日页的样式，不做主页编辑（那是 F8）。
- LVGL 只在 UI 任务里用；不新建常驻任务。
- 同时在跑的还有 confirm-durable、wireless-transfer。不要改它们的文件。

## 验收

1. 今日页和 AI 快照读到的卡片是同一份（测试）。
2. 换了成员，看不到别人的卡片；去掉过滤，测试失败（反例）。
3. 页面不可见时不更新；可见时更新有节流（测试，给出更新次数）。
4. 点卡片和 AI 打开走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 250 行。
