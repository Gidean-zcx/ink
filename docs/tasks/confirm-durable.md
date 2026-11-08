# confirm-durable：确认结果落盘和结果查询

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/confirm-durable/REPORT.md。

## 基线

你的分支建在 member-quiesce 的分支上：它还没合入主线，已经自己验收完。只在它上面加；它做的部分不改，除非是修它的 bug（写进报告）。

## 要做的（docs/tasks/backlog.md 原文）

预算 300 行产品代码。确认结果落盘、`confirm.status`、`confirm/request`，自动待执行动作的持久标识和结果查询（中枢设计 §7.5）。验收：断电和重启后能查到结果，不重复执行；绑定成员和内容版本的检查不放松。

## 从哪里看起

- 中枢设计 §7.5：在 docs/ 下搜中枢设计文档的 7.5 节和 confirm。
- 现有确认流程：system/policy/ 下搜 confirm、`INK_SRC_CONFIRMED`、D-105（cap.c 的 gate 和活动记录）。
- 落盘用已有的数据服务（system/services/data/，`ink_db_*`），不另写文件格式。
- 断电和重启的测试写法，看 tests/services_finish/ 里 shutdown、lowbattery 相关的测试。

## 边界

- 用已有的执行器和数据服务，不新建常驻任务。
- 确认绑定成员和内容版本：重启后换了人或内容变了，旧确认不能用。
- 同时在跑的还有 wireless-transfer（文件和传输服务）、ui-input-host（system/ui 和文本服务）。不要改它们的文件。

## 验收

1. 确认后在写盘前、写盘后各断一次电再重启：结果查得到，动作不重复执行（测试）。
2. `confirm.status` 能查到待确认、已同意、已拒绝、已过期、已执行（测试）。
3. 换了成员或内容版本，旧确认被拒；去掉检查，测试失败（反例）。
4. 新操作进能力表（在代码里注册）；全量测试和内存检查通过；产品代码不超过 300 行。
