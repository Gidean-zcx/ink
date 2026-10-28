# 第34批续：Anki 30项 directory-only 官方 Markdown 采集

采集时间：2026-10-27（逐请求 UTC 时间见 `fetch-log.json`）。仅以 Python 标准库对正式目录中的精确 `source_url` 做普通公开 GET；未安装或运行 Anki、同步服务器或手册中的任何命令，未修改 `docs/`。

## 采集结果与停止条件

- 目标：30项 `directory_only`。
- 成功保存：16项，均为 HTTP 200、`text/markdown; charset=utf-8`。
- 第17项 M020-P35 `Windows permission problems` 返回 HTTP 200，但响应正文触发脚本的 challenge 关键词检测，按任务约束未保存该正文，并立即停止 `docs.ankiweb.net` 这一来源。
- M020-P36～P47、P51 共13项随后均标为 `source_stopped_after_challenge`，没有发出请求。因此 **Styling & HTML 未取得正文**，macOS及Linux各页也未取得。
- 本次没有401、403、412、429或网络异常。触发词也可能是权限故障文档自身提到的错误短语；因约束要求触发即停，本批不重新请求、不绕过，也不据此认定网站确实展示了反机器人挑战。

原件保持 URL 相对路径保存于 `raw/manual/...`。每项请求 URL、最终 URL、HTTP状态、Content-Type、字节数、SHA-256、UTC请求时间、保存路径或停止原因均在 `fetch-log.json`。

## 逐页事实摘要（成功保存的16页）

### M020-P01 Introduction

- URL：https://docs.ankiweb.net/manual/intro.md
- 标题/内容：桌面版手册导言；区分移动客户端手册，提供 Quickstart、Getting Help、翻译和旧版文档入口。
- 操作：主要是导航到快速开始、帮助或其他手册，不构成独立产品操作。
- 版本条件：旧版手册指向 Internet Archive；旧调度器另指 FAQ。正文没有给导言功能的最低版本。

### M020-P02 Getting Started

- URL：https://docs.ankiweb.net/manual/getting-started.md
- 标题/主要操作：安装入口；解释 Cards、Card States、Decks、Notes & Fields、Card Types、Note Types、Collection；Shared Decks 段写明下载包后双击或用 **File > Import** 导入，之后可同步到 AnkiWeb。
- 状态边界：共享牌组不能直接加到 AnkiWeb，需先在 desktop、AnkiMobile 或 AnkiDroid 导入再同步。
- 版本条件：部分视频由旧版 Anki 制作，但正文称概念仍适用；未给基本概念最低版本。

### M020-P03 Background

- URL：https://docs.ankiweb.net/manual/background.md
- 标题/内容：Active Recall Testing、Use It or Lose It、Spaced Repetition、Why Anki；介绍学习原理和选择 Anki 的背景。
- 操作：无闭合用户操作，不应造成功能任务。
- 版本条件：提及 Anki 跨 Windows、macOS、Linux/FreeBSD；关于其他程序与旧算法的比较属于背景陈述，不是当前界面保证。

### M020-P04 Getting help

- URL：https://docs.ankiweb.net/manual/getting-help.md
- 标题/主要操作：先搜索手册与支持渠道；提问时提供版本、系统、复现步骤、错误信息等；按 Anki Desktop/AnkiWeb、AnkiDroid、AnkiMobile 分流；安全报告和商业咨询可用私密工单。
- 版本条件：无具体构建门槛；明确不同客户端支持渠道不同。

### M020-P19 Math and Symbols

- URL：https://docs.ankiweb.net/manual/math.md
- MathJax：Anki 2.1+、AnkiMobile 和 AnkiDroid 开箱支持。编辑器最右按钮选择 MathJax；内容用 TeX，行内分隔符 `\(...\)`，展示公式 `\[...\]`；包含 mhchem。自定义需按页面给出的加载时机配置。
- LaTeX：复习时桌面端调用本机 LaTeX 生成图片并放入 collection media；移动/网页端可显示已生成图片，之后同步上传媒体。近期 Anki 默认拒绝生成 LaTeX 图片，用户自己的卡需在 Preferences 开启 **Generate LaTeX images**；手册明确 LaTeX 命令可能读写非 Anki 数据。
- 环境条件：LaTeX 需用户自行安装；Windows列 MiKTeX、macOS列 MacTeX/BasicTeX、Linux列常见发行版包。图片只能由电脑端生成。
- 模板冲突版本：Anki 2.1.20 / AnkiMobile 2.0.56 / AnkiDroid 2.13 起，旧的模板花括号 workaround 不再需要；兼容更旧版本时才参考旧写法。
- 未知：没有实机验证任一公式、包、同步或安全设置；未执行任何 LaTeX 命令。

### M020-P23 Self-Hosted Sync Server

- URL：https://docs.ankiweb.net/manual/sync-server.md
- 定位与限制：可不用 AnkiWeb而运行自托管同步服务；客户端更新后若协议变化，服务器也需更新，否则同步可能停止。第三方服务器未由 Anki 测试；客户端消息即使连接自建服务仍可能写“AnkiWeb”。
- 安装/运行选项（仅记录，未执行）：桌面包内服务器、独立 Python/Rust 实现、源码或 Docker。桌面内置方式为 Anki 2.1.57+；独立 Python 要求 Python 3.9+；Rust 实现从 Anki 2.1.66+ 可用。页面另写 25.07～25.09.4 的可执行文件名替换条件。
- 数据与账号：环境变量配置用户；支持多用户及哈希密码。服务器默认存储路径 `~/.syncserver`，服务端和客户端必须保留各自副本；应通过同步写入，不能手工把文件复制进服务器目录。
- 网络：默认监听未加密 HTTP，不适合直接暴露公网；页面建议在前面放 VPN 或 HTTPS 反向代理，并可用 `SYNC_HOST`/`SYNC_PORT` 改绑定。客户端需设服务器 URL与凭据。
- 明确兼容条件：iOS不支持 TLS 1.3，反向代理需启用 TLS 1.2（按本次正文原话记录，未独立验证）；大请求和反代缓冲另有配置段。
- 未知：未部署、未测试认证、媒体同步、反代、失败恢复或任何版本组合。

### M020-P25 Contributing

- URL：https://docs.ankiweb.net/manual/contrib.md
- 标题/主要操作：公开分享牌组、私下分享牌组、分享插件、翻译、贡献代码。公开分享先同步后通过 AnkiWeb；私下分享用 **File > Export** 并选择包格式。
- 状态边界：重新导入更新包时可导入新增内容而不改变学习进度，但笔记类型已改动会影响更新条件；这是分享/贡献说明，不等同普通导出恢复的完整保证。
- 版本条件：正文提到既往版本分享；未给统一最低版本。

### M020-P26 Resources

- URL：https://docs.ankiweb.net/manual/resources.md
- 标题/内容：Resources；正文很短，为外部资源入口。
- 操作/版本：无可闭合产品操作，无明确版本门槛。

### M020-P27 Platform Notes

- URL：https://docs.ankiweb.net/manual/platform/intro.md
- 标题/内容：Platform Notes，仅列 Windows、macOS、Linux 三个平台子页。
- 操作/版本：目录父页，不应造成功能。

### M020-P28 Windows

- URL：https://docs.ankiweb.net/manual/platform/windows/intro.md
- 标题/内容：Windows平台父页，指向安装/升级及问题子页。
- 操作/版本：主要是导航，不是独立安装步骤。

### M020-P29 Installing & Upgrading Anki on Windows

- URL：https://docs.ankiweb.net/manual/platform/windows/installing.md
- 要求：x64需64位 Windows 10 1703+或 Windows 11；ARM64需 Windows 11。Windows 7/8.1 最后支持版为 Anki 2.1.49；32位 Windows 最后支持版为 Anki 2.1.35-alternate。
- 安装/升级：选择与架构匹配的 Windows 10+ x64 或 Windows 11 ARM 包。由 Anki 2.1.6+ 升级无需先卸载，只需关闭程序后安装；更早版本或 standard/alternate 切换建议先卸载程序，但正文称不会删除卡片数据。
- 其他：降级前需遵循链接的降级说明；插件可能不兼容新版，可查发行页旧版。未执行安装、升级或降级。

### M020-P30 Windows installation issues

- URL：https://docs.ankiweb.net/manual/platform/windows/installation-issues.md
- 主要问题：安装时 “Error opening file for writing” 与杀毒软件误报；并指向 startup issues。
- 操作/版本：属于故障分流；正文未给统一版本条件。未运行安装器或调整杀毒软件。

### M020-P31 Windows startup issues

- URL：https://docs.ankiweb.net/manual/platform/windows/startup-issues.md
- 主要排查：程序无窗口时检查外接/多显示器并安装最新版；Python DLL 等错误通常与 Windows 更新/库有关；另列显卡、杀毒/防火墙、管理员权限、多份安装并存和调试。
- 明确版本条件：Windows 7/8可能需额外更新；调试段针对 Anki 25.07～25.09.4 给出特定命令分支。
- 边界：页面含命令行调试和“从 Python 运行”等后续路径，本批未执行任何命令、卸载或旧版运行。

### M020-P32 Windows Display Issues

- URL：https://docs.ankiweb.net/manual/platform/windows/display-issues.md
- 主要操作：显示异常可切换图形驱动；Anki 23.10+ 可从 Preferences 更改，也给出命令行软件模式路径。
- 版本条件：全屏模式在 Anki 2.1.50+出现但曾因问题受限；Anki 23.10+配合默认 Direct3D 驱动支持全屏。
- 未知：未实际切换驱动或观察菜单栏、空白屏等结果。

### M020-P33 Copy and paste issues

- URL：https://docs.ankiweb.net/manual/platform/windows/copy-and-paste.md
- 主要事实：Windows复制粘贴问题可能由监视剪贴板的词典、剪贴板管理器或 clipping 工具与 Anki 所用工具包冲突引起。
- 操作/版本：短故障说明；无明确版本门槛。未停止任何本机程序。

### M020-P34 Text Size

- URL：https://docs.ankiweb.net/manual/platform/windows/text-size.md
- 主要操作：列出 Windows 下影响 Qt 高 DPI/缩放的环境变量，并说明可加入批处理文件以便启动。
- 明确设置：`ANKI_NOHIGHDPI=1` 会关闭部分 Qt high-DPI 支持；其余变量与值详见原件。
- 版本/未知：未给统一 Anki最低版本；未创建批处理、改环境变量或观察缩放结果。

## 未成功取得的14项

- M020-P35 Windows permission problems：请求返回200但触发 challenge检测，未保存正文。
- M020-P36 macOS
- M020-P37 Installing & Upgrading Anki on macOS
- M020-P38 Display Issues on macOS
- M020-P39 Linux
- M020-P40 Installing & Upgrading Anki on Linux
- M020-P41 Missing Libraries
- M020-P42 Display Issues on Linux
- M020-P43 Blank Main Window
- M020-P44 Packages Distributed by Linux Distributions
- M020-P45 Anki not picking up GTK theme on Gnome/Linux
- M020-P46 Wayland
- M020-P47 Input Methods on Linux
- M020-P51 Styling & HTML

后13项因同来源已停止而未请求；不能从标题补造正文操作或版本条件。Styling 的重点摘要因此仍未知，需由根代理决定是否在后续独立、经审查的采集批次处理。

## 执行命令、文件与状态

1. 写入 `work/research/batch34-anki/fetch.py`：Python标准库 `csv/json/hashlib/pathlib/urllib`；从正式 inventory 读取30个精确URL，顺序普通GET并执行停止条件。
2. `python3 work/research/batch34-anki/fetch.py`：退出0；输出 `records=30, saved=16, stopped_hosts={docs.ankiweb.net: challenge}, errors=[]`。
3. `python3 ...` 读取 `fetch-log.json` 并逐项打印状态，以及 `find work/research/batch34-anki/raw ...`：退出0。
4. `rg -n -i 'captcha|cloudflare|attention required|verify you are human|access denied' work/research/batch34-anki/raw || true`：整体退出0；已保存的16份原件无命中。
5. 对16份原件执行 `rg -n` 提取标题、操作和版本词用于人工摘要：退出0。
6. 写入本报告、生成 `SHA256SUMS` 并校验：见最终命令输出。

## 未知与证据边界

- 没有视觉观察、账户操作、实机测试或连续流程验证。
- 手册没有统一页面修订日期；`checked_at` 只是本次请求时间。
- P35触发可能来自权限故障正文中的普通词语，也可能来自挑战页；因正文未保留且未重试，本批无法判定。
- 所有版本条件均按成功取得的官方 Markdown 文本记录；未独立验证当前发行版行为。
- 自托管同步页中的命令、凭据、端口、TLS、Docker、Python、Rust均未执行。

`state_changed`：新增/更新仅限 `work/research/batch34-anki/fetch.py`、`fetch-log.json`、`report-fetch.md`、`SHA256SUMS` 与 `raw/` 下16份Markdown。未修改 `docs/`。
