# board-vendor-1 执行报告

基线：`40d965f5b4b5d8d510064396dc59e3ae7e96855c`。执行日期：2026-11-11。仅本机编译与宿主机测试，不连接设备，不推送。

当前结论：功能单元与四项反向对照通过，但普通及 ASan/UBSan 全量均存在本次引入的回归，**未达到合入验收**。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 开机默认寄存器、欠压与再充 | 【实测】单元通过 | 假 CX 寄存器 + 假 BQ 电量计，覆盖 2500/3100/3101/3200/3201/4399/4400 mV、5 mA 和充电状态边界 | `taskpolicy -c utility ctest --test-dir build -R '^board-vendor-' --output-on-failure -j2` |
| 1b 前光 | 【实测】单元通过 | 每 5 ms 一级、升降/改目标、暖光、到位停表、关屏/锁屏立即到零且不复亮 | 同上 |
| 1c 反向对照 | 【实测】4/4 预期失败 | `usb-cut`、`uv-3200`、`busy-kick`、`instant-light` 各退出 1，命中对应断言；`work/reverse.log` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/board_vendor/reverse.py --build build` |
| 1d 浅睡暂停、恢复与实际电源任务节奏 | 【实测】通过 | `board-vendor-power`：18 项检查，`test.finished=1`，无警告；覆盖清醒 2/10 秒、锁屏但保持清醒、让出 CPU 的 15 秒浅睡期间零调用及醒后恢复；`work/board-vendor-tests.log` | `taskpolicy -c utility ctest --test-dir build -R '^board-vendor-' --output-on-failure -j2` |
| 1e 开机日志 | 【实现】完成；未上机读日志 | `ports/esp32s31/s31_board.c:28,80-83` 在首次写充电寄存器前和写后分别读取 VREG、ITERM、EN_TERM 与 BQ 电压，并标 I²C 状态；S31 编译通过 | 源码核对与下列 S31 编译 |
| 2a 普通构建与全量 | 【实测】构建退出 0；全量未通过 | 原话 `97% tests passed, 11 tests failed out of 379`；退出 8，3 项跳过；1105.62 秒；`work/ctest.log` | `taskpolicy -c utility cmake --build build -j2`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2b ASan/UBSan 全量 | 【实测】构建退出 0；全量未通过 | 原话 `97% tests passed, 11 tests failed out of 379`；退出 8，823.30 秒；同普通版的 11 项失败、3 项跳过；`work/ctest-asan.log` | 同普通构建，仅换 `build-asan` 并加 `-DINK_SANITIZE=ON`；`ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2` |
| 2c S31 编译、gcccheck | 【实测】通过 | S31 退出 0；`INK_S31_LIGHT_SLEEP=1`、`-Werror` 已核对，日志 `warning:`/`error:` 均 0；`work/s31-build.log`。gcccheck 原话 `gcccheck: 7 compiled, 0 with findings`，无跳过；`work/gcccheck.log` | `INK_S31_LIGHT_SLEEP=ON taskpolicy -c utility bash ports/esp32s31/build.sh`；`taskpolicy -c utility python3 ../../tools/gcccheck.py build drivers/chips/cx25601n/cx25601n.c simulator/models/dev_power_chips.c system/services/display/light.c system/services/power/power_service.c tests/board_vendor/charger.c tests/board_vendor/light.c tests/board_vendor/power.c` |
| 2d 隐私与差异范围 | 【实测】通过 | 总规则第 6 节的路径/MAC 正则扫描 0 匹配；`git diff --check` 无错误；15 个改动文件均在本任务范围，没有设计稿、固件或其他构建产物 | 总规则第 6 节隐私扫描；`git diff 40d965f5b4b5d8d510064396dc59e3ae7e96855c..HEAD --stat`；`git diff --check` |
| 3 原厂差异与上机核对 | 【实现】完成记录 | 见下方原厂行号、哈希、差异与主会话核对顺序；未做实机测试 | 核查原厂只读源码及 `git diff` |

## 没做到的

- 真机验证由主会话完成，本任务不执行。
- 未移植原厂 SOC 电压估算、可选充电电流、前光输出曲线，均超出本任务。
- 普通全量 365 通过、11 失败、3 跳过。手动把四个改动的 C 单元换回基线源码，用当前编译参数重编译并链接同一测试程序：结果见下表。临时命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" work/baseline_control.py`；日志 `work/baseline-control.log`。未将依赖 Git 历史的对照注册到 CTest。

| 失败项 | 本分支 | 基线对照 | 归因边界 |
| --- | --- | --- | --- |
| `ui-scene-ImeVoice`、`ui-scene-big-ImeVoice` | 最低内部 RAM 32568 B，小于 32768 B | 32776 B、退出 0 | 【实测】本次回归；新增前光互斥量与定时器合计占用 208 B |
| `i18n-shell`、`integration-people`、`integration-member-wake`、`scenario-standby-metalio-plus`、`clock-extras` | 页面稳定、成员门禁、唤醒/计时等断言或预算失败 | 全部退出 0 | 【实测】本次回归；尚未修复，不能称为既有失败 |
| `device-events`、`integration-text-nocard`、`integration-text-baddict`、`small-gaps-3-backup` | 失败 | 也失败 | 【实测】首条失败和失败数分别相同（1、2、4、16）；这不等于已证明没有其他差异 |

- 现有配置跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，不记作通过。
- 内存检查跑了整套 379 项（含新增三项），仍有上述功能/预算失败；四个改动的宿主产品 C 单元及三个新测试均确认含 ASan+UBSan 编译参数，输出未发现 sanitizer 错误。原有 `unit-s31-osal-ticks` 在 CMake 启用 sanitizer 前定义，未插桩，不把它记为已做内存插桩检查；S31 实机代码不在宿主 ASan 范围内。

## 和设计不一致的

- 【实现】保留 1 A（芯片量化 960 mA），直接初始化，不经过原厂驱动的 500 mA 临时值；原厂板级最终同样恢复默认 1 A。
- 【实现】原厂独立任务改接现有电源任务；浅睡调用期间同一任务不执行充电检查，返回后恢复，无新常驻任务。
- 【实现】当前首轮启动及浅睡返回后立即检查，后续按 2/10 秒间隔；原厂启动时先延后 2/10 秒再检查。首轮相位未完全照搬，列为未达完整一致，不能将周期单测通过理解成所有调度时序相同。
- 【实现】按任务要求每轮重写 VREG；原厂 `apply_vreg_fixed` 对相同缓存值会提前返回，不能纠正寄存器漂移。
- 【实现】前光只移植渐变与立即关闭/恢复；保留现有暖光混合与 PWM 映射，未移植原厂 10% 输出上限（任务未要求改变曲线）。
- 【实现】私有寄存器解锁或写入失败会返回失败并由板级记录，原厂部分写入忽略返回值；不新增重试，解锁最多十次沿原厂。

## 改过的已有测试

- 暂无。

## 要用户定的

- 是否开放可选充电电流不在本任务范围，保持 1 A。
- 上机前核对实际电芯/标签的最高充电电压是否支持 4400 mV、1 A（量化 960 mA）、终止 60 mA、预充代码 12；3.1 V 断电和有效采样下限 2.5 V 也需结合电池保护规格确认。原厂电池测试曲线仅充至 4.3 V，不能据此证明 4.4 V 适用。
- 后续修复范围需由主会话定：现有语音场景只有 8 B 内部 RAM 余量，前光新增同步对象需回收 208 B；不能提高预算。检查可用的更小方案时，测试准备阶段都设置了 40% 亮度，单纯延迟创建仍会占用这两项对象；借用刷屏锁会阻塞 5 ms 回调；新增 OSAL 外部内存创建接口超出任务范围，均未擅自实施。
- 【实现】原厂 kick 的 200 ms + 500 ms 等待目前发生在现有电源任务，期间该任务不能处理下一次状态通知；【估算】可能影响上述时序场景。若改为分段非阻塞控制或额外执行器调度，需要新增状态/调度机制；总规则不允许本任务自行扩大，故记录待主会话确定，未逐个场景补丁、加 sleep 或放宽测试。
- 【实测】待机场景的唤醒到可见最大值从基线 822.765 ms 增为 1432.102 ms，超过原有 1200 ms；原基线有一次 `power.sleep_cancelled`，本分支未记录到。这是明确回归，不修改脚本点按时刻掩盖。

## 顺手发现

- 原厂关闭 DPDM，`VBUS_STAT` 可能在插电时仍读 0（原厂 `docs/battery-soc-and-charge.md` §4.1、§5）。本次按任务保留原厂欠压判定，不据电量页的 `external_power` 推导替换；寄存器模型的“插电不切”只覆盖 VBUS 非零。实机须核对输入状态，不能将这项宿主机结论推广到误报 0 的硬件读数。
- 模拟器先前把 0x14 bit2 同样当作 EN_CHG，并把 0x86 私有寄存器折回 0x06；为接入真实默认值，已同步修正这两个直接阻塞本任务的寄存器模型问题。电池充电曲线仍是估算模型。

## 新增的机制

- 任务 1 要求：现有电源任务新增 2/10 秒充电截止核对期限和欠压滞回标志；无新线程、队列、文件格式或设置项。
- 任务 3 要求：共享定时器新增前光渐变回调及互斥保护；立即关闭会取消过渡。

## 要改的文档

- 主会话合并时更新 `docs/tasks/vendor-feature-audit/REPORT.md` 的前光渐变、再充、VREG、BATFET 行及相应接续索引；本任务未修改受保护的共享文档和历史审计/研究资料。

## 原厂依据与上机顺序

查阅日期：2026-11-11。原厂公开仓库 `CloudZao/Metalio-E-INK4-Plus`，v2.0.77、提交前缀 `874538e`；以下路径均相对此快照。

| 依据 | 原厂位置 |
| --- | --- |
| 默认寄存器、私有解锁和开充 | `main/boards/common/power/cx25601n.cc:211-228,314-341` |
| VREG、周期、欠压、终止和 kick | 同文件 `:89-209`；充电使能 `:428-447`；IINDPM 联动 `:548-581` |
| 浅睡暂停/恢复、BATFET | 同文件 `:385-413` |
| 板级最终默认 1 A | `main/boards/metalio-e-ink4-plus/periph/metalio_power.cc:60-63` |
| 5 ms 一级、恢复和立即熄灭 | `main/boards/metalio-e-ink4-plus/periph/frontlight.cc:24,154-185,215-245` |
| 电池边界与 VBUS 限制 | `docs/battery-soc-and-charge.md` §2、§3、§4、§5；任务提到的根目录 `README_zn.md` 在此快照不存在 |

SHA-256：`cx25601n.cc` = `8af78da4b8e806c406b783a0185f85559f771d5c8196a9cf548a1b89474dee3b`；`frontlight.cc` = `9ecc59ea946efaf53e59566bda58e0c672d42636f66eedd540597be714f450c2`；随驱动附带的 `LICENSE.vendor` = `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05`，保留原厂 MIT 版权声明。

主会话上机第一步仅核对 `charger before:` / `charger after:` 日志中的 VREG、ITERM、EN_TERM、VBAT 与两项 I²C 状态；未读到的字段记为 -1，不伪装成有效读数。写后预期 VREG=4400 mV、ITERM=60 mA，EN_TERM 与原厂初始化一样保留原位，随后按电压调整。主会话确认寄存器与电池规格后，再观察一次完整充电与放电，并核对插拔电状态、低压边界、浅睡恢复和前光。宿主机结果不代表已经上机验证。

## 改动规模与分层

- 产品 C/H 净增 186 行（211 增、25 删，包含模拟器修正）；加上 14 行 CMake 测试注册和 22 行许可证也只有 222 行，低于 300 行。测试源码和报告另计。
- 为什么不能更小：约百行是原厂寄存器序列、读数和保护条件，其余是板级日志/采样、现有电源循环期限与共享定时器过渡。只在 HAL 单次调用内等待整个渐变会阻塞调用方；另起原厂常驻任务违反任务约束；逐个 UI/AI 调用方补渐变会重复根因，因此未采用。
- 【实现】充电策略只在驱动及 HAL 下层，系统电源服务仅调平台接口；前光仍由现有 `settings.set` 原子能力统一入口控制，按钮与 AI 同路。未加页面、设置项或 AI 私有旁路。
