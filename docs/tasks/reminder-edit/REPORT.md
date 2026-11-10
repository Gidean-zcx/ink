# reminder-edit 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。分支：`claude/codex-reminder-edit`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测】 | 创建 327 项、重启 51 项检查，均零失败；真实触摸、通知提示音、改时、重复、删除、AI 互通和成员隔离 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/reminder_edit/run.py --build build` |
| 2 | 部分通过【实测】 | 最终相关 8/8，S31、GCC 通过；普通及 ASan 全量、备份恢复独跑均已执行。仍有两项文档目录校验待同步 | 见执行记录 |
| 3 | 通过【实测】 | `build/reminder-edit/add.png`、`detail.png`、`modified.png`、`picker.png`；逐张视觉检查，无裁切、遮挡 | 同上 |

## 没做到的

- 全量一次全绿尚未达成：`capability-table-doc`、`services-mid-capability-doc` 等待主会话同步目录；ASan 整轮备份恢复曾超时，未定位原因，原限制下单独复跑通过。未连接设备，证据均为模拟器或编译结果。

## 和设计不一致的

- `docs/system/system-services-design.md` §1.3 的旧目录尚未列入本任务要求新增的 7 个能力；按总规则保留设计文档，由主会话同步。行为沿用其 §5.4 的提醒通知方式。
- 日期时间选择页在 `system/apps/calendar/calendar.c` 的 `editor_create`，复用既有圆形按钮和胶囊按钮，年、月、日、时、分分别选择；合并时可由主会话与 time-settings 统一。

## 改过的已有测试

- `tests/integration/ai_surface.c` 的 `FOR_AI` 新增 7 项：`reminder.update` 必填 `id,at,title,repeat`；`reminder.open` 必填 `id`；`reminder.draft` 参数均可选；`reminder.new/time/focus/save` 不收参数。原因：既有测试要求每个可调用能力显式登记并核对中枢收到的 schema。没有删除断言、放宽预算或修改日历／闹钟／时间测试。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】现有参数校验未落实 `minLength`；提醒入口已直接拒绝空内容，没有改通用校验器。

## 新增的机制

- 无新增产品任务、线程、状态机、持久格式或配置项。任务要求的 `reminder.update` 与六个编辑导航／保存能力复用既有能力表、短输入和页面机制。
- 测试驱动任务 `reminder-test`、测试阶段变量 `INK_REMINDER_PHASE`：仅用于验收 1 的触摸与重启检查，沿用现有测试模式。

## 要改的文档

- 能力表按任务重新生成到构建目录；合并时由主会话运行 `python3 tools/capabilities.py` 更新共享的 `docs/system/capabilities.md`。
- `docs/system/system-services-design.md` §1.3 时间行需补 `reminder.update` 和 `reminder.new/open/time/focus/draft/save`。未修改文档以迁就代码。

## 执行记录

- 【实测】开工工作区干净，HEAD 与指定基线一致；已读总规则和任务书。构建及测试使用 `taskpolicy -c utility`，并行度不超过 2。
- 【实测】最终定向证据：`build/test-out/reminder-edit-j6fd3_ie/`。提醒按既有设计走高优先级通知和短提示音，一次性提醒到点移除；测试核对两次 `reminder.fired`、通知正文和实际播放计数。
- 【实现】新增 `reminder.update` 和编辑页面的导航能力；保存仍走同一提醒提供者，按成员检查原 ID，沿用原存储格式和排程器。重复提醒的首次发生不早于选定日期。所有新界面字符串使用 `TR()`，补齐英文。
- 为什么不能更小：需要两个有 AI 快照的页面、短输入框、五项日期时间选择和四种重复选项；仅补能力或继续用语音入口不能满足手动编辑，直接输入时间字符串也不满足选择组件要求。没有另建选择控件框架或排程机制，产品代码仍低于 300 行预算。
- 【实测】反向对照：临时把三个产品 C 文件替换为指定基线、重编 `sim-reminder-edit` 后，`run.py --negative` 得到 `exit=1, pass=9, fail=1, finished=1`（缺手动入口）；随后完整恢复文件并重编成功。日志：`build/reminder-edit/reverse/{baseline-build,baseline-test,restored-build}.log`。该手动比较不注册进 ctest，不要求 CI 保留历史。
- 【实测】`taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/calendar/calendar.c system/services/time/time_caps.c system/services/time/time_service.c tests/reminder_edit/reminder_edit.c tests/integration/ai_surface.c`：原话 `gcccheck: 5 compiled, 0 with findings`，退出 0；日志 `build/reminder-gcccheck.log`。UI 尺寸检查 0 违规，翻译表校验通过。
- 【实测】能力表生成 467 个定义，普通及 ASan 的运行时均匹配 391 个能力、27 个设置：`taskpolicy -c utility python3 tools/capabilities.py --check --output build/reminder-edit-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`，ASan 将 runtime 路径换成 `build-asan/test-out/capability-table/capability-inventory.json`；均退出 0。共享能力表未改。
- 产品改动：C 源码新增 172、删除 13 行；连同 CMake、翻译文件共新增 206、删除 14 行，均低于 300 行预算。测试和报告另计。
- 【实测】普通全量：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`，退出 8，原话 `99% tests passed, 2 tests failed out of 318`，501.90 s。初次失败为 `capability-table-doc` 和尚未登记新增能力的 `integration-ai-surface`，日志 `build/reminder-full-ctest.log`。
- 【实测】补齐 AI 能力清单后，`integration-ai-surface` 通过；`services-mid-capability-doc` 因旧设计目录缺 7 项而失败。日志 `build/reminder-followup-ctest.log`。当前两项待同步检查均是文档目录，没有跳过或放宽它们。
- 【实测】最终普通版本重编后，`ctest --test-dir build --output-on-failure -j2 -R '^(reminder-edit|calendar|clock-alarm|integration-time-.*|integration-ai-surface)$'`：原话 `100% tests passed out of 8`，17.81 s，退出 0。日志 `build/reminder-final-targeted.log`；原日历、闹钟、时间场景未改。
- 【实测】S31：`taskpolicy -c utility bash ports/esp32s31/build.sh` 完整编译及最终增量重编均退出 0，生成 `build-s31/size.txt`、`size-components.txt`、`stack.txt`；日志 `build/reminder-s31-final.log`。仅编译，未上机。
- 【实测】内存检查：`build-asan` 与普通版同配置，增加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`；`UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2` 跑全部 318 项，退出 8。原话 `99% tests passed, 3 tests failed out of 318`，1264.30 s；两项文档检查失败，`backup-restore-recovery` 在 240.06 s 超时。日历、闹钟、四项时间服务、新提醒、AI 能力集成、确定性均通过，新提醒仍为 327/51 项零失败。日志 `build-asan/reminder-full-ctest.log`、`reminder-full-lasttest.log`。
- 【实测】备份恢复独跑：相同 ASan 环境，`ctest --test-dir build-asan --output-on-failure -j1 -R '^backup-restore-recovery$'`，退出 0，原话 `100% tests passed out of 1`，用时 186.33 s；原断言、240 s 超时与测试模式均未改。日志 `build-asan/reminder-backup-rerun.log`；整轮及复跑完整输出的 ASan／UBSan 错误诊断均为 0。
- 【实测】最终差异按总规则 §6 隐私模式扫描：退出 1、无匹配；差异文件检查和截图路径检查通过。未推送、未开 PR。
