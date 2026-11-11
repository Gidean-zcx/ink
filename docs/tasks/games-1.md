# games-1：五子棋（移植 CrossMux 的棋盘和电脑对手，人机、双人）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 11、12 条）。

用户在功能清单里勾了“小游戏：五子棋（人机、双人）、贪吃蛇、记忆测试”（`docs/tasks/reuse-crosspoint-1/picks.md`）。Game Boy 模拟器用户备注“游戏卡带有版权问题，要单独商量”，本任务不做。

输入：

- `docs/tasks/reuse-crosspoint-1/REPORT.md` 里 `minigames` 一行和“换入代码怎样接上 AI 与软总线”表的“棋类”一行；`port-queue.json` 的 A08。
- CrossMux 固定副本（MIT，`$INK_DEPS/../ink-hw/upstream/oss/crossmux`，提交 a6fd569）：`src/activities/apps/gomoku/`（`GomokuBoard`、`GomokuAI` 三档难度、`GomokuStore` 存档、`GomokuGameActivity` 和 `GomokuMenuActivity` 界面），`src/activities/apps/README.md`、`GameUi`、`GameSaveDebouncer.h`。

## 要做的

1. **棋盘和电脑对手原文入库**：`GomokuBoard.{h,cpp}`、`GomokuAI.{h,cpp}` 整文件拷进 `system/apps/gomoku/vendor/crossmux/`，旁边 `SOURCE.md`（来源、提交、SHA-256、改了哪几行）和 MIT 许可证。它依赖的 `HalStorage` 用一个小胶水头换成我们的存储接口；C++ 的编译设置如果主工程还没开（`reader-epub-1` 可能同时在开），在报告里写清你怎么做的，不要和它冲突：只在 `CMakeLists.txt` 里加五子棋自己的几行。
2. **界面**：照 `GomokuMenuActivity`（选人机/双人、难度、继续上一局）和 `GomokuGameActivity`（棋盘、落子光标、悔棋、重开、胜负提示）的布局和交互，用 LVGL 和我们的主题令牌重现（`system/apps/gomoku/gomoku.c`）。触摸直接点交叉点落子；墨水屏只刷变化的格子（D-127），电脑思考时显示“思考中”。
3. **存档**：照 `GomokuStore` 的内容（棋盘、轮到谁、难度、模式）存进当前成员的数据区（现有存储接口，按成员隔离），不用它的文件路径和格式。
4. **AI 接入**（人和 AI 共用）：能力 `games.start`（模式、难度）、`games.state`（棋盘、轮到谁、合法着法数、胜负）、`games.move`（落在哪）、`games.undo`、`games.reset`；页面快照给棋盘和当前回合。走现有孩子应用名单和时段规则。
5. 注册成应用（在应用列表里出现“五子棋”），图标用现有图标集里合适的。

## 范围和预算

- 只改：新增 `system/apps/gomoku/`、`CMakeLists.txt` 和固件 `ports/esp32s31/main/CMakeLists.txt` 里五子棋自己的几行、注册应用列表需要的那一处、生成的 `docs/system/capabilities.md`、新增测试、`docs/tasks/games-1/`。不碰别的任务在改的文件（阅读器、键盘、输入框、短信电话、锁屏、设置页、4G、电源、蓝牙、相机）。
- 胶水和界面新增不超过 400 行（拷来的原文不算）。
- 贪吃蛇和记忆测试没有固定的可移植来源：报告里照实写，列出找过的地方和可选的 MIT 来源，不在本任务自己写。

## 验收

1. 测试：五连判胜（横竖斜）、不能下在已有子的位置、悔棋、电脑三档都只走合法步、人机和双人、存档按成员隔离、关掉再开能继续；AI 通过能力下完一局和页面一致；反向对照。
2. 主工程普通全量 ctest 过；S31 编译通过、无新警告（报告写固件增大多少）。
3. 截图（菜单、对局、胜负，常规和 120%，`docs/tasks/games-1/`，单张不超过 300 KB）；上机核对清单。
