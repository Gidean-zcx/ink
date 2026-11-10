# cell-features-merge：把移动网络和紧急呼叫合到现在的主线上，并找出开机变慢的原因

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/cell-features-merge/REPORT.md。**只编译、跑宿主机测试，不碰设备。**

## 背景

mobile-network-2 和 emergency-call-2 都做完了（报告在各自分支的 `docs/tasks/mobile-network-2/REPORT.md`、`docs/tasks/emergency-call-2/REPORT.md`），提交保存在本仓库的只读分支 `archive/mobile-network-2`、`archive/emergency-call-2`（克隆里是 `origin/archive/...`）。它们都基于 951bdfc1，那之后主线又合入了 audit14-merge-fix（能力闸门恢复先读电源状态）、boot-flow 截图等安静再拍、integration-races 准备步骤放宽、能力调用失败记 WARN。

2026-11-11 凌晨把 mobile-network-2 合到 8fff0412 上试过，出现：

1. **`hub-simulator-e2e` 失败**：合并前成员夹具 2417 ms 就绪；合并后 `test.hub_member_confirmed_ms` 到 5054.9 ms 才到（`member.add` 确认后执行了约 2.5 s），脚本 3 s 的第一次点按落在首次设置页上，后面全乱。`metrics.storage.wait_for_idle_ms.max` 达 4150 ms，像是存储写入一直在等“空闲”。已排除（单独改过、仍是 5054 ms）：能力调用失败的日志级别；把 `ink_comm_init` 里同步打开 MOBILE 库改到后台执行器；每日流量检查点不等落盘。4G 链路规则只多了 `!CELL.data_off`，注册要求原来就有。
2. **`integration-ai-surface`**：mobile-network-2 把 `sms.send` 的必填改成了 `to,text`；主线的产品照 sms-multipart-2 只要求 `to`，测试应保持 `{"sms.send", "to"}`。

注意：模拟器的设备日志环 `test-out/<场景>/logs.bin` 在多次运行间**不清空**，解码前先删输出目录；短的场景可能还没刷进环里（行在 RTC 尾巴里），看不到不代表没发生。

## 要做的

1. 从现在的主线开始，把两个分支的功能都合进来，语义和它们的报告一致；冲突照两边的意图合，能力闸门不加名字前缀特判（emergency-call-2 已改成标记），紧急呼叫不受孩子应用名单影响。
2. 找出第 1 条开机变慢的**真正原因**：哪一步让存储一直等空闲、为什么合并后才出现。写清证据。是产品问题就修产品（开机不应因为这两个功能多等 2.5 s）；如果确认是测试在固定时刻点按的假设被打破、产品没有变慢，就改测试让它等就绪再点，并写明依据。不放宽断言、预算。
3. 能力清单重新生成（`python3 tools/capabilities.py`）。

## 验收

1. 模拟器全量测试通过（中枢测试要建好中枢环境，见 `hub/README.md`），ASan 版也通过；两个分支原有的测试都通过。
2. S31 能编过（产品代码 0 警告）；gcccheck 通过；做隐私扫描。
3. 报告写清第 1 条的原因、证据和修法，以及和两个原分支实现的差别。
