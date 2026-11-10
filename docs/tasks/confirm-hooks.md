# confirm-hooks：确认前的绑定和说明由能力自己提供

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/confirm-hooks/REPORT.md。

## 背景

通用确认策略 `system/policy/confirm.c` 里按能力名特判了两处：

- `call.dtmf`（audit7-fix）：确认前调用通信服务的 `ink_comm_bind_dtmf` 绑定通话代次和对方号码，说明也用绑定后的参数；为此策略层引用了 `ink/sys/comm.h`。
- `system.factory_reset`（audit5-fix）：`describe()` 里按能力名补一句“同时清空存储卡上的书和音频／存储卡上的书和音频保留”。

策略层不该认识具体能力；以后每有一个需要绑定上下文的能力，就得再改一次策略层（R-061：软总线加原子能力，模块各管各的）。

## 要做的

预算：产品代码净增不超过 80 行。

1. 在能力定义（`ink_cap_t`）里加一个可选钩子，在确认请求进入等待之前由确认策略调用：能力在这里绑定当时的上下文（往参数里加只给自己核对用的字段），也可以补充确认说明里要显示的内容；钩子返回失败就不进入等待，回执写清原因（例如已经没有通话）。执行时核对绑定仍由能力自己做。
2. `call.dtmf` 和 `system.factory_reset` 改用这个钩子；`confirm.c` 里不再出现任何具体能力名，也不再引用通信服务的头文件。
3. 说明文字、确认规则（D-105）、家长密码、AI 不能代确认都不变；已有测试（audit5-b5-01、audit7-b7-01、confirm-durable、confirm-pages、phone-calls 等）原样通过。
4. docs/system/app-framework.md 里写应用怎么用这个钩子（一小段，按真实头文件）。

## 边界

- 只做这一处重构，不顺手改别的确认逻辑。
- 同时在做的有 index-prune 和稍后 batch10 审计的修复，只改各自的文件。

## 验收

1. `grep -n '"call\.\|"system\.' system/policy/confirm.c` 没有结果；confirm.c 不再包含 comm.h。
2. 新测试：一个测试用能力通过钩子绑定上下文和补充说明，确认后执行时能核对到绑定；钩子返回失败时不进入等待。
3. 全量测试和内存检查通过；S31 能编过。
