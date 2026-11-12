# replan-reader：阅读和传书，所有候选按成熟度和体验排名（调研）

总规则先读 `docs/tasks/codex-rules.md`。

本任务只调研、写报告：不改产品代码（只写 `docs/tasks/<本任务>/`），不需要跑主工程的构建和全量测试（`codex-rules.md` 第 5 节的构建测试要求对本任务不适用）。不动 `$INK_DEPS/../ink-hw/upstream/` 里已有的只读副本；需要新的源码就在本任务克隆的 `work/`（已被忽略）里只读克隆。不下载、不运行固件二进制。

评估标准（用户 2026-11-12 原话）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“闭源的固件可能也是移植的开源项目 可以同时调用是否有可以复用的开源项目代码 不一定找不到”。所以：按功能成熟度和使用体验排名，移植工作量只在打平时参考；协议不作限制，只看有没有源码；闭源的追查它的开源来源，确实找不到才当设计参照；不默认原厂。

已有资料（用来起步，不要只复述）：`docs/tasks/registry-research-1/REPORT.md`（52 个固件的版本、精选、说明，以及同目录的 registry-snapshot.json、source-evidence.json、web-evidence.json）、`docs/tasks/reuse-crosspoint-1/REPORT.md` 和 picks.md（用户勾的 72 项）、`docs/tasks/picks-status.md`（逐项现状）。已固定的只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/`：crosspoint-reader、CrossInk、crossmux、crosslink、kiikoread、kiikoread-xiugai、cnlunar、lunar-javascript、dr_libs、stb，原厂在 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/`。

每条结论都要有证据：开源的写文件和行号，闭源的写网址和原文；拿不准的写“不确定”和原因。报告用中文，结论在前。

## 要回答的

1. 阅读器：把所有候选排名——开源的 CrossPoint、CrossInk、CrossMux、旧版 crosslink、kiikoread（就跑在我们这台 S31 硬件上）、kiikoread 改版、dpReader、原厂；闭源的 CrossLink Pixel、Crosslink2、Mogo（作者说源自 kiikoread Pico）、PageRest、ThingOS、听趣等。比较：中文排版质量（标点挤压、避头尾、两端对齐、字体和 TTF、灰阶抗锯齿）、翻页速度和刷新策略、EPUB 深度（CSS、插图、脚注、表格）、TXT（编码、自动目录）、导航（目录、书签、搜索、进度）、阅读设置、书架（封面、分类、最近）、统计、书摘、同步（KOReader、微信读书）、OPDS、触屏和按键交互是否顺手。
2. 能编出来的就编出来亲眼看：kiikoread 改版自带网页模拟器（`tools/simulator/`），CrossPoint 一系如有模拟器或宿主机构建也试。在 `work/` 里构建，模拟器不联网；截阅读页、书架、设置、传书页，放进 `docs/tasks/replan-reader/`（单张不超过 300 KB），并排比较。编不出来写原因。
3. 传书：网页界面（上传、文件夹、改名删除、进度）、热点、FTP、U 盘、坚果云和网盘、上传时自动生成目录和元数据，同样排名。
4. 结论：阅读器和传书各推荐一个开源底子（为什么体验上胜过其他），哪个闭源的定了体验标杆、要追平哪些具体设计；如果最好的做法是“整块搬过来、尽量不改它的界面”，写清它依赖什么（框架、屏幕和输入接口）、和我们系统接起来的方式。
