# driver-audit-1a：4G、Wi-Fi、蓝牙耳机

先读 `docs/tasks/driver-audit-1.md`（总说明），照它做，报告写到 `docs/tasks/driver-audit-1a/REPORT.md`。

本块范围：

- 4G 模组整条链：`drivers/chips/nt26/`、`ports/esp32s31/s31_modem.c`、`s31_uhci.cc`、`s31_cell_rx.c`、`hal_cellular_s31.c`、`s31_cell.h`、`vendor/uart-uhci/`，对照原厂 `uart_eth_modem.*`、`nt26_board.*`、`dual_network_board.*` 和它用的 iot_eth。逐段比：帧格式和重组、唤醒握手（MRDY/SRDY）、AT 收发和超时、URC、开机初始化顺序、数据通道启动和握手、网卡和 DHCP、睡眠前后、P14 共享电源和断电通知、出错恢复、切卡流程。总说明第 6 条里“开机时数据通道已开”这个问题要给出原厂代码层面的解释和建议。
- 另外评估：把我们的模组驱动整体换成原厂 `uart_eth_modem.cc` 原文（加 iot_eth）要多大工作量、我们的 AT 允许列表和日志脱敏怎么包在外面、宿主机测试怎么接。
- Wi-Fi：`ports/esp32s31/hal_wifi_s31.c` 对照原厂 `wifi_board.cc`、`dual_network_board.cc`（初始化、省电模式、重连、和 4G 切换、SNTP）。
- 蓝牙耳机 BH1098：`drivers/chips/bh1098g/`、`ports/esp32s31/hal_bt_s31.c`、`s31_bh1098.*`，对照原厂代码和 `docs/bh1098-audio-protocol.md`。
