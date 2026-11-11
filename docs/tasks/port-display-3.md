# port-display-3：屏幕输出层换到原厂 v2.0.77，屏幕电源照原厂

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1b/REPORT.md` 和 `DISPLAY-CHANGES.md`（尤其“hal_display_s31.c 升级到新版的具体适配”一节）。这就是 backlog 里的 `port-display-2`（用户 2026-11-11 同意补缺任务合完后做）。

## 要做的

1. `ports/esp32s31/display_upstream.cmake` 的固定提交从 `690a023` 改到 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，构建时自己的克隆（构建目录下）取新提交；**不移动** `ink-hw/upstream/Metalio-E-INK4-Plus` 的 HEAD（用环境变量 `INK_UPSTREAM` 时也不许）。照原厂原文用 `output_lcd/lcd_driver.c/.h`、`output_common/line_queue.c/.h` 及它们需要的头。
2. `ports/esp32s31/hal_display_s31.c` 只做适配：新版 abort 合同（任务上下文里调用，等旧中断退出，不回调完成；返回后才改回调、复用扫描缓冲）、中断所在核、DMA 结束处理、原生扫描时序（我们的 8 位总线配置会走原厂新加的原生时序）。PQ-BN 接口和算法不动。
3. 原厂 `lcd_isr` 带 `ESP_INTR_FLAG_IRAM`，里面调用的 `start_ckv_cycles`、`gpio_reset_pin` 不在 IRAM：查清 874538e 是否已处理；没处理的话，照公共规矩第 1 条最小改动让中断路径里调用的函数都在 IRAM（写进 `SOURCE.md`）。B02：原厂 sdkconfig 里和 GDMA IRAM 相关的配置照原厂进 `ports/esp32s31/sdkconfig.defaults`（只加屏幕/GDMA 相关行）。
4. 屏幕电源和保护照原厂：`bus/tps65185.c/.h`、`display/epd_board_metalio_eink4_plus.c` 里 TPS 上电/下电/mask/restore 的原文（B05：I2C 写失败时如实保留“未确认关电”，不能当成已关）。我们的屏保护（guard）对外入口和自检保留。
5. 审计 B01、B02、B04、B05 逐条处理；B13（TPS/TCA 的 STOP 分隔读）里 TPS 这一半归本任务，TCA 那一半归 `port-board-2`。

## 范围和预算

- 只改：`ports/esp32s31/display_upstream.cmake`、`hal_display_s31.c`、`drivers/chips/tps65185/`、`ports/esp32s31/sdkconfig.defaults`（只加屏幕/GDMA 行）、`ports/esp32s31/tests/` 里屏幕相关测试和桩、新增 `vendor/` 或 `ports/esp32s31/vendor/` 下的拷贝。
- 胶水新增不超过 200 行。

## 验收

1. 构建取的是 874538e 的屏幕输出层（构建日志“factory screen output layer @874538e”），没有落到“没有屏幕输出层”的占位。
2. S31 宿主机测试全过（屏幕测试、反向对照：abort 后旧回调不能再读队列）；主工程普通全量过；S31 编译通过。
3. 报告有上机核对清单：刷新日志（`refresh N ... ms= missed=0`）、整屏和局部刷新、换页、灰阶（要用户看）、屏保护自检、超时中止后下一帧正常。
