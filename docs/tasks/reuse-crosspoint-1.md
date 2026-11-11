# reuse-crosspoint-1：从 CrossPoint 一系开源固件和原厂仓库里挖出能复用的能力，排出移植顺序

总规则先读 docs/tasks/codex-rules.md（第 9、10 节优先）。**这是调研加规划任务：不改产品代码、测试和构建文件，不碰设备，不跑全量测试和 ASan**（报告里写一句）。报告写到 `docs/tasks/reuse-crosspoint-1/REPORT.md`，边查边写、分段提交。

## 背景

用户 2026-11-11 在功能清单页（`https://claude.ai/artifact/7BhWvMokGgh7sVuSox48LA`，清单内容见 `docs/tasks/registry-research-1/` 和本任务附的 `picks.md`）勾了 71 项想要的功能，并说：“这些功能我希望你能尽量走移植复用 不要重复造轮子 改造和适配我们的系统 你之前很多动作都是多余在重复踩坑”“键盘能不能复用一下 这种就属于轮子”“输入法 官方仓库有没有现成可用的 或者crossink crosslink 应该都比较成熟了 这两个开源项目尽量搞出大量能复用的能力出来”。

要挖的开源项目（只读，可以浅克隆到你克隆里的 `work/`，不提交）：

- `crosspoint-reader/crosspoint-reader`（上游，默认分支 develop）
- `uxjulia/CrossInk`（MIT，CrossPoint 分支：书签、阅读统计、主题）
- `DaisonChun/crosslink`（MIT，CrossPoint 分支，中文排版，参考 `allocated/crosspoint_chinesetype`）
- `0x1abin/crossmux`（中日韩优化，已支持 Metalio E-Ink 4：微信读书、OPDS、WebDAV、Calibre 无线、OTA、阅读统计和热力图、时钟和老黄历待机、小游戏）
- 原厂仓库 `CloudZao/Metalio-E-INK4-Plus` 874538e（本机只读副本见 codex-rules 第 10 节；原厂阅读器在 `main/reader`、`main/display/screen/book_screen` 等）和 `CloudZao/Metalio-E-INK4`（E-Ink4 的阅读统计、WorkBuddy 等，公开仓库）
- LVGL 9.5 自带的 `lv_keyboard` 和 `lv_ime_pinyin`（含九宫格模式；我们的依赖里已有，`lv_conf.h` 没开）
- 固件仓库页面里所有带源码的仓库（用户：“我给你的固件页中也有大量开源仓库链接 你都可以薅出来看看有什么可以复用的”）：
  - `CloudZao/Metalio-E-INK4-Plus`、`CloudZao/Metalio-E-INK4`、`CloudZao/MetalioClaw4`（原厂三块板，Gitee 上有同名镜像）
  - `wegooo-cell/Metalio-S31-kiikoread`（Plus 上的阅读固件，分范围授权：Apache-2.0 的文件可用，个人学习许可的部分不用）、`wegooo-cell/read-pico-reader`、`oouu00/Metalio-S31-kiikoread-xiugai`、`dephia31/dpreader31`（kiikoread 系，分范围授权）
  - `TaoRone/Metalio-E-Ink4-Plus-TingQuS1`（MIT，微信读书书架和记录、KOReader 同步、局域网剪贴板；其中逆向的部分不用）
  - `Changkai9888/CloudZao-Projects`（固件口袋协议、亮证、`plus-noto-font`）
  - `AlayaElla/MetalioClaw4-AgentUI`、`AlayaElla/CodexRemote`（语音遥控电脑上的 AI）
  - `446599/Metalio-WhiteAI`、`WMnologo/xingzhi-ai`
  - `andjiang0083/retro-go-tab5`（retro-go 是 GPL-2.0，只记录不复用）
  - CrossLink 官网 `crosslink-reader.top` 的固件：找它对应的开源仓库（可能就是 `DaisonChun/crosslink` 或其它 CrossPoint 分支），找不到照实写
  调研报告 `docs/tasks/registry-research-1/REPORT.md` 和 `source-audit.json` 已经核过其中一部分的许可证和固定提交，直接用，不重复。
- 必要时补充其它成熟开源实现（例如农历库、MP3/FLAC 解码、Game Boy 模拟器 peanut-gb、BLE HID 主机例程），一律先核许可证。

## 要做的

1. **许可证逐项核实**（最先做）：每个项目的根许可证、子目录和单个文件的许可证头、第三方组件（特别是 wolfSSL 这类 GPL 组件会不会传染到要用的文件）。CrossPoint 和 CrossMux 的 GitHub 许可证显示为 NOASSERTION，要读原文判断。GPL/AGPL 代码一律不用；能用的写清要保留的声明。
2. **模块清单**：每个项目按模块列出（EPUB 解析和排版、TXT、图片、字体和中文排版、词典、高亮和书摘、书签、目录、主题和睡眠画面、阅读统计和热力图、网页传书、WebSocket 上传、WebDAV、OPDS、Calibre 无线、微信读书、OTA、插件、小游戏、待机时钟和老黄历、输入法和键盘、设置网页），写：源文件路径和规模、依赖（Arduino/PlatformIO 库、它们自己的绘图层 GfxRenderer 等）、和硬件的耦合程度、移植到我们系统（C 加 LVGL、ESP-IDF、系统服务和能力表、宿主机模拟器）要做什么。
3. **对上用户勾的 71 项**（`picks.md`）：每项给出最合适的复用来源（项目、模块、文件）、许可证、改造量（小于半天 / 1～2 天 / 更多）、依赖的其它项，和“没有可复用来源”的项（照实写）。用户没勾的不排。
4. **输入法和键盘**单独给结论：LVGL `lv_keyboard`+`lv_ime_pinyin`（26 键和九宫格）、这些项目里有没有中文输入、我们现有的拼音引擎（`system/services/text/pinyin.c`，词库在闪存）怎么配合；推荐一种，写清改哪些文件、多大量。
5. **移植排队**（含第 6 条里建议推倒重来的模块）：按“系统优先级（阅读是本机核心 > 传书 > 在线书源 > 锁屏桌面 > 声音蓝牙 > AI 和其它）× 难度 × 复用度”排成一串 Codex 任务，每个约 1～2 小时、写清改哪些文件、和别的任务不改同一个文件、先后依赖。正在进行的任务不要重复拆：`port-cell-3`、`port-display-3`、`port-board-2`（底层换原厂代码）、`ui-audit-2`（全页面 UI 审计）、`comm-ux-2`（短信联系人电话）、排队中的 `port-wifi-2`、`port-bt-audio-2`、`port-power-2`。
6. **我们现有的模块要不要推倒重来**（用户：“当前有做的不好的也可以看看是不是推倒重来 一些AI接入的能力看看能不能在移植过来的东西上面改造”）：和移植来源重叠的模块（阅读器 `apps/reader/`、书架、键盘和输入法、文件和传书、锁屏和待机、时钟、短信电话联系人的界面、小游戏和儿童内容等），逐个比较我们现在的实现和可移植的实现（功能、成熟度、代码量、维护是否活跃、真机上跑过没有），给出“换成移植的 / 保留我们的 / 合并”的建议和理由。凡是建议换的，写清**怎样把我们的 AI 接入层加到移植来的代码上**：哪些操作暴露成原子能力（能力表，人和 AI 共用同一套）、页面快照怎么提供给 AI、事件怎么上软总线、成员隔离和存储准入怎么接，以及移植代码里要改的最小范围。用户勾的功能里和 AI 相关的（多家 AI 服务、WorkBuddy、语音遥控电脑等）同样先找可移植的，再说怎么接到我们的家庭中枢和能力表。
7. 我们的约束照旧：能力表和 AI 可调用、成员隔离、存储准入、墨水屏少刷新（D-127）、隐私（不把家庭资料发给第三方，除非用户选了那个服务）、确认卡规则（D-105）。微信读书、网易云这类要账号或非官方接口的，标“要用户定”，写清风险（例如 CrossMux 自己说它的微信读书客户端不校验服务器证书）。

## 验收

1. 许可证表、模块清单、71 项对照表、输入法结论、移植任务队列都在报告里。
2. 只新增 `docs/tasks/reuse-crosspoint-1/` 下的文件；不出现账号、令牌、个人信息。
