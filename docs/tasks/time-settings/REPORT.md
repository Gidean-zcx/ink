# time-settings 执行报告

基线：`ec78378e05f2694b9fa410780ab2bcf9d8de1c99`。分支：`claude/codex-time-settings`。本地执行结束，有未达验收项，见“没做到的”。以下“实测”指本机模拟器、编译器和测试命令，不是设备实测。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 日期时间、自动对时 | 【实测】通过 | 专项 227 项通过：手动整组修改、闹钟触发、闰日、切回自动立即对时 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/time_settings.py --build build` |
| 2 时区与夏令时、闹钟与日历 | 【实测】设备侧通过；实际服务器来源未接 | 城市点选、闹钟按新时区响、可见日历 UTC 提醒随时区变、欧美跳时与重复小时；网络字段用合成服务器验证 | 同上 |
| 3 离开时长与孩子权限 | 【实测】通过 | 人工选 5 分钟，4:59 回原页，5:01 要密码；孩子人工和 AI 均不能改 | 同上 |
| 4 共用能力与 AI 快照 | 【实测】通过 | 页面快照及生产 AI 上下文、按钮和 AI 共用设置与 time.set；能力表另行生成 | 同上；`python3 -B tools/capabilities.py --output work/time-capabilities.md` |
| 5a 原有时间、闹钟、日历、成员唤醒 | 【实测】通过 | 定向 11/11；这几组原测试源码未改 | 下方定向命令 |
| 5b 普通全量 | 【实测】317/318，1 项文档失败；无 CTest 跳过 | 唯一失败 `capability-table-doc`；其他检查全部通过 | `INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3` |
| 5c ASan/UBSan 全量 | 【实测】317/318，1 项文档失败；无 CTest 跳过 | 未检出 ASan/UBSan 错误；最后补编译后专项 227 项通过 | `INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3` |
| 5d S31 | 【实测】构建通过 | 退出 0，镜像 1346610 B；只编译 | `INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 5e GCC | 【实测】通过 | `gcccheck: 8 compiled, 0 with findings`，无跳过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/ui/shell/time_settings.c system/ui/shell/settings_pages.c system/ui/shell/settings_family.c system/ui/shell/navigation.c system/services/time/time_service.c system/services/hub/hub_link.c tests/time_settings.c tests/integration/ai_surface.c` |
| 6 中英文与截图 | 【实测】通过 | 已核看中文、英文截图；新增 35 条译文，文件已加入两端翻译源清单 | 同专项命令；截图路径见下文 |

定向命令：

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3 -R '^(time-settings|integration-ai-surface|integration-time-.*|clock-alarm|calendar|integration-member-wake.*|ui-scale-audit|i18n-tables)$'
```

## 没做到的

- 当前 Python 服务器（`hub/ink_hub/gateway/server.py` 的 `hello`）只下发 `now_ms`，没有设备所在地时区来源。本次实现设备侧可选 `hello.time_zone` 接收，已用合成网络验证；字段缺失时保留当前时区并显示“等待网络提供时区”，没有冒用服务器主机时区。
- 本机为 macOS；未运行 Linux 全量测试。已用真实 GCC 检查相关 C 文件，不将它写成 Linux 运行证据。
- 共享能力文档依总规则第 7 节未修改，因此普通和 ASan 全量的 `capability-table-doc` 均失败，不能写成全量通过；生成稿与真实运行目录已通过另行对照，待主会话统一更新。

## 和设计不一致的

四个新页面按 D-117 自行排布；没有修改设计文档迁就实现。新自动时区字段属于本任务扩展，尚未登记进旧设计总表，见“要改的文档”。

## 改过的已有测试

- `tests/settings_a.py`：`SetAbout` 精确行清单加入 `time`；总行数从 9 到 10，原字段和断言均保留。
- `tests/integration/ai_surface.c`：在 AI 可调用清单登记 7 个新增页面能力，草稿能力必填参数为 `field,delta`；未删除或放宽断言。时间、闹钟、日历、成员唤醒测试源码未改。

## 要用户定的

实际网络时区由谁提供：建议服务器按家庭或设备配置下发 IANA 名称；运营商或定位来源需要单独确认。本次保守地保留原手动时区，自动时区默认关闭，不猜所在地。

## 顺手发现

暂无。

## 新增的机制

- 设置 `time.zone_auto`（默认 0），由任务书第 2 项的独立自动/手动时区要求引入。
- 没有新增常驻任务、状态机、队列、后台重试或持久文件格式；日期草稿只在页面内存中，沿用现有执行器、设置存储、确认卡与服务通知。

## 要改的文档

- `docs/system/capabilities.md`：已生成到 `work/time-capabilities.md`（468 个定义），共享原文件未动。新增 `shell.time`、`shell.time_edit`、`shell.time_zones`、`shell.return`、`shell.time_step`、`shell.time_prev`、`shell.time_next`，以及设置 `time.zone_auto`。时区继续走已有 `settings.set(time.zone)`，没有再造同名能力。
- `docs/system/ui-dependencies.md`、设置缺口清单：补 `SetTime`、`SetDateTime`、`SetTimeZone`、`SetReturn` 及入口、快照和设置依赖。
- 时间服务设计及服务器协议：补独立自动时区设置、可选 `hello.time_zone`、紧凑表 API 与美国 2007 年前后规则。

## 放法、接口与体积

【实现】设置首页沿用“关于和更新”入口，内部增加“日期和时间”；日期编辑和城市选择再进一层。家人页增加“离开多久要重新验证”，进入四档选择。设置首页与“显示和亮度”没有增加行。每分钟只更新日期时间文本与既有状态栏，不重建整页。

【实现】界面复用既有组件，时间换算和闹钟重排仍由时间服务负责，网络层只交付时钟与时区元数据。按钮和 AI 通过 `time.set`、`settings.set` 及页面草稿能力操作，四页都有字段和动作快照；成员验证和身份设置确认沿用原服务。

【实现】`ink_time_zone_count()`、`ink_time_zone_name(index)`、`ink_time_zone_city(index)` 提供稳定的 0～8 索引、IANA 名称和中文翻译键；`ink_time_to_zone(index, utc_s, out)` 为世界时钟转换且不改全局时区。声明见 `system/include/ink/sys/time.h`。扩表需同时追加 `ZONES` 与 `ZONE_NAMES`，不重排已有索引。手动日期控件范围 2002～2100，含合法日数钳制；时间为 24 小时制。自动时区只接受表内 IANA 名称；手动选择保留，关闭自动即恢复。

表共 9 项：上海、UTC、东京、香港、新加坡、伦敦、巴黎、纽约、洛杉矶。沿用原有顺序，补中文名称；规则按 [IANA tzdb 2026f](https://data.iana.org/time-zones/tzdb-2026f/) 核对（2026-11-10），不是全量历史数据库。许可为 public domain，原许可文本在 `TZ-LICENSE`。

| 上游文件 | SHA-256 |
| --- | --- |
| [asia](https://data.iana.org/time-zones/tzdb-2026f/asia) | `02b69728085f6b0c999da70de2f1f9a2d8952b533a450d53bb41e03eb7d0e92e` |
| [europe](https://data.iana.org/time-zones/tzdb-2026f/europe) | `fec11bf6c6bd0619a8777bbdaa2ecd184fde291cb4b97efa87d334c7145ba3c2` |
| [northamerica](https://data.iana.org/time-zones/tzdb-2026f/northamerica) | `da7366f6e0862ed15cb787dd056dca3a255df2ec50d19ff0a9f54eeb1562083a` |
| [LICENSE](https://data.iana.org/time-zones/tzdb-2026f/LICENSE) | `0613408568889f5739e5ae252b722a2659c02002839ad970a63dc5e9174b27cf` |

【估算】S31 32 位布局：9 行结构 108 B、枚举指针 40 B、IANA 字符串 123 B、中文字符串 148 B，合计 **419 B**，不含链接对齐、英文译表、字库与转换代码。【实测】S31 对象 `nm -S`：`ZONES=0x6c`、`ZONE_NAMES=0x28`，与前两项相符（`work/time-zone-size.log`）。

产品 C/CMake 改动按新增与删除合计 **272 行**（251 加、21 删，包含时区表）；即使再计 35 行译文也为 307 行，未超 350。为什么不能更小：任务要求日期编辑、城市分页、验证时长三个操作面及设置入口、快照和网络接线；已把四页合在一个 178 行页面代码，复用开关、步进按钮和 F4 列表。看过更小方案：全部挤进“关于”会使编辑与列表混杂；只循环修改时区不能提供按城市点选；只开放 AI 则缺人工操作，因此未采用。

截图（专项命令加 `INK_KEEP_EVIDENCE=1` 保留）：

- 中文：`build/time-settings-evidence/sim-time-settings/clean-time-zh.png`
- 英文：`build/time-settings-evidence/sim-time-settings/clean-time-en.png`
- 同目录 `snap-time-zh.png`、`snap-time-en.png` 为模拟墨水屏的带残影输出；`clean-*` 为界面缓冲截图。均非实机图。

## 过程记录

- 已读取总规则与任务书；工作区干净，HEAD 与指定基线一致，已在独立任务分支。
- 第一里程碑：设置入口、四个二级页面及中英文、网络时间恢复和可选网络时区已实现；专项 `exit=0, pass=211, fail=0, finished=1`。原始证据在 `build/time-settings-evidence/sim-time-settings/`；内部 RAM 最低空闲 43144 字节，没有分配失败。
- 第二里程碑：专项更新为 226 项通过（含分钟局部刷新和 2006 年美国规则），6 组反向对照均产生指定失败；日志 `work/time-targeted-final.log`、`work/time-reverse.log`，复跑命令 `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/time_settings_reverse.py --build build`。GCC 与 S31 均通过，日志 `work/time-gcccheck.log`、`work/time-s31.log`。生成能力表与真实运行目录对照通过：`Runtime matches: 391 capabilities, 28 settings`。
- ASan 最终全量原话：`99% tests passed, 1 tests failed out of 318`；`Total Test time (real) = 853.82 sec`，退出 8。唯一失败为 `135 - capability-table-doc (Failed)`，没有 CTest 跳过；全部注册测试均运行，包括时间、闹钟、日历、成员唤醒和确定性测试。日志 `work/time-ctest-asan.log`。
- 最后给家人行缓冲补命名上限与编译期断言（实际容量仍为 24），分钟测试改成等待服务状态；ASan 重编后专项 `exit=0, pass=227, fail=0, finished=1`（`work/time-test-asan-final.log`）。6 组反向对照复跑全部命中（`work/time-reverse-final.log`），GCC 仍为 8/0，S31 增量编译通过（`work/time-s31-final.log`）。
- 普通最终全量原话：`99% tests passed, 1 tests failed out of 318`；`Total Test time (real) = 535.34 sec`，退出 8。唯一失败同为 `135 - capability-table-doc (Failed)`，无 CTest 跳过（`work/time-ctest-final.log`）。普通专项同为 227 项通过。两套专项内部 RAM 最低空闲均为 43144 B，内外堆分配失败均为 0。
- 配置采用任务书命令，额外开启 `CMAKE_EXPORT_COMPILE_COMMANDS`，两套均指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 使用现有锁定环境；ASan 另加 `-DINK_SANITIZE=ON`。构建最多 `-j4`，S31 为 `-j2`，CTest 最多 `-j3`，均使用 `taskpolicy -c utility`，构建和全量测试没有重叠。窗口测试使用 SDL dummy。
- 最后核对 `git diff --stat`，21 个变更文件均属本任务，没有构建产物、设计导出或共享文档改动；每个文件均小于 5 MB。按总规则第 6 节检查与基线差异，隐私扫描 0 匹配（grep 退出 1）；文件与空白检查通过。
