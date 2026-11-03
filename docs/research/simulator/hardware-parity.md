# 模拟器与官方硬件的对照

2026-11-03 用户要求：模拟的各类硬件尽量与原始官方项目的硬件设计一致（R-043）。本文逐项对照官方板级代码和模拟器现状，作为补模型的清单。

- **官方依据**：`CloudZao/Metalio-E-INK4-Plus` 固定提交 `690a023`（2026-10-30），主要是板级引脚表 [config.h][cfg] 和 `main/boards/metalio-e-ink4-plus/` 下的驱动，2026-11-03 查阅。官方没有公开完整原理图，引脚和地址以代码为准；代码注释写明了 V1.2 与 V1.3/V1.4 的差异，本文按代码里实际启用的定义。
- **证据分级**：官方一栏都是【代码事实】，除非另注；模拟器一栏写的是当前实现，其中的时间和电流多为估计值，见各档案的 `_notes`。
- **结论**：分四栏判断。**接线**：芯片、地址、引脚、共享线是否按官方代码；**行为**：正常工作时的寄存器、命令、时序；**故障与供电**：总线出错、断电、复位、共享电源与中断线的耦合；**实机**：我们这台设备上核对过什么（第 0 步出厂固件日志、第 1 步测量固件实测），没核对的写“未”。四栏都按官方且没有关键假设才算**一致**；有估计或假设影响结论、只到行为级、或与实机记录不符的，算**部分**；**缺** = 没有模型。
- 2026-11-03 Codex 审计（AUD2-023）指出上一版“21 项都一致”与实现不符：当时按键与唤醒绕过寄存器、浅睡只改电流账、音频不依赖蓝牙芯片的时钟、摄像头配置不完整。这些在审计修复中已改（见[审计记录](../../audits/README.md)），本表按修复后的代码重写；上一版的结论只作历史，不再引用。第二轮复审又指出：第 3、7 项的故障路径有缺口（刷新途中屏电源掉了仍报成功；扩展芯片开机没设置好时按键绕过寄存器），第 19 项的“重新挂载”只是一段延时。前两项已在模拟器里补上并有自检（AUD3-003、AUD2-012、AUD3-002），第 19 项改了写法；本表是补完之后的判断。

## 对照表

| # | 部件 | 官方依据 | 模型与驱动 | 接线 | 行为 | 故障与供电 | 实机 | 结论 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 主控与存储 | ESP32-S31 双核 320 MHz，16 MB Flash，16 MB PSRAM | 档案；虚拟时间内核、内存池、Flash 停顿 | — | 按档案；CPU 成本经指令级模拟器校准 | Flash 操作冻结全部任务 | 第 0、1 步：芯片型号、送行时间 | 一致 |
| 2 | 墨水屏 | 1216×684，8 位并行，VCOM 1200 mV（[L7][c7]、[L22][c22]） | 刷新管线按上游 EPDiy 端口 | 按官方 | 相数按上游源码（GC16 36 / GL16 37）；帧时间等 3 项用模拟器默认值 | 欠载、残影、电源失败；刷新途中断电时，EPDiy 管线的模型在波形结束才把图交给屏，所以屏不变（真机会留下已驱动的几相），PQ-BN 逐相生效 | 第 0 步：出厂“balanced”档翻页 20 相，与模型不同 | 部分 |
| 3 | 屏电源 | TPS65185@0x68，经 TCA9555 P00–P02、P07（[L65][c65]） | 寄存器级模型 + 本项目驱动 | 按官方 | 上电顺序、等待按上游 | 无 PWR_GOOD、总线故障、重试后失败（场景 panel_power_dead）；刷新途中掉电：每次刷新结束（PQ-BN 在相与相之间）核对 PWR_GOOD 脚和 PG 寄存器，掉了就报失败，下一次刷新重新上电（接线自检） | 第 0 步：一次上电重试；VCOM 1200 mV | 一致 |
| 4 | I2C1 总线 | GPIO0/1，100 kHz，挂 9 颗芯片（[L82][c82]、[L85][c85]） | 共享总线计时 + 按地址分发 | 按官方 | 每颗芯片寄存器级模型（含摄像头 SCCB） | 每个器件可单独注入故障 | 第 0 步：一次扩展芯片超时 | 一致 |
| 5 | 触摸 | FT6336U，INT GPIO5，RST 接 P12（[L95][c95]、[touch.c#L34][t34]） | 寄存器级模型 + `drivers/chips/ft6336u` | 按官方 | 两个触点槽位、按住每 10 ms 读一次 | P12 拉低时不应答 | 第 0 步：chip 0x64、固件 0x03 | 一致 |
| 6 | 盖板虚拟键 | 触摸区 Y≈1600，X 100/200/300（[L104][c104]–[L109][c109]） | 触摸芯片报点 + `boards/metalio_plus/metalio_touch` | 按官方 | 按位置区分盖板键与手指 | 同触摸 | 未 | 一致 |
| 7 | 实体键 | POWER P17、VOL P10/P11，INT→GPIO2 与加速度计线与；BOOT GPIO61（[L84][c84]、[L112][c112]） | 扩展芯片寄存器模型；驱动读两口、确认读后去抖 | 按官方 | 按键只改引脚电平，事件来自寄存器 | 读失败不出事件；两口 INT；睡眠时只有 P17 能唤醒；开机没设置好时不出按键事件、中断里补做设置（`selftest-expander-dead`）；醒后恢复出错时后台重试；服务后 GPIO2 仍为低（加速度计锁存或某口读失败）时定时再看（接线自检） | 第 0 步：P1 空闲 0xB7 | 一致 |
| 8 | 关机 | P13 发 15 个 100 ms 脉冲（[L116][c116]–[L119][c119]） | 经板级驱动发脉冲 | 按官方 | 按官方 | 电源芯片在第几个脉冲动作是**假设** | 第 0 步：长按 3 s 关机 | 部分 |
| 9 | 振动马达 | P03，短震 50 ms（[L70][c70]、[L80][c80]） | 经扩展芯片开关 | 按官方 | 按官方 | 开着时拒绝浅睡；电流为估计 | 未 | 一致 |
| 10 | 前光 | GPIO6/7，LEDC 20 kHz（[L122][c122]） | 冷暖两路千分比 | 按官方 | **行为级**：无 PWM 外设模型 | 浅睡前关掉 | 未 | 部分 |
| 11 | 电量计 | BQ27220@0x55 | 寄存器级模型 + 驱动 | 按官方 | 电压按官方实测放电曲线；电量是本项目的**估计**（不是芯片的算法） | 内阻为估计 | 第 0 步：在线 | 部分 |
| 12 | 充电 | CX25601N@0x6B，默认 1 A | 寄存器级模型 + 驱动 | 按官方 | 设 1 A（实际 960 mA）按官方；充电曲线与复位值为**估计** | — | 第 0 步：VREG 4400 mV、960 mA | 部分 |
| 13 | 实时时钟 | PCF8563@0x51；V1.4 起 INT 接 P16，低有效（[L87][c87]） | 寄存器级模型 + 驱动；P16 跟随时钟中断 | 按 V1.4 | 走时、VL、闹钟按手册 | 闹钟只经 P16、扩展芯片、GPIO2 唤醒；P16 空闲不为高时不用它唤醒 | **不符**：出厂日志 13 次开机 P16 都为低，而时钟芯片报告没开中断、没有标志 | 部分 |
| 14 | 加速度计 | SC7A20H@0x19，INT1 与 TCA INT 线与（[L90][c90]、[acc][acc]） | 寄存器级模型 + 驱动（按官方初始化与静音） | 按官方 | 重力、运动中断 | 浅睡前静音、放开 GPIO2 | 第 0 步：在线 | 部分（芯片在板上的坐标轴方向是**假设**） |
| 15 | 音频 | BH1098G 时钟 I2S，UART1 AT；功放 P04（[L146][c146]、[L153][c153]） | I2S 环形缓冲；串口协议模型；`drivers/chips/bh1098g` | 按官方 | 没有模式或断电时 I2S 无时钟；P14 回来后端口重设模式；协议按官方代码（**无手册**） | 功放只在 P14 供电时开 | 未 | 部分 |
| 16 | 共享电源与复位 | P14：4G、蓝牙、功放；P12：触摸与 4G 复位；P05：摄像头/屏（[L97][c97]、[L100][c100]、[L72][c72]） | 扩展芯片模型的钩子 | 按官方 | 开机电平按官方 | 复位、断电对每个器件生效；违反接法告警 | 第 0 步：待机时断 P14 | 一致 |
| 17 | 4G | NT26（EC718PM），UART2 网卡，两路卡（[L157][c157]–[L160][c160]） | AT 命令层模型 + `drivers/chips/nt26` | 按官方 | 卡槽、注册、短信、电话；主动上报格式是**假设** | P12/P14 生效；通话声音未建模 | 第 0 步：固件版本、约 3.6 s 联网 | 部分 |
| 18 | Wi-Fi | S31 内置 | 链路模型 | — | **行为级** | 断网、恢复 | 未 | 部分 |
| 19 | TF 卡 | SDMMC 4 线（[L134][c134]） | 电脑目录 + 时延模型 | 按官方 | **行为级**：速度与写入尖峰为估计 | 浅睡醒来只加一段重新挂载的延时（档案 `sd_remount_us`），不模拟卡的状态变化或挂载失败 | 第 0 步：SDHC，20 MHz | 部分 |
| 20 | 摄像头 | GC2145，DVP，SCCB@0x3C（[L73][c73]） | SCCB 寄存器模型 + `drivers/chips/gc2145`（厂商 VGA 序列） | 按官方 | 设置不对就收不到帧；帧率 15 fps 是**估计**；DVP 时序为行为级 | P05、P12 掉电清掉设置；拍照中断电失败 | 未 | 部分 |
| 21 | 屏温度 | 不做补偿，固定 25 °C | 同样不建模 | — | 同官方 | — | — | 一致 |
| 22 | 浅睡与唤醒 | `power_hw_light_sleep_once`：停屏电源、静音加速度计、屏蔽扩展芯片、只留 P17、断 P05 再断 P14 | `hal_power_sleep` 经驱动按上游顺序 | 按官方 | 唤醒由 GPIO2 电平触发，醒后读 P1 判断原因 | 外设忙时拒绝；GPIO2 一直低时放弃 | 第 0 步：出厂待机断 P14、60 s 分片 | 部分（RTC 经 P16 唤醒、4G 经 SRDY 唤醒是自研系统的扩展，后者是**假设**；电流为估计） |

合计：一致 9 项（1、3、4、5、6、7、9、16、21），部分 13 项，缺 0 项。

## 与实机记录不符、待查

1. **P16 的电平**（第 13 项）：出厂固件 13 次开机都打印 `RTC_INT via TCA P14 ready (low=event, now=active)`（P14 是扩展芯片的线性编号，即 P16），按键初始化读到 P1 = 0xB7（P16 为低）；同一批日志里时钟芯片 Control_status_2 为 0x00 或 0x40，即没有开闹钟或定时中断、也没有标志。按 config.h 的 V1.4 接法，此时 P16 应为高。官方注释说 V1.4 之前 P16 接的是模拟开关（现改到 GPIO36），所以这台可能是更早的板子版本【推断】。要看板子丝印的版本号才能确定。在确定之前：模型按 V1.4 接法；系统浅睡时只有 P16 空闲为高才把它当唤醒输入，否则只靠定时唤醒（与上游一样，上游从不用 P16 唤醒）。
2. **翻页相数**（第 2 项）：出厂“balanced”档翻页 20 相，档案按上游源码是 37 相，要靠实机观察画质和残影（SP-01）。

## 还没做的，或只能靠假设的

1. **通话的声音**：4G 模组的语音怎么到喇叭和麦克风，现有资料说不清（[蜂窝调研](../eink4-plus-cellular.md)），先不模拟。
2. **EC718PM 的 AT 细节**：短信、电话的主动上报格式按 3GPP 通用写法；有数据时是否拉 SRDY 也是假设；要拿到模组的 AT 手册或实机日志再核对。
3. **蓝牙音频芯片**：协议来自官方代码，没有手册；耳机扫描、连接的状态行只记在驱动注释里，模型没有模拟耳机。
4. **电流**：各部件的电流都是估计，待 SP-10、SP-14 实测；电流数字只用来比较方案，不代表续航。

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
