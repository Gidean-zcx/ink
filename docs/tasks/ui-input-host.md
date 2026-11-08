# ui-input-host：短输入框的输入宿主（应用框架 F2）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/ui-input-host/REPORT.md。

## 基线

你的分支建在 ui-semantics（F1 控件语义合同）的分支上：它还没合入主线，已经自己验收完。只在它上面加；它做的部分不改，除非是修它的 bug（写进报告）。

## 要做的

docs/system/app-framework-design.md §9 的 F2，预算 250 行产品代码：搜索、网址这类短输入框；键盘在屏幕的固定区域（用户 2026-11-08 定了 §7 候选 3 选 A：短框放固定区，长文和表单进专用编辑页，长文这里不做）；焦点看得见；输入完回到原来的位置；AI 代填和人输入的结果一样；换了成员，旧的输入会话被拒绝。

## 从哪里看起

- app-framework-design.md：§9 的 F2 一行，以及讲输入和 Ime 版式的章节（自己找）。
- F1 的接口：基线里 ui-semantics 加的文件，和 docs/tasks/ui-semantics/REPORT.md。
- 现有输入法和文本服务：system/services/text/、system/services/input/；界面在 system/ui/。

## 边界

- 只做短输入框；长文和表单编辑页是 F3 和 text-long，不做。
- LVGL 只在 UI 任务里用；不新建常驻任务。
- 同时在跑的还有 confirm-durable（system/policy）、wireless-transfer（文件和传输服务）。不要改它们的文件。

## 验收

1. 测试页面上的短输入框：键盘出现在固定区域，输入框不被挡住（快照测试）。
2. 焦点在快照里看得见；输入完回到原来的位置（测试）。
3. AI 代填和人输入，得到同样的字段值和事件（测试）。
4. 换了成员，旧的输入会话被拒；去掉检查，测试失败（反例）。
5. 全量测试和内存检查通过；产品代码不超过 250 行。
