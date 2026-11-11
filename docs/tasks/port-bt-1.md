# port-bt-1：S31 接上蓝牙耳机（BH1098 模式 2），照原厂 v2.0.77

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-bt-1/REPORT.md。**只编译、跑宿主机测试，不碰设备。** 真机验证由主会话做。

## 背景

用户 2026-11-11 要求把原厂对比任务（`docs/tasks/vendor-feature-audit/REPORT.md` 第 59、60 行，蓝牙耳机模式 2、扫描和连接，都是“部分”）补齐。

- 现状：S31 的 `hal_bt_*` 全是占位（`ports/esp32s31/hal_stubs.c:40-46`）。
- 接口 `platform/include/ink/hal_bt.h` 的注释写着“扫描、配对、连接的串口协议没见过”。现在原厂 v2.0.77 有完整文档和代码，这句注释要跟着改。
- 系统侧已做好：`system/services/bt/bt_service.c`、模拟器耳机 `ports/sim/hal/hal_bt_sim.c`、蓝牙设置页。
- BH1098 驱动：`drivers/chips/bh1098g/bh1098g.c`（切模式）。S31 音频上电和模式 1 初始化在 `ports/esp32s31/hal_audio_s31.c:62`。

原厂只读副本：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e`（v2.0.77），下文记作 V。主会话已导出下面这些目录：

- V`/docs/bh1098-audio-protocol.md`（254 行）：模式、命令、应答、时序和限制。UART1 115200 8N1；命令以 CR 或 CRLF 结尾；文中的 GPIO7/8 是 BH 芯片一侧的编号。
- V`/main/display/screen/bluetooth_screen/bluetooth_uart.cc`（541 行）：模式、名称、版本、SCO、HID、文本应答。
- V`/main/display/screen/bluetooth_screen/bluetooth_screen.cc`（1068 行）：扫描、连接、复位、模式入口的流程。
- V`/main/boards/common/audio/simple_uart.hpp`、`bt_audio_codec.cc`：串口和 I2S。
- V`/main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc`：上电、模式 1 初始化、应答等待。
- V`/main/boards/common/power_policy/`：共享电源 P14、硬件复位。

## 要做的

1. **照原厂实现 `hal_bt_*`**（C，放 `ports/esp32s31/`）：扫描、配对、连接、断开、忘记、连上和断开的事件，用原厂的命令、应答和时序，写清每处对应的原厂文件和行号。
2. **声音跟着走**：连上耳机时切模式 2，声音和通话走耳机；断开回模式 1，走喇叭。
   - 和现有音频共用 UART1 和模式切换，只能有一个地方管 BH1098 的串口，不能各发各的。
   - 通话时的 SCO 照原厂文档做；文档没写清的，不猜，写进报告。
3. **不做的**：D-120 已排除“当蓝牙音箱”和“翻页器”，所以模式 3、模式 5、ESP 片上 BLE 翻页器都不做。
4. 共享电源 P14 断开再接通后，按原厂恢复 BH1098。本任务在 `port-cell-2`（P14 时序照 v2.0.77）合入之后开始，沿用它留下的断电前和恢复后通知，不另起一套。

预算：产品代码净增不超过 600 行。

## 验收

1. **宿主机测试**：用假的 BH1098（按原厂文档应答）测：
   - 扫描到设备，配对，连接，断开；
   - 连上后切模式 2，断开回模式 1；
   - 应答超时；耳机自己断开时发事件；
   - 和音频的模式切换不打架。
   - 反向对照：连上不切模式、断开不回模式 1、两处同时发串口命令，各改一处，测试都要失败。
2. 模拟器全量和 ASan 版通过；S31 编过，产品代码 0 警告；gcccheck 通过；能力清单检查通过；隐私扫描（不得出现真实蓝牙地址）。
3. 报告写清：
   - 和原厂的差别；
   - 上机第一步：只读查 BH1098 版本和当前模式；扫描要用户在场，拿用户自己的耳机连接；
   - S31 上会发的每一条 BH1098 命令，以及谁会触发它。
