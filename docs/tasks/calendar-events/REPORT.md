# calendar-events 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。分支：`claude/codex-calendar-events`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 静默到点 | 通过【实测】 | 仅响铃对照触发 1 次提醒、1 条通知、60 ms 提示音；撤掉静默判断后失败 | A、B；`silent-test.log` |
| 1.2 月历显示 | 通过【实测】 | 空心圈标日程、实心点标提醒，选日混排；交换标记后失败 | A、B；`month-test.log` |
| 1.3 跨天 | 通过【实测】 | 次日、开始后及重启后可查；已响提醒保留原编辑日期且能保存；取消重叠／日期恢复／保存条件分别失败 | A、B；`cross-day-test.log`、`fired-*-test.log` |
| 1.4 全天 | 通过【实测】 | 本地零点到次日零点，结束日期不重入；取消归零后失败 | A、B；`all-day-test.log` |
| 1.5 换人隔离 | 通过【实测】 | 页面、查询隔离，孩子越权改删被拒；去掉成员过滤后失败 | A、B；`member-test.log` |
| 1.6 AI 列出与新增 | 通过【实测】 | 范围筛选、重复展开、静默新增；撤掉筛选／静默赋值分别失败 | A、B；`ai-list-test.log`、`ai-add-test.log` |
| 2.1 普通全量 | 部分通过【实测】 | 原话 `99% tests passed, 1 tests failed out of 365`；唯一失败共享能力文档；3 项中枢测试跳过 | C；`work/calendar-events/ctest-full.log` |
| 2.2 ASan／UBSan | 部分通过【实测】 | 同为 365 项、仅共享能力文档失败；无 ASan／UBSan 错误诊断；最后补丁后 11/11 通过 | D；`asan-ctest-full.log`、`asan-target-final.log` |
| 2.3 能力清单 | 通过【实测】 | 531 个定义；普通／ASan 运行时均匹配 449 个能力、31 个设置；共享文件按规则未改 | E；`capabilities-final.log` |
| 2.4 S31 | 通过【实测】 | 编译、链接、资源包、尺寸及栈报告退出 0；镜像 3,277,728 字节；编译器零告警，5 条 SDK 私有 include 配置告警 | F；`s31-build.log`、`build-s31/size.txt`、`build-s31/stack.txt` |
| 2.5 gcccheck | 通过【实测】 | 原话 `gcccheck: 5 compiled, 0 with findings`，未跳过 | G；`gcccheck-final.log` |
| 2.6 ui-scale | 通过【实测】 | 原话 `ui-scale: 51 files, 0 violations` | H；`ui-scale-final.log` |
| 2.7 隐私 | 无匹配【实测】 | 含本报告，按总规则 §6 同一模式扫描，退出 1、零匹配 | `work/calendar-events/privacy-final.log` |
| 3 设计与截图 | 通过【实现＋实测】 | 差异见下节；四张截图已目视检查，无裁切、遮挡 | A；截图路径见下方 |

日志未另写目录的均在 `work/calendar-events/`；反向测试日志在 `build/calendar-events-reverse/`。

## 没做到的

- 两轮全量未全绿：唯一失败 `capability-table-doc`，共享能力文档过期；任务明确禁止修改，由主会话合并时同步。没有跳过、放宽这项检查。
- `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 被既有环境检查跳过。未触设备，宿主机测试与 S31 编译均不代表实机验证。

## 和设计不一致的

- 按本任务扩展 `docs/system/system-services-design.md` §5.4 原先“提醒到点发高优先级通知”的范围：静默日程不发通知、不响铃、不设唤醒，开始后保留；响铃提醒沿原路径执行，有结束时间的记录保留供跨天查询。没有修改设计来迁就实现。
- 月历增加日期下方的两种标记；既有编辑页增加响铃开关、全天和结束时间，复用原日期选择页，没有新建日程服务或独立编辑页。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记 `calendar.list` 必填 `from,until`；`reminder.time` 从无参数改为参数全部可选，供结束时间选择。原因是正式能力接口扩展。没有改其他已有测试的期望、删断言或放宽预算。

## 要用户定的

- 任务未规定区间终点，暂按 `end`、`until` 不含终点；全天默认本地零点到次日零点。实现可逆，复核时确认。

## 顺手发现

- 【实现】日程共用既有 64 条闹钟／提醒总额，历史静默日程也占此额度；未扩大容量或加自动清理。
- 【实现】主页卡片 `apps/home/home.c:10` 和主页摘要 `system/ui/shell/navigation.c:879` 仍只读闹钟，不列提醒／日程；按任务范围未改主页。

## 新增的机制

- 持久记录：任务要求增加静默、全天、时长，在既有记录尾部扩展；集合和 schema 沿用原值，旧长度记录仍按默认响铃读取。已用旧长度记录验证重启兼容。
- 能力：任务要求的日期范围查询 `calendar.list`；增改删、编辑继续走原提醒提供者。范围结果超过原 4 KiB 缓冲时报错，请求缩小范围，不静默漏项。
- 新产品任务／线程、状态机、配置项、队列：无。测试驱动、测试阶段变量和反向编译产物仅用于验收。

## 要改的文档

- 主会话合并时重新生成 `docs/system/capabilities.md`；本任务在构建目录生成的副本已通过 `--check` 及两种运行时匹配。
- `docs/system/system-services-design.md` §1.3／§5.4 补记 `calendar.list`、`ring/all_day/end`、静默保留及区间投影语义。
- `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的 G8 状态由主会话同步。未修改这些共享文件。

## 执行与证据

- 【实测】普通全量退出 8，用时 940.79 s；ASan／UBSan 全量退出 8，用时 1141.39 s。日历、提醒、时间、AI 集成及确定性均通过，失败和跳过项见验收表。
- 【实测】全量之后补了已响提醒的编辑日期与保存分支（产品净增 1 行）。最终普通、ASan 均重新完整构建，并各补测日程／日历／提醒编辑／闹钟／四项时间服务／AI 能力／两项确定性共 11 项，均为 `100% tests passed out of 11`，分别 9.70 s、43.11 s。未把该补测冒称重跑全部 365 项。
- 【实测】最终新场景创建 169、重启 31 项检查均零失败。普通证据：`build/test-out/calendar-events-pimjnn6w/`；ASan：`build-asan/test-out/calendar-events-pjgmk7yo/`。默认不保留成功的临时证据；复现时用 A 的环境变量。
- 【实测】最终九组反向对照均为编译 0、链接 0、测试 1，并命中对应断言；`work/calendar-events/reverse-final.log`。只在构建目录编译变体，未改产品源码或原可执行文件，不依赖 Git 历史，不注册进 CTest。
- 【实测】最后两种宿主机构建均启用 `INK_WERROR=ON`；内存版增加 `INK_SANITIZE=ON`，全量和最终 11 项均用 `UBSAN_OPTIONS=halt_on_error=1`。S31 栈报告只计已知直接调用链，没有当作完整峰值。
- 【实测】构建、测试均用 `taskpolicy -c utility`，并行上限 2；只提交相关源文件、测试和本报告，无构建产物、设计稿、推送或 PR。

## 复现命令与截图

普通版配置使用总规则 §5 的全部依赖参数；ASan 相同，只改构建目录并增加 `INK_SANITIZE=ON`。

```bash
# A：真实触摸、默认响铃、旧格式与重启
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/run.py --build build
# B：九组手动反向对照
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/reverse.py --build build
# C、D：完整构建和两轮全量（串行执行）
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# E：生成、核对（ASan 再换 runtime 路径复核）
taskpolicy -c utility python3 tools/capabilities.py --output build/calendar-events-capabilities.md
taskpolicy -c utility python3 tools/capabilities.py --check --output build/calendar-events-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
# F：只构建 S31
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
# G：GCC 检查
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/calendar/calendar.c system/services/time/time_caps.c system/services/time/time_service.c tests/calendar_events/events.c tests/integration/ai_surface.c
# H：尺寸规范
taskpolicy -c utility python3 tools/ui_scale_check.py
```

最终 11 项的 CTest 选择条件：`-R '^(calendar-events|calendar|reminder-edit|clock-alarm|integration-time-.*|integration-ai-surface|determinism|determinism-reader)$'`。

截图：`build/calendar-events/month.png`、`cross-day.png`、`all-day.png`、`editor.png`，由 A 自动输出，不提交图片或构建产物。

## 规模

- 产品代码（含 CMake、翻译）新增 141、删除 33，净增 108 行；C／头文件净增 101 行，低于 300 行预算。
- 为什么不能更小：需同时覆盖记录兼容、静默排程、跨天投影、范围能力、共用编辑与月历标记。看过仅加能力或仅屏蔽提示音的做法，都会漏掉手动操作、跨天保留或 AI 范围查询；未另建服务、缓存或存储集合。
