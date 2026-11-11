# port-board-2：板级底层照原厂（I2C 总线、开机清理、共享电源、前光、触摸、按键、加速度计、马达）

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1b/REPORT.md`（B06、B07、B10、B11、B12、B13 的 TCA 部分）和 `docs/tasks/driver-audit-1c/REPORT.md`（C10、C19 和 P0 开机清理、P14 复位）。

## 要做的

1. **I2C 总线**照原厂 `bus/metalio_i2c1.c/.h`：100 kHz、10 ms 时钟拉伸上限（B12）、总线卡死恢复、闪存操作前的总线排空接到我们现有的存储入口；TCA9555 读照原厂 STOP 分隔读和共同持锁（B13 的 TCA 部分），`common/power/power_i2c.c/.h` 的回退读法。
2. **开机清理**照原厂 `display/epd_board_metalio_eink4_plus.c:483-520`：软件复位后残留的 GPIO 中断、UHCI、引脚保持在所有驱动接管前清掉（C10）。
3. **P14 共享电源（4G、蓝牙、功放）**照原厂 `epd_board_metalio_eink4_plus.c:263-318` 和 `power_policy/power_hw.cc:242-265`：只有一个所有者；断电、马达放电 500 ms、稳定等待、恢复的时序照原厂；断电前后通知 4G 和蓝牙的现有接口（`ink_s31_cell_suspend/resume`、BH1098 电源通知）**不改**。MCU 重启而 BH1098 没断电时的处理照原厂（C19）。
4. **前光**照原厂 `periph/frontlight.cc:246-278,311-323`：硬件初始化、10% 输出上限、冷暖混色（B10：我们满亮度是原厂的 5～10 倍）。系统的亮度、冷暖设置和现有渐变保留，不引入原厂 NVS 和第二套渐变定时器。
5. **触摸**照原厂 `display/metalio_touch.c/.h` 的复位初始化（P12 复位、10 ms 低 / 300 ms 高、就绪等待、读 ID 失败重试，B06）和坐标映射（B07 横屏轴布局）；触摸固件升级不在本任务（要用户定）。
6. **按键**照原厂 `input/metalio_keys.cc/.h` 的采样、消抖、恢复；**加速度计**照原厂 `common/sensor/sc7a20h.cc/.h` 和 `periph/metalio_accel.cc` 的探测、配置、读数、初始化失败时清共享 INT（B11）；**马达**照原厂 `input/metalio_vibe.cc/.h` 引擎（P2，放电脉冲不受触感开关影响）。系统的按键语义（1.5 s 菜单、锁屏、音量）和触感三档保留。

## 范围和预算

- 只改：`ports/esp32s31/s31_board.c`、`hal_i2c_s31.c`、`hal_input_s31.c`、`s31_irq.c`、`hal_device_s31.c`（如前光、马达在这里）、`drivers/chips/tca9555/`、`drivers/chips/ft6336u/`、`drivers/chips/sc7a20h/`、`ports/esp32s31/tests/` 里对应测试和桩、新增 vendor 拷贝目录。**不改** `hal_display_s31.c`、`drivers/chips/tps65185/`（屏幕任务的），不改 4G、蓝牙、充电文件。
- 胶水新增不超过 350 行。

## 验收

1. 上面 6 项都走原厂原文，`SOURCE.md` 列清来源和改动行。
2. S31 宿主机测试全过（每个修掉的坑一条测试加反向对照）；主工程普通全量过；S31 编译通过。
3. 报告有上机核对清单：I2C 拉伸、前光 0/50/100 亮度和冷暖、触摸冷启和软重启、两点触摸、按键和长按、加速度计 INT 释放、马达三档、P14 复位后 4G 和蓝牙恢复。
