# port-hal-1：S31 端口的第一批 HAL（只编译，不上机）

通用规矩：`ink-codex/briefs/subagent-rules.md`（仓库外，和仓库同级的工作目录）。

完成标准要求“各项硬件驱动正常”。现在 S31 端口（`ports/esp32s31/`）能编译链接，但 HAL 全是空实现（`hal_stubs.c`）。这一批先写不接设备也能写对的部分，靠 S31 构建和 CI（`s31-port.yml`）验证能编过。**不烧录、不连设备、不写任何设备。**

## 要做的

只改 `ports/esp32s31/` 下的文件（必要时加新文件），不改 `system/`、`drivers/`、`boards/` 的行为；需要的板级常量从 `boards/metalio_plus/` 引用，不要抄两份。

1. **I2C**：用 ESP-IDF 的 `driver/i2c_master.h` 实现 HAL 的 I2C 接口（总线、读写、超时、错误码映射）。引脚和速率按仓库里的资料：`docs/research/simulator/hardware-parity.md`（第 4 项等）、`tools/hw-bench/main/bench.c`（量过的引脚）、`docs/hardware/README.md`。
2. **GPIO 中断**：板子上芯片的中断脚（如 GPIO2）用 `gpio_isr_handler_add` 接到 HAL 的中断回调，ISR 里只做最少的事、把事件交给任务。
3. **UART**：用 `driver/uart.h` 实现 HAL 的串口接口，给 4G 模组（NT26）和音频芯片（BH1098G）用；波特率、引脚按上面的资料。
4. **板级接线**：让 `drivers/chips/*` 和 `boards/metalio_plus/` 在 S31 上用上面这三样真接口跑起来（模拟器里它们走的是模型）；电量计、充电、RTC、前光、触摸这些 I2C 芯片的初始化能编过、调用顺序和模拟器一致。
5. **屏幕输出层评估（D-124：自己写，不用 EPDiy 的代码）**：只写一份简短的设计说明和工作量估计（放在报告里），内容是用 S31 的 LCD/并口外设把每帧的墨水屏数据送出去要做哪些事（时序、DMA、和现有刷新算法 `drivers/display/pqbn/` 的接口），每项估多少个一小时任务。不写代码，也不要看或抄 EPDiy 的源码（它是 LGPL）。
6. 每个 HAL 函数在报告里列一张表：已实现、仍是空实现、要上机才能确认的点（比如中断电平、时序）。

## 验收

1. `ports/esp32s31/build.sh` 退出 0，没有新的编译警告；模拟器全量测试和内存检查不受影响（只改了端口目录）。
2. 报告里有 HAL 状态表、引脚和参数的出处（仓库内文件和行号）、屏幕输出层的设计说明和工作量估计、上机时要先核对的清单。
3. 隐私：不写设备的 MAC、序列号等标识；不读 `ink-hw`。
