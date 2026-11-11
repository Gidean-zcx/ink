# driver-audit-1c：电源和充电、电量、时钟、睡眠和开机、存储、摄像头、声音、系统配置

先读 `docs/tasks/driver-audit-1.md`（总说明），照它做，报告写到 `docs/tasks/driver-audit-1c/REPORT.md`。

本块范围：

- 充电和电源：`drivers/chips/cx25601n/`、`ports/esp32s31/hal_power_s31.c`、`s31_board.c`，对照 `periph/metalio_power.cc`、`main/boards/common/power_policy/`（先读 README）。电量计 `drivers/chips/bq27220/`；时钟 `drivers/chips/pcf8563/` 对照 `periph/metalio_rtc.cc`。
- 睡眠、唤醒和开机：`ports/esp32s31/vendor/s31_sleep/`、`s31_boot.c`、`main/main.c`、`s31_panic.c`，对照原厂的开机流程（`app_main.cc`、板子初始化顺序）和电源策略。
- 系统配置：我们的 `ports/esp32s31/sdkconfig.defaults` 对照原厂 `sdkconfig.defaults.esp32s31` 和 `sdkconfig`，只列影响稳定性的差异（缓存和 IRAM 选项、PSRAM、闪存模式和频率、调频和电源管理、看门狗、FreeRTOS、中断），每条说清可能的后果。分区表对照原厂 `partitions*`。
- 总说明第 6 条的开机崩溃（核 1 在闪存操作期间访问缓存区）：结合 ESP-IDF v6.1 在 S31 上闪存操作时核 1 会跑的代码路径（跨核中断、系统节拍、调频钩子等）和两边 sdkconfig 的差异，给出最可能的原因和原厂配置是否避开了它。
- 存储：`ports/esp32s31/hal_storage_s31.c`（littlefs 在内部闪存、TF 卡）对照原厂的存储和 TF 卡用法。
- 摄像头：`drivers/chips/gc2145/`、`ports/esp32s31/vendor/gc2145/`、`hal_camera_s31.c` 对照原厂 camera 代码。
- 声音：`ports/esp32s31/hal_audio_s31.c`、编解码芯片驱动，对照 `periph/metalio_audio.cc`（I2S 配置、功放使能、上电顺序、采样率）。
