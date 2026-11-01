# 底座专题研究（2026-11-01）

用户要求“认真了解需求、竞品信息和架构”后再设计第一版底座（I-034）。以下五份报告由并行研究完成，每份都区分有据事实（代码、官方文档、报道）与推断，并列出未能核实的部分。没有任何实机测试。

| 报告 | 回答的问题 |
| --- | --- |
| [竞品库对底座的含义](library-implications.md) | 仓库里 65 组产品、996 个任务的交互研究，对数据对象、状态、恢复、AI 动作、家庭模式意味着什么 |
| [墨水屏产品与开源系统架构](eink-architectures.md) | Mudita Pure/MuditaOS、CrossPoint、KOReader、Kindle、Kobo、reMarkable、BOOX 等怎么建系统，教训是什么 |
| [AI 原生设备与能力协议](ai-native-protocols.md) | xiaozhi 设备侧 MCP、MCP 2026-07-28、A2UI、App Intents、AppFunctions、语音时延与儿童安全 |
| [上游 Plus 固件架构](upstream-firmware.md) | 目标板官方固件的真实代码结构、可移植的驱动与要避开的部分 |
| [ESP32-S31 平台与运行时](platform-runtime.md) | 芯片与板卡资源、内存预算、排版与字体、音频、存储、运行时、模拟与主机端开发 |

结论汇总见[架构对标](../architecture-benchmarks.md)，设计落实见[底座设计](../../system/foundation.md)。
