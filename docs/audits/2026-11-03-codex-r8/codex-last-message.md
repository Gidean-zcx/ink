复审已完成，详见 [report.md](/Users/bytedance/Documents/Workspace/ink-audit-8/docs/audits/2026-11-03-codex-r8/report.md)。

**除已知 F-6 外，仍有高级问题。** 本轮新增：严重 0、高 1、中 1。

- **高**：PQBN 静止维护在指令级模型中需约 37–58 ms，模拟器仅计 0.046 ms。
- **中**：长跳相不满足状态等价性。

三项修复核实：1 项已修复、2 项部分修复。常规测试 **43/43**、ASan/UBSan **42/42** 通过。

报告、两份 JSON 和复现实验均在指定目录；未修改产品文件。