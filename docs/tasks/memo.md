# memo：备忘（memo-calendar-clock 的第二块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/memo/REPORT.md。

## 要做的

预算 350 行产品代码：`Memo`（列表）、`MemoDetail`、`MemoEdit`、`MemoChecklist`（清单）、`MemoUndo`（撤销删除）、`MemoSaveFail`（保存失败的提示）。备忘存进数据服务，按成员；编辑用 text-long 的长文编辑页（已合入，草稿断电不丢）；列表用 F4。`MemoRecord`（录音备忘）和 `MemoPending` 是后面的任务，这里不做。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- docs/tasks/text-long/REPORT.md（长文编辑页 `ink_textfield_long_*`、草稿）；F1、F4 的报告；数据服务 `ink_db`。

## 边界

- 新代码放 system/apps/memo/；共用的文件只在末尾加行。同时在做的有 phone-calls、sms、settings-a、power-pages，以及在修的 clock-alarm、ai-chat，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 新建、编辑、删除（能撤销）、清单勾选，重启后都还在（测试）。
2. 保存失败时提示，内容不丢（测试）。
3. 换了成员看不到别人的备忘；去掉检查，测试失败（反例）。
4. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
5. 新操作进能力表；全量测试和内存检查通过；产品代码不超过 350 行。
