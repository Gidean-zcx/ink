# S2：主机模拟器的悲观性能建模（ESP32-S31 / E-Ink4 Plus）

查阅日期 2026-11-01～02。标注说明：
- 【官】官方文档或数据手册；
- 【码】本次读取的源码与配置（没有编译，也没有上机）；
- 【测】第三方实测；
- 【推】推算或推断；
- 【估】经验估计；
- **未核实**：没有拿到一手来源。

"上游"指 CloudZao/Metalio-E-INK4-Plus 的 `690a023` 提交。IDF 源码取自 master `d445e94`（2026-10-28），v6.1 文档取自 `v6.1` 标签。

## 0 结论

1. **ESP-IDF linux 目标和 FreeRTOS POSIX 端口只能做功能测试，不能当时间模型。**
   - POSIX 端口的 tick 由一个专用线程产生：先 `usleep(portTICK_RATE_MICROSECONDS)`，再向当前任务所在线程发 `pthread_kill(SIGALRM)`，所以跟随主机墙钟【码 port.c】。
   - IDF 文档写明调度是"cooperative preemption"：只有任务调用内核 API 时才会切换，忙等永远不会被抢占；时间粒度等于一个 tick；即使用 SMP 配置也按单核运行【官 host-apps.rst】。
   - linux 上的 `heap_caps_malloc` 直接调用 `malloc`，源码注释是 "No distinction between external vs internal memory on linux"【码 heap_caps_linux.c】。
2. **推荐做法：OSAL 的主机后端自己写一个确定性离散事件内核。**
   - 代码在虚拟时间里执行耗时为零，与 Zephyr native_sim 相同；耗时只在 HAL/OSAL 调用处显式注入。
   - 两个核各维护一个虚拟时钟。
   - 外设做成定时事件（见 §2）。
3. **悲观模型必须覆盖的几项，也是最容易被高估的地方：**
   - **Flash 擦写冻结整机。** 一次 4 KB 扇区擦除 45 ms（典型）～300 ms（最大）；64 KB 块擦除 0.25～1.6 s；IDF 默认的 8 KB 写块 16～77 ms。上游既没有开 auto-suspend，也没有开 XIP-from-PSRAM。
   - **墨水屏每相至少 12 ms**，这是代码强制的下限；源码注释里还记录过行时间被拉长 3 倍的异常。刷新期间，两个核上各有一个最高优先级的 feeder 任务在运行。
   - **TPS65185 每次上电，仅代码里的固定延时就超过 100 ms**；而且上游默认每次刷新完都会断电。
   - **`CONFIG_FREERTOS_HZ=100`**：所有 delay 和超时都按 10 ms 量化。
   - **4G 带宽受 UART 限制。** 2 Mbaud 的 UART 网卡把 4G 限制在 1.6 Mbps 以内；每个 1.5 KB 的帧单向还要多花约 7.6 ms。
   - **I-cache 未命中代价高。** Flash 按 DIO 80 MHz 运行，一次 I-cache 未命中约 3.5 µs。
4. **校准策略。** 校准之前一律取"耗时取上界、资源取下界"。硬件到货后的第一周跑完 §5 的基准，把结果写入带出处的参数文件，再用单边验收标准把关（只许悲观，不许乐观）。

## 1 虚拟时间方案对比（事实）

| 方案 | 时间怎么推进 | 多核与抢占 | 对本项目 |
|---|---|---|---|
| FreeRTOS POSIX 端口 | 墙钟 tick（usleep 加 SIGALRM）。每个任务一个 pthread，非运行任务阻塞等待；标准库内部的锁可能引起死锁【码】 | 单核 | 不确定、不可复现 |
| ESP-IDF linux 目标 | 同上，粒度为 1 tick；临界区用 mutex 实现【官】 | 协作式，单核【官】 | 只用于组件级功能测试 |
| Zephyr native_sim | 模拟 CPU 的时钟"infinitely fast"，CPU 运行时不流逝时间；只有硬件模型的事件推进时间。默认与墙钟解耦，`--rt`/`--rt-ratio` 可锁定到实时；正常运行结果确定【官】 | 单 CPU。没有让出 CPU 的忙等会变成死循环，必须改用 `k_busy_wait`【官 arch_soc.rst】 | **时间模型的直接样板** |
| native_simulator | 硬件事件以 µs 为单位；每处理完一个事件就推进到下一个最早的事件；同一时刻的多个事件按优先级分成多个 delta cycle 依次执行；为了确定性，假设每个 MCU 是单线程处理器【官 Design.md】 | 单核 | 事件调度器的样板 |
| Renode | 虚拟时间分辨率 1 ns。同一时间源下的 CPU 之间，虚拟时间差保证不超过一个 quantum。`PerformanceInMips` 默认 100，相当于恒定 CPI；`AdvanceImmediately` 让仿真尽可能快地运行【官】 | 多核按 quantum 同步 | 可借鉴其同步思路；固定 MIPS 的 CPU 模型不可取 |
| SystemC TLM-2.0 LT | 时间解耦：每个发起者维护一个本地时间偏移，超过 quantum 时 `need_sync` 为真，然后调用 `sync()` 与全局时间对齐【官 Embecosm】 | 任意 | 借鉴"抢占误差不超过一个 quantum"的界 |

## 2 推荐的时间与成本架构

**调度**
- 每个任务对应一个主机线程或协程，用接力（baton）方式保证同一时刻只有一个在执行，因此结果确定、可复现。
- 照搬 IDF FreeRTOS SMP 的规则：任务可以绑核；每个核运行最高优先级的就绪任务；同优先级的任务在 tick 时轮转。
- tick 固定为 10 ms，并复现 `pdMS_TO_TICKS` 的向下取整。例：上游在等待 PWR_GOOD 的循环里写了 `vTaskDelay(pdMS_TO_TICKS(1))`，HZ=100 时它等于 `vTaskDelay(0)`，实际上只是 yield【码】【推】。

**保证前进**
- 上游的 `pace_phase` 用 `vTaskDelay(0)` 加 `esp_timer_get_time()` 自旋凑满 12 ms【码】。在零耗时模型里，这种循环会像 native_sim 文档描述的那样卡死。
- 规定：每次调用 `osal_now()` 至少计 0.5 µs，每次 yield 计一次上下文切换的耗时。

**耗时注入**
- 用 `sim_cost(core, ns)` 注入耗时。OSAL 原语和 HAL 调用自带固定耗时；计算代码用工作量计数器换算耗时。
- 长计算按 ≤1 ms 的块分段注入，这样抢占误差不超过 1 ms【推，与 TLM quantum 同理】。

**双核**
- 每次推进虚拟时间最小的那个核；同一时刻按核号决定先后（保守 PDES）。
- mutex、队列、I2C 总线、PSRAM 带宽、MSPI 都建成"占用型资源"。

**全局冻结事件**
- Flash 擦写开始时，双核上所有非 IRAM 的任务和非 IRAM 的中断一起停住（真实硬件上另一个核在忙等），PSRAM 不可访问；I2S、LCD 等 DMA 照常消耗缓冲【官 spi_flash_concurrency.rst、external-ram.rst】。
- 上游 feeder 任务的栈在 PSRAM，所以 feeder 也会被冻结。这正是上游需要 `EpdFlashGuard` 的原因【码】。

**事件源**
- LCD 每一相的扫描、I2S 描述符耗尽、UART 字节流、I2C 事务、SD 块读写、网络包、触摸 INT。

**两种运行模式**
- **确定模式**：虚拟时间自由运行，用于 CI。网络使用模拟器内部的链路模型，参数可以照 netem 的延迟分布、丢包、限速来设计。
- **实时锁定模式**：类似 native_sim 的 `--rt`，用于人工体验和接入真实中枢；此时外部可以再串接 toxiproxy、tc netem 或 macOS Network Link Conditioner。这三个工具本次没有打开官方文档，**未核实**。

**CPU 成本模型**

以 (b) 为主，(c)(d) 用来校准，(a) 不采用：
- **(b) 工作量计数（主路线）。** 按操作统计工作量：各类内存之间拷贝的字节数、填充和混合的像素数、字形数、Opus 帧数、TLS 握手次数、JSON 字节数。再乘以校准得到的单价。
- **(c) ISS 计指令（热点内核）。** 把可移植的内核编译成不带 Xesp 扩展的 RV32IMAFC 代码，在通用 ISS 上统计指令数。耗时按"指令数 × CPI 1.3 + 未命中次数 × 未命中代价"计算。（2026-11-02 已实现为带流水线与 cache 时序的 ISS，见 [iss-calibration.md](iss-calibration.md)。）
- **(d) 开发板实测（校准）。** 在 S31 开发板或实机上实测，用来校准单价。
- **(a) 主机耗时 × 系数（不采用）。** 原因有三：
  - 主机是乱序超标量，带 MB 级缓存；目标是 5 级顺序流水线，代码靠 Flash XIP 执行，只有单精度 FPU【官，见 D 报告】。
  - IDF 文档指出，二进制布局造成的缓存未命中模式差异，会让小型微基准的计时大幅波动【官 speed.rst】。
  - 精度对比（**推断，没有找到公开数据**）：
    - (a)：不同内核之间可差 3～10 倍；
    - (b)：标定后，流式内核约 ±15%；
    - (c)：纯计算约 ±20%，内存受限时会更差；
    - (d)：同一芯片约 ±5%。

## 3 内存建模

**IDF 堆的事实**
- 每个连续内存区各是一个 multi_heap，底层用 TLSF（`tlsf_create_with_pool`）实现。
- `largest_free_block` 由 `tlsf_fit_size` 给出，等于当前能成功分配的最大尺寸。
- 每个堆记录 `minimum_free_bytes` 低水位【码 multi_heap.c；官 heap_debug.rst】。
- IDF 自带一个在主机上运行的 multi_heap 测试，用 `-m32` 编译【码 test_multi_heap_host】。

**主机上的做法**
- 把 IDF 的 multi_heap.c 和 tlsf 原样编译进模拟器，开三个相互独立的 arena：
  1. 内部 DMA 预留池，64 KiB（上游 `SPIRAM_MALLOC_RESERVE_INTERNAL=65536`）；
  2. 内部通用池；
  3. PSRAM 池。
- 分配路由照搬上游：小于 `ALWAYSINTERNAL=16` 字节的走内部，更大的优先走 PSRAM；mbedTLS 的内存走 PSRAM【码】。
- macOS arm64 不能用 `-m32`，块头会变大，需要在 arena 内改用 32 位偏移，或者单独记账【推】。
- 支持注入分配失败；按池报告低水位、最大空闲块和碎片率。

**栈**
- POSIX 端口在创建线程时自己分配栈，不使用创建任务时传入的栈【码，2020 年提交说明】，所以主机上测到的高水位没有意义。
- 改为用 RISC-V 交叉编译的 `-fstack-usage` 加调用图，算出每个任务的上界；实机上再用 `uxTaskGetStackHighWaterMark` 核对。

**到货前就能做的标定**
- 用 `idf.py --preview set-target esp32s31` 编出真实固件。`idf.py size` 会给出静态 DRAM/IRAM 的准确占用，据此倒推内部堆的预算。

## 4 参数表（悲观默认值）

| 参数 | 悲观默认 | 依据 | 标定方法 |
|---|---|---|---|
| cpu.coremark_core | 每核 1000（320 MHz） | 【官】双核 2195.20，即 6.86/MHz（D 报告引用数据手册）；按线性折成单核约 1098【推】，再打 9% 折扣 | 分别跑单核、双核 CoreMark |
| cpu.simd | XespV 只在一个核上，SIMD 加速只计入该核 | 【官】【码】D 报告 | 绑核前后对比 |
| cpu.fp64 | double 按软浮点计，×20 | 【官】只有单精度 FPU；倍数为【估】 | 微基准 |
| icache.miss_flash | 3.6 µs/次（约 1150 周期） | 【码】上游 Flash 为 DIO 80 MHz。【推】0xBB 读命令 8+12+4 clk，加 64 B 数据 256 clk，共 280 clk ≈ 3.5 µs。行长按 64 B 计，**未核实** | cache_cnt 计数器（S31 有 4 个计数单元【码 soc_caps】）加代码足迹扫描 |
| icache.miss_psram_xip | 0.4 µs/次（仅开启 XIP-PSRAM 时） | 【推】8 位 DDR 200 MHz，64 B 约 32 clk，加命令开销 | 开启后实测 |
| psram.cpy_p2i | 55 MB/s | 【测】S3 octal，PSRAM→IRAM memcpy 56.8 MB/s；P4 hex 200 MHz 为 105～186 MB/s | memcpy 矩阵 |
| psram.cpy_i2p | 32 MB/s | 【测】S3 32.5 MB/s；P4 85 MiB/s | 同上 |
| psram.cpy_p2p | 21 MB/s | 【测】S3 21.2 MB/s，另一来源也是 21 MB/s；P4 51 MiB/s | 同上 |
| psram.seq_read_miss | 120 MB/s | 【测】S3 octal 120 MHz 为 124.8 MB/s，约为理论值的 52%。S31 在 200 MHz DDR 下理论值 400 MB/s【推】 | 4 MB 顺序读 |
| psram.miss_latency | 300 ns/行 | 【推】64 B 在 200 MHz DDR 下约 32 clk，加命令和 latency。【测】S3 在 4 MB 范围随机读为 8.2 MB/s | 随机读测试 |
| psram.dma_contend | 有 DMA 时 CPU 访问 PSRAM 的带宽 −46% | 【测】S3：CPU 加一路 DMA 时，CPU 从 126 MB/s 降到 67 MB/s | LCD 扫描期间跑 memcpy |
| sram.cpy | 350 MB/s；PIE 1.8 GB/s | 【测】S3 366 MB/s，PIE 1830 MB/s；P4 352 MiB/s | memcpy 矩阵 |
| ram.int_app | 应用可用内部堆 96 KiB，其中最大块 ≤32 KiB；DMA 池最大块 ≤16 KiB | 【码】总量 512 KiB；EPDiy 固定占约 85 KiB；预留 64 KiB；上游告警线 50 KiB。具体数值为【估】 | `idf.py size`、启动日志、`heap_caps_get_info` |
| ram.psram | 可用 15.5 MiB：2026-11-03 实测为 16 MB，见[第 0 步](../../hardware/2026-11-03-phase0.md)；显示常驻约 3.45 MB | 【官】有 16/32 MB 芯片和 8 MB 模组；显示常驻量来自 A 报告【码】【推】 | `esp_psram_get_size()` |
| flash.tPP | 每 256 B 页 2.4 ms（典型 0.5 ms） | 【官】GD25B128E；板上实际的 Flash 型号**未核实** | 写入计时 |
| flash.tSE_4K | 300 ms（典型 45 ms） | 【官】GD25B128E；GD25LQ128E 典型 70 ms | 擦除计时 |
| flash.tBE_64K | 1.6 s（典型 0.25 s） | 【官】GD25B128E | 擦除计时 |
| flash.freeze_write | 每个写块冻结整机 77 ms（典型 16 ms） | 【官】`SPI_FLASH_WRITE_CHUNK_SIZE` 默认 8192，帮助文本说明分块是为了缩短关缓存的时间。冻结时间按 32 页 × tPP 推算【推】 | IRAM 定时器翻转 GPIO，同时 PSRAM 任务发心跳，测心跳间隙 |
| flash.freeze_erase | 每条擦除命令冻结整段 tSE 或 tBE；单条命令超过 20 ms 时，结束后让出 1 tick（10 ms） | 【官】Kconfig 中 YIELD_DURING_ERASE 默认开、20 ms、1 tick；HZ=100【码】 | 同上 |
| flash.suspend | 关（上游）。若打开：每次挂起约 40 µs，擦除总时长会被拉长。板上 PY25Q128HA 支持挂起（≤ 30 µs），但 IDF v6.1 的通用驱动不对它开启，见[第 0 步](../../hardware/2026-11-03-phase0.md) | 【码】`# CONFIG_SPI_FLASH_AUTO_SUSPEND is not set`。【官】IDF 文档称 tsus 典型约 40 µs；GD25B128E 数据手册 tSUS ≤20 µs | 打开后实测 |
| fs.littlefs_write | 20 ms/KB（含擦除和元数据提交） | 【测】IDF 示例输出：LittleFS 写入 0.066 MB/s，raw 写入 0.647 MB/s（未注明芯片）；esp_littlefs README 在 ESP32 上约 53～73 KB/s。【官】使用率 50% 时元数据更新平均为 2 倍开销，75% 时为 4 倍 | 跑 perf_benchmark |
| epd.line | 17 µs | 【码】Plus 公式：(304+9+4+10−1)/20 MHz 取整后 +1。EPDiy 原公式为 16 µs | 启动日志中的 `line width` |
| epd.lines | 688 | 【码】684 向上取整到 8 的倍数 | — |
| epd.phase | **16 ms**（下限 12 ms，理论 11.7 ms，压力档 33 ms） | 【码】`EPD_MIN_PHASE_US=12000`；每相开始扫描前要预填 192 行。注释记录过"EOF 仍是 384us"，按每 8 行算即 48 µs/行，688 行就是 33 ms【推】。该异常是否已修复**未核实** | 上游自带的 `EpdTiming phase cadence avg/min/max` 日志 |
| epd.GC16 / GL16 | 48 相 → 0.77 s（下限 0.58 s，压力档 1.6 s）；gray reveal 模式再加 9 相 | 【码】波形表头注明 48 相；见 epd_refresh.cc | 慢动作录像加日志 |
| epd.DU | 20 相 → 0.32 s（下限 0.24 s，压力档 0.66 s） | 【码】 | 同上 |
| epd.cpu | 波形播放期间，两个核各有 70% 被 epd_prep 占用（优先级 configMAX−1） | 【码】两个 feeder 分别绑在两个核上，上游注释"128 行约 2～3ms"；70% 为【估】 | FreeRTOS run-time stats（上游已开启） |
| epd.submit_copy | 每次提交约 20 ms | 【码】EpdSubmit 对整帧 406 KiB 做 memcpy；按 406 KiB ÷ 21 MB/s 推算【推】 | esp_timer 计时 |
| tps.on | 140 ms；失败时最多重试 3 次（间隔 100 ms），仍失败则 `esp_restart` | 【码】拉高 WAKEUP 后等 50 ms，拉高 PWRUP 后等 50 ms，PG 寄存器按 1 tick 轮询，另有约 15 次 I2C 访问。数据手册中的时序延时**未核实** | 上电前后打时间戳 |
| tps.off | 10 ms；`keep_power` 为假时每帧刷完都断电 | 【码】 | 同上 |
| i2c.byte | 90 µs | 【推】100 kHz 下每字节 9 bit | 逻辑分析仪 |
| i2c.touch | 每次读 1.2 ms（线上 9 字节约 0.85 ms，加驱动开销）；按住期间每 10 ms 读一次；要排队等其他器件的事务 | 【码】`ft_read(0x02, buf, 6)`；总线上共挂 6 类器件 | 同上 |
| sd | 读 4 MB/s，写 1 MB/s，写忙尖峰 250 ms | 【推】4 线 40 MHz 理论 20 MB/s；【码】S31 上走 4 KiB 内部中转缓冲；250 ms 为 SD 规范写超时，**未核实** | perf_benchmark。其 README 称 SDMMC 只支持 ESP32/S3，需改造后使用 |
| uart.link | 1.2 Mbps；1.5 KB 帧单向 7.6 ms | 【码】2 Mbps，自定义 4 B 帧头；【推】8N1 理论 1.6 Mbps | 经模组跑 iperf |
| lte | 带宽取 UART 上限与 0.3～1.2 Mbps 中的较小值；RTT p50 80 ms，p99 400 ms；空闲 10 s 后首包 +300 ms（RRC 恢复）；丢包 1%；每小时 1～2 次 2～5 s 断流 | 全部为【估】，**未核实** | 现场 1000 次 ping 加 iperf |
| wifi | RTT p50 8 ms，p99 60 ms；modem-sleep 时下行首包 +100～300 ms（DTIM 间隔）；应用层吞吐 5 Mbps | 【估】。IDF 默认省电模式是 MIN_MODEM，**未核实**；上游开了 PM【码】 | 省电开、关各测一次 ping |
| i2s.buf | `dma_buffer_size = dma_frame_num × slot_num × bit/8`，不超过 4092 B；中断间隔 = `dma_frame_num / fs` | 【官】v6.1 i2s.rst | — |
| audio.runway | 60 ms（例如 4 个描述符 × 240 帧，16 kHz） | 【估】按任务给定 | 生产者停顿注入测试 |
| audio.underrun | 生产者停顿时间超过 runway 即欠载。一次 4 KB 擦除（45～300 ms）几乎必然造成欠载 | 【推】 | 同上 |
| os.tick | 10 ms | 【码】`CONFIG_FREERTOS_HZ=100` | — |
| os.ctxsw | 上下文切换 10 µs，队列操作 5 µs | 【估】 | 任务间乒乓基准 |

**参考对照（不直接进模型）**
- ESP32-S3 RGB 800×480，帧缓冲在 PSRAM：LVGL 8 平均 12～16 FPS，LVGL 9.2 平均 9～11 FPS【测 esp_lvgl_port 文档的分叉副本】。
- S31 1024×600 跑 LVGL benchmark 43 FPS【第三方文章转述乐鑫数据，**未核实**】。
- 没有找到 LVGL 9.5 在任何芯片上的跑分。

## 5 墨水屏外观、标定与验收

**墨水屏外观**
- 直接使用设备端的波形表（my_waveform_gc、gl、du，分别为 48、48、20 相），逐相、逐像素执行 2 bit 动作码，编码以 EPDiy 源码为准。用饱和积分的反射率模型，就能复现 GC16 的反相闪烁【推】。
- 残影：GL16 和 DU 每次更新累加 `g += β·|ΔL|`，GC16 时清零，屏幕显示值为目标值加 g。β 和每相步长先用估计值，到货后拍摄实屏拟合【估】。
- EPDiy 文档写明刷新速度随环境温度变化【官 tips.rst】，模型需要留出温度参数。
- 没有检索到公开的残影模型；KOReader、Inkplate 的做法未查。

**到货第一周的基准**
- CoreMark：单核、双核各一次。
- memcpy 矩阵：内部/PSRAM 的源与目的两两组合，块大小 1 KB～1 MB，分冷、热缓存，并在 LCD 扫描期间再测一遍。
- PSRAM 随机读。
- I-cache 足迹扫描。
- Flash 擦写的冻结窗口。
- EPD 时序：上游的 `EpdTiming` 日志和 `scan_us`。
- `epd_poweron` 的时间戳。
- I2C 波形。
- `perf_benchmark`（SD、FAT、LittleFS）。
- ping：Wi-Fi 分省电开、关；4G 分空闲后和连续两种情况。
- I2S 停顿注入。
- 场景基准：Opus 解码每帧、整页渲染、TLS 握手。

**验收**
- 微基准误差在 ±10% 以内。
- 场景 KPI：p50 在 ±15% 以内，p95 在 ±25% 以内。
- **单边条件**：p95 延迟和峰值内存的预测值不得低于实测值的 0.9 倍。
- 达标之前叠加悲观系数：CPU ×1.3，I/O 尾延迟 ×1.5，内部 RAM 预算 −25%【推】。

**CI**
- 每次提交都跑确定性场景，记录虚拟时间 KPI、各池低水位、最大空闲块和欠载次数；任一项退化超过 5% 即判失败。
- 参数文件用 YAML，每一项写明来源、日期、固件哈希、板号和温度。
- 到货后每晚跑一次 HIL，对比预测值与实测值。

## 6 未能核实

- S31 本身的 PSRAM 带宽和延迟实测；I-cache 是 16 KB 还是 32 KB；I-cache 行长。
- Plus 板上 Flash 的具体型号及其是否支持 suspend。W25Q128JV 的 AC 参数表没能读到。
- TPS65185 数据手册里的上电时序延时（TI 页面被限流）。
- S31 上每相的真实时长，以及注释里那个 384 µs 异常的现状。
- S31 的 SDMMC 吞吐；LTE Cat.1 和 Wi-Fi 的 RTT、抖动分布；netem、toxiproxy、NLC 的文档。
- 公开的残影模型。
- 各成本模型精度的公开数据。
- Renode 的默认 quantum 值。

## 来源

**虚拟时间与调度**
- FreeRTOS POSIX 端口：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/ThirdParty/GCC/Posix/port.c
- POSIX 端口的引入提交：https://pigweed.googlesource.com/third_party/github/FreeRTOS/FreeRTOS-Kernel/+/90a35847498bb68ec1dbd0403c577ea1b7895d0f
- ESP-IDF host apps：https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/host-apps.rst
- Zephyr native_sim：https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html
- Zephyr POSIX arch：https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/native/doc/arch_soc.rst
- native_simulator：https://github.com/BabbleSim/native_simulator/blob/main/docs/Design.md
- Renode：https://renode.readthedocs.io/en/latest/advanced/time_framework.html
- TLM quantum keeper：https://www.embecosm.com/appnotes/ean1/html/ch09s01s03.html

**ESP-IDF 堆、Flash 与外设**
- `components/heap/heap_caps_linux.c`、`multi_heap.c`：https://github.com/espressif/esp-idf/tree/master/components/heap
- heap_debug.rst
- spi_flash 文档：https://github.com/espressif/esp-idf/tree/master/docs/en/api-reference/peripherals/spi_flash
  - spi_flash_concurrency.rst
  - auto_suspend.inc
- spi_flash Kconfig：https://github.com/espressif/esp-idf/blob/master/components/spi_flash/Kconfig
- external-ram.rst
- speed.rst
- `components/soc/esp32s31/include/soc/soc_caps.h`
- v6.1 I2S 文档：https://github.com/espressif/esp-idf/blob/v6.1/docs/en/api-reference/peripherals/i2s.rst
- v6.1 perf_benchmark：https://github.com/espressif/esp-idf/tree/v6.1/examples/storage/perf_benchmark

**PSRAM、CPU 与 LVGL 实测**
- S3 memcpy 基准：https://github.com/project-x51/esp32-s3-memorycopy
- S3 内存带宽与缓存：https://qiita.com/elect-gombe/items/5221c0490125c7628453
- S3 N16R8 测试：https://esp32-s3.mculab.workers.dev/index-en
- P4 PIE 讨论：https://github.com/espressif/developer-portal/discussions/353
- S31 与 S3 对比：https://viewedisplay.com/esp32-s31-vs-esp32-s3/
- esp_lvgl_port 性能文档（分叉副本）：https://github.com/Lzw655/esp-bsp/blob/master/components/esp_lvgl_port/docs/performance.md
- Waveshare LVGL 性能：https://files.waveshare.com/wiki/common/Performance.pdf

**Flash 与文件系统**
- GD25B128E 数据手册：https://gigadevice.com.cn/Public/Uploads/uploadfile/files/20240902/DS-00594-GD25B128E-Rev1.1.pdf
- GD25LQ128E 数据手册：https://download.gigadevice.com/Datasheet/DS-00533-GD25LQ128E-Rev1.3.pdf
- littlefs 设计文档：https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md
- esp_littlefs：https://github.com/joltwallet/esp_littlefs

**墨水屏驱动与上游固件**
- EPDiy：https://github.com/vroland/epdiy
  - `src/output_common/render_context.c`
  - `src/output_lcd/lcd_driver.c`
  - `src/output_i2s/render_i2s.c`
  - `doc/source/tips.rst`
- 上游固件 @690a023：https://github.com/CloudZao/Metalio-E-INK4-Plus
  - `output_lcd/lcd_driver.c`、`output_lcd/render_lcd.c`
  - `epd_board_metalio_eink4_plus.c`
  - `main/display/epd_refresh.cc`
  - `metalio_touch.c`
  - `waveforms/*`
  - `sdkconfig`
