# replan-sysapps：电话类系统应用，给每个应用找最成熟的实现（调研）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/<本任务>/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

评估标准（用户 2026-11-12 原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“闭源的固件可能也是移植的开源项目 可以同时调用是否有可以复用的开源项目代码 不一定找不到”。所以：按功能成熟度和使用体验排名，移植工作量只在打平时参考；协议不作限制，只看有没有源码；闭源的追查它的开源来源，确实找不到才当设计参照；不默认原厂。

已有资料（用来起步，不要只复述）：`docs/tasks/registry-research-1/REPORT.md`（52 个固件的版本、精选、说明，以及同目录的 registry-snapshot.json、source-evidence.json、web-evidence.json）、`docs/tasks/reuse-crosspoint-1/REPORT.md` 和 picks.md（用户勾的 72 项）、`docs/tasks/picks-status.md`（逐项现状）。已固定的只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/`：crosspoint-reader、CrossInk、crossmux、crosslink、kiikoread、kiikoread-xiugai、cnlunar、lunar-javascript、dr_libs、stb，原厂在 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`。

每条结论都要有证据：开源的写文件和行号，闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

我们自己写的系统应用（`system/apps/*`、`system/ui/shell/*`）：电话、短信、联系人、时钟（闹钟、计时、秒表、世界时钟）、计算器、日历和日程、天气、相册、备忘、文件管理、设置、桌面、相机界面、远程网页。用户怀疑它们不是最好的。

1. 每个应用找 2～3 个最成熟的开源实现，范围放大：乐鑫 ESP-Brookesia 和它的应用、ESP-BOX/ESP32-P4 示例、LVGL 官方 demos、手表系统（PebbleOS 已开源、InfiniTime、ZSWatch、Watchy、Bangle.js 应用）、带 4G/SIM 的 ESP32 手机项目、墨水屏手机项目、Flipper Zero 应用、AgentUI（MetalioClaw4-AgentUI，P4 触屏的电话、相机、文件）、本社区的布丁和开哥的小应用（追查有没有源码）。成熟的手机应用（AOSP、Fossify 等）也可以拿代码或交互。
2. 每个候选写：仓库、提交、语言和框架、功能完整度（具体到功能点）、界面好在哪、墨水屏适不适合（动画、颜色依赖、触控大小、刷新）。
3. 读我们的实现，逐个对比：我们缺什么、哪里体验差（参考 `docs/tasks/ui-audit-2/REPORT.md`）。
4. 结论：每个应用“整块换成 X”“搬 X 的哪些部分”“保留我们的（为什么没有更成熟的）”，并给出要对照重做的体验参照。
