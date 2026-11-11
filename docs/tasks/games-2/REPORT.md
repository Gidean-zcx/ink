# games-2 执行报告

基线：`1b68f7f18197f492431ffdc794082bb77c158c2a`。分支：`claude/codex-games-2`。功能和专项完成；普通全量未全过，共享容量补丁待主会话整合。本地提交，未推送、未接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 规则、成绩、AI、生命周期、反向对照 | 通过 | 【实测】普通与 ASan/UBSan 均为常规 1048、重启 13、120% 754 条检查，零失败；覆盖配对／翻回／计步／通关、移动／吃食物／碰撞／暂停继续、家人成绩隔离与持久化、触摸和双翻页键、名单／时段及离页停止；12 组反向对照预期失败 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/run.py --build build`；内存目录换 `build-asan`；同解释器运行 `tests/games2/reverse.py --build build` |
| 2 全量、内存检查、S31 | **全量未全过**；S31、GCC 和最终游戏内存专项通过 | 【实测】普通全量 2/415 失败；ASan/UBSan 全量 1/415 失败，无 sanitizer 诊断；S31 exit=0，固件 +23,968 字节，无新警告 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2`；内存目录换 `build-asan`；`taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 3 截图 | 通过 | 【实测】十二张已逐张核对，13,529～23,398 字节；最终源码复查后重跑，全部像素一致 | 专项自动截图；链接见末表 |

## 没做到的

- 普通全量尚有 `settings-b` 容量冲突，以及独立的 `unit-s31-uhci-negative` 一次失败；ASan 全量仅剩同一容量冲突。没有改共享源文件、删能力或放宽原测试来隐藏失败。
- 未接触设备；真实屏幕的 500 ms 步进、刷新延迟和残影待上机验证。未运行 Linux 全量；本机为 macOS clang，另做真实 GCC 检查。
- 可选的 4×5 和中速档未增加，按任务允许的固定 4×4、500 ms 慢速档执行。

## 和设计不一致的

未发现与任务书冲突。上游的适配逐段写在 [翻牌来源](../../../system/apps/memory/SOURCE.md) 和 [贪吃蛇来源](../../../system/apps/snake/SOURCE.md)：翻牌使用八种系统图标、Fisher–Yates 洗牌、配对即时保留；贪吃蛇初始三格、每份食物加一分、每步一次转向。上游以长度记分，本实现分数为长度减三；未移植可选预输入队列。均保留 MIT 许可及版权，未拷图片、字体或 CSS。

## 改过的已有测试

- `tests/integration/apps.c`：应用数 20→22，说明中增加两游戏。
- `tests/integration/ai_surface.c`：登记十个新增可调用能力及必填参数。原有断言、预算和跳过条件均未改。

## 要用户定的

无新的产品选择。共享能力表的整合需主会话处理：[任务书](../games-2.md) 的“只改”名单不含 `system/policy/cap.c`，本次未改它。

【实测】新增能力后，`settings-b.c:113` 注册 25 个合成能力触到 `cap.c:30` 的 512 项上限。[CAPACITY.patch](CAPACITY.patch) 给出 544 项的一行候选；在源码副本中只改这个数，原设置测试在普通和 ASan/UBSan 下均 `write: PASS checks=410`、`restart: PASS checks=20`。仓库原文件哈希不变。命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/capacity.py --build build`，内存目录换 `build-asan`。若整合，指针表预计增加 S31 128 字节 PSRAM、macOS 256 字节【估算】；主会话须结合其它并行任务重新核算。

## 顺手发现

- 普通全量的 UHCI 负向测试在 `cell.c:16 at_ok` 断言 `r==HAL_AT_OK` 失败；该测试未链接游戏。未改驱动或断言，随后串行复测通过（28.16 秒），ASan 全量中同项也通过；未据此认定并行失败已修复。
- 新测试首轮遗漏等待异步数据库就绪和触摸转向落到下一步；已等待确切状态。反向失败的诊断曾截断中文快照，已删掉这段诊断打印，原断言保留。
- 成功翻牌统一在能力处理末尾打开页面，保证从主页调用 AI 翻牌时也有翻回定时器；专项和反向对照覆盖此路径。

## 新增的机制

- `memory.best`：任务要求的成员最好成绩记录，schema 1，8 字节，先比较步数、同步数比较用时。
- `snake.best`：任务要求的成员最高分记录，schema 1，4 字节。
- 页面 LVGL 定时器：任务要求的 1200 ms 翻回与 500 ms 蛇步进；离页删除，暂停／结束不执行游戏步进。复用 UI 任务，无新增常驻任务。
- `games.memory.changed`、`games.snake.changed`：任务要求的软总线接入，事件按成员发出。

## 要改的文档

任务书明确允许更新生成的能力清单，已运行原生成器；大面积差异来自新增分组使锚点重编号。英文表只追加本任务使用的 26 条。共享容量补丁由主会话整合后重新跑设置测试和全量；未改需求、想法、决策或已有调研／审计资料。

## 规模、来源与架构

- 【实测】产品 C/H 共 416 行，加所有新增 CMake 和启动行共 **429 行**（含测试注册行的保守口径）；另有 26 条英文词条，低于 600 行预算。
- 超过 100 行的原因：任务包含两组规则、成员成绩、异步存储接缝、十个能力、两组界面及快照。更小的“按钮直接改状态”“全家共用成绩”方案分别违反人／AI 共路和成员隔离。没有另建通用框架。
- 规则分别归各自 `engine.c`，存储复用成员数据库，定时与绘制归 UI。人、AI、翻页键共用能力；快照和状态只给可见牌面／蛇身及成绩。翻牌不泄露未翻开的图案。
- 【实测】固定提交及输入 SHA-256、每段函数对应均见两份 `SOURCE.md`；许可证文字完整，贪吃蛇许可证仅规范 CRLF→LF。

## 验证记录

- 普通全量原话：`99% tests passed, 2 tests failed out of 415`，`Total Test time (real) = 923.47 sec`；失败为 `settings-b`、`unit-s31-uhci-negative`，日志 `work/ctest.log`。
- ASan/UBSan 全量原话：`99% tests passed, 1 tests failed out of 415`，`Total Test time (real) = 1362.58 sec`；仅 `settings-b` 失败，日志 `work/asan-ctest.log`。以 `-DINK_WERROR=ON -DINK_SANITIZE=ON` 全量构建后跑全部 ctest；四个游戏 C 编译条目均含 `-fsanitize=address,undefined`。
- 两轮均有同样三项既有环境跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。
- 最后将两处 JSON 格式化字符串先复制到 17／145 字节局部缓冲，再重编并跑普通和内存版游戏的全部三阶段，均零失败；结果见 `work/final-games2.log`、`work/final-asan-games2.log`。没有为了诊断调整断言。
- 反向对照在隔离源码副本中编译，覆盖配对、翻回、计步、增长、自撞、暂停、两游戏成员隔离、离页停止、两游戏最好成绩及离页翻牌；每组 exit=1 并命中预期断言，工作源码哈希不变。结果见 `work/final-reverse.log`、`build/games2-reverse/summary.json`。
- 【实测】GCC 复查四个产品 C、专项驱动、两个登记测试和启动点：`gcccheck: 8 compiled, 0 with findings`，日志 `work/final-gcccheck.log`。两个页面 `ui-scale: 2 files, 0 violations`，能力清单校验通过。
- 【实测】翻牌最大刷新区域 9,604 像素；贪吃蛇普通／120% 分别 58,696／70,840 像素，检测到 A2 快速刷新。未整盘重画。
- 【实测】S31 基线和最终代码均 exit=0；bin **4,868,640→4,892,608 字节，增加 23,968 字节（23.41 KiB）**，5 MiB 应用分区余 350,272 字节（6.68%）。基线映射确认未链接两游戏对象。两版编译器警告均 0，SDK 的六条 CMake 警告文字相同，新增 0。日志 `work/s31-baseline.log`、`work/s31-final.log`，比较见 `work/s31-summary.json`。
- 文件、链接、范围和截图大小检查通过，`git diff --check` 无问题。按总规则第 6 节对基线至 HEAD 执行隐私扫描：0 命中（grep exit=1）。构建产物和本地路径未提交。

## 上机核对清单

- 由复核者烧录：列表中有“记忆翻牌”“贪吃蛇”，启动无两应用初始化失败日志。
- 翻牌逐张响应、不整盘闪动；失败约 1.2 秒翻回，通关显示步数／用时；两位家人分别游戏并重启，最好成绩各自保留。
- 蛇约 0.5 秒一步，核对连续转向延迟与残影；四个屏幕方向键、上一页相对左转、下一页相对右转可用；暂停、离页不动，继续与重开正常。
- 核对撞墙／自撞／吃食物及最高分；名单、禁用时段和 AI 能力受同一规则限制。模拟器结果不能替代上述实机核对。

## 截图

| 场景 | 100% | 120% |
| --- | --- | --- |
| 翻牌 开始 | [memory-start-100.png](memory-start-100.png) | [memory-start-120.png](memory-start-120.png) |
| 翻牌 进行 | [memory-playing-100.png](memory-playing-100.png) | [memory-playing-120.png](memory-playing-120.png) |
| 翻牌 结束 | [memory-end-100.png](memory-end-100.png) | [memory-end-120.png](memory-end-120.png) |
| 贪吃蛇 开始 | [snake-start-100.png](snake-start-100.png) | [snake-start-120.png](snake-start-120.png) |
| 贪吃蛇 进行 | [snake-playing-100.png](snake-playing-100.png) | [snake-playing-120.png](snake-playing-120.png) |
| 贪吃蛇 结束 | [snake-end-100.png](snake-end-100.png) | [snake-end-120.png](snake-end-120.png) |
