# port-cell-3 执行报告

日期：2026-11-11。基线：`281dbe7130514a602c68e4c6de2b7e53c81a5db1`。仅本地提交，不推送、不接设备。状态：执行完成，未全部通过验收；未通过项没有冒充完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 原厂唯一实现、来源、安全 | 部分完成【实现】 | modem、iot_eth、UHCI 已合一；原文哈希和改动行见两个 SOURCE。C10 跨文件限制、ECSCLKEX 的失败后接网行为未完全满足任务书，见下文 | `git diff`、逐行对照、SHA-256 |
| 2 构建、正反测试、固件 | 部分完成【实测】 | S31 宿主普通/ASan/UBSan 均 38/38；13 项反向及 UART/供电 4 项通过；固件编过。主工程构建失败，全量 ctest 4 项失败、3 项跳过，见结果 | 下文 E1–E6 |
| 3 上机核对清单 | 清单完成【实现】 | 列出启动顺序、保电重启、切卡、PDU 和通话；没有真机通过结论 | 文末清单 |

来源：[modem / 生命周期](../../../vendor/uart-eth-modem/SOURCE.md)、[iot_eth](../../../vendor/iot_eth/SOURCE.md)、既有 [UHCI](../../../vendor/uart-uhci/SOURCE.md)。上游均取 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，没有移动上游 HEAD。

## 没做到的

- **主工程构建/全量未通过**：根 `CMakeLists.txt:39–41` 仍用旧 C cell 源列表，缺少 C++/vendor/替身，链接时报 HAL 和 fixture 符号未定义。该根文件不在白名单，没有越界修改；根 ctest 的 cell 和两项负向测试因找不到二进制失败。需主会话按独立 S31 宿主目标同步根目标的 CXX20、源文件、头路径、版本宏和 Threads。
- 主工程 `integration-races` 在 `tests/integration/races.c:120` 失败：`it is in the inbox of who it is for (0)`。未修改模拟器/系统/此测试，也未做基线复现，不判定根因、不越界修复。
- **C10 的端到端浅睡唤醒未修完**：原厂 `InitGpio` 是 SRDY 低电平，宿主断言核对了这一点；`hal_power_s31.c:98` 仍配置 EXT1 ANY_HIGH，会覆盖它。该文件不在任务允许范围，没有越界修改。默认浅睡仍关闭。
- 没有运行设备、实际 DMA/中断/电流、运营商网络、短信或电话测试；没有 Linux 原生 CI。交叉 GCC 和 S31 编译不能代替 Linux 原生测试。
- 不宣称保留了旧 C 解析器“坏尾字节后在同一个 DMA 缓冲内滑窗找下一帧”的行为；原厂会丢掉该缓冲余部。新测试核对清理半帧/半行、独立完整帧和迟到结果恢复。

## 和设计不一致的

- **ECSCLKEX：保留待复核项。** 原厂 `uart_eth_modem.cc:1677–1682` 写睡眠参数；本轮检索与原厂代码没有证明对应固件的持久性，所以允许列表仍拒绝它，返回 `ESP_ERR_NOT_SUPPORTED`、发 `config_readonly` 事件，不打印初始化成功。当前代码在已经真实握手时仍接 iot_eth，以保留换装 14/15 的联网行为；**这不符合“任一配置拒绝后不继续后续接网”的严格解释，本条未完全通过**。NAT 和波特率条件不符时则不接网、不写配置，只留 AT 诊断。
- 已开通道仍发原厂 SPIC；仅无 ACK 时关开射频，每次开机最多一次。依据 [试用记录](../../hardware/2026-11-10-trial.md) 换装 13–15；没有把未收到 ACK 写成已握手。
- 原厂 Board/UI/NVS、状态栏和重启整机策略没有移植；网络初始化失败但 AT 已探测成功时保留 AT，供查卡、切卡和诊断。P14 恢复交给已有通信任务，不新增原厂额外的恢复任务。

## 改过的已有测试

- `cell.c` 从直接 include 私有 C 实现，改为 C HAL 调用单独编译的原厂 C++；补 FreeRTOS 队列、事件、线程和 iot_eth/netif 替身。禁止命令、参数注入、PDU 提示、付费动作不重发、分块回复、URC、流量、切卡、供电通知和 DMA 归还仍有断言。
- 私有状态断言改为 HAL 链路/事件、UART 帧、任务栈参数和缓冲所有权观察。探测仍核对丢前两次探测、2/3 Mbit/s，以及恢复时 9 次探测加一次实际 AT；原厂额外 NAT/CFUN 初始化使总写次数不同，不再把整个初始化误当成只有探测。
- 原厂有一次正常的 `CFUN=1`，射频恢复断言相应核对“正常一次 + 恢复两次”，并核对第二次重建不再关开射频。旧代码的零次正常 CFUN 期望没有沿用。
- 坏尾字节后的滑窗断言差异见“没做到的”，不是悄悄删掉后声称全保留。溢出现在通过原厂溢出回调和真实主队列注入，核对清理、错误返回和全部借用归还，不伪称测试了真实 GDMA 溢出。
- `cell_negative.py` 改为重编译实际目标的单点变体，不依赖 Git 历史；`uhci_negative.py` 的两条 RX 对照转到它，原供电顺序两条对照保留。没有放宽业务断言或产品资源预算。
- 原厂 C 文件按 IDF 的 C23 编译；宿主仅对它们沿用 IDF 的 `-Wno-old-style-declaration`，因为原文有 `const static`。其余 `-Wall -Wextra -Werror` 保留。

## 要用户定的

- 请主会话复核 ECSCLKEX 的失败语义：严格失败就必须暂时阻止当前原厂初始化的接网；若保留已有通道，应明确允许将“睡眠参数未配置”与“数据链路可用”分开。未扩大允许列表，未假定命令不持久化。需要对应 NT26 串口网卡固件的 AT 手册，而非其它型号的睡眠说明。
- 来电/短信依赖模组现有 CLIP/CNMI 默认报告方式；本次不擅自开放配置写。APN 写入仍拒绝。

## 顺手发现

- 原厂普通 SendAt 只留最终帧；直接套 C HAL 会丢长回复，已在同一接收入口修正。
- 原厂注册事件可以先于 DHCP 报 Connected，已改成实际 IP 事件才在线；同时过滤其它 netif 的 IP 事件。
- 首次探测完全失败后，HAL 原本可能一直拿着已停止对象；现在由下一次现有 HAL 请求按原厂 5 秒节流重建，没有后台重试线程。

## 新增的机制

- 原厂主任务 4096 字节 / 优先级 10，TX 3072 / 9，初始化 4096 / 4：任务书第 1、6 项要求，替换旧的 4096 字节 PSRAM 轮询任务。没有第四个恢复任务。
- 原厂主队列增加 RX 缺口事件：保留既有溢出与缓冲归还合同；没有另建队列、线程或状态机。
- 没有新增配置项、持久格式、缓存或通用框架；反向对照复用编译数据库。

## 要改的文档

- 主会话同步根 CMake 的 cell 测试构建接线（共享源文件，不是本分支允许修改的文档）。
- 电源任务同步 SRDY 的 EXT1 极性、保网浅睡和 P14 停放调用；另核对正常关机前是否调用蜂窝 PreparePowerOff。本任务未修改 `hal_power_s31.c` 或板级共享文件。
- 主会话统一更新底层驱动接线说明；原生能力和业务 HAL 未变，无需生成能力表。按钮和 AI 仍经过同一业务 HAL，没有给 AI 开原始 AT 口。

## 审计项处理

原厂行号均指上游提交，不是适配后行号；详见 SOURCE 的行映射。

| 项 | 结果 |
| --- | --- |
| C01 | 原厂帧头、校验、跨 DMA 重组；坏缓冲按原厂丢余部，见断言差异 |
| C02 | 原厂事件主任务 + 独立 TX、唤醒窗口后强制活动；宿主核对栈、优先级和强制发送 |
| C03 | 复用既有 UHCI；Stop 停 DMA并归还尚在事件队列的指针，才 Deinit |
| C04 | GPIO ISR flags=0，保留原厂中断安排；不据此宣称 Cache-off 实机已安全 |
| C05 | SendAt 同一出口允许列表；回复跨帧累积、PDU、迟到结果隔离、超时不重发 |
| C06 | 0/1 URC 可插在普通 AT 中；立即撤下旧握手/IP，并通知现有通信任务重建 |
| C07 | 原厂 AtDetect、先 CFUN 再查卡；NAT/波特率配置写拒绝，ECSCLKEX 待复核 |
| C08 | state=1 重新 SPIC；只在无 ACK 时一次射频恢复，收到 DHCP 才在线 |
| C09 | 原厂 iot_eth + glue，完整 driver handle、GARP 禁用、DNS 清理；保留选网/流量 |
| C10 | 原厂 GPIO 低唤醒已接；外层 EXT1 覆盖未解决，不通过端到端验收 |
| C11 | 已有 suspend/resume 接原厂 TearDown/Prepare/Restore，先 Stop 再让外层切 P14；通用板级通知仍需其它任务配合 |
| C12 | 原厂共享所有权、epoch、操作锁和 5 秒节流；首次探测失败、通道通知、DHCP 无地址均有重建测试 |
| C13 | nt26 业务原样保留；D-128 三条精确命令通过同一允许列表；APN 仍只读 |
| C14 | 原来来电/收信报告前置条件未扩大，留给对应固件实测 |

## 规模和内存

产品新增 **396 行 / 450 行**：旧文件对任务基线，复制文件对原厂逐行数新增，删除不抵扣；测试、文档和原文不计。之所以超过 100 行：需替换 C/C++ 边界、生命周期、回复累积/PDU、统一安全闸口和网络状态；检查过“只包 HAL”方案，挡不住原厂内部配置写，也接不上 PDU，所以不用。

【实测】`build-s31-baseline/size.txt` 对 `build-s31/size.txt`（基线源码归档在 `work/port-cell-3/baseline`，共用相同字库和构建依赖）：

| 静态尺寸（字节） | 基线 | 本次 | 差值 |
| --- | ---: | ---: | ---: |
| Flash | 4233416 | 4265064 | +31648 |
| DIRAM | 150712 | 150886 | +174 |
| 外部 RAM BSS | 683688 | 676688 | -7000 |

【实现＋估算】静态尺寸与动态栈分开：原厂稳态任务栈 7168 字节，初始化时 11264 字节，走内部 RAM（IDF `components/freertos/heap_idf.c:14–20,43–55`）。另有 1600 字节内部重组、两条 32 项队列、TCB/事件/字符串及待发 DMA 帧；旧 4096 字节 PSRAM 栈撤下。4×1600 DMA 缓冲两版都有。没有实机堆峰值，不把静态差值当总增量。

## 验证命令与证据

均在仓库根目录，使用用户提供的 `INK_DEPS`，构建/ctest 并行均最多 2，命令带 `taskpolicy -c utility`。

```sh
# E1：S31 普通宿主
 taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPython3_EXECUTABLE=$INK_DEPS/build-deps/pyenv/bin/python
 taskpolicy -c utility cmake --build build-s31-host -j2
 INK_KEEP_EVIDENCE=1 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-s31-host --output-on-failure -j2
# E2：目录换 build-s31-host-asan，配置另加 -DINK_SANITIZE=ON，其余相同
# E3：反向对照（可另传 --case 名称单跑）
 taskpolicy -c utility python3 ports/esp32s31/tests/cell_negative.py build-s31-host
 taskpolicy -c utility python3 ports/esp32s31/tests/uhci_negative.py build-s31-host
# E4：按 codex-rules.md §5 的字体/LVGL参数配置主工程，构建和测试使用 -j2
 taskpolicy -c utility cmake --build build -j2
 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2
# E5：只编译 S31
 INK_UPSTREAM=$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh
# E6：真实 GCC 检查六个 C 文件
 taskpolicy -c utility python3 $INK_DEPS/../ink-codex/tools/gcccheck.py build-s31-host ports/esp32s31/s31_modem.c ports/esp32s31/s31_cell_rx.c ports/esp32s31/hal_cellular_s31.c ports/esp32s31/tests/cell.c vendor/iot_eth/iot_eth.c vendor/iot_eth/iot_eth_netif_glue.c
```

最终证据在本地 `work/port-cell-3/evidence/`，`work/` 和 `build*` 不提交：

| 检查 | 原话/退出码 | 证据日志 |
| --- | --- | --- |
| 普通宿主全量 | `100% tests passed out of 38`，exit 0 | `host-final-tests.log`；`Total Test time (real) = 52.00 sec` |
| ASan + UBSan 全量 | `100% tests passed out of 38`，exit 0；设置 `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`，LastTest.log 没有 sanitizer 报告 | `asan-final-tests.log`；`Total Test time (real) = 53.47 sec` |
| 反向 | 13/13 均编译/链接 0、运行 -6；按下述四轮取证，不伪称有一次 13 项全跑成功 | `negative-first.log`、`uhci-negative.log`、`negative-independent.log`、`privacy-negative.log` |
| UART/供电反向 | `UHCI/rails reverse controls: 4/4 rejected; vendor/HAL and rail baselines passed`，exit 0 | `uhci-negative.log` |
| S31 固件 | exit 0；warning/error 均无命中，镜像通过分区尺寸检查 | `s31-build.log`、`build-s31/size.txt` |
| 主工程构建 | exit 2，旧 cell 目标未解析符号；其它已构建的主程序仍按要求运行全量 | `main-build.log` |
| 主工程全量 | `99% tests passed, 4 tests failed out of 391`，`Total Test time (real) = 860.63 sec`，exit 8 | `main-tests.log` |
| GCC | 6 个 C 文件、4 个 C++ 文件均零发现；不是 Linux 原生 CI | `gcccheck-final.log`、`gcccheck-cxx.log` |

主工程失败项：`unit-s31-cell`（Not Run）、`unit-s31-cell-negative`、`unit-s31-uhci-negative`（同一根目标接线阻塞），以及 `integration-races`。跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。

反向取证：首轮确认 detect-rebuild、frame-checksum、force-wake、control-not-data、vendor-policy、response-chunks、channel-urc、dhcp-rebuild、dma-return 九项；UART 脚本再确认 rx-gap；`--case rebuild-throttle --case radio-once` 确认两项；`--case vendor-log` 的独立 privacy 场景确认日志回归。日志测试核对 MAC/标识/位置以及 ATD 超时参数，不依赖唤醒场景来判断隐私。

曾出现正向 protocol 在宿主替身的 200 ms 唤醒等待处偶发失败，导致一次反向脚本提前退出。按任务规则第 7 节停止继续改时序机制；分开验证其它项，不抬产品超时。最终完整普通/内存两轮均通过，**不把这个偶发失败说成已消除**。早期反向 rx-gap 暴露测试中前导换行会清掉残留半行，已增加无前导换行的迟到结果；撤掉半行清理后现在确实失败。

`git diff --check` 通过；范围检查仅有任务白名单、两个 vendor 目录和本报告；没有夹带需求/决策/共享根构建修改。按任务规定的隐私 grep 只有两处任务允许的假 MAC 否定断言，没有本机绝对路径或真实标识；提交后按同一命令复查。所有新增文件小于 5 MB。

## 上机核对清单

1. 启动看 `Detecting modem` → `probe_2000000` 或 `probe_3000000`；看 `network_config_query result=1`，只读配置不符应有 `config_readonly`，不能出现写配置后的伪成功。
2. 看 `radio_on result=1` 先于 `sim_query`，再看注册。SIM/位置/号码/IMEI/ICCID 和短信正文不能出现在日志。
3. 已开通道应有 `Network device already started` → `Handshake successful`；随后 `Ethernet Got IP Address` 和已有对时结果。只有 SPIC ACK 不算已联网；无 ACK 才允许一次 CFUN 关开，不切共享 P14。
4. 保持模组供电重启主控，检查再次握手、DHCP、对时与上网恢复；模拟数据通道 0/1 或无 DHCP 时检查状态先掉线，再节流重建；检查流量累计不含控制帧。
5. 设置页分别手选两张卡，核对 CFUN0 → SimSlot → CFUN1、注册恢复，不重启整机。P14 睡眠/恢复先核对 Stop、DMA 归还、1 秒 settle；C10 未解决前不据此启用浅睡。
6. 仅在主会话另有明确授权时测短信/电话：短信 `>` 后只发一次 PDU+Ctrl-Z；无最终确认显示结果不确定，不自动补发；ATD、ATA、CHUP 及来电 URC/音频路径分别验证。宿主假号码不证明真机业务可用。
