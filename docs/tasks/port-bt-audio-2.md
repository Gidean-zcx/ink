# port-bt-audio-2：蓝牙耳机和声音通路照原厂 BH1098 原文

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1a/REPORT.md` 的“BH1098 耳机”一节（B02、B04、B05）和 `docs/tasks/driver-audit-1c/REPORT.md` 的声音部分（C19 和 P1 音频）。原厂协议文档：导出里的 `docs/bh1098-audio-protocol.md`。

## 要做的

1. 照原厂 `periph/metalio_audio.cc/.h` 的上电、`ApplyMode`、模式应答等待和有界重试（B02：首次模式命令或应答丢了不能一次就失败）、电源通知；`common/audio/bt_audio_codec.cc/.h` 的 I2S（主控是从、16 kHz、32 位立体声槽、采集右移 12 位限幅、播放单声道送两槽）；`display/screen/bluetooth_screen/bluetooth_uart.cc` 里模式、扫描、连接应答、SCO 的原文（不带页面和 LVGL）；需要的 `common/audio/simple_uart.hpp` 接口。
2. B05：耳机断开或忘记时，音频路由、蓝牙服务状态、配对记录要一致（照原厂顺序：先切回模式 1 并确认应答，再报告断开）。B04：BH 主动回连和耳机离线的通知照原厂处理；原厂文档没定义的断连协议照实写进报告，不自己编。
3. C19：MCU 重启而 BH1098 没断电时，照原厂处理旧模式（P14 复位本身由 `port-board-2` 负责，这里只调用它现有的接口或照原厂做模式重设）。
4. 保留：单个 UART 所有者（现在的接收任务）、蓝牙能力和配对库、事件、成员隔离；原厂 `SimpleUart` 会新增串口任务，不能和现有接收任务并存，选一个并写明资源。

## 范围和预算

- 只改：`ports/esp32s31/hal_bt_s31.c`、`hal_audio_s31.c`、`s31_bh1098.c/.h`、`drivers/chips/bh1098g/`、`ports/esp32s31/tests/` 里蓝牙和声音测试与桩、新增 vendor 拷贝目录。不改 `hal_cellular_s31.c`（通话时切路径的调用点只用现有接口）。
- 胶水新增不超过 250 行。

## 验收

1. B02、B04、B05、C19 各有测试和反向对照；S31 宿主机测试全过（含 audio、bt 负向测试）；主工程普通全量过；S31 编译通过。
2. 报告有上机核对清单：开机首次录音和播放、扫描连接耳机、断开后回到外放、通话时 SCO、MCU 重启后第一次放音。
