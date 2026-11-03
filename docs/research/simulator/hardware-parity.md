# 模拟器与官方硬件的对照

2026-11-03 用户要求：模拟的各类硬件尽量与原始官方项目的硬件设计一致（R-043）。本文逐项对照官方板级代码和模拟器现状，作为补模型的清单。

- **官方依据**：`CloudZao/Metalio-E-INK4-Plus` 固定提交 `690a023`（2026-10-30），主要是板级引脚表 [config.h][cfg] 和 `main/boards/metalio-e-ink4-plus/` 下的驱动，2026-11-03 查阅。官方没有公开完整原理图，引脚和地址以代码为准；代码注释写明了 V1.2 与 V1.3/V1.4 的差异，本文按代码里实际启用的定义。
- **证据分级**：官方一栏都是【代码事实】，除非另注；模拟器一栏写的是当前实现，其中的时间和电流多为估计值，见各档案的 `_notes`。
- **结论**：一致 = 行为和参数都按官方；部分 = 行为对、细节缺；缺 = 没有模型。

## 对照表

| # | 部件 | 官方硬件 | 模拟器现状 | 结论 |
| --- | --- | --- | --- | --- |
| 1 | 主控与存储 | ESP32-S31 双核 320 MHz，16 MB Flash（Puya PY25Q128HA），16 MB PSRAM | 档案相同；第 0、1 步实测核对过 | 一致 |
| 2 | 墨水屏 | 原生 1216×684，逻辑竖屏 684×1216，8 位并行，VCOM 1200 mV（[L7][c7]、[L22][c22]） | 档案相同；刷新管线与波形按上游 EPDiy 端口 | 一致 |
| 3 | 屏电源 | TPS65185@0x68，经 TCA9555@0x20 的 P00 VCOM_CTRL、P01 PWRUP、P02 WAKEUP、P07 PWR_GOOD 控制（[L65][c65]） | 寄存器级芯片模型 + 本项目驱动（D-41 第一步） | 一致 |
| 4 | I2C1 总线 | GPIO0/1，100 kHz；挂 TCA9555、TPS65185、FT6336U@0x38、BQ27220@0x55、CX25601N@0x6B、PCF8563@0x51、SC7A20H@0x19（备用 0x18）、摄像头 SCCB@0x3C（[L82][c82]、[L85][c85]） | 共享总线的占用与计时；除摄像头外每颗芯片都有寄存器级模型，系统经本项目的驱动访问 | 一致（摄像头不做，见 20） |
| 5 | 触摸 | FT6336U，INT 接 GPIO5 下降沿，手指按住期间每 10 ms 读一次；RST 接 TCA P12（[L95][c95]、[touch.c#L34][t34]） | 寄存器级模型（芯片 ID 0x64、INT 模式、两个触点槽位）+ 驱动 `drivers/ft6336u`；P12 拉低时不应答 | 一致 |
| 6 | 盖板虚拟键 | HOME、返回/上页、下页是盖板上的触摸区：FT 报点 Y≈1600（屏外），X 实测 100/200/300，容差 30，长按 500 ms（[L104][c104]–[L109][c109]） | 触摸芯片按官方坐标报点，`drivers/board/metalio_touch` 按位置区分盖板键和手指；长按判定属于系统 | 一致 |
| 7 | 实体键 | BOOT=GPIO61（AI 对讲）；POWER=TCA P17，VOL+=P10，VOL−=P11；TCA 的 INT 接 GPIO2，与加速度计线与（[L84][c84]、[L112][c112]） | 按键改变 TCA 输入脚，INT 拉低 GPIO2，驱动读 P1；GPIO2 按线与建模 | 一致 |
| 8 | 关机 | POWER 长按 3 s 后，TCA P13 输出 15 次 100 ms 高低脉冲，电源芯片硬关机（[L116][c116]–[L119][c119]） | `hal_power_shutdown` 经板级驱动发脉冲，第 15 个脉冲后断电；窗口里按电源键重新开机 | 一致（电源芯片在第几个脉冲动作是假设） |
| 9 | 振动马达 | TCA P03，高有效，短震 50 ms（[L70][c70]、[L80][c80]） | 经 TCA 驱动开关 P03 | 一致 |
| 10 | 前光 | YX6016N，GPIO6 冷光、GPIO7 暖光，LEDC 20 kHz、10 bit；每帧刷完后才恢复前光（[L122][c122]） | 冷、暖两路千分比，按亮度计电流；窗口按冷暖色调显示 | 一致（行为级：片上 PWM，无总线访问） |
| 11 | 电量计 | BQ27220@0x55；界面电量按电压估算 | 寄存器级模型（电压、电流、电量、容量）+ 驱动；电压按官方实测的放电曲线，加内阻修正（估计） | 一致 |
| 12 | 充电 | CX25601N@0x6B，充电电流 0.5/1/1.5 A，默认 1 A | 寄存器级模型 + 驱动；开机按官方设 1 A（实际 960 mA）；插 USB 后恒流到 90 %，再降流到满（估计） | 一致 |
| 13 | 实时时钟 | PCF8563@0x51，开机从 RTC 恢复时间；V1.4 起 INT 接 TCA P16，低有效（[L87][c87]） | 寄存器级模型 + 驱动：走时、VL、闹钟；闹钟拉低 P16、唤醒芯片 | 一致 |
| 14 | 加速度计 | SC7A20H，INT1 与 TCA INT 线与到 GPIO2；阅读时每 200 ms 读重力 x/y，另有运动中断（[L90][c90]、[accel.cc][acc]） | 寄存器级模型 + 驱动（按官方初始化）；重力随设备朝向，晃动或转向触发 INT1 | 一致（芯片在板上的坐标轴方向是假设） |
| 15 | 音频 | BH1098G 蓝牙音频芯片；ESP 为 I2S 从机（BCLK42/WS43/DOUT44/DIN45），16 kHz 立体声 32 bit 槽，DMA 4×240；麦克风双声道（含回采）；UART1 115200 发 AT 切模式（TX39/RX38）；功放 NS4150 由 TCA P04 使能（[L146][c146]、[L153][c153]） | I2S 环形缓冲、启动时序、Opus 开销按上游参数；功放经 TCA P04；喇叭声音可在电脑上播放 | 部分：BH1098G 的 UART AT 未建模；麦克风不接电脑 |
| 16 | 共享电源与复位 | P14 同时给 4G、蓝牙芯片和功放供电；P12 是触摸与 4G 共享复位，4G 工作时须为高；P05 是摄像头/屏电源（[L97][c97]、[L100][c100]、[L72][c72]） | P12 拉低：模组和触摸芯片都进复位（模组在线时告警）；P14 断开：模组断电，功放开着时告警；开机电平按官方 | 一致 |
| 17 | 4G | NT26（开机日志显示芯片为 EC718PM），UART2 2 Mbps（2M/3M 自动探测）的“UART 网卡”，AT 与以太网帧复用；TX58/RX59/MRDY60/SRDY3（[L157][c157]–[L160][c160]）。两路卡：外置卡槽、内置卡，同一时间用一张，卡槽是模组里的设置，用 `AT+CFUN=0`、`AT+ECSIMCFG=SimSlot,n`、`AT+CFUN=1` 切换 | 链路模型：时延、带宽、UART 上限、空闲恢复、随机断网；模组的 AT 命令层：卡槽设置（重启后保留）、开关射频、注册、软复位，两个卡槽可插拔不同的卡 | 部分：AT 只做了切卡和注册用到的几条，短信电话未建模 |
| 18 | Wi-Fi | S31 内置 | 链路模型 | 一致（行为级） |
| 19 | TF 卡 | SDMMC slot0 4 线（CLK24/CMD25/D0–D3=GPIO20–23），FATFS（[L134][c134]） | 读写速度、延迟、写入忙尖峰；文件放在电脑目录 | 一致（行为级） |
| 20 | 摄像头 | GC2145，DVP，SCCB@0x3C（[L73][c73]） | 没有 | 本项目目前没有摄像头需求，不做 |
| 21 | 屏温度 | 不做温度补偿，固定按 25 °C | 同样不建模 | 一致 |

2026-11-03 第二轮（D-41 A 第二步）之后：21 项里 18 项一致，2 项部分一致，1 项不做。

## 还没做的

都不影响开始设计系统框架，等需求或模组能力确认后再补：

1. **BH1098G 的 UART AT**：开机切模式（`AT+RX=2`、`AT+MODE=1`）和耳机等模式。
2. **NT26 的短信、电话**：要等需求和模组能力确认（R-038、RK 记录）。
3. **麦克风接电脑**：现在麦克风听到的是合成的“说话、安静、噪声”。
4. **网络只给环境，不替系统选网**：现在窗口和脚本的 `net lte|wifi|none` 直接指定设备用哪条链路，混进了系统的选网决定（R-043 的补充说明：模拟器只模拟硬件）。改为只模拟环境（有没有 Wi-Fi、4G 覆盖与信号强弱），由系统决定用哪条；要等系统有了选网逻辑一起改。

已核对一致的部分，补模型时不改它们的行为。

[cfg]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h
[c7]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L7
[c22]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L22
[c65]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L65
[c70]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L70
[c72]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L72
[c73]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L73
[c80]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L80
[c82]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L82
[c84]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L84
[c85]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L85
[c87]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L87
[c90]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L90
[c95]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L95
[c97]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L97
[c100]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L100
[c104]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L104
[c109]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L109
[c112]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L112
[c116]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L116
[c119]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L119
[c122]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L122
[c134]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L134
[c146]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L146
[c153]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L153
[c157]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L157
[c160]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L160
[t34]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/metalio_touch.c#L34
[acc]: https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/periph/metalio_accel.cc
