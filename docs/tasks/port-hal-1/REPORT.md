# port-hal-1 报告：S31 端口的第一批 HAL（只编译，没有上机）

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。全程没有连接、烧录或写任何设备，没有读 ink-hw。）

检出 `ink-codex/port-hal-1/repo`，分支 `claude/sub-port-hal-1`，基线 `59faf152`。

## 验收表

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| S31 构建 | 通过 | 【实测】`ports/esp32s31/build.sh` 两次退出 0，`warning:` 0 条；新文件都编过；链接产物里有 `hal_i2c_write_read`、`hal_power_battery`、`ink_s31_board_init`、`s31_irq_watch`（nm）；`hal_uart_*` 暂时没人调用，被链接器裁掉 |
| 全量测试 | 一条偶发 | 【实测】`99% tests passed, 1 tests failed out of 306`，失败的 `integration-media`（3 条 CHECK）单独重跑 `100% tests passed out of 1`；这次没改宿主机代码，判断是并行负载下的偶发 |
| 内存检查 | 部分 | 【实测】build-asan 里只编了 S31 的两个单元测试：`100% tests passed out of 2`；全量交给 CI |
| 新测试和反向对照 | 通过 | 【实测】新增 `unit-s31-hal-map`（I2C 错误码映射、串口分行）；撤掉 NACK 映射、空行跳过、行结束清零、溢出保护任一处都失败，恢复后通过 |
| GCC | 通过 | 【实测】宿主机测试文件 gcccheck 0 处；端口的 `.c` 用 S31 的 RISC-V GCC（`-Wextra`）编，0 警告；gccscan、rscan 无输出 |
| 能力表 | 通过 | 没有新的用户操作，`PASS: 439 definitions` |

产品代码：新文件 410 行（`hal_i2c_s31.c` 82、`hal_uart_s31.c` 60、`s31_irq.c` 75、`s31_board.c` 113、`s31_board.h` 38、`s31_hal_map.h` 42），`hal_stubs.c` +4/−21，`main/main.c` +2，两个组件的 CMakeLists、README 少量。

## 做法

- **I2C**（`driver/i2c_master.h`）：HAL 总线 0 对应 I2C_NUM_1，SDA GPIO0、SCL GPIO1，100 kHz，glitch 7，内部上拉（和量过的 bench 一样）；总线第一次要设备时创建，设备表最多 16 个、同地址同句柄，有静态互斥锁；超时 50 ms（同模拟器）；错误码映射在宿主机可测的 `s31_hal_map.h`。
- **GPIO 中断**：`gpio_install_isr_service` + `gpio_isr_handler_add`，下降沿、上拉；ISR 只给 0 核上的 "irq" 任务（优先级 10、栈 4 KB）置位通知；处理完线还低就 20 ms 后再处理，逐次加倍到 1 秒（照模拟器 GPIO2 的复查）；登记时线已低立即处理一次。接口是端口内部的 `s31_irq_watch`（HAL 头里本来没有 GPIO 中断接口）。
- **UART**（`driver/uart.h`）：端口 1 = UART1 TX39/RX38 给 BH1098G；端口 2 = UART2 TX58/RX59 给 NT26；8N1、无流控、接收缓冲 2 KB；读一行丢 CR、遇 LF 结束、跳空行、超长保留开头、超时的半行留到下次接着拼。
- **板级接线**（`s31_board.c`）：`ink_s31_board_init()` 顺序同模拟器：扩展芯片 → 充电芯片设 1 A 并开充电 → 时钟、加速度计 → 前光 LEDC → 触摸 → 登记 GPIO2；每步失败记日志。实现了 `hal_power_battery`、`hal_rtc_*`、`hal_accel_read`、`hal_accel_motion_count`、`hal_light_set`（照模拟器里对应的驱动调用）。GPIO2 处理：先读加速度计运动锁存并计数，再读扩展芯片口 1、口 0 放开中断脚。`main.c` 在自检之后、检查屏幕之前调用。
- 芯片地址用 `drivers/chips/*` 的宏，扩展芯片引脚位用 `boards/metalio_plus/metalio_board.h`；GPIO 编号只有设备用得到，放在 `s31_board.h`，只此一份并注明出处。

## HAL 状态

| 函数 | 状态 | 上机要确认 |
| --- | --- | --- |
| `hal_i2c_*` | 已实现 | 内部上拉和板上外部上拉是否冲突；50 ms 够不够；扩展芯片偶发超时后总线能否恢复 |
| `hal_uart_*` | 已实现（端口 1、2） | 端口 2 的 TX58/RX59 和“控制台 UART0 在 GPIO59/58”的记录冲突；BH1098G 行尾 |
| GPIO 中断（`s31_irq_watch`） | 已实现，登记 GPIO2 | 下降沿和上拉；开漏线与的复查间隔和耗电；GPIO5（触摸）还没登记 |
| `hal_power_battery` | 已实现 | 读数和出厂日志对得上（960 mA、VREG） |
| `hal_rtc_*` | 已实现（PCF8563） | P16 开机一直低（hardware-parity 第 13 项） |
| `hal_accel_*` | 已实现（SC7A20H，地址 0x19） | 备用地址 0x18、坐标轴方向 |
| `hal_light_set` | 已实现（LEDC 20 kHz、10 位） | 极性按高电平点亮写的，是**假设**；LEDC 时钟源能否出 20 kHz/10 位 |
| `hal_input_*`、`hal_display_*`、`hal_power_*`（睡眠、关机等）、`hal_cell_*`/`hal_modem_*`、`hal_bt_*`/`hal_audio_*`/`hal_opus_*`、`hal_camera_*`/`hal_jpeg_*`、`hal_fs_*`、`hal_usb_*`、`hal_ota_*`、`hal_net_*`/`hal_ws_*`/`hal_wifi_*`、`hal_provision_*`、`hal_transfer_*`、`hal_asset_*` | 仍是空实现 | NT26 上游走 UART2 2 Mbps 的“UART 网卡”帧协议加 MRDY60/SRDY3，不是逐行 AT，要另写帧层；I2S 从机 BCLK42、WS43、DOUT44、DIN45 |

引脚和参数出处见 `ports/esp32s31/s31_board.h` 的注释（`docs/research/simulator/hardware-parity.md`、`docs/research/foundation/upstream-firmware.md`、`tools/hw-bench/main/bench.c`、`docs/research/eink4-plus-cellular.md`、模拟器对应文件）。

## 屏幕输出层（D-124：自己写，没看也没抄 EPDiy 源码）

已知：面板 E0470A14，1216×684，8 位总线，像素时钟 20 MHz；屏电源 TPS65185 经扩展芯片控制，驱动已有；刷新内核 `drivers/display/pqbn/pqbn_v4.h` 每相先 `pqbn_v4_phase()`，再逐行 `pqbn_v4_exec_row()`，每像素 2 位，扫描节奏稳定在 12 ms。

| 步骤 | 一小时任务数 |
| --- | --- |
| 引脚和时序表（数据线、源驱动时钟/锁存/起始、栅极时钟/起始/模式，只抄上游公开描述里的引脚号） | 1 |
| 用 LCD_CAM 的 i80/并口模式送行（每行 304 字节，8 位、20 MHz），先用逻辑分析仪核一行波形 | 2 |
| 栅极驱动行时序（GPIO、RMT 或第二路外设） | 1 |
| DMA 和行缓冲（GDMA 链表、内部 RAM 双缓冲；每帧 684 行在 12 ms 内送完，每行约 17.5 µs，304 字节约 15 µs，余量很小，要先确认） | 3 |
| 扫描任务和节奏（每 12 ms 一帧、规划超时先扫空行、记来不及送的行） | 2 |
| 屏电源和 VCOM 上下电顺序、断电保护先在实机上触发一次证明管用 | 2 |
| `hal_display_*` 外壳 | 1 |
| 上机标定（对照 12 ms 推力、和面板表对齐） | 2（上机） |

合计约 14 个一小时任务，其中 4～5 个要上机；最大风险是 LCD 外设在 8 位、20 MHz 下行间能否不停顿切换控制信号。

## 上机先核对

1. UART 引脚冲突：控制台 UART0 是否在 GPIO59/58（`docs/hardware/README.md`、phase0 记录），NT26 在 TX58/RX59；如冲突，控制台改只走 USB。
2. I2C 波形，扫出 9 颗芯片。3. GPIO2 空闲高、按键有下降沿、读完两个口后回高。4. 前光极性：先设 0 再小占空比往上。5. 充电 1 A 读回 960 mA。6. PCF8563 和 P16。7. 触摸芯片号 0x64、固件 0x03。8. 写设备前要整片备份和用户同意；这批代码烧进去后开机会写扩展芯片和充电芯片（内容和上游一样）。

## 没做到的

`hal_input`（按键和触摸分发、去抖、10 ms 采样、盖板虚拟键、GPIO5）；NT26 的帧层和 MRDY/SRDY；摄像头 DVP、屏电源 PMIC 没接进板级初始化；前光和刷新、浅睡的联动；全量内存检查交给 CI。

## 顺手发现

- 根 `CMakeLists.txt` 里原来的 `test-s31-osal-ticks` 定义在设置 sanitizer 和警告参数之前，所以它在 build-asan 里没开 ASan、也没加 `INK_WARN`；新测试放在那段之后，旧的没动。
- `integration-media` 在 `-j3` 负载下失败过一次（3 条 CHECK），单独重跑通过。

## 要改的文档

`docs/system/ui-dependencies.md`、backlog 的 S31 端口行；`hardware-parity.md` 或 `docs/hardware/README.md` 记下 UART 引脚冲突待查；端口 README 已改。
