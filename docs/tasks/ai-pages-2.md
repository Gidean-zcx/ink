# ai-pages-2：AI 的离线、超时、额度、记录和回执页面（ai-pages 的第二块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/ai-pages-2/REPORT.md。

## 要做的

预算 300 行产品代码：`AiOffline`、`AiTimeout`、`AiBudget`、`AiLog`、`AiReceipts`。接 AI 服务现有的状态（离线、超时、额度），以及能力总线写的活动记录（system/policy/cap.c 的 ACTIVITY 集合：AI 做过什么）和回执。

## 从哪里看起

- 这五页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 已合入的 AI 对话页 system/ui/shell/ai_chat.c、ai_panel.c；AI 服务 system/services/ai/；活动记录在 cap.c（`activity_flush`）。
- 列表用 F4（`ink_ui_list`）。

## 边界

- 只做这五页；各种确认页是 confirm-pages 的任务。
- 新页面放新文件；共用的文件只在末尾加行。同时在做的有 kid-1、confirm-pages、update-pages、calendar、files-app-1，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟断网、超时、额度用完时，对应的页面出现，也能返回（测试）。
2. `AiLog` 分页列出当前成员最近让 AI 做过的事；换了成员看不到别人的（测试，反例）。
3. `AiReceipts` 显示每次操作的回执（测试）。
4. 每页有 AI 快照（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
