# replan-arch：三方应用怎么接进我们的系统（调研和可行性验证）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/<本任务>/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

评估标准（用户 2026-11-12 原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“闭源的固件可能也是移植的开源项目 可以同时调用是否有可以复用的开源项目代码 不一定找不到”。所以：按功能成熟度和使用体验排名，移植工作量只在打平时参考；协议不作限制，只看有没有源码；闭源的追查它的开源来源，确实找不到才当设计参照；不默认原厂。

已有资料（用来起步，不要只复述）：`docs/tasks/registry-research-1/REPORT.md`（52 个固件的版本、精选、说明，以及同目录的 registry-snapshot.json、source-evidence.json、web-evidence.json）、`docs/tasks/reuse-crosspoint-1/REPORT.md` 和 picks.md（用户勾的 72 项）、`docs/tasks/picks-status.md`（逐项现状）。已固定的只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/`：crosspoint-reader、CrossInk、crossmux、crosslink、kiikoread、kiikoread-xiugai、cnlunar、lunar-javascript、dr_libs、stb，原厂在 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`。

每条结论都要有证据：开源的写文件和行号，闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

我们要保留的是平台：原厂底层驱动、系统服务、能力总线（每个动作都给 AI 用）、家人成员隔离、AI 和家庭中枢、4G 电话短信、输入法和界面外壳。应用层要换成最成熟的三方实现。问题是怎么接，三种做法逐个评估：

- 甲：把三方应用的代码改写进我们的 LVGL 页面和能力总线（现在的做法）。
- 乙：三方界面代码几乎原样运行，下面垫一层兼容层：例如 CrossPoint 一系的 GfxRenderer/Activity 画到我们的屏幕 HAL、按键和触摸转成它的输入，kiikoread 的界面框架同理；它的动作再包成我们的能力。
- 丙：用 kiikoread（跑在同一台 S31 硬件上）当底座，把我们的服务、电话、AI、成员加进去。

对每种做法：读我们的应用框架（`system/include/ink/sys/` 里的活动、页面描述、能力注册）和候选项目的界面框架，写清兼容层要实现多少接口（列出来、估行数）、刷新策略（我们的最少刷新 D-127）、多语言、字号缩放、AI 暴露、成员隔离分别怎么办、内存和应用分区（5 MiB，现剩约 8%）够不够。能做最小验证就做：在 `work/` 里拿 kiikoread 或 CrossPoint 的一个界面模块，配一套最小兼容头文件，看能不能编过、缺哪些接口。

结论：推荐的整合架构和理由，每类应用用哪种做法。
