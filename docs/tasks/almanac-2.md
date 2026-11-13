# almanac-2：老黄历数据整块搬 Mogo 的离线历法模型（lunar-javascript 生成的静态表），提供 `time.almanac` 给锁屏和日历用，用 cnlunar 对照

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 1、2、6、7、11、12 条；第 8 条不碰设备）。

**D-140（总规则第 11 节）**：本任务是“搬代码”（Mogo 的离线历法模型）。原文整文件或整函数拷进来，`SOURCE.md` 写来源、固定提交、改了哪些行和为什么；为这台设备改的地方（墨水屏刷新、灰阶、108 px、随字号一起放大）也逐条记进 `SOURCE.md`。原文搬不动就写证据、停下，写进“要用户定”，不悄悄换成自己写。

## 为什么

用户 2026-11-12：老黄历宜忌“可以做 你自己研究有没有公开的 api”；之后定了方向（D-129）：选最成熟的整块搬，“不需要在乎开源协议”。在线接口要账号或收费，设备只连中枢（D-04），所以用离线数据。原来的 `almanac-1`（cnlunar）停了，换成 Mogo 用的那一套：Mogo 的锁屏“老黄历”就是用 lunar-javascript 的默认规则离线生成静态 C 表，固件里不跑 JavaScript；它同时给锁屏样式（以后 `lock-styles-1`）和新日历（以后 `calendar-3`）用。

## 成熟来源（搬代码）

- Mogo，`$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`，提交 `864f2f9`：
  - `main/time/calendar.h`、`main/time/calendar.c`（83 行）：公历 2000–2099 的农历、干支、宜忌、冲煞、喜神福神财神方位、吉神凶煞、建除、值神、节气；
  - `main/assets/calendar_data.h`（生成的静态表，约 227 KB 源码）；
  - `tools/generate_calendar_data.cjs`（生成器，`--check` 检查表是不是最新、`--oracle <文件>` 输出逐日对照数据）；
  - `tools/vendor/lunar-javascript/lunar.cjs`（1.7.7，MIT，Copyright 6tail）和同目录的许可；
  - `tools/calendar_host_test.py`（穷举百年逐日核对，宿主机编 C 再比）；
  - 规则说明：`docs/development/guides/CALENDAR.md`（宜忌、神煞、建除用 lunar-javascript 默认的日粒度规则；节气当天整日按新月份；喜神、福神取哪一派；这些方位和神煞是民俗资料）。
- 对照：cnlunar，`$INK_DEPS/../ink-hw/upstream/oss/cnlunar`，提交 `3f674fe`（Python，MIT）。只用来对照，不搬。
- 许可：Mogo 的 C 文件和生成器写的是 `LicenseRef-Kiikoread-Personal-Study-1.0`，lunar-javascript 是 MIT。用户说不看许可证（D-129），照原样带上，`SOURCE.md` 写清，以后公开发布时由主会话处理。

## 要做的

1. **原文入库**：上面要搬的文件整文件拷进 `system/services/time/vendor/mogo-calendar/`（**不要**放进 `third_party/mogo/`，那里归 `graft-mogo-1`），旁边 `SOURCE.md`（来源提交、每个文件 SHA-256、改了哪几行和为什么）和许可文件。生成器只在宿主机用（需要 node），固件只编静态表。
2. **接到时间服务**（新文件，例如 `system/services/time/almanac.c` 和 `system/include/ink/sys/` 里的头文件）：
   - C 接口：给一个公历日期，返回 Mogo `calendar_date_t` 里的全部内容（农历年月日、干支、宜、忌、冲、煞、财神喜神福神方位、吉神、凶煞、建除、值神、节气）。锁屏和日历以后直接调。
   - 能力 `time.almanac`：参数是日期（不给就是今天，按当前时区），返回同样的内容；只读，孩子也能用；回执的一句话写成“今天农历十月廿二，宜……，忌……”这类短句，并带一句“这些是民俗说法”。范围外的日期（2000 年以前、2099 年以后）如实返回“不在范围”。
   - 时区：Mogo 固定按北京时间算（`CALENDAR.md` 第 9 行）；我们按系统时区的当天日期算，写进 `SOURCE.md`。
3. **我们自己的农历**：时间服务里已有 `system/services/time/lunar.c`（日历和锁屏现在用它显示农历）。这次**不改它**；写一个测试把它和 Mogo 的农历在 2000–2099 每一天比一遍，不一致的日期全部列进报告，换不换由以后的 `calendar-3` 定。
4. Mogo 的锁屏画法（`main/ui/ui_calendar`、`main/lock/lock_calendar`）不在本任务，归以后的 `lock-styles-1`。

## 范围和预算

- 只改：新增 `system/services/time/vendor/mogo-calendar/`、新增 `system/services/time/almanac.c` 和对应头文件、`system/services/time/time_caps.c` 里登记能力的那几行、新测试、CMake 里新文件的那几行、`docs/tasks/almanac-2/`。
- 不碰：`system/services/time/lunar.c`、`time_service.c`（`notify-svc-1` 在接定时）、日历和时钟应用（`clock-3`、以后的 `calendar-3`）、锁屏（`lock_faces`、以后的 `lock-styles-1`）、`third_party/mogo/`（`graft-mogo-1`）。
- 胶水不超过 250 行（拷来的原文和生成的表不算）；固件增量不超过 60 KB（实际编出来量，写进报告；超了停下写清，不砍内容）。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。
- 起步：主线，今晚就能派，没有前提。

## 验收

1. **照 Mogo 的核对方法走一遍**：用 `tools/generate_calendar_data.cjs --check` 确认拷来的表和生成器一致；用 `--oracle` 生成逐日对照数据，宿主机测试把我们的 C 接口在 2000–2099 每一天和它比（照 `tools/calendar_host_test.py` 的做法），全部一致。需要 node 的这两步放在手动或“有 node 才跑”的测试里，没有 node 的环境（CI）明确跳过并打印原因。
2. **CI 上也能跑的固定抽样**：挑 50 天写成测试夹具（含闰月的月份、节气当天、春节前后、2033 年冬月、2000-01-01 和 2099-12-31 边界），期望值来自 lunar-javascript 的对照数据；ctest 里每次都比。反向对照：故意把一天的宜忌改错，这条必须失败。
3. **cnlunar 对照**：同样的 50 天用 cnlunar 算一遍，把和 lunar-javascript 不一样的地方（通常是宜忌的流派差异）列成表写进报告，不改数据。
4. **我们的农历**：`lunar.c` 和 Mogo 农历在 2000–2099 每天比一遍，结果写进报告（一致多少天，不一致的列出）。
5. **能力**：用能力调用（来源是 AI）查今天、查 2026-02-17（春节）、查 1999-12-31（不在范围），返回内容和 C 接口一致；孩子也能调；不需要确认。这是数据任务，没有页面改动：“改前改后对照”用这几天的输出表代替截图，写进报告。
6. 主工程普通全量 ctest 过；ASan 跑新测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、表在 flash 里多大、查一天的耗时、产品代码行数、`gcccheck` 结果，以及给 `lock-styles-1`、`calendar-3` 的接口说明。
