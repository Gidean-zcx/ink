# kid-3：孩子的主屏、今日、全部

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/kid-3/REPORT.md。

## 要做的

预算 250 行产品代码。规格第 85 行：孩子有自己的版本（`KidHome`、`KidToday`、`KidApps`），结构和大人一样，书和进度是孩子自己的；没有设置、文件、网页；今日屏下面写一行“爸爸妈妈可以看到你的记录”；电话和短信不限制（D-25）。

现在孩子看到的是大人的主页加过滤（system/ui/shell/navigation.c 里的 `child()`）。对照三张画板把差距补齐：

- `KidHome`：孩子的主屏（时钟、接下来、正在进行的事），没有大人才有的入口。
- `KidToday`：孩子的今日屏，卡片来自孩子自己的数据（不受大人在 TodayEdit 里的选择影响），最下面那行说明。
- `KidApps`：孩子的“全部”，只列孩子能用的（`child_ok` 和孩子的白名单、用时策略），没有设置、文件、网页；时间到了只剩电话和短信（kid-2 的 `ink_member_time_up`）。
- 三页的 AI 快照写孩子的版本（页面名用画板名），按钮和 AI 走同一个能力。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 KidHome、KidToday、KidApps，对照大人的 Main、Today、Apps。
- 主页和“全部”：system/ui/shell/navigation.c（`tools()`、`apps_walk`、今日屏卡片、`child()`）；用时策略：system/policy/members.c（kid-2）；TodayEdit：同文件 fw-f8 那部分。

## 边界

- 不改大人的三页的行为；孩子的差异只在 `child()` 分支里。
- 同时在做的有 privacy-records、file-index-3、factory-pages、fw-f13、port-skeleton，不要改它们的文件；共用文件只在末尾加行。

## 验收

1. 孩子空间里三页的快照页面名分别是 KidHome、KidToday、KidApps；大人的不变（测试）。
2. KidApps 没有设置、文件、网页；时间到了只剩电话和短信；KidToday 有那行说明（测试）。
3. 大人在 TodayEdit 里的选择不影响 KidToday（测试，反例）。
4. 三页都有截图，按钮和 AI 走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 250 行。
