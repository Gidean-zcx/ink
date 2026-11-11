# port-wifi-2 执行报告

日期：2026-11-11。基线：`9fbe0e93e4c9ba590155df768999329399279cfe`。状态：开发与可执行验证结束，未全部验收通过；主工程有基线阻塞，W04 有接口缺口，待复核及上机。

只操作本分支，不接设备、不烧录、不读串口、不推送。证据标签：【实现】代码核查；【实测】本次宿主命令结果；【估算】待上机核对的判断。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| W02 | 宿主通过【实现＋实测】 | 原厂 NONE、11b/g/n、功率请求 48；省电/协议/功率反向对照均失败 | `build-s31-host/test-s31-wifi`；`python3 ports/esp32s31/tests/wifi_negative.py build-s31-host` |
| W03 | 宿主通过【实现＋实测】 | 同 AP 重连 5 次、RSSI 候选轮换、10 秒重扫；LTE 下发现后切回 Wi-Fi；4 条专用反向对照失败 | 同上 |
| W04 | HAL 路径通过，设置链路有边界【实现＋实测】 | NONE/LTE 停射频、恢复 profile、旧重试不得重启；同网型时上层不调用 HAL，见下文没做到的 | 同上；`system/services/net/net_service.c:131` |
| 保留项 W01/W05/W06 | 宿主通过【实现＋实测】 | RAM/NVS、4 个 HAL 监听者、双网默认路由、`ink_s31_net_changed`、Wi-Fi/ETH SNTP 与 UTC 回调；未改服务、HAL 头或模组 | `build-s31-host/test-s31-wifi`；全量中的 `integration-net-auto`、`unit-s31-net-time` |
| T1 | 通过【实测】 | `100% tests passed out of 30` | `ctest --test-dir build-s31-host --output-on-failure -j2` |
| T2 | 未通过【实测】 | 全量 391 项：390 通过，`integration-races` 因基线编译错误未运行；无跳过 | `cmake --build build -j2`；补建其余目标 `cmake --build build -j2 -- -k`；`ctest --test-dir build --output-on-failure -j2` |
| T3 | 编译通过【实测】 | `build.sh` 退出 0；镜像 4,356,864 B，5 MiB 分区余量 17%；编译器 0 警告，SDK 配置仍有已有 5 条警告 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| T4 | 核对完成【实测】 | 原文/许可哈希吻合；新增胶水 184 行；gcccheck 2 个单元、0 发现、0 跳过；隐私 0 命中；原文保留 4 处尾空格 | 下文复现命令；`git diff --stat`；总规则 §6 隐私命令；`git diff --check` |

## 没做到的

- 主工程普通构建被基线的 `ink_state_get_bool` 编译错误阻塞（退出 2）；`git show` 逐字节核对 `tests/integration/races.c` 与基线一致。全量原话：`99% tests passed, 1 tests failed out of 391`，失败项 `244 - integration-races (Not Run)`；CTest 退出 8、总耗时 519.68 秒。没有超范围修改测试、删除注册或隐藏错误。真机验证由主会话执行。
- 公共移植规则第 7 条明确主工程不用跑 ASan；已用 ASan/UBSan 编译并运行 Wi-Fi 定向宿主测试，退出 0，无诊断。
- 没有运行 Linux 宿主环境或全套 S31 ASan；实际内存检查仅上述 Wi-Fi 测试，GCC 检查使用现有工具的 RISC-V GCC 编译到汇编，不能写成 Linux 运行验证。
- 现有 HAL 只有选网，没有独立 Wi-Fi 开关。已在 LTE 上做短时发现时，若用户此刻关闭 Wi-Fi，系统因为选网仍是 LTE 而不再调用 `hal_net_use`；HAL 无法收到这个取消，当前这次扫描完成后才停射频，此后不会再后台扫描。不能声称这个窗口也已做到立即停射频；修复需上层传开关/取消信号，超出本任务限定文件及接口不变的范围。

## 和设计不一致的

- 按任务保留 HAL 的 RAM 凭据、在线双网和 SNTP，不搬原厂 NVS、SsidManager、UI、整机重启选网和包含网络标识的日志。
- [SOURCE.md](../../../ports/esp32s31/vendor/wifi_station/SOURCE.md) 列出全部 C 适配点。完整原文另外保存；实际运行的是其 C/HAL 适配，不能称整个 C++ 类直接运行。
- 选中 Wi-Fi 时照原厂定时重扫；LTE/NONE 下定时器只通知系统。系统在 Wi-Fi 开启且自动选网时查询可用性，才排入短时扫描，扫完停射频；固定 4G 不后台扫描。原因是本任务同时要求 LTE 下重新发现和 Wi-Fi 关闭后不自行恢复，且不允许改 HAL、系统接口。没有自定射频参数。

## 改过的已有测试

- `wifi.c` 初始化顺序断言改为原厂 netif 在 driver init 之前，启动从初始化阶段移到按需恢复阶段。
- 原来断线立即轮换 AP，改为原厂同 AP 最多重连 5 次，再轮换；原来手动失败立即重试已知列表，改为原厂候选耗尽后延迟重扫。旧期望与新任务冲突，未删除对应覆盖。
- `esp_wifi_set_ps` 原先断言“保持 SDK 默认”，改验原厂 `WIFI_PS_NONE`，并加协议/功率断言；扫描超时的 stop 计数改为相对计数，因为停射频流程也会 stop scan。
- `wifi_negative.py` 的重连及扫描结果定位随内部实现更新，原有 8 条反向对照全部保留；新增 10 条 W02/W03/W04 对照。

## 要用户定的

- 是否允许主会话补充“Wi-Fi 启用/取消扫描”到 HAL 的信号，覆盖已在 LTE 扫描时关闭 Wi-Fi 的即时停止。推荐允许小范围补接口；本任务先按现有接口做可逆适配，没有越界改系统服务或头文件。

## 顺手发现

- 基线 `tests/integration/races.c:313` 调用不存在的状态读取 API；`system/include/ink/sys/state.h:45` 提供的是 `ink_state_get_int`。需负责集成测试的主会话修复；本任务不改该文件。
- S31 的 5 条 SDK 配置警告均为 `esp_wifi`/`wpa_supplicant` 私有 include/依赖声明，与 [port-wifi-1 报告](../port-wifi-1/REPORT.md) 所记相同；未改 SDK、未关闭检查。实际 C/C++ 编译和链接没有警告。

## 新增的机制

- 单次 IDF 扫描定时器（10 秒）：任务 W03 和原厂 `WiFiScanTimer` 要求，复用 `ESP_TIMER_TASK`，不新增任务栈。
- 最多 8 个已知网络索引的候选表、每 AP 重连计数：任务要求照原厂扫描/重连流程；索引表 32 B，凭据仍只在原有 PSRAM 表。
- 原厂暂停/恢复和系统可用性查询门控：任务 W03/W04 要求；复用既有互斥量、事件任务和通知，不加设置项、持久格式、线程。

## 规模与阶段证据

- 保守统计：HAL 新增 76 行、删除 29 行，运行适配 `station.inc` 108 行；新增合计 184 行，净增 155 行。原文副本、测试和文档不计。即使把适配中逐字复制的函数也全算胶水，仍不超过 200 行。
- 为什么不能更小：须同时接原厂 profile、保留 netif 的暂停/恢复、5 次重连、候选轮换和 10 秒扫描；只加 3 个射频 API 或只停驱动会遗漏 W03。直接编译原厂整类会引入 NVS/SsidManager，并需改任务范围以外的构建文件。
- 宿主反向对照：未修改版本退出 0；18 个变异版本均编译退出 0、运行退出 -6（断言命中），包括切网后旧重试事件的对照。没有依赖 Git 历史或放宽断言。
- 本地日志位于忽略目录 `work/port-wifi-2/`；当前 S31 全量日志 `s31-host-ctest.log`，反向对照 `negative.log`。构建证据不提交。
- 固件代码提交 `a580c683`，之后只补报告。镜像 `0x427b00` B，最小应用分区 `0x500000` B，剩余 `0xd8500` B。`sdkconfig` 中 Wi-Fi NVS、PHY 校准持久化均为未启用；ELF 无 `nvs_flash_init`、`nvs_flash_erase`、`esp_phy_store_cal_data_to_nvs` 符号。这些静态证据不等于真机行为。
- `gcccheck-host.log` 与 `gcccheck-s31.log` 各为 `gcccheck: 1 compiled, 0 with findings`，均退出 0，没有跳过。
- 提交差异隐私检查 0 命中（总规则命令的 grep 退出 1）；diff 文件范围只有本报告、Wi-Fi HAL/测试/桩和 vendor 新目录。整份差异空白检查退出 2，唯一问题为原文 `upstream/wifi_station.cc:99、310、481、498` 的 4 处尾空格；按“原文保持不动”保留，排除该原文目录后退出 0。
- R-061 核对：HAL 头、系统服务和能力表未变，人和 AI 仍走既有原子能力；扫描/链路变化通过既有网络事件和回调，不把原始 Wi-Fi 配置或密码开放给 AI。

## 复现命令

环境变量沿用任务提供值；构建最多 2 并行，不并跑构建或全量测试。原厂副本全程只读，HEAD 仍为 `690a023c66862af103a3187464373ee7c5efcd14`，文件用 `git show 874538e:<路径>` 读取。

```sh
export PYTHONDONTWRITEBYTECODE=1
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
mkdir -p work/port-wifi-2
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility python3 ports/esp32s31/tests/wifi_negative.py build-s31-host
taskpolicy -c utility cc -std=gnu11 -Wall -Wextra -Werror -UNDEBUG \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I ports/esp32s31/tests/wifi_stub -I platform/include -I ports/esp32s31 \
  ports/esp32s31/tests/wifi.c -o work/port-wifi-2/wifi-asan
taskpolicy -c utility work/port-wifi-2/wifi-asan

taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
# 上条因基线错误失败；补建其余目标，仍以退出 2 结束。
taskpolicy -c utility cmake --build build -j2 -- -k
PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2

taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/wifi.c
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/hal_wifi_s31.c
```

## 要改的文档

- 主会话合并时补充端口说明：原厂固定 profile、5 次重连及 10 秒重扫、LTE 下短时发现；另记录上述独立 Wi-Fi 开关信号缺口。共享文档本次不改。

## 上机核对清单

- 开关 Wi-Fi、飞行模式及切换 4G：看 `STA paused (radio off, netif kept)` 和 SDK 停止日志；重开看到 `STA resumed`、`link up (DHCP)`。固定 4G 或 Wi-Fi 已关时，定时通知不得自行产生恢复日志；LTE 正在短时扫描时关闭的取消信号缺口单独核对，不能据此报告即时停止已通过。
- AP 消失后恢复：断线时默认网卡改 LTE；回覆盖区后出现 `scan done ... known=1`，按现有自动选网稳定窗口切回 Wi-Fi，重新获得 DHCP。固定 Wi-Fi 时候选耗尽后也要 10 秒重扫。功率是原厂请求值 48，不将其写成实测发射功率。
- 连上后看 `sntp received` 和时间服务的 `RTC updated`，核对设备时间；只有 SNTP 接收日志不能说明 RTC 已写成功。
- 4G 与 Wi-Fi 往返切换：默认网卡、DNS 可达性与联网状态一致，蜂窝 `ETH_GOT_IP` 仍能启动对时，回调不包含 SSID、密码、IP 或硬件地址。
