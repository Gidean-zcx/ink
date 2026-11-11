# driver-audit-1b：屏幕、触摸、按键、触感、前光、加速度计

先读 `docs/tasks/driver-audit-1.md`（总说明），照它做，报告写到 `docs/tasks/driver-audit-1b/REPORT.md`。

本块范围：

- 屏幕输出：`ports/esp32s31/hal_display_s31.c`、`display_upstream.cmake`（现在固定取原厂 690a023 的 `output_lcd/lcd_driver.c`、`output_common/line_queue.c`），对照 874538e 的同名文件和 `epd_board_metalio_eink4_plus.c`、`epdiy.c`、`highlevel.c`。列出 690a023 → 874538e 屏幕输出层的全部改动和它们修的问题（中断放在哪个核、DMA 结束处理、中止帧、CKV 时序、原生扫描时序），以及我们 `hal_display_s31.c` 换到 874538e 要改哪里。注意原厂 `lcd_isr` 带 `ESP_INTR_FLAG_IRAM`，里面调用的函数是否都在 IRAM。
- 屏幕电源：`drivers/chips/tps65185/`、屏保护（guard）对照原厂 `bus/tps65185.c`、`epd_board_*`。
- 触摸：`drivers/chips/ft6336u/` 对照 `display/metalio_touch.c`（含固件升级相关的时序）。
- IO 扩展和按键：`drivers/chips/tca9555/`、`ports/esp32s31/hal_input_s31.c`、`s31_irq.c`，对照 `bus/pca9555.c`、`input/metalio_keys.cc`（方向、锁存、中断、去抖、长按、电源键）。
- 触感马达：对照 `input/metalio_vibe.cc`；前光：对照 `periph/frontlight.cc`；加速度计 `drivers/chips/sc7a20h/` 对照 `periph/metalio_accel.cc`；I2C 总线 `ports/esp32s31/hal_i2c_s31.c` 对照 `bus/metalio_i2c1.c`（时钟、超时、总线恢复）。
