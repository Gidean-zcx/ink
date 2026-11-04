# 小实验运行说明

运行环境：本地 Python 3、系统 C 编译器、标准库 SQLite。无网络、无模型调用、无真实家庭数据。脚本不修改产品代码，临时 C 可执行文件与 SQLite 数据库放系统临时目录，退出自动删除；不产生构建日志。

从仓库根目录运行：

```sh
python3 docs/audits/2026-11-04-codex-hub-design/experiments/protocol_checks.py
python3 docs/audits/2026-11-04-codex-hub-design/experiments/recovery_checks.py
python3 docs/audits/2026-11-04-codex-hub-design/experiments/cost_checks.py
```

本轮三条命令均退出 **0**。

## 协议词法上限

`protocol_checks.py` 用 `cc -std=c11 -Wall -Wextra -Werror` 编译原样的 `system/util/ink_json.c` 与本目录的 `protocol_probe.c`；只替代不影响解析结果的 OSAL 成本计数函数。Python 独立统计 token，再与真实 C 解析器结果比较。

结果摘录：

```text
compile_exit=0; unchanged system/util/ink_json.c
caps_17_three_fields: tokens=128, parser_rc=0, parsed_tokens=128
caps_17_plus_rev: tokens=130, parser_rc=-1, parsed_tokens=-1
caps_15_five_fields_rev: tokens=176, parser_rc=-1, parsed_tokens=-1
caps_10_five_fields_rev: tokens=121, parser_rc=0, parsed_tokens=121
members_8: tokens=85, parser_rc=0, parsed_tokens=85
asr_text: tokens=15, parser_rc=0, parsed_tokens=15
asr_open: tokens=25, parser_rc=0, parsed_tokens=25
```

它复现的是 **本仓库真实解析器** 的边界，不代表握手/ASR 服务已实现。目录三字段响应包括 JSON-RPC 外层；可选字段、长字符串、转义和更大的成员记录还应另做字节上限测试。

## 快照和增量恢复反例

`recovery_checks.py` 是 **拟议算法的最小反例，不是运行中枢的测试**。第一项使用真正 SQLite：先备份 hub，再合法新增成员及其记录，后备份 family；两个文件各自完整，组合后引用不一致。后两项用合成字典演示已 ACK 墓碑丢失、恢复后版本去重冲突。

```text
independent_VACUUM_INTO: each_integrity_check=ok; orphaned_member_references=1
restore_after_delete_ACK: server_has_deleted_object=True; device_changes_empty=True
device_restore_version_reuse: new_body_dropped_by_documented_dedupe_key=True
```

这些反例说明设计需要一致写入边界、历史缺口和数据代次，不证明某个尚未编写的实现已经丢了用户数据。

## 费用与吞吐

`cost_checks.py` 用 Decimal 重算调研 §10.3，单价直接取被审文档。每天两份报纸，每份 40 次摘要、1 次编排；每月对话 1500 轮、主模型调用 1950 次、对话输入 9.75M、输出 0.39M。

基础费（不含未给定用量的向量）28.164 元；主模型与敏感性结果见 [报告 H](../report.md#h-费用与规格)。听写每日 5 分钟 2.97 元/月；可选朗读 38 元。单价不等于已核实可购买报价，脚本不查价。

1MiB 下载、16KiB 停等、1.6Mbit/s 的模型：`8×1048576/1600000 + 64×RTT`，RTT 50/100/200ms 时分别 8.44288/11.64288/18.04288 秒。未计调度、协议开销、重传、限流，实际只能更慢；若使用窗口并行则不适用此停等公式。

音频容量：`512×0.06 = 30.72` 秒，仅是队列帧数容量，不是端到端延迟测量。
