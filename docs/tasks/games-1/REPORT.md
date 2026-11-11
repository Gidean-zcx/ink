# games-1 执行报告

基线：`4a8b0af038ff6991ed130f3ee746a199fb302952`。工作分支：`claude/codex-games-1`。本报告随里程碑更新；没有推送或接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 棋局、能力、存档及反向对照 | 专项通过 | 【实测】main 289、reboot 18、big 79 条检查，0 失败；三组撤销行为的反向对照均 exit=1 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/gomoku/run.py --build build`；同解释器运行 `tests/gomoku/reverse.py --build build` |
| 2 全量测试、内存检查、S31 | 普通全量未全过；其余待完成 | 【实测】原话：`97% tests passed, 12 tests failed out of 406`，`Total Test time (real) = 941.56 sec`；C++ 接入影响的五项回归修正后全部通过，UHCI 单独重跑通过 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2`；详见验证记录 |
| 3 截图与上机清单 | 模拟器截图通过，真机未测 | 【实测】菜单/对局/胜负各 100% 和 120% 六张，22,800～35,169 字节，已逐张查看；普通/120% 落子最大区域分别 54,696 / 66,768 px | 专项测试自动截图；见下方链接 |

## 没做到的

- 英文词条未并入共享 `assets/i18n/en.tsv`：任务的“只改”范围不包含它；新增文字已用 `TR` 标记，中文可用，英文目前回退中文。`i18n-literals` 因缺词条失败，不掩盖此结果。
- 普通全量尚有六项需主会话同步共享项：`i18n-shell-remaining`、`i18n-extras`、`i18n-literals`（英文词条）；`capability-table-doc`（能力文档）；`integration-ai-surface`（静态能力名单）；`integration-apps`（应用总数断言）。没有把失败写成通过。
- 贪吃蛇、记忆测试：在固定副本 crosspoint-reader、CrossInk、crosslink、crossmux 中按文件名搜索 `snake`、`memorygame`、`memory-game`、`memorytest`、`memory-test`、`matchinggame`，只命中字体缓存的 `SdCardFontMemoryTest.cpp`，不是游戏；并核对 CrossMux 应用 README 和 gomoku 邻近目录，未找到独立可移植模块。没有自行实现。
- 候选（2026-11-12 查许可证，未下载或集成）：[JavaScript-Snake](https://github.com/patorjk/JavaScript-Snake/blob/main/LICENSE) 为 MIT、依赖浏览器 DOM；[memory](https://github.com/taniarascia/memory/blob/master/LICENSE) 的逻辑为 MIT、依赖 JavaScript/DOM。它们不是现成 C/LVGL 引擎，移植前还需固定提交、拆出逻辑并核对素材许可。

## 和设计不一致的

- 上游先触摸瞄准再按落子；本任务明确要求直接点交叉点落子，已按任务重现。沿用上游黑棋先手、人机悔棋退两手、9/15 路规则和三档搜索；不移植统计及计时页面（验收未要求）。

## 改过的已有测试

- 无。

## 要用户定的

- 暂无；按本任务书执行，不修改共享需求和决策文件。

## 顺手发现

- 普通模拟器首次运行因为只构建专项目标而缺界面字体、图标，补齐全部资源后继续。随后 ASan 定位到本次新增快照漏填 `role` 的空指针，已补齐字段；没有改公用快照接口。
- 新写测试最初漏设父密码，导致策略变更在后台被拒绝；已照现有家长确认流程用合成密码完成设置并等待确切状态，没有放宽策略。新测试还改为拒绝落子后重读状态、终局只暴露可用动作；基线的已有测试未改。
- 普通全量的 `unit-s31-uhci-negative` 出现假 GPIO 对端的 2 秒等待断言；随后 `ctest -j1 -R '^unit-s31-uhci-negative$'` 所在专项串行轮通过（28.85 秒）。未改 4G 源码、测试、等待时间或预算，不能据此声称其并行稳定性已解决。

## 新增的机制

- `gomoku.game` 成员数据库记录（任务第 3 条要求）：现有数据库内一个 455 字节、全字节字段的记录，保存模式、难度、盘面大小、棋子、历史及胜者；回合由历史步数恢复，不另建文件或路径。
- `games.changed` 成员事件（任务 AI/软总线接入要求）：与保存成功同步发出。没有新线程、任务、重试、缓存或配置项；搜索使用既有 `exec.bg`，LVGL 仅在 UI 执行器操作。

## 要改的文档

- 总规则第 7 节要求共享能力表留给主会话；任务书允许改该生成文件，存在冲突。本次按公共协作限制保留共享文件，将生成增量放在 [CAPABILITIES.patch](CAPABILITIES.patch)，合并时应用或运行 `python3 tools/capabilities.py`。当前 `capability-table-doc` 预期失败。
- 将 [I18N-ADD.tsv](I18N-ADD.tsv) 的 30 条英文词条并入 `assets/i18n/en.tsv`，再把页面加入主工程和 S31 的 i18n 源清单；不得以删 `TR`、放宽检查代替。
- [TEST-INVENTORY.patch](TEST-INVENTORY.patch) 给出共享测试所需的两处更新：`tests/integration/ai_surface.c` 增列五个能力（start 参数全可选，move 必填 row/col）；`tests/integration/apps.c:146` 总数 19→20 并加五子棋说明。只交补丁，未改或放宽已有测试。

## 实现规模和来源

- 【实测】原文四文件均来自 CrossMux `a6fd569f7552db4ea1a6550ced7bc06568ae0e61`，无修改，SHA-256 及 MIT 许可见 [SOURCE.md](../../../system/apps/gomoku/vendor/crossmux/SOURCE.md)。
- 主工程原本已为 4G 开启 C++；只给五子棋所在目标加源码、兼容头路径和 C++20 要求，没有改顶层语言或阅读器配置。S31 的追加也仅在 `main/CMakeLists.txt`，未修改共享组件 CMake。
- 为接入这次新增的 C++ 静态对象，关闭主工程的 `CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES`，保持纯 C 测试模板由 C 驱动链接，CMake 自动补 C++ 运行库；含 C++ 源码的目标仍走 C++。单独固定新式模拟器 helper 未涵盖旧式 `sim-media` 模板，最终改在语言传播这一处处理，未逐个改测试。
- 【实测】GCC 对原文 `GomokuAI.cpp:70` 的构造参数 `aiSide` 报 `-Wshadow`，Apple clang 不报。为保持原文，仅这个第三方文件关闭 `-Wshadow`；产品胶水仍保持完整 `INK_WERROR`。修改后重跑 GCC 检查。
- 超过 100 行的原因：本任务是新应用移植，需要成员存档、异步搜索接缝、五个能力、菜单、棋盘和快照。更小方案“界面直接调用引擎”“在 UI 里搜索”“沿用全局存档”分别违反人/AI 共路、交互响应和成员隔离要求。
- 【实测】最终检查页面、引擎胶水、两个原文 C++ 文件及测试驱动：`gcccheck: 5 compiled, 0 with findings`。

## 验证记录

- 普通全量原始日志：`work/ctest.log`（12 失败、3 跳过；跳过为三项中枢 Python 环境测试，未改跳过条件）。
- C++ 链接驱动修正后，`ctest --test-dir build --output-on-failure -j1 -R '^(services-mid-regressions|file-index-regressions|confirm-durable|thumb-wiring|index-durable)$'` 原话：`100% tests passed out of 5`，51.16 秒；日志 `work/ctest-link-2.log`。
- 专项测试：`work/gomoku-test-4.log`；反向对照：`work/gomoku-reverse.log`、`build/gomoku-reverse/summary.json`，分别移除判胜、禁用能力落子、取消换成员重载，测试都确实失败，工作源码哈希未变。
- `ui-scale: 52 files, 0 violations`；四个原文文件逐字比较一致。日志和构建目录均在忽略范围，未提交产物。

## 上机核对清单

- 由主会话烧录，观察启动日志没有 `gomoku failed to start`；成人/孩子允许名单中都能看到“五子棋”，禁用名单和禁用时段确实拦截。
- 在屏幕四角和中心交叉点落子，核对触摸坐标、黑白棋子、光标及悔棋；落子仅刷新变化格和回合文字，不整盘闪动；累计清残影仍由 D-127 公用策略决定。
- 三档人机均出现“思考中”，搜索期间返回键、来电和锁屏仍响应；测 S31 实际思考时长、`exec.bg` 栈高水位与峰值内存。模拟器 CPU 成本是估算，不能当真机性能。
- 横、竖、两种斜线判胜，悔掉胜手后可继续；重开不改模式/难度。切两位家人、退出应用、断电重启，分别继续各自棋局。
- AI 依次调用 `games.start/state/move/undo/reset`，每步盘面、回合和胜负与页面一致。

## 截图

| 场景 | 常规 | 120% |
| --- | --- | --- |
| 菜单 | [menu-100.png](menu-100.png) | [menu-120.png](menu-120.png) |
| 对局 | [game-100.png](game-100.png) | [game-120.png](game-120.png) |
| 胜负 | [win-100.png](win-100.png) | [win-120.png](win-120.png) |
