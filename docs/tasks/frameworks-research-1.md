# frameworks-research-1：输入法、通知栏和控制中心、设置，各找最成熟的完整实现（调研）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/frameworks-research-1/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

## 背景

用户 2026-11-12 定的方向：“我们系统还是要自研的 我更接受你提出的移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”。评估标准（用户原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用”；“闭源的固件可能也是移植的开源项目…花一点精力去搜索 比后续从头开发要好”。

之前的调研没覆盖输入法和通知栏；设置在 `docs/tasks/replan-sysapps/APPS.md` 有一节（Lineage 设置、PebbleOS、ESP-Brookesia），结论是“不整块换”，这次按新方向重新看。我们现在的实现：输入法在 `system/ime/` 和 `system/ui/shell/keyboard.c`（LVGL 键盘、九宫格、拼音词库），状态栏和控制中心在 `system/ui/shell/`，通知在 `system/services/notify*`，设置在 `system/apps/settings/`（以实际目录为准）。设备是 4.7 寸墨水屏触屏（竖屏 684×1216，约 297 dpi），ESP32-S31，16 MB PSRAM，应用分区只剩约 400 KB。

每条结论都要有证据：开源的写仓库、固定提交、文件和行号；闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

1. **输入法**：引擎和键盘界面分开排名。引擎候选至少看：Google 拼音（安卓原生拼音输入法的 C++ 内核，Qt 虚拟键盘也用它）、librime 及其拼音数据、libpinyin/sunpinyin、fcitx5-android 用的引擎、Mogo 的输入法（`$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p/main/ui/ui_ime.c`）、固件口袋的输入法（CloudZao-Projects 源码包，用 rime 拼音数据）、kiikoread 九宫格、我们现在的。比：整句和词组联想、九宫格拼音、简拼、用户词频学习、中英混输、词库大小和内存、在 ESP32 上有没有人跑过。键盘界面候选：fcitx5-android、Trime、AOSP 拼音、Mogo、口袋，比布局、候选栏、切换和符号页的设计。手写输入有没有可用的开源识别（如 Zinnia 及模型），列出来供用户选，不当成需求。
2. **通知栏和控制中心**（下拉的通知列表、快捷开关、亮度、勿扰）：候选至少看 AOSP SystemUI（设计和行为）、PebbleOS（通知、勿扰、动作和快捷回复，C）、InfiniTime（LVGL）、MuditaOS（墨水屏手机的状态栏、通知、手机模式）、ESP-Brookesia（LVGL 的状态栏、导航栏、最近任务）、ZSWatch、LilyPhone。比：通知分组和操作、勿扰和例外、快捷开关、在墨水屏上的刷新和点按。
3. **设置**：候选至少看 Lineage/AOSP 设置（层级、搜索、当前值）、MuditaOS 设置（墨水屏手机）、PebbleOS、ESP-Brookesia、InfiniTime。比：结构、搜索、每页的当前值和反馈、手机模式、声音和铃声、显示和前光、网络和 SIM、安全和家人。
4. 每一类给结论：推荐哪个当底子（为什么体验上胜过其他），哪些部分整块搬代码、哪些只能照着它逐页翻译、哪些留我们自己的（家人隔离、AI 能力、确认卡这些我们独有的合同）；搬过来要接我们的哪些服务；有参考截图的放进报告目录（单张不超过 300 KB），后面用来做预览页。
