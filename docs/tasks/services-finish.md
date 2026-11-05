# 任务：系统服务收尾

报告写到 `docs/tasks/services-finish/REPORT.md`。

依据：`docs/audits/2026-11-05-codex-services-a/`、`-b/`、`-c/` 的报告（可能不完整，以写出来的为准；还有 `docs/audits/README.md` 的第十六轮）；`docs/system/system-services-design.md` §7、§15.3、§15.4；`docs/hub/hub-architecture.md` §7.5、§13；`docs/hub/device-protocol.md`（`ai.listen`、HUB2-006 那一条）。

可以改的：`system/services/`、`system/policy/`、`tests/`、上面的设计文档 §7、§15、`docs/audits/README.md`（加这一轮的处理表）。`system/ui/shell/` 另有任务在重写，只有修问题必须时才小改，并列进报告。

不做：D-95 的计时（等用户定），界面外观，中枢代码，真机端口。

## 验收

| 编号 | 要求 | 怎么证明 |
| --- | --- | --- |
| S1 | 三路报告里每条严重和高：先复现，确认成立就修并配测试和反向对照；不成立写明依据。中级小的修，大的列清楚。低级只列 | 处理表逐条有结论 |
| S2 | 确认规则按 D-105 改代码（要动的地方见 §15.4）：人亲口要求的对外动作（`_meta.asked`）和非高风险设置直接执行，回执说结果，能撤销的给撤销；高风险能力标 `ask_by_hand`；AI 自己的主意、网页或收到的消息里的文字，不会自己执行对外动作；活动记录 `activity.list` 能查 AI 做过什么。AI 仍然不能代替人输密码或确认，确认卡的绑定和重新校验不放松 | 每类至少一个正例一个反例的测试；§7 同步改 |
| S3 | `ai.listen` 的 `actions` 不再因为 768 字节缓冲溢出变空（HUB2-006）：放 PSRAM，当前能力表完整发出，不增加内部 RAM | 用实际能力表的测试；预算测试过 |
| S4 | 内部 RAM 等预算不放宽；已有测试全过；内存检查全过 | ctest |
| S5 | 总规则第 6 节的结束检查 | 报告 |
