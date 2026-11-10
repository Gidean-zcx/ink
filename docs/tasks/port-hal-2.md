# port-hal-2：S31 端口第二批 HAL（输入、电源），只编译，不上机

总规则先读 docs/tasks/codex-rules.md，第 7、8 节优先。报告写到 docs/tasks/port-hal-2/REPORT.md。

port-hal-1 已经在 `ports/esp32s31/` 写好 I2C、UART、GPIO 中断和板级接线（见 `docs/tasks/port-hal-1/REPORT.md`，里面有 HAL 状态表和上机核对清单）。这一批接着写输入和电源。**不烧录、不连设备、不写任何设备，不读 ink-hw。**

## 要做的

只改 `ports/esp32s31/` 下的文件（必要时加新文件）；板级常量从 `boards/metalio_plus/` 和 `ports/esp32s31/s31_board.h` 引用，不抄两份；芯片驱动用 `drivers/chips/*` 现有的。

1. **`hal_input_*`**：按键（经 TCA9555 扩展芯片，GPIO2 中断里读口、去抖，和模拟器 `ports/sim/hal/hal_input_sim.c` 的语义一致：短按、长按、组合键）；触摸（FT6336U，GPIO5 中断，有触点时按模拟器的节奏采样、抬起后停止，坐标转换和模拟器一致）；把事件送进 `hal_input_set_sink` 的回调。登记 GPIO5 中断用 port-hal-1 的 `s31_irq_watch`。
2. **`hal_power_*`**：浅睡、深睡（关机）、重启、CPU 提频的进入和退出顺序，移植模拟器 `ports/sim/hal/hal_power_sim.c` 的流程（唤醒源：电源键、RTC、GPIO2，以及 accel-use 新加的 `HAL_WAKE_MOTION`）；振动马达（`hal_haptic_*`，经扩展芯片或 LEDC，照资料）。
3. 每个函数在报告里列：已实现、仍空、上机要确认的点（中断电平、去抖时长、浅睡电流、唤醒原因的读法）。资料出处写仓库内文件和行号。

## 验收

1. `ports/esp32s31/build.sh` 退出 0，没有新警告；模拟器全量测试不受影响（只改端口目录和端口的单元测试）。
2. 能在宿主机上测的纯逻辑（例如按键去抖和长按判定、触摸坐标转换）抽成和 port-hal-1 的 `s31_hal_map.h` 一样的可测头文件，配单元测试和反向对照。
3. 报告里有 HAL 状态表、出处、上机先核对清单。隐私：不写设备标识。
