# port-cell-2：4G 照原厂 v2.0.77 补齐：DMA 收发、共享电源时序

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-cell-2/REPORT.md。**只编译、跑宿主机测试，不碰设备。** 真机验证由主会话做。

## 背景

用户 2026-11-11 要求 4G 参考原厂最新代码，事后又问“4g 之前说要参考原厂最新代码 你参考了吗”。`port-cell-1`（已合入）照原厂 `690a023` 写了帧协议，但有两处没跟原厂：

- **接收**：原厂新旧两版都用 UHCI DMA 收 2 Mbps 帧。v2.0.77 把组件收进了仓库：`components/78__uart-uhci`（78/uart-uhci 0.2.2，Apache-2.0，C++；组件目录里没有许可证文件，全文从 https://github.com/78/uart-uhci 取，或用标准 Apache-2.0 文本，V 里 `main/boards/common/camera/GC2145-LICENSE` 就是标准全文）。`port-cell-1` 改成了 IDF 串口环形缓冲 4 KB 加一个 I/O 任务（`ports/esp32s31/s31_modem.c:182`、`:259-269`）。
- **共享电源 P14**（4G、蓝牙、功放共用）：v2.0.77 新加了放电和稳定时间，断电前后还要通知 4G。我们的 `ink_s31_cell_suspend`／`resume`（`s31_modem.c:413`、`:423`，调用处 `hal_power_s31.c:157`、`:188`）没有跟上。

原厂只读副本：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e`（v2.0.77），下文记作 V。**不要动** `ink-hw/upstream/Metalio-E-INK4-Plus`（固定在 `690a023`，S31 构建按它校验屏幕输出层）。

原厂相关位置（主会话已核对存在）：

- V`/components/78__uart-uhci/`：`include/uart_uhci.h`（190 行）、`src/uart_uhci.cc`（562 行）、README（缓冲池、空闲 EOF、owner 机制、溢出恢复、PM 锁、同步 FIFO 发送）。
- V`/main/boards/common/board/uart_eth_modem.cc`：`InitUart` `:526`（只配参数和引脚，不装串口驱动，RX 上拉）；UHCI 初始化 `:147-165`（4 块 × 1600 字节，`dma_burst_size` 32，见 `uart_eth_modem.h:103-104`）；中断回调 `:865-878`（把缓冲指针送进事件队列）；`HandleRxData` `:958-1086`（处理完 `ReturnBuffer`）；进入 active 时 `StartReceive` `:1181`；回到 idle 时 `StopReceive` `:1228`；发送 `:795`。
- 电源：V`/main/boards/metalio-e-ink4-plus/config.h:81-82`（`POWER_DISCHARGE_MOTOR_PULSE_MS 500`、`POWER_RAIL_SETTLE_MS 80`）；V`/main/boards/common/power_policy/power_hw.cc:182-200`（断外围电源前 `PreparePowerOff`，恢复成功后 `OnPowerRestored`）、`:242-266`（P14 复位：先 `PreparePowerOff`，等当前波形结束并挡住刷屏，关马达和功放，P14 拉低，马达放电 500 ms，P14 拉高，等 80 ms 再允许刷屏，然后恢复音频和 4G）；V`/main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c:270-318`（放电脉冲）、`:338-352`（恢复外围电源后等 80 ms）、`:702-709`（开机 TCA 初始化后等 80 ms 再启 TPS）；V`/main/boards/common/board/nt26_board.cc:112`（`PreparePowerOff`）、`:321`（`OnPowerRestored`）。

## 要做的

1. **收发改用原厂 UHCI 组件**：把 V`/components/78__uart-uhci` 原样放进 `vendor/uart-uhci/`（字节一致，附 Apache-2.0 许可证全文和来源、版本说明），作为 S31 的 IDF 组件编译。写一个很薄的 C 封装（`extern "C"`，放 `ports/esp32s31/`）。`s31_modem.c` 改成和原厂一样：
   - 串口只配参数和引脚，不装驱动；4 块 × 1600 字节缓冲池。
   - 中断回调只把缓冲交给 I/O 任务。I/O 任务把数据喂给现有的帧解析，处理完立刻归还缓冲。
   - 进入 active 开始接收，回到 idle 停止接收；发送走组件的 `Transmit`。
   - 溢出时照组件的恢复办法来，报告写清帧解析怎样重新对齐、丢掉的那段会怎样（AT 回复、URC、以太网帧各自的后果）。
2. **共享电源时序照 v2.0.77**：
   - `ink_s31_cell_suspend`／`resume` 对应原厂 `PreparePowerOff`／`OnPowerRestored`，调用时机和原厂一致：断 P14 之前、P14 恢复成功之后。
   - 断 P14 后马达放电 500 ms；P14 恢复后等 80 ms，才放行下一路电源、屏幕总线或 TPS；开机 TCA 初始化后同样等 80 ms 再启 TPS。
   - 位置在 `boards/metalio_plus/metalio_board.c` 和 `ports/esp32s31/hal_power_s31.c`，常量写成和原厂同名的宏。
   - 已经一致的写明“已一致”和行号，不要重写。
3. 报告里加一节“和原厂 v2.0.77 的差别”：逐条列出还剩哪些不同和原因（包括 `port-cell-1` 报告第 20-24 行列的那些），没有就写没有。

预算：产品代码净增不超过 300 行（不含原样放进来的组件）。不新增与本任务无关的机制。

## 验收

1. **宿主机测试**：把收数据的胶水层（缓冲进、喂解析、归还）做成能在宿主机编译的小文件，用假的 UHCI 封装测：
   - 一帧跨两块缓冲能拼上；空闲 EOF 来的短缓冲正常处理。
   - 缓冲全被占住时触发溢出；归还后能重新对齐，下一帧正确解析。
   - 每块缓冲都归还，不漏。
   - 发送走 `Transmit`。
   - P14 时序：断电前先通知 4G，放电 500 ms，恢复后等 80 ms，最后才通知 4G 恢复。
   - 反向对照：不归还缓冲、忽略溢出、去掉 80 ms 等待、把通知放到断电之后，各改一处，测试都要失败。
2. 模拟器全量和 ASan 版通过；S31 编过，产品代码 0 警告，报告镜像大小；gcccheck 通过；隐私扫描（不得出现 IMEI、IMSI、ICCID）。
3. 报告写清真机上怎么验：刷屏时连续收 AT 回复和 URC 不丢；有数据流量时整屏刷新不断网。
