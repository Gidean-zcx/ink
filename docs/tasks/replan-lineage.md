# replan-lineage：闭源固件追源，找可复用的开源代码（调研）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/<本任务>/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

评估标准（用户 2026-11-12 原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“闭源的固件可能也是移植的开源项目 可以同时调用是否有可以复用的开源项目代码 不一定找不到”。所以：按功能成熟度和使用体验排名，移植工作量只在打平时参考；协议不作限制，只看有没有源码；闭源的追查它的开源来源，确实找不到才当设计参照；不默认原厂。

已有资料（用来起步，不要只复述）：`docs/tasks/registry-research-1/REPORT.md`（52 个固件的版本、精选、说明，以及同目录的 registry-snapshot.json、source-evidence.json、web-evidence.json）、`docs/tasks/reuse-crosspoint-1/REPORT.md` 和 picks.md（用户勾的 72 项）、`docs/tasks/picks-status.md`（逐项现状）。已固定的只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/`：crosspoint-reader、CrossInk、crossmux、crosslink、kiikoread、kiikoread-xiugai、cnlunar、lunar-javascript、dr_libs、stb，原厂在 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`。

每条结论都要有证据：开源的写文件和行号，闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

对 `registry-research-1` 里每个没给源码的固件（CrossLink Pixel、Crosslink2 和 Crossslink_2.0、Mogo、PageRest、ThingOS、ModernClassicOS、lnkboot、布丁、墨集、Eink4Boy、ardmos、开哥的录音机/浏览器/天气/五子棋/贪吃蛇/记事本/点读机/亮证/黑猩猩测试/森林里的朋友、BoBoOS、RickyOS、小智畅享版、微信读书适配版、Metalio-WhiteAI、CrossMux 倾斜翻页版、CrossPoint 非官方 Plus 移植、Claw4 GBA 版、retro-go-gba）：

1. 自己的源码有没有发布在任何地方：GitHub、Gitee（crosslink 在 Gitee，之前匿名取不到，换办法：镜像、fork、旧版）、GitLab、作者主页、发布附件、论坛帖子里公开的链接。
2. 开源来源：作者说的“基于、Fork、移植”，界面相似，说明里提到的库，作者同名账号下的其他仓库。中文和英文都搜：GitHub 和 Gitee 搜索、B 站、小红书、知乎、酷安、贴吧、V2EX。
3. 它很可能用到的开源组件（CrossPoint、kiikoread Pico、LVGL、Peanut-GB、gnuboy、retro-go、Arduboy 模拟器、ESP-ADF、ESP-Brookesia、Launcher、小智 xiaozhi-esp32、微信读书接口库、OPDS 库等），给出仓库网址和实现对应功能的具体文件。
4. 每个固件写置信度；最后列出我们之前不知道、最有价值的开源来源。
