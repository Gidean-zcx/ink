# calendar-events 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。分支：`claude/codex-calendar-events`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 静默到点 | 通过【实测】 | 唯一响铃对照触发通知；移除静默判断后断言失败 | 下方命令 A、B；`silent-test.log` |
| 1.2 月历显示 | 通过【实测】 | 日程空心圈、提醒实心点；选日混排；交换标记后失败 | A、B；`month-test.log` |
| 1.3 跨天 | 通过【实测】 | 次日查询、列表、开始后与重启后保留；取消区间重叠后失败 | A、B；`cross-day-test.log` |
| 1.4 全天 | 通过【实测】 | 本地零点到次日零点，结束日期不重入；取消归零后失败 | A、B；`all-day-test.log` |
| 1.5 换人隔离 | 通过【实测】 | 查询和页面隔离，跨成员改删被拒；去掉成员过滤后失败 | A、B；`member-test.log` |
| 1.6 AI 列出与新增 | 通过【实测】 | 范围筛选、重复展开、静默新增；分别撤掉筛选／静默赋值后失败 | A、B；`ai-list-test.log`、`ai-add-test.log` |
| 2.1 普通全量 | 部分通过【实测】 | 原话 `99% tests passed, 1 tests failed out of 365`；唯一失败共享能力文档；3 项中枢测试跳过 | C；`work/calendar-events/ctest-full.log` |
| 2.2 ASan／UBSan | 部分通过【实测】 | 全量同为 365 项、仅共享能力文档失败；无 ASan／UBSan 诊断；最后补丁后 11/11 通过 | D；`work/calendar-events/asan-ctest-full.log`、`asan-target-final.log` |
| 2.3 能力清单 | 通过【实测】 | 531 个定义；运行时 449 个能力、31 个设置匹配；共享文件按规则未改 | E |
| 2.4 S31 | 未运行 | 等内存检查完成后串行编译 | F |
| 2.5 gcccheck | 初检通过【实测】 | `gcccheck: 5 compiled, 0 with findings`，最终复核待补 | G |
| 2.6 ui-scale | 通过【实测】 | `ui-scale: 51 files, 0 violations` | H |
| 2.7 隐私 | 初检无匹配【实测】 | 最终提交后再扫描 | 总规则 §6 模式 |
| 3 设计与截图 | 通过【实测＋实现】 | 见下方差异和截图路径；已目视检查 | A |

## 没做到的

- 普通全量未全绿：唯一失败为共享能力文档过期，任务明确禁止修改该文件。三项中枢测试因环境条件跳过。ASan 全量同样只有该文档检查失败，S31 结果待补；不把宿主机结果称为实机验证。

## 和设计不一致的

- 任务要求在提醒中加入静默日程，扩展了 `docs/system/system-services-design.md` §5.4 原先“提醒到点发高优先级通知”的范围：仅响铃提醒走该路径。旧提醒行为保留，没有修改设计来迁就实现。
- 月历日期下方增加空心圈（日程）与实心点（提醒）；既有编辑页增加响铃开关、全天、结束时间并复用日期选择页，无新独立画板。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记新能力 `calendar.list` 的必填 `from,until`；`reminder.time` 从无参数改为全部可选参数（结束时间选择）。原因是正式能力接口发生扩展；原有断言和预算未放宽。

## 要用户定的

- 任务未规定区间终点语义，暂按结束时间及 `until` 不含终点；全天默认本地零点到次日零点。实现可逆，复核时确认。

## 顺手发现

- 【实现】共用既有 64 条闹钟／提醒总额，历史静默日程也占此额度；没有扩大容量或新增自动清理。
- 【实现】主页卡片 `apps/home/home.c:10` 和主页摘要 `system/ui/shell/navigation.c:879` 仍只读闹钟，不列提醒／日程；本任务仅扩展“接下来”和月历，未改任务外主页。

## 新增的机制

- 持久记录：按任务要求，在既有记录尾部增加静默、全天和时长；旧长度记录仍按默认响铃读取，集合和 schema 沿用原值。
- 能力：任务要求的日期范围查询 `calendar.list`；增改、编辑仍走原提醒提供者。
- 新产品任务／线程、状态机、配置项、队列：无。测试驱动与反向编译产物仅用于验收。

## 要改的文档

- 合并时由主会话重新生成 `docs/system/capabilities.md`；本任务生成副本已通过 `--check` 和运行时匹配。
- `docs/system/system-services-design.md` §1.3／§5.4 补记 `calendar.list`、`ring/all_day/end` 以及静默保留、区间投影语义；`docs/system/ui-dependencies.md` 的 G8 状态由主会话同步。

## 执行记录

- 【实测】工作区开工干净，HEAD 与指定基线一致；已读总规则、任务书及相关项目资料。
- 构建与测试用 `taskpolicy -c utility`，并行上限 2。环境变量使用任务指定目录，不把本机绝对路径写入提交。
- 【实测】首轮定向通过：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/run.py --build build`，创建 159 项、重启 31 项检查，均零失败。证据：`build/test-out/calendar-events-a0d1bxfw/`；日志：`work/calendar-events/target-test.log`。
- 【实现】既有记录尾部扩展静默、全天、时长字段；旧长度记录按默认响铃读取。静默日程不进入排程唤醒或通知路径，开始后保留；有时长的已响提醒也保留供跨天查询。没有新增存储集合或后台任务。
- 【实现】`calendar.list` 使用本地日期的左闭右开范围，列出重叠日程和提醒并展开重复；结果超过原 4 KiB 缓冲时报错，请求缩小范围。`reminder.add/update` 接受 `ring`、`all_day`、`end`；未传的新提醒默认响铃，更新未传的新字段保留，空结束字符串清除时长。
- 【实测】初次 gcccheck 原话 `gcccheck: 5 compiled, 0 with findings`；ui-scale 原话 `ui-scale: 51 files, 0 violations`。最终检查和反向对照待补齐。
- 【实测】普通版全量已跑完，退出 8，原话 `99% tests passed, 1 tests failed out of 365`，940.79 s。唯一失败 `capability-table-doc`：共享能力文档过期，按规则不修改；`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 被既有环境检查跳过。日志：`work/calendar-events/ctest-full.log`。
- 【实测】全量中的 `calendar`、`calendar-events`、`reminder-edit`、时间服务、AI 集成和确定性检查均通过。新月历标记已改为日期下方独立圆圈／圆点，已逐张检查最终月历、跨天、全天和编辑页截图。
- 【实测】生成及运行时核对：`taskpolicy -c utility python3 tools/capabilities.py --output build/calendar-events-capabilities.md`，再以 `--check --output build/calendar-events-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json` 校验，退出 0；原话 `Runtime matches: 449 capabilities, 31 settings`、`PASS: 531 definitions, build/calendar-events-capabilities.md`。
- 【实测】七组反向对照全部通过：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/reverse.py --build build`，退出 0。各变体编译、链接均退出 0，运行均退出 1，分别命中静默到点、月历区分、跨天、全天、成员隔离、AI 范围筛选、AI 静默新增断言。日志：`work/calendar-events/reverse.log`、`build/calendar-events-reverse/*-test.log`。该手动脚本不依赖 Git 历史、不改源码或原可执行文件、不注册进 CTest。

## 复现命令

先按总规则 §5 配置普通版；ASan 使用相同依赖参数与 `INK_WERROR=ON`，只改目录并增加 `INK_SANITIZE=ON`。

```bash
# A：定向（含真实触摸、默认响铃、旧格式与重启）
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/run.py --build build
# B：九组手动反向对照，详细日志在 build/calendar-events-reverse/
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/reverse.py --build build
# C：普通全量
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# D：ASan + UBSan 全量
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# E：生成与核对
 taskpolicy -c utility python3 tools/capabilities.py --output build/calendar-events-capabilities.md
 taskpolicy -c utility python3 tools/capabilities.py --check --output build/calendar-events-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
# F：只构建 S31
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
# G：最终 GCC 检查
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/calendar/calendar.c system/services/time/time_caps.c system/services/time/time_service.c tests/calendar_events/events.c tests/integration/ai_surface.c
# H：尺寸规范
 taskpolicy -c utility python3 tools/ui_scale_check.py
```

截图：`build/calendar-events/month.png`、`cross-day.png`、`all-day.png`、`editor.png`。由 A 自动输出，不提交图片或构建产物。

## 规模

- 产品代码（含 CMake、翻译）新增 141、删除 33，净增 108 行；C／头文件净增 101 行，低于 300 行预算。
- 为什么不能更小：需要同时覆盖记录兼容、静默排程、重叠日期投影、范围能力、共用编辑页和月历标记。看过仅加能力或仅屏蔽提示音的做法，都会漏掉手动操作、跨天保留或 AI 范围查询；未另建服务或存储集合。

- 【实测】ASan／UBSan 全量退出 8，原话 `99% tests passed, 1 tests failed out of 365`，1141.39 s；唯一失败仍为 `capability-table-doc`，三项中枢测试同样跳过；整轮无 ASan／UBSan 错误。日志：`work/calendar-events/asan-ctest-full.log`。
- 【实测】全量后补了已响提醒的编辑日期与保存分支（产品净增 1 行）。最终普通版、ASan 版重新完整构建后，各跑日程／日历／提醒编辑／闹钟／四项时间服务／AI 能力／两项确定性共 11 项，原话均为 `100% tests passed out of 11`，用时分别 9.70 s、43.11 s。新场景创建 169、重启 31 项检查均零失败。两轮全量先于这一小补丁，最终补测单列，未冒称最后补丁重跑全部 365 项。
- 【实测】最终反向对照增为九组，全部变体编译、链接退出 0，测试退出 1；新增两组分别移除已响提醒的原日期恢复、已响提醒保存条件，命中日期和保存失败断言。日志：`work/calendar-events/reverse-final.log`、`build/calendar-events-reverse/fired-*-test.log`；最终普通版证据 `build/test-out/calendar-events-pimjnn6w/`。
