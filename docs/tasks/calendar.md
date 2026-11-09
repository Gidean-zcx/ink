# calendar：接下来和日历（memo-calendar-clock 的第三块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/calendar/REPORT.md。

## 要做的

预算 300 行产品代码：`Upcoming`（接下来：闹钟、提醒、日程合在一起按时间排）、`Calendar`（月历，选一天看当天的提醒和日程）。数据来自时间服务的闹钟和提醒（system/services/time/）。日程如果还没有后端，先只显示闹钟和提醒，并写进报告。

## 从哪里看起

- 这两页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 时间服务；已合入的闹钟应用 system/apps/clock/；列表用 F4（`ink_ui_list`）。

## 边界

- 新代码放 system/apps/calendar/；共用的文件只在末尾加行。同时在做的有 kid-1、ai-pages-2、confirm-pages、update-pages、files-app-1，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. `Upcoming` 按时间列出今天和之后的闹钟、提醒（测试）。
2. `Calendar` 能翻月，选一天显示当天的内容（测试）。
3. 换了成员看不到别人的；去掉检查，测试失败（反例）。
4. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
