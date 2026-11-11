# games-2 执行报告

基线：`1b68f7f18197f492431ffdc794082bb77c158c2a`。状态：实现接入完成，正在验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 专项、12 组反向对照通过 | 【实测】常规 1048、重启 13、120% 754 条检查，均零失败；含离页 AI 翻牌回归，另 12 组撤销行为均预期失败 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/run.py --build build` |
| 2 | 待验证 | 【实测】普通全量 2/415 失败（设置能力容量、独立 UHCI 负向测试）；ASan 与最终 S31 进行中 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2` |
| 3 | 十二张截图及视觉核对通过 | 【实测】十二张，13,529～23,398 字节，均低于 300 KB | 专项自动截图；见下表 |

## 没做到的

- 普通全量已出现 `settings-b` 失败：新增能力后注册其 25 个合成能力超出 `system/policy/cap.c:30` 的 512 项上限，后续活动记录检查连带失败。未改共享文件、删能力或放宽原测试；全量、ASan/UBSan 和最终 S31 结果待补。
- 未接触设备，500 ms 步进与残影表现只在模拟器验证；真机留给复核者核对。
- 未运行 Linux 全量；本机为 macOS clang，另用真实 GCC 检查八个相关 C 文件。
- 可选的 4×5 和中速档未增加；按任务允许的固定 4×4、慢速档执行。

## 和设计不一致的

未发现与任务书冲突的实现。上游适配明确列在两个 `SOURCE.md`：翻牌用八种系统图标和 Fisher–Yates，配对即时保留；贪吃蛇初始三格，每份食物加一分，每步一次转向，未移植可选预输入队列。按任务固定 4×4、500 ms 慢速档。

## 改过的已有测试

- 应用数 20→22，登记新增的两个应用。
- AI 能力名单增加十个可调用能力；未删断言、未放宽预算。

## 要用户定的

无新的产品选择。全量测试暴露的共享能力表容量冲突需主会话统一处理；该文件不在“只改”名单内，未修改。候选 [CAPACITY.patch](CAPACITY.patch) 只把 PSRAM 指针表从 512 项改为 544 项，保留全部测试断言；【实测】仅编译 544 项的源码副本、工作源码哈希不变，未修改的 `settings-b` 写入 410／重启 20 检查全过。命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/capacity.py --build build`。S31 指针表约增加 128 字节 PSRAM、macOS 约 256 字节【估算】，未增加内部 RAM。

## 顺手发现

- 新测试首轮遗漏等待异步成员数据库打开及触摸转向落到下一步，3 条失败；已等待明确的就绪／步进状态，未加盲等或改产品预算。
- 【实现】核对时发现离页 AI 翻牌也需要重建页面定时器，成功操作统一在能力处理末尾打开页面，增加回归。

## 新增的机制

- `memory.best` 成员集合：任务要求按家人存最好步数与用时；复用现有数据库。
- `snake.best` 成员集合：任务要求按家人存最高分；复用现有数据库。
- 页面 LVGL 定时器：任务要求翻回延迟和贪吃蛇 500 ms 步进；离开即删除，暂停／结束不运行（翻牌用时仍在页面内运行时更新）。
- `games.memory.changed`、`games.snake.changed` 成员事件：任务要求 AI 与软总线接入；不新增线程或常驻任务。

## 要改的文档

任务书明确允许生成能力清单，已用原有生成器更新；新增词条只追加本任务用到的 26 条英文。共享能力容量需主会话整合候选补丁后重新跑 `settings-b` 和全量。

## 里程碑

- 已核对两个固定提交及 MIT 许可证，函数对应和输入哈希见各游戏的 `SOURCE.md`。
- 【实测】S31 未注册两游戏的基线构建 exit=0，固件 4,868,640 字节；主工程基线构建 exit=0。
- 【实现】规则、UI、成员成绩、能力和测试已接入；产品 C/H 414 行；连同所有新增 CMake 和启动行共 427 行（含测试注册行的保守口径），低于 600 行；另有 26 条英文词条。
- 【实测】两个页面 `ui-scale: 2 files, 0 violations`。
- 【实测】GCC 检查覆盖四个新增产品 C 文件、专项驱动、两个登记测试及启动点：`gcccheck: 8 compiled, 0 with findings`。

## 截图

| 场景 | 100% | 120% |
| --- | --- | --- |
| 翻牌 开始 | [memory-start-100.png](memory-start-100.png) | [memory-start-120.png](memory-start-120.png) |
| 翻牌 进行 | [memory-playing-100.png](memory-playing-100.png) | [memory-playing-120.png](memory-playing-120.png) |
| 翻牌 结束 | [memory-end-100.png](memory-end-100.png) | [memory-end-120.png](memory-end-120.png) |
| 贪吃蛇 开始 | [snake-start-100.png](snake-start-100.png) | [snake-start-120.png](snake-start-120.png) |
| 贪吃蛇 进行 | [snake-playing-100.png](snake-playing-100.png) | [snake-playing-120.png](snake-playing-120.png) |
| 贪吃蛇 结束 | [snake-end-100.png](snake-end-100.png) | [snake-end-120.png](snake-end-120.png) |

## 规模与架构核对

- 超过 100 行的原因：任务是两款完整应用，需要两组规则、成员成绩、异步存储接缝、十个操作能力、两组界面与快照。更小方案“按钮直接改状态”“全局存分”分别违反人／AI 共路和成员隔离；未为合并重复代码另建框架。
- 规则位于各自 `engine.c`，存储复用成员数据库，定时与绘制归 UI。全部人机操作走 `games.memory.*`、`games.snake.*`，翻页键也调用能力；状态给可见牌面或蛇身与分数，变化事件带成员。
- 十二组反向对照在隔离副本编译，工作源码哈希不变；命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/reverse.py --build build`，最后补充一组可用 `--case memory-background` 单独重现。
- 反向对照日志曾因诊断中的中文快照被字节截断而无法解析；已只删除诊断快照文本，原断言保留，第十二组重跑 exit=1 并命中预期断言。

## 上机核对清单

- 由复核者烧录：应用列表有“记忆翻牌”“贪吃蛇”，启动没有两应用初始化失败日志。
- 翻牌逐张响应、不整盘闪动；不配对约 1.2 秒翻回，八对完成后显示步数和用时；两位家人分别通关后重启，最好成绩各自保留。
- 贪吃蛇约 0.5 秒一步，只更新变化格；观察连续转向的刷新延迟和残影。屏幕上／右／下／左四键、上一页相对左转、下一页相对右转都可用；暂停、返回主页后蛇身不动，继续与重开正常。
- 实机验证撞墙、撞自己、吃食物、最高分，以及成人／孩子名单和禁用时段；模拟器结果不替代这一项。

## 全量验证原始结果

- 【实测】普通全量原话：`99% tests passed, 2 tests failed out of 415`，`Total Test time (real) = 923.47 sec`。失败 `settings-b`、`unit-s31-uhci-negative`；跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`（已有环境条件，未改跳过规则）。日志 `work/ctest.log`。
- 【实测】`unit-s31-uhci-negative` 原失败为 `cell.c:16 at_ok: r==HAL_AT_OK`；未链接本次游戏，未改其驱动或断言。随后串行 `ctest --test-dir build --output-on-failure -j1 -R '^unit-s31-uhci-negative$'` 通过，原话 `100% tests passed out of 1`，28.16 秒。不能据此声称并行失败已修复。日志 `work/uhci-retest.log`。
- 【实测】容量实验只改隔离副本的 `MAX_CAPS`，现有设置测试 `write: PASS checks=410`、`restart: PASS checks=20`，证实容量阻塞；仓库原文件和所有原测试不变。日志 `work/capacity-check.log`。
