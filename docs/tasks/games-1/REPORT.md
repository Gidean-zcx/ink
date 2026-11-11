# games-1 执行报告

基线：`4a8b0af038ff6991ed130f3ee746a199fb302952`。工作分支：`claude/codex-games-1`。本报告随里程碑更新；没有推送或接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 棋局、能力、存档及反向对照 | 部分完成，验收仍在跑 | 【实测】普通编译通过；首轮缺资源，补齐后运行崩溃，正在用 ASan 定位 | `taskpolicy -c utility cmake --build build --target sim-gomoku -j2` |
| 2 全量测试、内存检查、S31 | 待验证 | 基线与工作区已核对，初始无改动 | `git status --short`、`git rev-parse HEAD` |
| 3 截图与上机清单 | 待验证 | 尚未生成 | 待记录 |

## 没做到的

- 英文词条未并入共享 `assets/i18n/en.tsv`：任务的“只改”范围不包含它；新增文字已用 `TR` 标记，中文可用，英文目前回退中文。`i18n-literals` 因缺词条失败，不掩盖此结果。
- 贪吃蛇、记忆测试：在固定副本 crosspoint-reader、CrossInk、crosslink、crossmux 中按文件名搜索 `snake`、`memorygame`、`memory-game`、`memorytest`、`memory-test`、`matchinggame`，只命中字体缓存的 `SdCardFontMemoryTest.cpp`，不是游戏；并核对 CrossMux 应用 README 和 gomoku 邻近目录，未找到独立可移植模块。没有自行实现。
- 候选（2026-11-12 查许可证，未下载或集成）：[JavaScript-Snake](https://github.com/patorjk/JavaScript-Snake/blob/main/LICENSE) 为 MIT、依赖浏览器 DOM；[memory](https://github.com/taniarascia/memory/blob/master/LICENSE) 的逻辑为 MIT、依赖 JavaScript/DOM。它们不是现成 C/LVGL 引擎，移植前还需固定提交、拆出逻辑并核对素材许可。

## 和设计不一致的

- 上游先触摸瞄准再按落子；本任务明确要求直接点交叉点落子，已按任务重现。沿用上游黑棋先手、人机悔棋退两手、9/15 路规则和三档搜索；不移植统计及计时页面（验收未要求）。

## 改过的已有测试

- 无。

## 要用户定的

- 暂无；按本任务书执行，不修改共享需求和决策文件。

## 顺手发现

- 暂无。

## 新增的机制

- `gomoku.game` 成员数据库记录（任务第 3 条要求）：现有数据库内一个 455 字节、全字节字段的记录，保存模式、难度、盘面大小、棋子、历史及胜者；回合由历史步数恢复，不另建文件或路径。
- `games.changed` 成员事件（任务 AI/软总线接入要求）：与保存成功同步发出。没有新线程、任务、重试、缓存或配置项；搜索使用既有 `exec.bg`，LVGL 仅在 UI 执行器操作。

## 要改的文档

- 总规则第 7 节要求共享能力表留给主会话；任务书允许改该生成文件，存在冲突。本次按公共协作限制保留共享文件，将生成增量放在 [CAPABILITIES.patch](CAPABILITIES.patch)，合并时应用或运行 `python3 tools/capabilities.py`。当前 `capability-table-doc` 预期失败。
- 将五子棋新增英文词条并入 `assets/i18n/en.tsv`，再把页面加入主工程和 S31 的 i18n 源清单；不得以删 `TR`、放宽检查代替。

## 实现规模和来源

- 【实测】原文四文件均来自 CrossMux `a6fd569f7552db4ea1a6550ced7bc06568ae0e61`，无修改，SHA-256 及 MIT 许可见 [SOURCE.md](../../../system/apps/gomoku/vendor/crossmux/SOURCE.md)。
- 主工程原本已为 4G 开启 C++；只给五子棋所在目标加源码、兼容头路径和 C++20 要求，没有改顶层语言或阅读器配置。S31 的追加也仅在 `main/CMakeLists.txt`，未修改共享组件 CMake。
- 超过 100 行的原因：本任务是新应用移植，需要成员存档、异步搜索接缝、五个能力、菜单、棋盘和快照。更小方案“界面直接调用引擎”“在 UI 里搜索”“沿用全局存档”分别违反人/AI 共路、交互响应和成员隔离要求。
- 【实测】`gcccheck.py build system/apps/gomoku/gomoku.c tests/gomoku/device.c`：`gcccheck: 2 compiled, 0 with findings`。后续变更后重跑。

## 上机核对清单

- 待实测后补充。
