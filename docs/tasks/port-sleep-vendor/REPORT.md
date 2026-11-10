# port-sleep-vendor 执行报告

基线：`eec5d569be028b539e8778690c239a6947767864`。执行日期：2026-11-10。
范围：只修改浅睡相关端口、板级时序和对应测试；只编译、跑宿主机测试，不连接设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 部分完成 | 【实测】S31 构建退出 0，0 条 warning/error；端口普通版 3/3 通过，ASan 待执行 | 见下方复现命令；`work/port-sleep-s31-final.log` |
| 2 | 通过 | 【实测】13/13 反向对照拒绝，原实现两项通过；要求的四项均编译 0、断言退出 −6 | `taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host` |
| 3 | 通过 | 【实现】下方逐项对照，含缺失端口和保留差异 | 文件/行号见下表 |
| 4 | 待执行 | 模拟器全量、内存检查、gcccheck、隐私扫描 | 待补 |
| 5 | 通过 | 【实现】下方八项上机清单；未把编译写成真机通过 | 本报告“上机先核对清单” |

## 原厂对照

【实现】核查日期 2026-11-10，原厂 [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14) 提交 `690a023c66862af103a3187464373ee7c5efcd14`。MIT 原文及源文件哈希在 `ports/esp32s31/vendor/s31_sleep/`。下表的原厂路径缩写：`R` = `main/boards/common/power_policy/README.md`，`H` = 同目录 `power_hw.cc`，`V` = 同目录 `s31_sleep/`，`B` = `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c`。本项目 `P` = `ports/esp32s31/hal_power_s31.c`，`M` = `boards/metalio_plus/metalio_board.c`，`E` = `boards/metalio_plus/metalio_epd_power.c`。行号均为源码核查，全部未经实机验证。

| 原厂步骤与出处 | 本项目代码行 | 一致性与原因 |
| --- | --- | --- |
| 进入 1：SD 已停放，R:77，H:156 | P:135 | 不一致：已有端口未接 SDMMC/CNNT 停启；本任务未扩展 SD 端口 |
| 进入 2：TPS 休眠、屏总线低，R:78，B:346 | P:136，E:156 | PMIC 一致；屏总线停放仍缺端口 |
| 进入 3：释放加速度计共享 INT，R:79，H:407 | P:137 | 有意不同：先锁采样再配传感器；保留运动监测，防止输入任务提前读走 INT1 锁存 |
| 进入 4：停采样、安全锁存，R:80，B:478 | P:137，M:56 | 一致：按键暂停后才改 TCA |
| 进入 5：GPIO2 输入、上拉、无下拉，等待高，R:81，H:85 | P:141 | 一致；启用 P16/P17 后与最终入睡前再次检查，高不成立就回退 |
| 进入 6：前光灭、P05→P14，R:82，B:292 | P:149，M:88 | 断电顺序一致；保留既有 keep_modem 分支；通信/音频真实端口的停止与恢复仍缺 |
| 进入 7：总线 hold、补丁浅睡，R:83，H:446 | P:155，`ports/esp32s31/s31_sleep_bridge.cc:8` | 补丁调用一致；屏总线 hold 仍缺 |
| PSRAM→XTAL、按 retention 关 MSPI、停 MPLL，V/s31_sleep_sdk.cmake:16 | 同名 vendor 文件:16 | 原样移植，生成构建目录内 PMU 副本；SDK 安装不写入 |
| 醒后 DREG/PVT→模拟初始化→MPLL→MSPI/PSRAM，V/s31_sleep.cc:46，V/s31_sleep_sdk.cmake:30 | 同名 vendor 文件:46、:30 | 低层流程一致；通过三个链接 wrap 接入 |
| 正常方向 0xC0/0xE3，R:117，B:518 | M:9、M:107 | 一致，已有正确实现保留 |
| mask 锁存→全输出→唤醒输入，R:118，B:478 | M:63，M:74 | 一致；P16 唤醒的例外见下 |
| P00/P01 低→50 ms→P02 低→120 ms；已全低仅 120 ms，R:123、:137，B:346 | E:156 | 一致，原实现已符合，本次新增真实板级顺序断言 |
| P03 马达/P04 功放保留锁存，R:124、:125 | M:60，P:122 | 一致；忙或锁存仍高时拒绝睡眠，不凭 mask 假称已关 |
| P05 保留后先切断，R:126，B:292 | M:60、M:91 | 一致 |
| P06/P07 输出 0，醒后输入，R:127、:128 | M:60、M:64、M:107 | 一致 |
| P10/P11 输出 1，醒后输入，R:129 | M:62、M:108 | 一致 |
| P12 保留锁存，R:130 | M:62、M:106 | 一致 |
| P13 输出 0，R:131 | M:62、M:106 | 一致 |
| P14 在 P05 后切、P05 前恢复，R:132，B:292、:303 | M:92、M:118 | 顺序一致；keep_modem=true 时按既有需求保留 P14 |
| P15 输出 0、醒后输入，R:133 | M:62、M:108 | 一致 |
| P16 mask 输出 0、不唤醒，R:134 | M:62、M:79 | mask 一致；任务要求保留 RTC 唤醒，arm 阶段预装高、读基线、再设输入；RTC 已低或共享 INT 不高就取消 |
| P17 mask 输出 1→输入；先锁存再恢复方向，R:135 | M:62、M:75，P:166 | 一致；短按已松开仍保留电源键语义 |
| 唤醒链 P17→TCA INT→GPIO2 EXT1 ANY_LOW，R:139，H:105 | P:89、P:147 | 一致；保留任务要求的 RTC、运动，以及既有保电模组 GPIO3 唤醒 |
| GPIO61 不作浅睡唤醒，R:104、:139 | P:123 | 一致，删除 GPIO 唤醒配置和原因映射；IDF `soc_caps.h:259` 的 LP 引脚数为 8 |
| 退出 1：恢复方向前锁存电源键，R:147，H:457 | P:166 | 一致；运动原因同样在恢复前读取 |
| 退出 2：关唤醒配置、恢复按键、等待稳定，R:148，H:463 | P:171，M:104，P:174 | 一致；输入任务最后恢复，保留已有 20 ms 去抖 |
| 退出 3：P14→P05、前光保持灭，R:149，H:467 | P:177，M:118 | 一致；音频/模组供电状态通知仍待真实端口 |
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
taskpolicy -c utility python3 ports/esp32s31/tests/sdk_check.py "$IDF_PATH" build-s31-host
```

端口 ASan/UBSan：目录换 `build-s31-host-asan`，配置仅加 `-DINK_SANITIZE=ON`。SDK 反向检查【实测】：正确 SDK 退出 0，改版本和删锚点各退出 1，明确显示版本/PMU 源码变化原因。测试只改临时副本。

【实测】S31 最终构建退出 0，日志没有编译 warning/error；SDK 自身仍有 Kconfig 的十六进制写法弃用 NOTE，未关闭检查。`build-s31/compile_commands.json` 中兼容 PMU 副本 1 项、原始应用 PMU 0 项、vendor 源 1 项；最终 ELF 的 `nm` 输出含三个 `__wrap_*`、`ink_s31_sleep_init` 与 `ink_s31_light_sleep`，确认不是只复制而未接入。

反向对照【实测】：要求的 `usb-revive`、`vendor-sleep`、`dynamic-frequency`、`gpio61-wake` 都编译退出 0、断言退出 −6；其余既有九项同样拒绝；两个未修改对照退出 0。证据：`work/port-sleep-{unit-tests,negative,sdk-negative}.log`。

## 上机先核对清单

1. 本任务未连接、烧录或监视设备。后续须在有人能按电源键的条件下测试，遵守 `docs/hardware/2026-11-10-selftest.md:67`。
2. 浅睡定时、电源键短按后松开/保持按住，分别验证补丁返回、PSRAM 数据完好、看门狗无复位。CPU retention 与非 retention 路径的硬件条件需分别核对；编译不证明低层时序已实测。
3. USB：电源键/RTC/运动后电脑重新枚举、恢复连续日志；纯定时不主动断开 pad；无主机连接时约 950 ms 内完成恢复等待。SDMMC 后续接入时，必须把 USB 放到 SD 初始化之后再测，当前无 SD/CNNT 恢复不能据此验收该路径。
4. 共享 GPIO2：空闲高、上拉开启；P16 实际接法与极性、RTC 告警清除、加速度计 INT1 锁存释放；mask、arm 后及即将入睡时拉低，都应取消该次睡眠并恢复方向与电源轨。
5. 逻辑分析核对 P00/P01→50 ms→P02→120 ms，TCA 方向与安全锁存，P17 在恢复方向前读取，P05/P14 断开和恢复顺序；确认 P16 短暂输出低及 arm 预装高适合此板 RTC 电路。
6. 固定空闲 160/活跃 320 MHz、重叠 boost 最后释放恢复空闲、I2C/UART/前光时钟稳定；不能用宿主机断言证明真实切频有效或节电幅度。
7. GPIO61 保持清醒时按键功能，不能当作浅睡唤醒；GPIO3 SRDY 既有极性仍未验证，须与保电模式的短信/来电唤醒一并核对。
8. 屏总线/SD 停启、音频与模组真实收尾仍缺，补齐后才量整板浅睡电流；不以本次编译代替整机电源验收。

## 没做到的

- 未上机验证，以上清单均待实机；屏总线/SDMMC、音频/模组收尾没有在本任务扩展。

## 和设计不一致的

- 与原厂不一致的保留需求与缺口逐行列于对照表。任务书把“拿起亮屏”记为 D-121；现有决策表实际为 D-122，本报告按行为执行，不改决策编号。
- 原厂文件在当前 GNU C++26/IDF 下的 volatile 连续赋值报错及单源唤醒接口警告，做两处编译适配；未改时钟/模拟寄存器次序，未放宽警告。

## 改过的已有测试

- `ports/esp32s31/tests/power.c`：频率期望由动态 160～320+IDF 锁改为固定 min=max，验证两路重叠 boost；RTC 初始低从“去掉 RTC 后继续睡”改为“取消这次浅睡”；AI+模组唤醒不再期待 GPIO61 原因，只保留模组原因。这些均由本任务明确要求；其余已有断言保留。
- `negative.py`：保留既有九项，新增四项；修正临时目录内上一项运动分类变异会污染后续电源变异的问题，每项先恢复原始头文件，重新跑完 13 项。USB/补丁撤除现在分别在缺失顺序标记处失败；未删测试、加 sleep 或提高预算。

## 要用户定的

- 暂无。

## 顺手发现

- 原厂文件适配后首次链接发现当前最小 IDF 构建未引入 USB 连接监测函数；已显式声明 `esp_driver_usb_serial_jtag` 组件依赖并使用其正式头文件，未伪造连接状态。
- 现有 SDK 默认配置同时启用 UART0 主控制台及 USB 副控制台，和任务背景“控制台只走 USB”的说法不同；本任务不修改控制台配置，上机前主会话须复核 UART0 默认引脚与模组复用。

## 新增的机制

- 原厂 S31 浅睡包裹、三个链接 wrap、构建目录 PMU 副本及 RTC 保留诊断：任务明确要求移植的原厂补丁；无诊断后台任务。
- CPU boost 合并计数与互斥：任务要求从 IDF 动态锁改为固定切频，沿用原厂临时 hold 语义，保留现有 HAL。
- SDK 版本/PMU 哈希构建检查：任务要求不兼容时失败。没有新增产品设置、任务、线程、状态机、持久文件格式、队列或重试机制。

## 要改的文档

- 主会话合并时更新 backlog、`docs/system/ui-dependencies.md` 与旧 port-hal-2 状态：浅睡已走补丁、USB 恢复、固定切频、GPIO61 不唤醒；能力表无需改变。
