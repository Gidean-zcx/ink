# port-sleep-vendor 执行报告

基线：`eec5d569be028b539e8778690c239a6947767864`。执行日期：2026-11-10。
范围：只修改浅睡相关端口、板级时序和对应测试；只编译、跑宿主机测试，不连接设备。

实现及编译、宿主机验收完成。RTC arm 先建立 RTC 输入基线、最后启用 POWER，保持 P16 锁存为低；原有 held-POWER 断言和预算保留。实机边界见下方清单。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】S31 构建退出 0，0 条 warning/error；端口普通、ASan/UBSan 各 `100% tests passed out of 3` | 见下方复现命令；`work/port-sleep-s31-final.log`、`work/port-sleep-build-s31-host{,-asan}-tests.log` |
| 2 | 通过 | 【实测】最终 14/14 反向对照拒绝，原实现三项通过；要求的四项及 RTC arm 检查均编译 0、断言退出 −6 | `taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host` |
| 3 | 通过 | 【实现】下方逐项对照，含缺失端口和保留差异 | 文件/行号见下表 |
| 4 | 通过 | 【实测】最终普通/ASan+UBSan 全量均 `100% tests passed out of 320`，分别 555.15/817.37 秒；gcccheck 8 个 C 编译单元，0 findings；隐私扫描无命中 | 下方模拟器与 gcccheck 命令、总规则 §6 隐私命令 |
| 5 | 通过 | 【实现】下方八项上机清单；未把编译写成真机通过 | 本报告“上机先核对清单” |

## 原厂对照

【实现】核查日期 2026-11-10，原厂 [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14) 提交 `690a023c66862af103a3187464373ee7c5efcd14`。MIT 原文及源文件哈希在 `ports/esp32s31/vendor/s31_sleep/`。下表的原厂路径缩写：`R` = `main/boards/common/power_policy/README.md`，`H` = 同目录 `power_hw.cc`，`V` = 同目录 `s31_sleep/`，`B` = `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c`。本项目 `P` = `ports/esp32s31/hal_power_s31.c`，`M` = `boards/metalio_plus/metalio_board.c`，`E` = `boards/metalio_plus/metalio_epd_power.c`。行号均为源码核查，全部未经实机验证。

【实现】原厂不启用 RTC 外部唤醒；本项目按任务保留 P16，mask 阶段输出低，arm 阶段先仅启用 RTC 输入、读基线并拒绝已低的 RTC，最后才启用 POWER，避免读取基线消费电源键中断。宿主机能检查次序与回退，不能验证 P16 的电气行为；列入下方第 4、5 项上机核对。

| 原厂步骤与出处 | 本项目代码行 | 一致性与原因 |
| --- | --- | --- |
| 进入 1：SD 已停放，R:77，H:156 | P:135 | 不一致：已有端口未接 SDMMC/CNNT 停启；本任务未扩展 SD 端口 |
| 进入 2：TPS 休眠、屏总线低，R:78，B:346 | P:136，E:156 | PMIC 一致；屏总线停放仍缺端口 |
| 进入 3：释放加速度计共享 INT，R:79，H:407 | P:137 | 有意不同：先锁采样再配传感器；保留运动监测，防止输入任务提前读走 INT1 锁存 |
| 进入 4：停采样、安全锁存，R:80，B:478 | P:137，M:56 | 一致：按键暂停后才改 TCA |
| 进入 5：GPIO2 输入、上拉、无下拉，等待高，R:81，H:85 | P:141 | 一致；启用 P16/P17 后与最终入睡前再次检查，高不成立就回退 |
| 进入 6：前光灭、P05→P14，R:82，B:292 | P:149，M:92 | 断电顺序一致；保留既有 keep_modem 分支；通信/音频真实端口的停止与恢复仍缺 |
| 进入 7：总线 hold、补丁浅睡，R:83，H:446 | P:155，`ports/esp32s31/s31_sleep_bridge.cc:8` | 补丁调用一致；屏总线 hold 仍缺，板级阶段诊断未映射（见下方边界） |
| PSRAM→XTAL、按 retention 关 MSPI、停 MPLL，V/s31_sleep_sdk.cmake:16 | 同名 vendor 文件:16 | 原样移植，生成构建目录内 PMU 副本；SDK 安装不写入 |
| 醒后 DREG/PVT→模拟初始化→MPLL→MSPI/PSRAM，V/s31_sleep.cc:46，V/s31_sleep_sdk.cmake:30 | 同名 vendor 文件:46、:30 | 低层流程一致；通过三个链接 wrap 接入 |
| 正常方向 0xC0/0xE3，R:117，B:518 | M:9、M:111 | 一致，已有正确实现保留 |
| mask 锁存→全输出→唤醒输入，R:118，B:478 | M:63，M:74 | 一致；P16 唤醒的例外见下 |
| P00/P01 低→50 ms→P02 低→120 ms；已全低仅 120 ms，R:123、:137，B:346 | E:156 | 一致，原实现已符合，本次新增真实板级顺序断言 |
| P03 马达/P04 功放保留锁存，R:124、:125 | M:60，P:122 | 一致；忙或锁存仍高时拒绝睡眠，不凭 mask 假称已关 |
| P05 保留后先切断，R:126，B:292 | M:60、M:95 | 一致 |
| P06/P07 输出 0，醒后输入，R:127、:128 | M:60、M:64、M:111 | 一致 |
| P10/P11 输出 1，醒后输入，R:129 | M:62、M:112 | 一致 |
| P12 保留锁存，R:130 | M:62、M:110 | 一致 |
| P13 输出 0，R:131 | M:62、M:110 | 一致 |
| P14 在 P05 后切、P05 前恢复，R:132，B:292、:303 | M:96、M:122 | 顺序一致；keep_modem=true 时按既有需求保留 P14 |
| P15 输出 0、醒后输入，R:133 | M:62、M:112 | 一致 |
| P16 mask 输出 0、不唤醒，R:134 | M:62、M:81 | mask 一致；任务要求保留 RTC 唤醒，arm 阶段先处理 RTC 基线并拒绝 RTC 已低，最后启用 POWER；S31 共享 INT 不高也取消 |
| P17 mask 输出 1→输入；先锁存再恢复方向，R:135 | M:62、M:75，P:166 | 一致；短按已松开仍保留电源键语义 |
| 唤醒链 P17→TCA INT→GPIO2 EXT1 ANY_LOW，R:139，H:105 | P:89、P:147 | 一致；保留任务要求的 RTC、运动，以及 D-37 可接听待机所需的既有保电模组 GPIO3 唤醒，后者极性仍须上机核对 |
| GPIO61 不作浅睡唤醒，R:104、:139 | P:123 | 一致，删除 GPIO 唤醒配置和原因映射；IDF `components/soc/esp32s31/include/soc/soc_caps.h:259` 的 LP 引脚数为 8 |
| 退出 1：恢复方向前锁存电源键，R:147，H:457 | P:166 | 一致；运动原因同样在恢复前读取 |
| 退出 2：关唤醒配置、恢复按键、等待稳定，R:148，H:463 | P:171，M:108，P:174 | 一致；输入任务最后恢复，保留已有 20 ms 去抖 |
| 退出 3：P14→P05、前光保持灭，R:149，H:467 | P:177，M:122 | 一致；音频/模组供电状态通知仍待真实端口 |
| 退出 4：非定时 SD 初始化后 USB 恢复，R:150，H:135、:168 | P:178 | USB 顺序一致：开总线→pad 断开 150 ms→PHY 默认→pad 接回→最多等 800 ms；本端口无 SD/CNNT 复用，放在电源轨恢复后、恢复输入前，纯定时唤醒跳过 |
| 退出 5：屏总线恢复，R:151，H:478 | P:135 | 不一致：屏总线端口尚缺，本任务未编造实现 |
| 固定 min=max、显式切频、临时 hold，R:205，H:292 | P:25、P:34、P:52 | 方式一致；本项目沿用 D-39 的空闲 160/活跃 320 MHz（原厂空闲默认 320，可选 160），不引入设置；多个 boost 合并到同一固定频率，最后释放才恢复请求频率 |

## 复现命令与证据

所有构建/测试串行安排，构建及 ctest 均最多 `-j2`，命令前加 `taskpolicy -c utility`。`INK_DEPS`、`INK_DESIGN` 按用户提供；设计稿未使用。

```bash
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host
taskpolicy -c utility python3 ports/esp32s31/tests/sdk_check.py "${IDF_PATH:-$HOME/esp/esp-idf}" build-s31-host
```

端口 ASan/UBSan：目录换 `build-s31-host-asan`，配置仅加 `-DINK_SANITIZE=ON`。SDK 反向检查【实测】：正确 SDK 退出 0，改版本和删锚点各退出 1，明确显示版本/PMU 源码变化原因。测试只改临时副本。

【实测】端口最终普通/ASan+UBSan 各 3/3，通过用时分别 1.31/3.67 秒。gcccheck：`8 compiled, 0 with findings`，覆盖电源 HAL、三个端口测试、板级 IO/PMIC、TCA/TPS 驱动，没有跳过；日志 `work/port-sleep-gcccheck.log`。端口内存检查的 9 个 C 编译单元全部带 `-fsanitize=address,undefined`（另一个是未改的触摸区域代码）。

【实测】S31 最终构建退出 0，日志没有编译 warning/error；SDK 自身仍有 Kconfig 的十六进制写法弃用 NOTE，未关闭检查。`build-s31/compile_commands.json` 中兼容 PMU 副本 1 项、原始应用 PMU 0 项、vendor 源 1 项；最终 ELF 的 `nm` 输出含三个 `__wrap_*`、`ink_s31_sleep_init` 与 `ink_s31_light_sleep`，确认不是只复制而未接入。

反向对照【实测】：要求的 `usb-revive`、`vendor-sleep`、`dynamic-frequency`、`gpio61-wake` 都编译退出 0、断言退出 −6；其余既有九项同样拒绝；新增 `rtc-arm-idle` 撤掉 P16 检查后在“必须返回 HAL_I2C_ERR”处断言失败，三个未修改对照退出 0。证据：`work/port-sleep-negative.log`、`work/port-sleep-sdk-negative.log`、`work/port-sleep-build-s31-host{,-asan}-tests.log`。

模拟器普通版的完整复现参数如下；内存检查只把构建目录换为 `build-asan`、加 `-DINK_SANITIZE=ON`。没有改断言、预算或测试集合。

【实测】最终普通版全量原话：`100% tests passed out of 320`，`Total Test time (real) = 555.15 sec`；日志 `work/port-sleep-build-tests.log`，没有跳过。

【实测】最终内存版全量原话：`100% tests passed out of 320`，`Total Test time (real) = 817.37 sec`；日志 `work/port-sleep-build-asan-tests.log`，没有跳过、没有 ASan/UBSan 报错。端口独立 3 项不计入上述 320 项。

```bash
export PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub"
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility python3 ../../tools/gcccheck.py build-s31-host \
  ports/esp32s31/hal_power_s31.c ports/esp32s31/tests/logic.c ports/esp32s31/tests/power.c \
  ports/esp32s31/tests/board.c boards/metalio_plus/metalio_board.c \
  boards/metalio_plus/metalio_epd_power.c drivers/chips/tca9555/tca9555.c drivers/chips/tps65185/tps65185.c
```

## 上机先核对清单

1. 本任务未连接、烧录或监视设备。后续先复核当前 UART0 主控制台/USB 副控制台与模组的引脚复用，再在有人能按电源键的条件下测试，遵守 `docs/hardware/2026-11-10-selftest.md:67`。
2. 浅睡定时、电源键短按后松开/保持按住，分别验证补丁返回、PSRAM 数据完好、看门狗无复位。CPU retention 与非 retention 路径的硬件条件需分别核对；编译不证明低层时序已实测。
3. USB：电源键/RTC/运动后电脑重新枚举、恢复连续日志；纯定时不主动断开 pad；无主机连接时约 950 ms 内完成恢复等待。SDMMC 后续接入时，必须把 USB 放到 SD 初始化之后再测，当前无 SD/CNNT 恢复不能据此验收该路径。
4. 共享 GPIO2：空闲高、上拉开启；P16 实际接法与极性、RTC 告警清除、加速度计 INT1 锁存释放；mask、arm 后及即将入睡时拉低，都应取消该次睡眠并恢复方向与电源轨。
5. 逻辑分析核对 P00/P01→50 ms→P02→120 ms，TCA 方向与安全锁存，P17 在恢复方向前读取，P05/P14 断开和恢复顺序；确认 P16 保持输出低直到切输入，读基线能释放空闲转换的中断，真实告警低电平会取消本次睡眠。
6. 固定空闲 160/活跃 320 MHz、重叠 boost 最后释放恢复空闲、I2C/UART/前光时钟稳定；不能用宿主机断言证明真实切频有效或节电幅度。
7. GPIO61 保持清醒时按键功能，不能当作浅睡唤醒；GPIO3 SRDY 既有极性仍未验证，须与保电模式的短信/来电唤醒一并核对。
8. 屏总线/SD 停启、音频与模组真实收尾仍缺，补齐后才量整板浅睡电流；不以本次编译代替整机电源验收。

## 没做到的

- 未上机验证，以上清单均待实机；屏总线/SDMMC、音频/模组收尾没有在本任务扩展。
- 原厂完整的板级 Checkpoint/延后重放没有接入；C 桥仅启动 cycle、调用真实补丁，诊断 `applied=0` 不代表板级未做动作。板级阶段依然由 HAL 原有局部标记回退，不伪造尚缺的 SD/屏总线诊断位；SDK 内部时钟与校准 trace 保留。
- 未在 Linux 运行测试；本机为 macOS，GCC 检查是编译验证，不等同于 Linux 运行结果。

## 和设计不一致的

- 与原厂不一致的保留需求与缺口逐行列于对照表。任务书把“拿起亮屏”记为 D-121；现有决策表实际为 D-122，本报告按行为执行，不改决策编号。
- D-38 仍写有 AI 键浅睡唤醒；本任务明确取消 GPIO61 唤醒，本端口目前只在清醒阶段处理该键。没有擅自换引脚或改硬件，共享设计说明需主会话同步。
- 原厂文件在当前 GNU C++26/IDF 下的 volatile 连续赋值报错及单源唤醒接口警告，做两处编译适配；未改时钟/模拟寄存器次序，未放宽警告。

## 改过的已有测试

- `ports/esp32s31/tests/power.c`：频率期望由动态 160～320+IDF 锁改为固定 min=max，验证两路重叠 boost；RTC 初始低从“去掉 RTC 后继续睡”改为“取消这次浅睡”；AI+模组唤醒不再期待 GPIO61 原因，只保留模组原因。这些均由本任务明确要求；其余已有断言保留。
- `negative.py`：保留既有九项，新增要求的四项及 RTC arm 一项；修正临时目录内上一项运动分类变异会污染后续电源变异的问题，每项先恢复原始头文件，重新跑完 14 项。USB/补丁撤除分别在缺失顺序标记处失败；未删测试、加 sleep 或提高预算。
- 原有 `selftest-board-wiring` 与 `selftest-board-wiring-pqbn` 的 held-POWER 期望未改。一次中间顺序回归被两项捕获（cause 0）；将 RTC 基线提前到启用 POWER 前，在相同 ASan 构建下两项通过。失败证据 `work/port-sleep-asan-held-key-failure.log`，修正后 `work/port-sleep-asan-held-key-focus.log`。

## 要用户定的

- 暂无。

## 顺手发现

- 原厂文件适配后首次链接发现当前最小 IDF 构建未引入 USB 连接监测函数；已显式声明 `esp_driver_usb_serial_jtag` 组件依赖并使用其正式头文件，未伪造连接状态。
- 现有 SDK 默认配置同时启用 UART0 主控制台及 USB 副控制台，和任务背景“控制台只走 USB”的说法不同；本任务不修改控制台配置，上机前主会话须复核 UART0 默认引脚与模组复用。
- 宿主机构建有 SDL2 的 macOS API 弃用警告及 `libink_port_sim.a` 重复链接提示；S31 构建 0 警告。这些第三方/链接提示未在本任务扩大处理。
- 根 CMake 的 `unit-s31-osal-ticks` 在 sanitizer 选项前定义，内存版编译数据库确认它未被插桩；本次修改的板级代码和端口独立测试均有 ASan/UBSan。未越界修改根构建文件。

## 新增的机制

- 原厂 S31 浅睡包裹、三个链接 wrap、构建目录 PMU 副本及 RTC 保留诊断：任务明确要求移植的原厂补丁；无诊断后台任务。
- CPU boost 合并计数与互斥：任务要求从 IDF 动态锁改为固定切频，沿用原厂临时 hold 语义，保留现有 HAL。
- SDK 版本/PMU 哈希构建检查：任务要求不兼容时失败。没有新增产品设置、任务、线程、状态机、持久文件格式、队列或重试机制。

## 要改的文档

- 主会话合并时更新 backlog、`docs/system/ui-dependencies.md` 与旧 port-hal-2 状态：浅睡已走补丁、USB 恢复、固定切频、GPIO61 不唤醒；能力表无需改变。
- 同步 D-38 及引用它的设计说明中的 AI 键浅睡唤醒限制；本任务按要求不改共享决策。

## 规模与架构审视

自有产品代码（含构建配置和 vendor 文件的两处适配）**+106/−43，按增删合计 149 行**，小于 250 行预算；测试、文档和原厂复制主体不计。为什么不能更小：需同时接入 C++ 原厂补丁与 C HAL、显式构建失败检查、USB 恢复和固定切频；已保留正确的 PMIC、正常方向和电源轨实现，只补验证。看过的更小方案：仅调用 IDF 不含硬件补丁；固定频率下继续用旧 IDF boost 锁不能从 160 升到 320；仅在 arm 前检查 INT 会遗漏恢复 RTC 输入时出现的有效低电平。

【实现】R-061：变更留在端口、板级和构建，系统与应用继续经同一电源 HAL、服务和能力路径；没有新用户动作或页面，无需新增 AI 快照或能力。SDK 和 USB 寄存器属于平台内部机制，未向上层泄漏 IDF 或模拟器依赖。

## 收尾核查

【实测】基线至最终提交仅 21 个相关文件；`git diff --check` 退出 0，文件/本地链接检查通过。总规则 §6 隐私命令无输出，grep 退出 1（无命中），证据 `work/port-sleep-privacy.log`。原厂哈希、MIT 许可证保留，未提交构建产物、设计稿或超过 5 MB 的文件。未 push、未开 PR、未改共享需求和决策文件。
