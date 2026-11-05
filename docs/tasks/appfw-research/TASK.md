# 任务：应用框架调研和设计

先把这份任务书原样复制到 `docs/tasks/appfw-research/TASK.md`。这是设计任务，不写产品代码。产出只有：`docs/system/app-framework-design.md`（设计）、`docs/research/app-framework-survey.md`（调研和来源）、`docs/tasks/appfw-research/REPORT.md`。不要改 `docs/requirements.md`、`docs/ideas.md`、`docs/decisions/README.md`，建议的决定写在设计文档里，等用户选。

用户的原话（2026-11-05）：

> 应用框架我感觉不止这一点东西吧 还有文本框 按钮 输入框什么乱七八糟的这些该不该做啊 我不太确定 你是要复用lvgl还是怎么玩 开发应用用什么语言完成 是类似typescript 还是安卓这样的玩法 还是每个应用独立设置 哪种可能和性能更合适一些 好像输入和应用的适配应该是一个难题 哪些应该弹出输入法 弹出后页面如何处理 哪些更符合我们ai first的理念也更好做集成和开发？

## 背景，先读

- 已定：D-08（应用模型分阶段：内置 → 声明式页面 → 脚本）、D-10（LVGL 9 + 墨水屏适配层）、R-037（不在设备上运行第三方原生代码）、R-056 和 D-65（语音输入）、R-061（软总线加原子能力，AI 原生）、D-105（默认自动）。可以提出重议，但要明确说这是重议，并说清为什么。
- 文档：`docs/system/architecture.md` 的 D-08 一节、`docs/system/foundation.md` §4.8（统一输入会话）、§5.7、§6.6（声明式页面）、`docs/system/system-services-design.md` §9、`docs/system/ui-dependencies.md` §4.2、`docs/product/system-ui-spec.md`（第 1、3、5、10 节和键盘相关流程）、`docs/system/risks-and-spikes.md` 的 SP-05、SP-08、RK-04、RK-08、RK-19。
- 代码：`system/ui/`、`system/ui/shell/`、`system/ui/textfield.c`、`system/policy/cap.c`、`apps/`、`system/apps/remote/`。界面框架正在另一个副本开发，最新状态读 `$HOME/Documents/Workspace/ink-codex/ui-shell/repo/system/ui/`（只读）。
- 设计稿：`$INK_DESIGN/png/`（`Components`、`Foundation`、`ImePinyin`、`ImeEdit`、`ImeLong`、`ImeVoice`、`SetNetwork` 等）。
- 硬件：ESP32-S31，RISC-V，320 MHz，内部 RAM 约 512 KB 且很紧，PSRAM 16 MB，Flash 16 MB，墨水屏 684×1216、16 灰阶，刷新慢。

## 文档要求

用平白的中文写，读者不是嵌入式工程师。每个外部事实带链接和查阅日期；数字不许凭记忆写，查不到就写“没查到”。证据分级：【文档】【实现】【实测】【估算】。判断和建议标明是建议。

`app-framework-design.md` 必须有这些节：

0. **一页结论**：直接回答用户的问题：文本框这类要不要做、哪些复用 LVGL、应用用什么语言、TypeScript 式还是安卓式还是别的、什么时候弹输入法、弹出后页面怎么办、哪种最 AI first。先给推荐，再给理由。
1. **现状**：现有代码已经有什么，缺什么，引用文件和行号。
2. **控件清单**：对照 LVGL、Android、iOS、ArkUI 和小设备框架，列全：文字、段落、按钮、图标按钮、单行和多行输入、搜索框、数字和密码输入、勾选、单选、开关、滑杆、分段、选择器（日期、时间、数字）、列表、网格、表格、页签、分页、对话框、底部面板、提示、进度、图片、头像、角标、卡片、空状态、菜单等。每项写：现在有没有、复用 LVGL 还是自己包一层、墨水屏要怎么改、给 AI 的语义、优先级（首版、以后、不做）。
3. **应用模型和语言对比**：至少比较：全部 C 加 LVGL 编进固件（参考 ESP-Brookesia）；声明式页面加原生能力（现有 A2UI-lite 方向、LVGL XML）；小脚本（Lua、Berry、MicroPython）；JS 和 TS（Moddable XS 加 Piu、QuickJS、JerryScript、MicroQuickJS，参考 Xiaomi Vela 快应用、鸿蒙轻量系统 JS UI、Zepp OS）；WebAssembly（WAMR）；中枢渲染；为什么安卓式不行。也看 Pebble、Garmin Connect IQ、Flipper Zero、Playdate 这些小设备的做法。比较项：Flash 占用、运行内存、启动和建页时间、S31 的 RISC-V 支持、应用隔离和权限、单独更新、调试和模拟器、对墨水屏刷新的控制、许可证、**AI 能不能读懂、操作、生成**。结论要分对象：系统应用、中枢或 AI 生成的应用、以后可能的第三方应用，不一定用同一种。
4. **输入和应用适配**：按字段语义（文字、搜索、数字、电话、密码、日期、单选等）给表：弹键盘、弹数字键盘、用选择器还是语音，哪些根本不用键盘。弹出后页面怎么办：比较上推、缩小、盖住、进全屏编辑页，用规范里的刷新代价评估，给推荐。覆盖：长表单、下一项和完成键、关闭和返回、被来电打断、实体键、回到原位置、换成员、密码不给 AI、AI 代填。参考 Android `windowSoftInputMode` 和全屏输入、iOS、其他墨水屏设备，查不到的写没查到。和规范已有的键盘流程冲突的地方单独列。
5. **AI first**：页面语义树、界面和 AI 读同一份描述、能力清单、AI 代填和操作、AI 生成页面时的权限边界。说明每个方案在这里差在哪。
6. **推荐方案**：分层图（Mermaid）、接口草图（C 接口或页面描述的小样例，够看懂就行）、和现有 D-08、D-10 的关系、对内存和性能的预估【估算】。
7. **要用户定的**：最多 5 项，每项写选项、推荐、代价、能不能改回来，用平白的话。编号用“候选 1”这样，不要占决策编号。
8. **实测计划**：下一个任务要量哪 2～3 个候选、量什么，哪些只有真机能答。
9. **落地拆分**：按一小时左右一个任务拆开，说明 `docs/tasks/backlog.md` 里 `app-framework-1`、`app-framework-2`、系统应用各节要怎么改。

## 验收

| 编号 | 要求 |
| --- | --- |
| A1 | 上面每节都有，一页结论能让不懂嵌入式的人看懂并做决定 |
| A2 | 比较表没有无来源的数字；调研文档列出每个来源的链接、日期、版本 |
| A3 | 控件清单和设计稿、规范逐项对过，没有漏掉设计稿用到的控件 |
| A4 | 输入规则表覆盖规范里所有有输入的页面 |
| A5 | 这一步控制在一小时左右，不写产品代码，不改任务书允许范围外的文件 |
