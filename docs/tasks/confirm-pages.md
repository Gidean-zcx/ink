# confirm-pages：各种确认页面（ai-pages 里的确认部分）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/confirm-pages/REPORT.md。

## 要做的

预算 300 行产品代码：`ConfirmWeb`、`ConfirmParent`、`ConfirmCall`、`ConfirmSetting`、`ConfirmExpired`、`StressConfirm`。确认事务已经有了（system/policy/confirm.c，D-105；结果落盘已合入）；现在的确认卡外观是占位（D-64），按画板做出这几种确认的样子。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 现有的确认卡 system/ui/shell/confirm_card.c；confirm.c 里的确认种类。

## 边界

- 不放松确认规则：绑定成员、参数和内容版本；只有触摸和按键能回答；家长确认要家长 PIN。
- 只改外观和页面；确认规则缺的写进报告。
- 新页面放新文件或 confirm_card.c 里；共用的文件只在末尾加行。同时在做的有 kid-1、ai-pages-2、update-pages、calendar、files-app-1，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 每种确认都能出现、同意、拒绝、过期（测试）。
2. `StressConfirm`（连续来好几个确认）排队正确（测试）。
3. AI 不能替人回答；去掉检查，测试失败（反例）。
4. 每种确认有 AI 快照（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
