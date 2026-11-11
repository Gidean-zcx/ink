# port-cell-1：S31 接上 4G 模组（NT26）：帧协议、AT 通道、上网、状态栏信号

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-cell-1/REPORT.md。**只编译、跑宿主机测试，不碰设备。** 真机验证由主会话做。

## 背景

用户 2026-11-11 在试用版上要求“要移植4G”，并且问“这个4G模块可以实现通话和短信吗……不只是蜂窝数据”。现状：

- S31 端口的蜂窝层全是占位：`ports/esp32s31/hal_stubs.c` 的 `hal_cell_*`、`hal_modem_at*`、`hal_modem_urc` 都返回不可用，所以设置页显示“未接入移动网络模组”，状态栏没有 4G。
- 系统侧已经做好：`drivers/chips/nt26/nt26.c`（AT 命令层）、`ports/sim/hal/hal_cellular_sim.c`（把 `hal_cell_*` 接到 nt26 的薄转接）、手机网络设置、短信、通话、紧急呼叫，都在模拟器里测过。
- 【实测】昨晚自检（`docs/hardware/2026-11-10-selftest.md` 第 37、42 行）：115200 和 2 Mbps 直接发 `AT` 都没回应，SRDY 为 1。原厂不是逐行 AT：NT26 跑成“串口网卡”，UART2 2 Mbps 帧协议，AT 命令也包在帧里。S31 的 UART0 默认引脚就是 GPIO58/59，控制台必须只走 USB。
- 原厂只读副本（MIT，`ink-hw/upstream/Metalio-E-INK4-Plus`，主会话已核对）：`main/boards/common/board/uart_eth_modem.cc/.h`（帧协议、MRDY/SRDY 握手、`SendAt`、`SendAtCollectUntil`、以太网帧接 lwIP、读 IMEI/IMSI/ICCID/运营商/版本/信号）、`nt26_board.cc`（上电和启动）、`dual_network_board.cc`（Wi-Fi 与 4G 二选一）、`power_policy/power_hw.cc`（共享电源断开前通知蜂窝）。`boards/metalio-e-ink4-plus/config.h`：`NT26_TX_PIN` GPIO58、`NT26_RX_PIN` GPIO59、`NT26_MRDY_PIN` GPIO60、`NT26_SRDY_PIN` GPIO3；扩展芯片 P12 是触摸和 4G 共用的复位（4G 工作时须为高），P14 是 4G、蓝牙、功放共用的电源。原厂代码里没有短信和通话，配置里也没有模组到 ESP 或音频芯片的声音线。

## 要做的

1. **照原厂移植帧协议**：在 `ports/esp32s31/` 用 C 实现 UART2（GPIO58/59，2 Mbps）加 MRDY/SRDY 握手和帧格式，行为与 `uart_eth_modem.cc` 一致（握手、重传、超时、URC），写清每处对应的原厂文件和行号；保留原厂的 MIT 署名。上电和复位走 `metalio_board` 已有的扩展芯片实例和锁：P12 保持为高；P14 同时管蓝牙和功放，正在放音时不能断（照 `power_hw.cc` 的通知做法）。
2. **AT 通道**：实现 `hal_modem_at`、`hal_modem_at_payload`、`hal_modem_urc`（契约见 `platform/include/ink/hal_net.h`），一次一条命令。
3. **蜂窝 HAL**：把 `drivers/chips/nt26/nt26.c` 和照 `hal_cellular_sim.c` 写的 S31 转接编进 S31 构建，替掉占位。
4. **上网**：照原厂 `InitIotEth` 把以太网帧接成 lwIP 网卡，接进现有的 `hal_net`，让系统在没有 Wi-Fi 时能经 4G 上网（选网规则用系统已有的，不新造）。
5. **状态栏**：注册上网络时，在 Wi-Fi 图标旁显示 4G 和信号格（两个可以同时出现），信号格数来自模组报告的信号强度；没卡、没注册、飞行模式各有清楚的样子。遵守 ui-scale 检查，模拟器里有对应截图。
6. **不改模组的固件和配置**（CLAUDE.md 硬规则）：S31 上不发任何会改模组存储配置的命令。至少包括切换 SIM 槽 `AT+ECSIMCFG=`、设 APN `AT+CGDCONT=`／`AT+CGAUTH=`、删短信 `AT+CMGD`、任何保存类命令（如 `&W`）、频段和 IMS 配置。S31 上 `hal_cell_select_sim`、`hal_cell_apn` 暂时返回不可用，界面照“暂不能更改”显示，写测试证明这些命令在 S31 构建里发不出去。只读查询和运行时命令（开关射频、注册查询、信号、拨号、接听、挂断、发短信本身）可以实现；发短信、打电话花钱，主会话真机上会先问用户。
7. **报告列出 S31 上会发的每一条 AT 命令**，标注只读、运行时还是写存储，以及谁会触发它。

预算：产品代码净增不超过 1200 行（原厂帧协议的移植占大头），测试另算。

## 验收

1. 宿主机测试：用假的串口模组（按原厂帧协议应答）测握手、AT 收发、超时、URC、上网帧收发、复位后恢复；反向对照（去掉握手、改坏帧校验、放开禁止的命令各一条，测试都要失败）。
2. 模拟器全量测试和 ASan 版通过；S31 能编过，产品代码 0 警告；gcccheck 通过；ui-scale 0 违规；隐私扫描（不得出现 IMEI、IMSI、ICCID）。
3. 报告写清：和原厂实现的差别；真机上第一步该怎么验（只读查询：模组型号和固件版本、SIM 状态、注册、信号、短信能力、VoLTE/IMS 能力）。
