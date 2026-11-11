# port-power-2：充电、电量计、时钟照原厂原文

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1c/REPORT.md` 的充电、电量、时钟部分（C02、C03 和 P1/P2 拷贝清单）。`board-vendor-1` 已经照原厂补了一部分充电维护（VREG 4400 mV 核对、终止、再充、3.1 V 切 BATFET，在共享执行器上），这次换成原厂驱动原文。

## 要做的

1. 照原厂 `common/power/cx25601n.{h,cc}`、`power_i2c.{h,c}` 的完整流程（再充、欠压、OTG 相关寄存器处理；C02：再充关了 EN_CHG 后，下一轮电量计读失败不能阻止重新使能，照原厂 200 ms）。原厂的策略任务不照搬成新线程：接到我们现有的共享执行器和定时（写明为什么等价）。
2. 照原厂 `common/power/bq27220_gauge.{h,cc}`（C03：不直接信未标定的 SOC，照原厂的电压/曲线做法；曲线是原厂电芯的，不冒充本机校准，报告里写清）。
3. 照原厂 `common/sensor/pcf8563.{h,cc}`、`periph/metalio_rtc.{h,cc}` 的初始化、校验、清标志（P2）；VL 位（时间不可信）向系统标记“时间未可信”，等可靠时间源，不默认改产品的时间策略。
4. 保留：`hal_power_*` 接口、电源和设置能力、电量显示、充电状态事件；不加原厂 NVS 设置。

## 范围和预算

- 只改：`drivers/chips/cx25601n/`、`drivers/chips/bq27220/`、`drivers/chips/pcf8563/`、`ports/esp32s31/hal_power_s31.c`、`ports/esp32s31/tests/` 里电源相关测试和桩、新增 vendor 拷贝目录。不改 `s31_board.c`（板级任务在改）。
- 胶水新增不超过 250 行。

## 验收

1. C02、C03 各有测试和反向对照；S31 宿主机测试全过；主工程普通全量过（`board-vendor-1` 加的充电测试不放宽）；S31 编译通过。
2. 报告有上机核对清单：充电截止和终止电流读数、再充、电量百分比走势、插拔 USB、RTC 走时和 VL 标记。
