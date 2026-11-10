# port-wifi-1 执行报告

基线：`1450591caecfffdff023b19dc057fad5174a050f`。仅编译和宿主机测试，不接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a | 条件通过，默认分区仍挡构建 | 【实测】S31 编译、链接无警告；忽略目录改用原厂 5 MiB 分区表后完整构建通过，镜像 2,226,176 B；默认 1.5 MiB 分区尺寸检查失败 | `bash ports/esp32s31/build.sh`；临时分区复核见下文 |
| 1b | 通过 | 【实测】端口普通/ASan 各 9/9；时间服务回调单测通过；8 个 Wi-Fi、3 个时间服务反向对照均在编译成功后失败 | 下文端口测试及 `wifi_negative.py` |
| 2a | 普通通过，ASan 进行中 | 【实测】普通全量 338/338；内存检查正在构建 | 下文全量测试命令 |
| 2b | 通过 | 【实测】gcccheck 共 6 个编译单元，0 发现、0 跳过 | 下文 gcccheck 命令 |
| 2c | 待最终核对 | 基线以来的完整提交差异隐私扫描 | 总规则第 6 节命令 |
| 3 | 说明已写，未上机执行 | 【实现】扫描、DHCP、SNTP 接收及 RTC 写入各有日志入口 | 上机先核对清单 |

## 没做到的

- 正在完成模拟器全量 ASan/UBSan 回归。
- 仓库默认 `SINGLE_APP_LARGE` 仍是 1.5 MiB 的 `factory` 分区，完整构建退出 2，错误为 `app partition is too small`。本任务没有接管并行 `port-boot-1` 的分区配置；仅在忽略目录用原厂表复核，不能把默认构建写成通过。
- 未上机。不能据宿主机替身或编译结果声称 S31 已成功起 Wi-Fi、连网或写 RTC。

## 和设计不一致的

- 原厂公开副本没有 `esp-wifi-connect` 组件源码，无法完整核实它定制后的初始化和国家码、省电参数。没有猜国家或发射功率，保留 IDF 世界安全国家码，省电使用 IDF 默认 `WIFI_PS_MIN_MODEM`；需上机核对原厂默认。原厂配置启用 NVS，本任务按闪存合同关闭。
- 闪存合同要求原厂分区表，当前已提交骨架仍是单个 `factory` 分区；本任务没有改表、刷写或改变原厂偏移。临时编译只验证代码和尺寸，试用刷机包还需合入 `port-boot-1`。
- 现有 `time.auto`、`time.synced` 的标题仍写家庭服务器；本次 SNTP 也沿用自动校时开关和该状态，未扩展界面词条。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 【实测】宿主机构建仍有已有的 `ld: warning: ignoring duplicate libraries: 'libink_port_sim.a'`，第三方 SDL 的弃用警告也存在；S31 两轮及最终增量构建未出现编译警告。未改动这些范围外的链接或依赖。

## 新增的机制

- Wi-Fi 连接状态（自动、扫描、连接、暂停）：任务第 1 条要求；复用 IDF 事件任务，使用一个互斥量和一个事件组，不新增项目线程、定时重试任务或队列。
- SDK 的 Wi-Fi、lwIP、默认事件任务：联网所需，均由 IDF 原生初始化路径创建，未另写线程或替代执行器。
- 已知网络 RAM 表（最多 8 项，与现有服务相同）：任务第 1 条要求，由 `/user` 中的现有服务提供，断线轮换连接。
- 网络时间 HAL 回调和一个 `exec.bg` 工作节点：任务第 2 条要求；沿用时间服务的自动校时开关、两秒阈值、RTC 写入及闹钟重排，补偿排队期间的单调时间。不新增时钟服务、持久格式或配置项。

## 要改的文档

- 主会话合并时更新端口 README：Wi-Fi 已移出桩，增加 Wi-Fi 单测和反向对照入口；分区配置说明与 `port-boot-1` 一起更新。
- 系统服务设计的时间来源说明和相关词条需补上 SNTP，保留与家庭服务器校时共用的两秒阈值和 RTC 路径。

## 实现规模

- 当前产品代码净增 250 行（含头文件、端口构建和默认配置，不含测试和文档），未超过 250 行。超过 100 行的原因：七个桩接口需要真实扫描、DHCP 状态、已知网络重连、四监听者、SNTP 和时间服务接入。看过只用阻塞扫描和一次连接的更小方案，会缺超时、重连或平台事件回调；直接从端口写 RTC 则绕过现有服务。

## 原厂和 SDK 出处

查阅日期：2026-11-10。原厂只读副本为 `CloudZao/Metalio-E-INK4-Plus@690a023c66862af103a3187464373ee7c5efcd14`。

- 【实现】原厂 `main/boards/common/board/wifi_board.cc:119–144`：按已知列表注册扫描、连接回调，调用 `WifiStation::Start()`；`407–409`：省电操作转交组件。`main/CMakeLists.txt:248` 引用 `esp-wifi-connect`，但仓库未提供该组件实现。
- 【参考代码，非原厂锁定版本】[78/esp-wifi-connect](https://github.com/78/esp-wifi-connect/tree/cc103899ea8199b0fbf18c9b6e65152d9a92091e) 的 `wifi_manager.cc:74–90` 为 netif → 默认事件循环 → Wi-Fi 初始化，`wifi_station.cc:126–147` 为 STA netif → 回调 → STA 模式 → 启动。本项目独立实现这些 IDF 调用，未引入第三方源码。
- 【实现】本机 ESP-IDF v6.1 的 `components/esp_wifi/include/esp_wifi.h:698` 说明默认省电为 `WIFI_PS_MIN_MODEM`，`:861` 说明默认国家码为 `01`、信道 1–11、自动国家策略。`components/esp_phy/src/phy_init.c:946–975` 在关闭校准存储时走完整校准，不走 NVS 加载/保存。
- 【实现】`components/esp_netif/lwip/esp_netif_sntp.c:34–51` 把 SNTP 时间投递到默认事件循环。本项目接这个事件，再交给既有时间服务的后台执行器，不在 lwIP 回调里操作 I2C。

## 上机先核对清单

由复核者上机执行，本次没有连接设备。

1. 启动先看 `ink_wifi: ready storage=RAM nvs=off ps=MIN_MODEM`。若无 NVS 初始化导致驱动失败，保留错误并停止，不初始化或擦除 NVS。
2. 设置中扫描：看 `ink_wifi: scan done count=N`，页面出现附近网络，RSSI 从强到弱。原厂实际国家码及省电模式尚未核实；核对前不承诺可扫信道 12、13。
3. 人在设备输入密码连接：看 `ink_wifi: link up (DHCP)`，并核对网络状态为在线。失败看 `link down reason=N`，日志不主动输出 SSID、密码、IP 或硬件地址。关闭/恢复 AP 后应重新出现 DHCP 日志，多个已知网络应能轮换。
4. 对时：看 `ink_wifi: sntp received` 和时间服务的 `time: RTC updated`，核对日期时间；重启核对 RTC 保留。已有时间与网络相差不超过两秒时无需再次写 RTC；关闭自动校时且时钟有效时也不写。`sntp received` 单独出现只说明收到时间，不能当作 RTC 写入成功。

## 构建与验证命令

环境使用任务提供的 `INK_DEPS`、`INK_DESIGN`；`INK_UPSTREAM` 指向允许读取的原厂副本。所有构建和测试均使用 utility 优先级，并行数最多 2；日志在忽略的 `work/port-wifi-1/`。

```sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
# ASan/UBSan：换为 build-s31-host-asan，配置增加 -DINK_SANITIZE=ON。
taskpolicy -c utility python3 ports/esp32s31/tests/wifi_negative.py build-s31-host --sim-build build
taskpolicy -c utility bash ports/esp32s31/build.sh
```

【实测】IDF 为 v6.1，提交 `fff9895c82d744c7237be8847347bdd1b07c6643`。SDK 生成配置确认 Wi-Fi NVS、PHY 校准存储均未开启。默认分区的失败镜像为 `0x21f860` B，超出 `0x177000` B 分区；最终增量镜像为 `0x21f800` B，原厂 `0x500000` B 应用分区剩余 `0x2e0800` B。

【实测】最终 ELF 的符号表中 `nvs_flash_init`、`nvs_flash_erase`、`esp_phy_store_cal_data_to_nvs` 均无匹配；这是静态证据，不能代替上机确认驱动启动。

临时分区复核只改忽略目录：复制 `$INK_UPSTREAM/partitions.csv` 为 `build-s31/partitions-trial.csv`；在生成的 `build-s31/sdkconfig` 关闭 `SINGLE_APP_LARGE`、开启 `PARTITION_TABLE_CUSTOM`，文件名设为 `../../build-s31/partitions-trial.csv`。沿用 `build.sh` 的 IDF 环境，然后执行：

```sh
taskpolicy -c utility cmake -S ports/esp32s31 -B build-s31 -DIDF_TARGET=esp32s31 -DSDKCONFIG="$PWD/build-s31/sdkconfig"
taskpolicy -c utility cmake --build build-s31 -j2
taskpolicy -c utility cmake --build build-s31 --target size -j2
```

gcccheck 均退出 0，无跳过：

```sh
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/hal_wifi_s31.c ports/esp32s31/hal_stubs.c
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/wifi.c
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/services/time/time_service.c ports/sim/hal/hal_net_sim.c tests/esp32s31/net_time.c
```

【实测】端口普通原话 `100% tests passed out of 9`，`Total Test time (real) = 0.45 sec`；端口 ASan/UBSan 原话同为 `9/9`，`Total Test time (real) = 1.51 sec`。覆盖扫描无序结果排序/截断/超时、扫描完成后紧随断线时返回值不被覆盖、DHCP、IP 丢失、错误密码/找不到/超时、已知列表复制与轮换、手动失败后恢复、忘掉连接中网络、重复监听者和选择、蜂窝不可用、SNTP 交付。时间服务测试验证后台执行器、RTC 写入、自动校时开关、两秒阈值、无效时钟补时。反向对照两组原实现退出 0；八个 Wi-Fi 变体退出 -6，三个时间服务变体退出 1，全部先编译成功，不依赖 Git 历史。

【实测】普通全量原话：`100% tests passed out of 338`，`Total Test time (real) = 690.41 sec`，没有失败或跳过。

全量配置沿用总规则第 5 节，增加编译命令导出和现成的中枢 Python 环境；普通与内存检查目录分别为 `build`、`build-asan`，后者仅增加 `-DINK_SANITIZE=ON`：

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```
