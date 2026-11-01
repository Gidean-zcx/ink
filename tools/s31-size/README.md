# ESP32-S31 静态内存测量

底座设计 §3.2 的内部 SRAM 账目里，“IRAM 代码 + 静态数据 + IDF 自身”一项目前只能估算（150–250 KiB）。这一项不需要实物，编译一次就能得到。本目录提供两种运行方式：

- **本机：** `bash tools/s31-size/run.sh`（macOS 或 Linux，需要能访问 GitHub 与 PyPI）。
- **GitHub：** 仓库页面 → Actions → “ESP32-S31 static size” → Run workflow；完成后在运行页面下载 `s31-size-reports`。仓库公开，Actions 不收费。

会编译三个工程：ESP-IDF 的 hello_world（系统基线）、wifi station（加上 Wi-Fi 的基线）、上游 Plus 固件（固定提交 `690a023`）。上游工程的依赖清单有遗漏（研究报告已指出），可能编译失败；前两个基线已经足够回填 §3.2 的静态占用下限。

背景：助手的云端工作区无法访问 PyPI，也不能读取 GitHub Actions 的结果，所以这一步需要在本机或 GitHub 上运行。结果拿到后贴回对话或放进本目录，助手会更新内存账与模拟器的参数。
