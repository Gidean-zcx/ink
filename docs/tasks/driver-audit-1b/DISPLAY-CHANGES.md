# 屏幕输出层：690a023 → 874538e

查阅：2026-11-11。提交全名、证据边界及 `U`／`V`／`IDF` 路径约定见 [REPORT.md](REPORT.md)。以下为【实现】；“解决的问题”由差分及代码注释推导，不表示本项目已做实机回归。

## 传输层全部差分

【实测】`git diff --numstat 690a023 874538e -- main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.c`：新增 148、删除 30 行；`.h` 新增 7、删除 1 行。下面逐一覆盖 `.c` 的 22 个 `diff -u` hunk（固定上下文 3 行），以及 `.h` 的 2 个 hunk。同路径 `output_common/line_queue.c/.h` **逐字节相同**；不是新队列版本。

| hunk | 旧文件起始行 → 新文件起始行 | 改动及解决的问题 |
| --- | --- | --- |
| 1 | 70→70 | S31 引入 `esp_ipc.h` 和 AXI DMA LL，供 IRQ 归核、屏蔽／清状态使用 |
| 2 | 107→111 | 注释明确 bounce 是 8 行供数，末块超出原生高度填无驱动数据，取消旧的“688 恰好可整除”假设 |
| 3 | 147→151 | 区分取整微秒延时与精确 `ckv_period_ticks`，防止延时取整反向改变硬件行周期 |
| 4 | 175→180 | 记录 SPV 下降沿时间，便于量实际相频而非仅量任务开始时间 |
| 5 | 185→191 | 保存 DMA 组内通道／寄存器和 IRQ 核；dummy 注释改为 S31 数据阶段无占位 |
| 6 | 203→210 | 新增原生扫描条件（S31、8 位、dummy_clocks_after_data>0）及实际行数 getter，供上层按 684 行供数 |
| 7 | 217→234 | 新增直接屏蔽 EOF／清 pending helper；完成路径先屏蔽 EOF，再判重入，避免结束通知后旧 EOF 继续消费队列 |
| 8 | 230→261 | 完成函数增加是否通知参数；正常结束才回调。abort 先屏蔽两路 IRQ，再 IPC 到 IRQ 核停止／reset／清 pending，返回即形成旧 ISR 退出边界，避免复用缓冲时仍被补数 |
| 9 | 248→295 | 增加 `epd_lcd_get_spv_start_us`，只在停止扫描后读 |
| 10 | 309→360 | RMT 优先使用精确 tick 周期；非原生分支保留旧微秒换算，避免 CKV 与 LCD 每行渐离 |
| 11 | 450→502 | LCD 正常扫描完调用 finish(true)，与 abort(false) 分开，避免任务中止误走完成 ISR 回调 |
| 12 | 508→560 | 增加在 IRQ 核注册 DMA callback 的 IPC 函数，断言核号，避免 LCD/GDMA 并行补数与完成 |
| 13 | 566→629 | 查询 AXI DMA group/channel，定位寄存器，在 LCD IRQ 核注册 GDMA，并初始屏蔽 EOF；其它芯片仍用原注册路径 |
| 14 | 712→787 | 原生扫描高度从补齐 688 改为实际 684；S31 dummy_bytes=0，去掉每行 1 字节占位；精确 tick 初值归零，避免首尾像素被截／错位与多扫行 |
| 15 | 845→922 | 从已分配 LCD 中断读取 `irq_core`，供 GDMA 注册和 abort 使用；不是把中断硬编码到核 0／1 |
| 16 | 1044→1124 | 每次重设像素时钟先清精确 tick，避免沿用旧时钟计算结果 |
| 17 | 1052→1133 | 原生扫描按 RMT 10 MHz tick 和真实 PCLK 共同定周期，保留至少 1 µs CKV 低电平及面板 post-data clocks；日志改显示 0.1 µs／clock，EOF 理论间隔改按 cycle 算 |
| 18 | 1123→1225 | 相／EOF 统计由整数微秒改按 `line_cycles/pixel_clock`，避免诊断理论值自己取整出错；仅日志统计改变 |
| 19 | 1156→1260 | 每相重置前先屏蔽 EOF，reset 后显式清 AXI pending，再发布新相状态；修复 DMA reset 不清中断造成旧 EOF 污染新相 |
| 20 | 1182→1293 | 描述符、bounce 就绪后才重开 EOF，再启动 DMA，缩小新旧相状态混用的窗口 |
| 21 | 1206→1320 | SPV 拉低之前记录软件时间戳，用于实测 gate 起始周期 |
| 22 | 1213→1328 | S31 在 SPV 拉高后不再多等一行，让 XCL/XLE/XSTL 提前同步；其它芯片保留旧延时 |
| 头文件 1 | 9→9 | 声明 SPV 时间戳 API，注明扫描停止后读取 |
| 头文件 2 | 52→55 | 声明实际扫描行数 API；abort 合同明确为任务上下文、等待旧中断结束、不通知完成、返回后才改回调／复用缓冲 |

没有新增的变化也须分清：内部 DMA bounce／描述符、EOF 奇偶计数选择 bounce、owner 交还 DMA、末尾补零后等 VSYNC 完成、LCD 中断的 IRAM 标志在旧版已有；不能重复列为本次上游修复。

## 同范围板级、高层变化

`epdiy.c` 两版相同。板级 `epd_board_metalio_eink4_plus.c` 的差分覆盖：

- `U:display/epd_board_metalio_eink4_plus.c:187–244`：各功能 bool 拼装改成两口 desired/confirmed/valid；引入共享输出锁，输出失败不当成已确认，I2C port 变量改名以区别新驱动。
- `255–318`：马达／功放走统一更新；蓝牙 P14 复位期间压制普通功放／马达请求，马达放电 500 ms，断电累计 1000 ms，不跨等待持输出锁，逐步错误向上传递，结束保留其它输出位。
- `331–408`：P05 断电、恢复和 TPS 入睡均返回错误；恢复等待 80 ms；只有 confirmed 与 desired 都是睡眠态才省掉切换。
- `523–593`：睡前 mask、唤醒输入与恢复键方向的写入逐步检查；武装电源键输入后等 10 ms、读 P1 清方向切换留下的 INT 并检查电源键已松开。
- `596–629`：关机脉冲、P12 复位改走统一输出，复位只有两次写都成功才记成功。
- `652–739`：地址探测后的两口输出由统一状态管理并强制首次写；启动取掉重复配置分支，P14 稳定等待用命名 80 ms；扫描／清 INT 使用统一 port。
- `741–781`：deinit 时在锁内关闭马达、切输入并使输出确认无效，再按统一 port 检查 PG、清输入。
- `785–1001`：板控与 TPS 位同一锁下更新并返回错误；上电每一步传递错误，失败重试不再盲走；poweroff 先令 tps_ok=false，再按 12 µs／一 tick 顺序关电。电压、UPSEQ、VCOM 主参数未在这次差分中改变。
- 对应 `.h:13–72`：去掉“legacy I2C driver”措辞，功放、蓝牙复位、TPS 入睡、mask／恢复及电源轨 API 改为有返回值，补充睡前／恢复方向说明。

`highlevel.c` 的四个差分：`125–129` 全屏更新不带 dirty 掩码；`206–215` 均黑模式扩大到全屏；`230` 均黑即使 diff 空也不提前返回；`246–250` 均黑不受 dirty 行／列遮罩。解决黑色均衡被“像素没变化”和区域掩码跳过的问题。`epdiy.h:117–120` 增加对应 `MODE_EQUALIZE_BLACK`。

## display 目录其余变化清单

【实测】`git diff --stat 690a023 874538e -- main/boards/metalio-e-ink4-plus/display` 共 **20 项**（二进制重命名按一项）、新增 1023、删除 540 行。为避免把“传输层”与未编入本项目的 EPDiy 算法混为一谈，下面补齐全部条目；行号为新版。

| 文件（相对 U:display） | 新版出处 | 改动／修的问题 | 当前本项目是否直接编入 |
| --- | --- | --- | --- |
| `output_lcd/lcd_driver.c` | 上表 22 hunk | IRQ／EOF／abort／原生扫描／计时 | 用旧版 |
| `output_lcd/lcd_driver.h` | 上表 2 hunk | API 与取消合同 | 用旧版 |
| `epd_board_metalio_eink4_plus.c` | 上节全部范围 | 输出并发、失败确认、复位及睡前流程 | 否，自写板级替代 |
| `epd_board_metalio_eink4_plus.h` | 13–72 | 上述 API 错误返回与注释 | 否 |
| `epdiy.h` | 117–120 | 黑色均衡标志及同色处理说明 | 输出层包含头，不等于编入高层算法 |
| `epdiy_riscv_ve_stub.c` | 83–123,149–202 | S31 LUT 从 padded 单值改四段预移位，减少热循环移位；自检适配新表格式 | 否 |
| `firmware/README.md` | 1–71 | V03→V04；面板限制、哈希、启动独占、校验／恢复／重试说明 | 否 |
| `firmware/ft6336u_v03_app.bin`→`ft6336u_v04_app.bin` | README:3–9 | 47176 字节换供应商 V04；不能从二进制差分推断具体硬件修复 | 否 |
| `ft6336u_upgrade_protocol.h` | 9–134 | 新镜像校验值、应用轮询及无效应用 Boot 恢复、强制第二轮，防止半写入被版本号误认为成功 | 否 |
| `metalio_touch_upgrade.cc` | 12–53 | 嵌入 V04、升级前复位、读诊断、Delay 至少一 tick、报告 attempts | 否 |
| `metalio_touch_upgrade.h` | 1–9 | 更新 V04 与 Bootloader 恢复的接口说明 | 否 |
| `highlevel.c` | 125–129,206–250 | 全屏掩码和均黑更新 | 否 |
| `output_common/lut.c` | 157–174,353–376,461–594 | 预移位 LUT；DU 保黑 helper；GL16 黑目标去推白并末段补黑；新增整页均黑 LUT，避免旧字洗白／遮挡范围黑度不一致 | 否 |
| `output_common/render_context.c` | 88–97 | LCD 不再在准备下一相时清扫描计数／行就绪标记，允许扫描尾部与下一相供数重叠 | 否 |
| `output_common/render_context.h` | 29–49,81–85 | 区分供数相与扫描相；新增队列空间／预填信号、等待标记、行相位序号及可选掩码标记 | 否 |
| `output_common/spatial_wipe.h` | 7–143 | band LUT 展开优化；零驱动／仅旧灰阶／仅目标灰阶可走双像素表，混合路径保留；提前读 PSRAM，降低送行耗时 | 否 |
| `output_lcd/render_lcd.c` | 27–74,97–310,403–500 | 队列满阻塞并由 EOF 唤醒，取消时唤醒等待者；主更新任务等待两路预填后开扫；尾扫时准备下一相；85 Hz 绝对时间轴；按扫描相报告欠载，统一取消等待两路退出后再复用，记录 SPV 频率 | 否 |
| `render.c` | 135–142,252–267,377–412,464–485 | 按驱动实际高度分配；新信号量创建／销毁；仅有 dirty_columns 时施加 mask；128 项队列大小不变 | 否 |
| `waveforms/my_waveform_trim.c` | 89–122 | 去掉常驻 16×16 序列表，两遍重新计算后输出，省内部 BSS；不是换了波形电压／裁切参数 | 否 |
| `waveforms/my_waveform_trim.h` | 25–30 | 明确源和目标不可重叠，配合两遍计算避免输入被覆盖 | 否 |

`metalio_touch.c`、`output_common/white_border.h` 也未在本轮改动。现有白边 helper 为 always_inline，默认白边像素为 0；新版没有替本项目打开或重写白边处理。

## hal_display_s31.c 升级到新版的具体适配

以下仅是后续实施清单，本任务未改任何代码。

1. `display_upstream.cmake:2,12–23` 更新固定提交和校验机制。当前 `INK_UPSTREAM` 要求 HEAD 等于固定提交；**不得把公用旧检出的 HEAD 移到新版**。用独立固定版本源码目录／检出，或按提交导出所需原文及传递头依赖，保留 MIT 许可证及来源提交。
2. `hal_display_s31.c:67–71,201–204`：任务侧先 `epd_lcd_abort_frame()` 等旧 ISR 结束，返回后才能清回调／reset 队列；清理可能遗留的 frame_done 通知。新版 abort 不产生完成通知，不能靠它唤醒等待正常完成的消费者。`fail:85–90` 仍须等两路生产者退出后才 reset；IPC 只隔离消费者，不能替代生产者确认。
3. `hal_display_s31.c:100–107` panic：保持仅 IRAM／寄存器／原有 best-effort 切电路径，**不能把新任务上下文的 abort/IPC 放进 panic**。guard:109–114 是任务，可以走新版同步 abort；它与 display 的重复中止应共用原厂停止边界，后续验证自然超时即可，不造“人为卡线程”的测试机制。
4. `init:335–339` 参数已满足原生扫描条件，不需另猜 CKV 参数。新版实际 684 行；`line_source:149–150` 旧的 H 后填零仍无害；可用 `epd_lcd_get_display_lines()` 核对，不能再在调用方硬补到 688 行。每行仍是 304 个驱动字节，dummy 已由新版消除，HAL 不要再自行补位。
5. `scan:184–207` 仍以首相预填／软件时间起点计算 12 ms；原厂新 `render_lcd.c:97–100,215–217` 则按 85 Hz 绝对时间轴、并用 SPV 下降沿量实际相频。拷传输层不会自动迁入它。PQ-BN 的计划、供数和相间空白有自己的契约，须保留算法接口，并核对物理 SPV 间隔、首尾有效驱动及灰阶，不能生搬原厂 85 Hz 或笼统声称换版等效。
6. `sdkconfig.defaults` 要带上原厂 `CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y`。当前原厂和本项目都没有启用 LCD_RGB_ISR_IRAM_SAFE；若后续决定启用，先修队列指针表及一切回调可达数据的内部内存放置，并检查整个调用链，不能仅加一个标记。

【估算】在 PCLK=20 MHz、RMT=10 MHz、宽1216／高684、porch=4、LE=9、tail=10 的既有参数下：数据窗口 317 clocks，CKV 高159 ticks=15.9 µs、低10 ticks=1.0 µs、周期338 clocks=16.9 µs；旧版为17 µs／688行。这里是源码整数算式复算，非示波器读数；启动与尾扫额外时序不能忽略，不能把 `684×16.9 µs` 当作整相墙钟。

## IRAM 可达性核查

| 路径 | 证据与边界 |
| --- | --- |
| LCD ISR→LL／finish→gdma_stop | ISR、finish 显式 IRAM；LCD LL 内联、延时是 ROM；`gdma_stop` 及其 HAL 需 `GDMA_CTRL_FUNC_IN_IRAM`。原厂 sdkconfig 开启，本项目 defaults 漏掉，这是实际配置差异 |
| LCD ISR→start_ckv_cycles | `U:lcd_driver.c:340` 普通 static，未显式 IRAM；可能被优化内联，不能只读源断定最终位置。当前 684 行不会走 >1000 行续批分支，最终 map 仍应检查 |
| finish→esp_timer_get_time | `IDF:components/esp_timer/src/esp_timer_impl_systimer.c:68–75` 指向带 ESP_TIMER_IRAM_ATTR 的实现；不是任意主机计时函数 |
| GDMA EOF→fill_bounce_buffer→white_border／esp_cache_msync | EOF／fill 显式 IRAM；白边 always_inline；`IDF:components/esp_mm/linker.lf:5–7` 将整个 esp_cache_msync 对象放 noflash。不能因 C 定义没写 IRAM_ATTR 就误报它必在 Flash |
| fill→本项目 line_source→lq_read | 回调／队列读代码显式 IRAM；行缓冲、bounce、DMA 描述符显式 INTERNAL（DMA 缓冲再带 DMA）；但 `queue.bufs=calloc(...)` 会优先 PSRAM，需单独核查。原厂两版都有这个盲点 |
| finish→本项目 frame_done | 回调显式 IRAM，调用 FreeRTOS FromISR 与 yield；最终 RTOS 函数、信号量存储及所有 LL/ROM 调用仍以 ELF/map 确认，本审计没有编译，未给“整个链条已验证”结论 |

结论：**不能回答“全都在 IRAM”**；源码入口属性、ISR 是否注册 cache-safe、数据是否在内部内存、依赖链接放置是四件独立的事。此次已知“屏幕空闲、核1 Cache error”的材料不足以归因于该屏幕调用链，不能冒称找到那次崩溃根因。
