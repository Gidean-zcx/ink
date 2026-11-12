# replan-media：锁屏桌面、声音、小游戏，所有候选排名（调研）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/<本任务>/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

评估标准（用户 2026-11-12 原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“闭源的固件可能也是移植的开源项目 可以同时调用是否有可以复用的开源项目代码 不一定找不到”。所以：按功能成熟度和使用体验排名，移植工作量只在打平时参考；协议不作限制，只看有没有源码；闭源的追查它的开源来源，确实找不到才当设计参照；不默认原厂。

已有资料（用来起步，不要只复述）：`docs/tasks/registry-research-1/REPORT.md`（52 个固件的版本、精选、说明，以及同目录的 registry-snapshot.json、source-evidence.json、web-evidence.json）、`docs/tasks/reuse-crosspoint-1/REPORT.md` 和 picks.md（用户勾的 72 项）、`docs/tasks/picks-status.md`（逐项现状）。已固定的只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/`：crosspoint-reader、CrossInk、crossmux、crosslink、kiikoread、kiikoread-xiugai、cnlunar、lunar-javascript、dr_libs、stb，原厂在 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`。

每条结论都要有证据：开源的写文件和行号，闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

分四块，每块把所有候选排名（开源的读代码，闭源的看说明并追查来源）：

1. 锁屏和待机：时钟、日历和老黄历、待办和阅读组件、句子锁屏、阅读小票、壁纸。候选含 CrossMux 的待机表盘、CrossInk 的仪表盘、Mogo、PageRest、CrossLink Pixel、小智畅享版、原厂。
2. 桌面和应用框架：主屏、多页桌面、应用安装和市场、返回启动器、多固件容器。候选含墨集（作者说 Fork 自 Launcher，查是哪个 Launcher）、固件口袋（S07 源码包，查清内容）、lnkboot、BoBoOS、原厂。
3. 声音：本地音乐（格式含 M4A、浏览、歌单、歌词、续播）、有声书（M4B 章节）、网络电台、录音机、蓝牙断线重连。候选含 ModernClassicOS、布丁、ThingOS、开哥的录音机、kiikoread 改版的音乐、AgentUI 的 radio 和 voice_recorder 示例、ESP-ADF 和 ESP-Brookesia 的播放器。
4. 小游戏和儿童：五子棋、贪吃蛇、记忆、Game Boy（Eink4Boy、Peanut-GB、retro-go）、Arduboy（ardmos）、儿童认知和电子宠物（森林里的朋友）。

每块给：排名表和理由、推荐的开源底子、定体验标杆的闭源项目和要追平的设计点、缺口。
