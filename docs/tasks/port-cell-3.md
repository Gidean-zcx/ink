# port-cell-3：4G 模组驱动换成原厂 uart_eth_modem 原文

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1a/REPORT.md` 的“4G 模组”“开机数据通道已开的专项结论”“直接拷贝清单与替换评估”“宿主机测试怎么接”几节。

## 要做的

1. 把原厂 `main/boards/common/board/uart_eth_modem.cc`、`.h` 和 `components/espressif__iot_eth/`（两份 `.c`、`include/`、`interface/`、构建和版本文件、许可证）原文拷进 `vendor/`，作为 4G 帧协议、唤醒、AT 收发、URC、开机初始化、数据通道、网卡、出错重建的**唯一实现**；`vendor/uart-uhci/` 已在，直接用。生命周期照原厂 `nt26_board.cc` 的 `TearDownModem`、`PreparePowerOff`、`StartOrRestartModem` 和 P14 电源恢复原文接（去掉原厂 Board/UI/NVS 依赖）。
2. 撤下我们自己写的 `ports/esp32s31/s31_modem.c`、`s31_uhci.cc`、`s31_cell_rx.c` 里与原厂重复的部分，保留并改接到原厂类的：`hal_modem_at`、`hal_modem_at_payload`（短信 PDU 的 `>` 提示）、URC 队列、`hal_cellular_s31.c` 的状态和流量接口、选网和数据漫游、`ink_s31_cell_suspend/resume`（P14 断电前后）。
3. **安全闸口放在原厂 SendAt/发送边界**：我们的允许列表（`s31_modem.c` 现有的精确命令和参数检查，含 D-128 切卡的三条）对原厂内部初始化发的命令同样生效。原厂会在“NAT 没配好”“波特率不对”时写模组配置（`AT+ECPCFG`、`AT+ECNETCFG="nat"...`、`AT+XJCFG=netPortBaudRate`、`AT+ECRST`）以及设睡眠参数 `AT+ECSCLKEX`：这些被拒时要变成结构化的“配置只读/条件不具备”，不能当成功继续，也不能悄悄发出去（CLAUDE.md：不改模组配置；`AT+ECSCLKEX` 是否属于持久配置先查资料，查不清就拒并写进报告）。
4. **日志脱敏覆盖原厂代码**：`QueryModemInfo` 的设备标识、超时日志里的命令参数、iot_eth 打的地址都不能进日志；只记固定事件、结果、长度和计数，沿用现有的位置码截断和十位以上数字遮盖。
5. 今天真机证实、要保留的行为：开机先 AT 探测（原厂 AtDetect，2/3 Mbit/s）、唤醒窗口后强制发送（原厂）、先 `AT+CFUN=1` 再查卡（原厂）、长回复读全、**开机时数据通道已开就照首次启动再握一次手，模组不回应才关开射频一次**（我们的偏差，换装 14、15 真机证实模组会回 ACK，见试用记录；原厂这里只等 DHCP，会卡住）、设置页手选切卡（D-128）。
6. 审计列的 4G 相关坑一并处理：C06（`+ECNETDEVCTL: 0/1` 通知不能忽略，通道掉了要让上层知道并在回来后恢复）、C02（照原厂主任务和独立 TX 的结构）、C10（浅睡时 SRDY 唤醒电平照原厂）、C12（出错重建照原厂节流）。
7. 宿主机测试：让拷来的 C++ 原文编进 `ports/esp32s31/tests/` 的 cell 测试（CMake 启用 CXX，补最小的 FreeRTOS 队列/事件/线程和 iot_eth 链路事件替身），C HAL 是被测入口。现有 cell 测试的全部断言保留（拒绝 AT、付费动作不重发、分块回复、URC、暂停前归还 DMA、电源通知、通道已开时的握手和关开射频）；新增：原厂内部配置写被拦、日志无标识和位置、`+ECNETDEVCTL` 0/1 插在普通 AT 中间、state=1 时 DHCP 不来的恢复。

## 范围和预算

- 只改：`ports/esp32s31/s31_modem.c`、`s31_uhci.cc`、`s31_uhci.h`、`s31_cell_rx.c`、`s31_cell.h`、`hal_cellular_s31.c`、`ports/esp32s31/CMakeLists.txt`（加组件目录）、`ports/esp32s31/tests/`（cell 相关和 CMake）、新增的 `vendor/` 目录。`drivers/chips/nt26/`（短信、电话、切卡等 AT 业务）保留，只在接口变化时改调用。
- 胶水和安全层新增不超过 450 行（撤下的旧代码不抵扣），拷来的原文不计。
- 资源：原厂 Start 建主任务 4096、TX 3072、初始化 4096 字节栈，内部 RAM 增量写进报告（`size.txt` 对比）。

## 验收

1. 4G 帧协议、唤醒、AT、URC、初始化、数据通道、网卡、重建都走原厂原文；`SOURCE.md` 列清来源和改动行。
2. S31 宿主机测试全过（含新增项和反向对照）；主工程普通全量过；S31 固件编译通过。
3. 报告有上机核对清单：开机日志里看哪几行（探测、CFUN、查卡、ECNETCFG、通道已开时的握手、DHCP、对时）、重启后上网恢复、切卡、短信 PDU 提示、通话 ATD/ATA/CHUP。
