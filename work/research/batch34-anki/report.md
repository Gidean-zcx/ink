# 第34批：M-020 Anki 目录-only 项分类与本地正文差异检查

检查日期：2026-10-27。范围只包括现有 `anki-controls-deepening-inventory.csv` 的60项及 `work/` 中已有文件；未联网、未安装或运行 Anki、未读取旧构建。没有深入编辑、浏览正文。

## 结论

目录共60项：`directory_only` 30项、`body_read_partial` 29项、`body_read_no_task` 1项。下列30项是目录中精确标为 `directory_only` 的全部项目。

按本地文件名、原 URL、现有 fetch 日志三种方式交叉检查，**30项均没有发现已下载到 `work/`、却尚未登记入库的官方 Markdown 正文**。`work/anki-deepening/llms.txt` 与 `work/anki-controls-deepening/llms.txt` 仅含这些页的目录链接，不是正文。结论限于当前 `work/` 文件与 JSON fetch 日志；若正文被改名且完全不含来源 URL、未进日志，则无法仅凭现有记录识别。

分类是本批为排查用途做的内容性质归类，不改变正式目录状态，也不代表产品优先级。

## A. 真正用户任务（7项）

| ID | 精确主题 | URL | 归类依据 |
|---|---|---|---|
| M020-P02 | Getting Started | https://docs.ankiweb.net/manual/getting-started.md | 用户开始使用、理解基本对象与流程。 |
| M020-P04 | Getting help | https://docs.ankiweb.net/manual/getting-help.md | 用户遇到问题后的求助任务。 |
| M020-P19 | Math and Symbols | https://docs.ankiweb.net/manual/math.md | 用户输入或显示数学公式、符号。 |
| M020-P33 | Copy and paste issues | https://docs.ankiweb.net/manual/platform/windows/copy-and-paste.md | Windows 用户执行复制粘贴时的具体问题。 |
| M020-P34 | Text Size | https://docs.ankiweb.net/manual/platform/windows/text-size.md | Windows 用户调整/处理文字尺寸。 |
| M020-P47 | Input Methods on Linux | https://docs.ankiweb.net/manual/platform/linux/input-methods.md | Linux 用户输入文字的具体任务。 |
| M020-P51 | Styling & HTML | https://docs.ankiweb.net/manual/templates/styling.md | 用户编辑卡片样式与 HTML；属制卡任务邻项。 |

说明：P33、P34、P47虽带平台故障/设置性质，但直接对应用户正在进行的输入与显示操作，因此放在“真正用户任务”。P51与根代理正在检查的编辑任务相邻，本批只列目录，不深入正文。

## B. 安装/维护（16项）

| ID | 精确主题 | URL |
|---|---|---|
| M020-P23 | Self-Hosted Sync Server | https://docs.ankiweb.net/manual/sync-server.md |
| M020-P29 | Installing & Upgrading Anki on Windows | https://docs.ankiweb.net/manual/platform/windows/installing.md |
| M020-P30 | Windows installation issues | https://docs.ankiweb.net/manual/platform/windows/installation-issues.md |
| M020-P31 | Windows startup issues | https://docs.ankiweb.net/manual/platform/windows/startup-issues.md |
| M020-P32 | Windows Display Issues | https://docs.ankiweb.net/manual/platform/windows/display-issues.md |
| M020-P35 | Windows permission problems | https://docs.ankiweb.net/manual/platform/windows/permission-problems.md |
| M020-P37 | Installing & Upgrading Anki on macOS | https://docs.ankiweb.net/manual/platform/mac/installing.md |
| M020-P38 | Display Issues on macOS | https://docs.ankiweb.net/manual/platform/mac/display-issues.md |
| M020-P40 | Installing & Upgrading Anki on Linux | https://docs.ankiweb.net/manual/platform/linux/installing.md |
| M020-P41 | Missing Libraries | https://docs.ankiweb.net/manual/platform/linux/missing-libraries.md |
| M020-P42 | Display Issues on Linux | https://docs.ankiweb.net/manual/platform/linux/display-issues.md |
| M020-P43 | Blank Main Window | https://docs.ankiweb.net/manual/platform/linux/blank-window.md |
| M020-P44 | Packages Distributed by Linux Distributions | https://docs.ankiweb.net/manual/platform/linux/distro-packages.md |
| M020-P45 | Anki not picking up GTK theme on Gnome/Linux | https://docs.ankiweb.net/manual/platform/linux/gtk-theme.md |
| M020-P46 | Wayland | https://docs.ankiweb.net/manual/platform/linux/wayland.md |
| M020-P28 | Windows | https://docs.ankiweb.net/manual/platform/windows/intro.md |

说明：P28是平台父页，可能只是导航，但其子项全部是 Windows 安装、启动、显示、权限维护，因此在本批放入安装/维护。P23涉及部署和维护自托管同步服务，不是普通桌面学习操作。

## C. 概念/非功能或目录父页（7项）

| ID | 精确主题 | URL | 归类依据 |
|---|---|---|---|
| M020-P01 | Introduction | https://docs.ankiweb.net/manual/intro.md | 产品/手册介绍。 |
| M020-P03 | Background | https://docs.ankiweb.net/manual/background.md | 学习与间隔重复背景，不是完成动作。 |
| M020-P25 | Contributing | https://docs.ankiweb.net/manual/contrib.md | 文档/项目贡献者流程，非目标用户功能。 |
| M020-P26 | Resources | https://docs.ankiweb.net/manual/resources.md | 外部资源汇总。 |
| M020-P27 | Platform Notes | https://docs.ankiweb.net/manual/platform/intro.md | 平台章节父目录。 |
| M020-P36 | macOS | https://docs.ankiweb.net/manual/platform/mac/intro.md | macOS章节父目录。 |
| M020-P39 | Linux | https://docs.ankiweb.net/manual/platform/linux/intro.md | Linux章节父目录。 |

## 本地正文差异核查

对30个 URL 做了以下检查：

1. 读取 inventory 的 `raw_work_path`：30项均为空。
2. 汇总 `work/**/*fetch*log*.json`，用完整 URL 等值匹配：30项均无记录。
3. 用 URL 路径末尾文件名扫描 `work/**/*`：30项均无同名 Markdown 文件。
4. 在 `work/` 搜索完整 URL：命中主要来自两个 `llms.txt` 目录以及其他正文中的交叉链接；交叉链接不能视为目标页正文。
5. `work/anki-deepening/llms.txt` 第7–10、25、29、31–32、36、40–47、51–65、72行提供这30项的精确目录 URL/标题。

因此本批“正文已在work而未入库”的数量为 **0**。没有生成正文哈希，因为不存在对应本地正文文件。

## 读取文件与证据

- `docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv`：61行（1行表头+60项）；状态计数30/29/1。
- `work/anki-deepening/llms.txt`：用于核对精确 URL 和主题。
- `work/anki-controls-deepening/llms.txt`：同类目录原件；未当正文。
- `work/anki-deepening/fetch-log.json`、`work/anki-controls-deepening/fetch-log.json`、`fetch-extra-log.json` 及 `work/` 下其他 `*fetch*log*.json`：按 URL 检查是否存在下载记录。
- `work/anki-deepening/`、`work/anki-controls-deepening/` 及整个 `work/` 文件表：按文件名检查可能正文。

## 精确命令与退出状态

以下命令均在 `/home/workspace/reader` 执行。

1. `pwd; wc -l docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv; python3 ...; find work -type f | rg '/anki[^/]*/|anki-' | sort`：退出0。确认工作目录、60项状态计数并列出30项 directory-only 与 Anki 工作文件。
2. `python3 ...` 遍历 `work/**/*fetch*log*.json` 并按30个完整 URL 等值匹配：退出0；全部 `HITS []`。
3. `rg -l -F -f <(python3 ...) work | sort`：退出0；命中目录索引和正文交叉链接，人工核定不是目标正文。
4. `python3 ...` 用各 URL basename 扫描 `work/**/*`：退出0；30项全部无同名文件。
5. `rg -n 'manual/(...)\.md' work/anki-deepening/llms.txt`：退出0；核对精确目录行。
6. `mkdir -p work/research/batch34-anki` 与写入本报告：退出0；这是唯一有意状态变化。

## 未知与边界

- 未读取30页在线正文，因此不判断页面内部是否包含新的用户任务、版本门槛或操作反馈。
- 未根据标题推断功能已经由 Anki Desktop 实现或在任一固定构建可用。
- 未核网页更新日期，也未验证平台父页是否仅导航。
- 若存在被任意改名、无来源 URL、无 fetch 日志的正文副本，本次方法无法可靠归属；现有证据未发现此类副本。
- 未深入 editing、browsing 或其交叉链接，避免与根代理任务重复。

`state_changed`：仅新增 `work/research/batch34-anki/report.md`；未修改 `docs/`、正式目录、coverage、证据表或案例。
