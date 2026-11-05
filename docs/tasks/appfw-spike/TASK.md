# 任务：应用框架实测

先把这份任务书原样复制到 `docs/tasks/appfw-spike/TASK.md`。先读 `docs/system/app-framework-design.md`（如果还没合进你的副本，读 `$HOME/Documents/Workspace/ink-codex/appfw-research/repo/docs/system/app-framework-design.md`），按它第 8 节选 2～3 个候选，必须包含“C 加 LVGL”作基线。

做法：每个候选实现同一个示例页：顶栏、8 行开关、一个文本框、一个底部按钮，点开关要调能力表里的一个能力，页面要能给出 AI 快照。代码放 `tools/appfw-spike/`，不进默认构建，不改产品代码。第三方运行时用时下载到被忽略的目录，不提交，记下地址、版本、哈希、许可证。

量这些，每个数字标【实测】和环境：代码行数；用 ESP-IDF 为 esp32s31 编译后的 Flash 和静态 RAM 增量（`~/esp/esp-idf/export.sh`，只编译，**绝不烧录，不碰设备**；环境不允许就写没测和原因）；运行时堆峰值、内部 RAM 和 PSRAM 的分别；启动运行时和建页时间（模拟器或 `tools/iss`，写清换算依据和不确定性）；点一次开关的处理时间；让 AI 读页面和写同样页面要多少字符。

产出：`docs/research/app-framework-spike.md`（一张对比表和几句结论，用平白的中文）、`docs/tasks/appfw-spike/REPORT.md`，并把结果回填到设计文档第 3、6 节，标【实测】。

| 编号 | 要求 |
| --- | --- |
| B1 | 每个候选都真能跑出示例页，有截图和命令 |
| B2 | 表里每个数字都能按记下的命令复现；量不了的写没测 |
| B3 | 不改产品代码，默认构建和全量测试不受影响 |
| B4 | 控制在一到两小时；只有真机能回答的问题单独列 |
