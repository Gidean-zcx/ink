# clock-extras 执行报告

日期：2026-11-10。基线：`eec5d569be028b539e8778690c239a6947767864`。分支：`claude/codex-clock-extras`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 秒表操作、计次、跨页与模拟时间 | 【实测】通过 | 0.1 秒误差窗口；三次计次、暂停继续、离页、锁屏、换人、99 条及重启清零 | 专项命令见下 |
| 2 世界时钟、夏令时、跨日与成员保存 | 【实测】通过 | 欧美春秋四个切换点、跨年跨日、九选六、增删与真实进程重启 | 同上 |
| 3 局部刷新、能力、快照及中文英文界面 | 【实测】通过 | 每秒更新频率及控件不销毁；AI 开秒表、加城市；两语言截图、文字宽度检查 | 同上 |
| 4a 原有回归、普通全量 | 【实测】320/321，未全过 | 原有时钟、闹钟、计时、时间、AI、确定性通过；唯一失败是共享能力文档未同步 | 全量命令见下 |
| 4b ASan/UBSan 全量 | 进行中 | 已配置并启动完整构建 | 待补 |
| 5 S31 与 gcccheck | S31 待运行；GCC 通过 | `gcccheck: 4 compiled, 0 with findings` | GCC 命令见下 |

## 没做到的

- 普通全量的 `capability-table-doc` 未通过：总规则第 7 节禁止本任务修改共享生成文档，留给合并会话统一生成。没有跳过或放宽这个测试。
- ASan/UBSan 全量、S31 及最后一轮反向对照尚在进行。
- 未运行 Linux 原生测试或实机测试；GCC 交叉编译检查不写成 Linux 运行证据。本任务未改硬件参数、HAL 或驱动，未接触设备。

## 和设计不一致的

- 新页按 D-117 复用现有主题、胶囊、卡片与分页布局；没有画板。尚未发现和任务设计不一致的行为。

## 改过的已有测试

- `tests/integration/ai_surface.c`：只在 AI 能力清单增加 11 个新能力及必填的 `zone` 参数，原期望未改。
- `tests/capability_table/test_catalogue.py`：新增翻译标记的字面量元数据检查，原断言未改。

## 要用户定的

- 暂无；秒表按现有计时页的成员边界实现，重启不保留。

## 顺手发现

- 单独检查 `clock.c` 的尺寸：仍只有原计时页的 `y` 无法静态解析一项，基线同样报这一项（原 291 行／现 301 行）；本任务新增布局无新增违规。全量 `ui-scale-audit` 通过，未改旧计时页迁就检查器。

## 新增的机制

- 成员秒表：任务书第 1 项要求；仅内存记录单调时钟起点、累计时长和 99 个累计用时计次，清零同时停止，重启不保留。
- 成员集合 `clock.world`：任务书第 2 项要求；沿用通用数据集合的 JSON 记录、持久化和成员隔离，记录 id 为稳定时区索引加一，正文为 `{"zone":索引}`，上限六条。
- 页面可见时的一个 LVGL 秒级定时器：任务书第 1 项要求；暂停或离页即删除，锁屏不刷新，不持有保持唤醒引用。不新增任务、线程、文件格式、队列或后台重试。

## 要改的文档

- 合并时统一生成 `docs/system/capabilities.md`，加入 `clock.stopwatch`、`clock.stopwatch_start/pause/lap/reset`、`clock.world`、`clock.world_pick/add/remove`、`clock.extra_prev/next`；本任务不修改共享生成文档。生成稿在 `work/clock-capabilities.md`：486 个定义，与运行时 409 个能力、28 项设置完全匹配。
- `docs/system/ui-dependencies.md`：登记 `ClockStopwatch`、`ClockWorld` 的入口、成员数据、局部刷新、分页和 AI 快照。

## 复现与里程碑

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/clock_extras.py --build build
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/apps/clock/clock.c system/services/time/time_service.c tests/clock_extras.c tests/integration/ai_surface.c
```

- 第一里程碑：【实测】创建轮 `exit=0, pass=422, fail=0, finished=1`，重启轮 `exit=0, pass=44, fail=0, finished=1`。日志 `work/targeted.log`。
- 第二里程碑：【实测】增加“设备时分相同但日期改变”的对照后，普通全量中的专项为创建 428、重启 44 项通过；内部 RAM 最低余量均 51,328 B，门槛仍为 32,768 B，内外堆分配失败均为 0。全量原话：`99% tests passed, 1 tests failed out of 321`、`Total Test time (real) = 712.94 sec`，退出 8，无跳过；唯一失败 `136 - capability-table-doc (Failed)`。日志 `work/build-full.log`、`work/ctest-full.log`。
- 【实测】GCC 四份相关 C 文件全部编译、零问题，无跳过（`work/gcccheck.log`）。目录生成器的 18 项测试通过；撤掉两行 `TR_NOOP` 字面量识别后生成目录退出 1（`work/catalogue-reverse.log`），没有放宽元数据验证。
- 【实现】两个新页面和 11 个能力均在时钟应用内，按钮和 AI 走同一能力；成员集合只读开放给通用数据能力，写操作由城市增删能力负责六条上限。页面快照含可见计次／城市、分页、秒表运行状态及精确毫秒值；孩子沿用计时器权限。
- 【实测＋实现】专项发现并集中修正一处旧根因：手动校时后分钟任务仍沿用旧截止点，改为按新墙钟重排；世界时钟同时订阅已有 `time.changed`，覆盖日期变化但当天分钟数相同的情况。没有给每个城市另加定时器。
- 初次专项因未生成资源而启动失败；资源生成后，ASan 定位到新字段漏填语义角色导致空指针，已补齐并通过上述专项。保留诊断日志 `work/asan-debug.log`，不把这次失败写成内存检查通过。

产品改动按新增与删除合计 **220 行**（含 CMake、译文和目录生成工具，不计测试与文档），低于 250 行。为什么不能更小：两页需要操作、分页、成员保存及完整 AI 动作；已复用时间换算、通用数据集合、主题、卡片和胶囊。看过的更小方案：共享设置表无法按成员保存城市；F4 列表固定动作最多三个，秒表需要三个操作及两个翻页动作，扩展共用组件会扩大范围；省掉人工界面或快照不符合任务要求。

截图（模拟器界面缓冲，非实机；使用上述命令保留）：

- 秒表中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-zh.png`
- 秒表英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-en.png`
- 世界时钟中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-zh.png`
- 世界时钟英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-en.png`
- 同目录 `snap-*.png` 为带墨水屏残影的输出；`clean-clock-home-en.png` 核查四个入口同排且文字不截断。首页原有闹钟正文仍为旧中文文案，本任务没有扩展旧页翻译范围。
