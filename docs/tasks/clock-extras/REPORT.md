# clock-extras 执行报告

日期：2026-11-10。基线：`eec5d569be028b539e8778690c239a6947767864`。分支：`claude/codex-clock-extras`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 秒表 | 【实测】通过 | 0.1 秒误差窗口；三次计次、暂停继续、离页、锁屏、换人、99 条及重启清零 | 专项命令见下 |
| 1b 世界时钟 | 【实测】通过 | 欧美春秋四个切换点、跨年跨日、九选六、增删与真实进程重启 | 同上 |
| 1c 局部刷新 | 【实测】通过 | 每秒更新频率；秒／分钟变化时只改文字、时钟控件不销毁；反向整页重画被检出 | 同上；反向命令见下 |
| 1d AI 能力与快照 | 【实测】通过 | AI 开秒表、加城市；完整动作及生产快照容量检查，孩子同样可用 | 专项；全量中的 `integration-ai-surface`、能力目录检查 |
| 2a 原有回归、普通全量 | 【实测】320/321，未全过 | 原有时钟、闹钟、计时、时间、AI、确定性通过；唯一失败是共享能力文档未同步 | 全量命令见下 |
| 2b ASan/UBSan 全量 | 【实测】320/321，未全过 | 同一个文档检查失败；其余测试通过，未检出 Sanitizer 错误 | 全量命令换 `build-asan` |
| 2c S31 | 【实测】编译通过 | 退出 0；实际镜像 1,391,056 B；只编译，未烧写 | S31 命令见下 |
| 2d GCC | 【实测】通过 | `gcccheck: 4 compiled, 0 with findings`，无跳过 | GCC 命令见下 |
| 3 中英文截图 | 【实测】通过 | 四张截图已逐图核看；按钮文字宽度检查通过 | 专项命令；路径见末尾 |

## 没做到的

- 普通及 ASan/UBSan 全量的 `capability-table-doc` 未通过：总规则第 7 节禁止本任务修改共享生成文档，留给合并会话统一生成。没有跳过或放宽这个测试，不能写成全量全过。
- 未运行 Linux 原生测试或实机测试；GCC 交叉编译检查不写成 Linux 运行证据。本任务未改硬件参数、HAL 或驱动，未接触设备。
- 秒表沿用计时页的固定成员槽；未扩展反复删除、重建家人后的槽回收逻辑，本次仅验收现有成员间切换。

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
- 服务设计的时钟段：登记 `clock.world` 成员集合及秒表重启清零规则。

## 复现与里程碑

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/clock_extras.py --build build
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/apps/clock/clock.c system/services/time/time_service.c tests/clock_extras.c tests/integration/ai_surface.c
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/clock_extras_reverse.py --build build
INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh
```

- 第一里程碑：【实测】创建轮 `exit=0, pass=422, fail=0, finished=1`，重启轮 `exit=0, pass=44, fail=0, finished=1`。日志 `work/targeted.log`。
- 第二里程碑：【实测】增加“设备时分相同但日期改变”的对照后，普通全量中的专项为创建 428、重启 44 项通过；内部 RAM 最低余量均 51,328 B，门槛仍为 32,768 B，内外堆分配失败均为 0。全量原话：`99% tests passed, 1 tests failed out of 321`、`Total Test time (real) = 712.94 sec`，退出 8，无跳过；唯一失败 `136 - capability-table-doc (Failed)`。日志 `work/build-full.log`、`work/ctest-full.log`。
- 最终内存检查：【实测】完整构建退出 0，确认系统编译参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`；启用 `UBSAN_OPTIONS=halt_on_error=1` 跑完整 321 项。原话：`99% tests passed, 1 tests failed out of 321`、`Total Test time (real) = 1425.97 sec`，退出 8，无跳过，唯一失败仍是能力文档。专项仍为 428／44 项通过，内部 RAM 余量及零分配失败同普通版本；没有最终 Sanitizer 错误。日志 `work/build-asan-full.log`、`work/ctest-asan-full.log`。
- 最终 S31：【实测】完整编译及尺寸／栈报告均退出 0，新 `clock.c` 已实际编译；`ink-s31.bin` 为 `0x1539d0`（1,391,056）字节。日志 `work/s31.log`，尺寸和栈报告在 `build-s31/size.txt`、`build-s31/stack.txt`。未执行烧写或监视设备。
- 最终反向对照：【实测】脚本退出 0，九个独立对象变体均编译成功、运行非零退出且 `finished=1`。撤掉累计用时／计次／秒表局部刷新／世界时钟局部刷新／秒表成员隔离／城市成员隔离／时区换算／校时事件／分钟截止点重排，分别检出 10／3／2／4／2／44／13／1／3 项失败。没有修改正常源码、可执行文件或断言。日志 `work/reverse-final.log`，明细 `build/clock-extras-reverse/summary.json`。
- 【实测】GCC 四份相关 C 文件全部编译、零问题，无跳过（`work/gcccheck.log`）。目录生成器的 18 项测试通过；撤掉两行 `TR_NOOP` 字面量识别后生成目录退出 1（`work/catalogue-reverse.log`），没有放宽元数据验证。
- 最后复核 GCC 仍为 4／0（`work/gcccheck-final.log`）。两套宿主配置按总规则，额外使用本地 SDL2／ImGui 源码、`CMAKE_EXPORT_COMPILE_COMMANDS=ON` 和 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`；ASan 仅另加 `INK_SANITIZE=ON`。构建及全量测试串行，最大 `-j2`、均为 `taskpolicy -c utility`，GUI 测试使用 SDL dummy，未开窗口。清理两套已完成全量的非本任务临时证据各 264 项，保留时钟证据、能力目录和完整日志。
- 【实现】两个新页面和 11 个能力均在时钟应用内，按钮和 AI 走同一能力；成员集合只读开放给通用数据能力，写操作由城市增删能力负责六条上限。页面快照含可见计次／城市、分页、秒表运行状态及精确毫秒值；孩子沿用计时器权限。
- 【实测＋实现】专项发现并集中修正一处旧根因：手动校时后分钟任务仍沿用旧截止点，改为按新墙钟重排；世界时钟同时订阅已有 `time.changed`，覆盖日期变化但当天分钟数相同的情况。没有给每个城市另加定时器。
- 初次专项因未生成资源而启动失败；资源生成后，ASan 定位到新字段漏填语义角色导致空指针，已补齐并通过上述专项。保留诊断日志 `work/asan-debug.log`，不把这次失败写成内存检查通过。

产品改动按新增与删除合计 **220 行**（含 CMake、译文和目录生成工具，不计测试与文档），低于 250 行。为什么不能更小：两页需要操作、分页、成员保存及完整 AI 动作；已复用时间换算、通用数据集合、主题、卡片和胶囊。看过的更小方案：共享设置表无法按成员保存城市；F4 列表固定动作最多三个，秒表需要三个操作及两个翻页动作，扩展共用组件会扩大范围；省掉人工界面或快照不符合任务要求。

【实测】最终变更共 12 个文件，均属本任务；文件及引用路径检查通过，变更文件均小于 5 MB。基线到提交的隐私扫描零匹配（grep 退出 1），空白检查通过；未提交构建产物、设计稿或共享生成文档。

截图（模拟器界面缓冲，非实机；使用上述命令保留）：

- 秒表中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-zh.png`
- 秒表英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-en.png`
- 世界时钟中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-zh.png`
- 世界时钟英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-en.png`
- 同目录 `snap-*.png` 为带墨水屏残影的输出；`clean-clock-home-en.png` 核查四个入口同排且文字不截断。首页原有闹钟正文仍为旧中文文案，本任务没有扩展旧页翻译范围。
