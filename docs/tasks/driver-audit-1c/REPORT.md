# driver-audit-1c 审计报告

日期：2026-11-11。基线：`87bf48655ee4820837b0f54a43c8f5aed37278c2`。

状态：审计进行中。本报告仅记录源码对照、风险和建议；不修改产品代码、测试或构建文件，不操作设备。

证据约定：【实现】为固定提交的源码核查；【实测】仅指本次执行的只读命令及文档检查；【估算】为尚需实机确认的因果推断。任务书中的既有上机现象不算本次实测。

路径约定：`U:` 指原厂 `CloudZao/Metalio-E-INK4-Plus@874538e`，`I:` 指 ESP-IDF v6.1 的本机只读源码，未加前缀的路径指本项目基线。原厂完整提交为 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；本地克隆 HEAD 保持 `690a023c66862af103a3187464373ee7c5efcd14`。本机 IDF 为 `v6.1`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 进行中 | 逐模块对照见下表 | 只读源码与行号核查 |
| 2 | 进行中 | 前十风险、直接拷贝和上机清单待补 | 固定版本源码对照 |
| 3 | 进行中 | 仅新增本目录；最终复核待补 | `git diff --name-only 87bf4865..HEAD` |

## 来源核查

【实测】2026-11-11 通过 GitHub 只读 API 查询 [原厂 main](https://api.github.com/repos/CloudZao/Metalio-E-INK4-Plus/branches/main)，返回 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，提交说明 v2.0.77，提交时间 2026-11-07 11:32:56 UTC。公开 main 没有更新的提交；网页烧录有 2.0.78 不等于公开源码已更新。以 `git show 874538e:<路径>` 补齐部分导出缺失的文件，不移动原厂 HEAD。项目相关未结 Issue 检索 `repo:Gidean-zcx/ink is:open driver-audit` 返回空。

## 模块对照

下列出处和差异均为【实现】；潜在故障与严重度为【估算】，未声称本次在真机复现。`U:power/`、`U:power_policy/`、`U:sensor/`、`U:storage/`、`U:camera/`、`U:audio/` 分别省略前缀 `main/boards/common/`；`U:periph/` 省略 `main/boards/metalio-e-ink4-plus/`。

| 模块 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑 | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| C01 充电初始化与限流 | `U:power/cx25601n.cc:314-337,548-575`；`U:periph/metalio_power.cc:40-80`：VREG 4400 mV、预充 120 mA、终止 60 mA、禁 WDT/DPDM、私有寄存器、ICHG 联动 IINDPM；先 500 mA，再恢复保存值（默认 1000） | `drivers/chips/cx25601n/cx25601n.c:20-42,75-113`；`ports/esp32s31/s31_board.c:77-84`：相同参数和寄存器，直接 1000 mA；初始化失败只记日志，设备指针保留 | 我们重写；主要寄存器与正常结果等价，初始化状态处理不一样；不是原文复用 | 初始化中途 NACK 时，后续仍可能轮询/修改半初始化芯片，无法仅凭“有指针”判定充电策略已就绪；本次无故障实机证据 | 中 | 拷 `cx25601n.{h,cc}` 与 `power_i2c.{h,c}`，以 HAL I²C 作桥接；保留初始化成功状态，电流设置走现有服务接口 |
| C02 再充、欠压 BATFET 与关机寄存器 | `U:power/cx25601n.cc:135-207`：再充关闭 200 ms，重新使能后等 500 ms；欠压无输入且 2500..3100 mV 时写 `0x18=1`，高于 3200 mV 清软件锁存 | `drivers/chips/cx25601n/cx25601n.c:53,115-140` 参数、滞回相同；拆成两次后续轮询；`ports/esp32s31/s31_board.c:40-43` 每次先要求电压/电流/SOC 三读全成功 | 我们不一样；无本次实机证据 | 关闭 EN_CHG 后若电量计下一次读失败，200 ms 的重新使能步骤被提前返回挡住，至少推迟 10 s，持续失败会一直不恢复；SOC 读失败还会阻断本来只需电压/输入状态的欠压保护 | 中 | 复用原厂驱动完整再充操作，避免把必需的收尾绑在下一次独立传感器成功上；不在本任务新增重试或任务 |
| C03 电量计、电量与外部供电判定 | `U:power/bq27220_gauge.cc:22-38,141-233,262-280`；`U:docs/battery-soc-and-charge.md:59-63` 明确不用未经标定的芯片 SOC，按电压曲线、充放电方向、满电 30 s 与滞回计算；地址 `0x55`、电压 `0x08`、有符号电流 `0x0c` | `drivers/chips/bq27220/bq27220.c:3-18` 直接读 SOC `0x2c`；`ports/esp32s31/s31_board.c:117-133` 原样作 percent，使用充电相位或 `ma>=-5` 辅助判有外电 | 我们不一样；寄存器端序及电流符号等价，百分比算法不等价；外电补判已有项目试用记录依据（代码注释），SOC 没有标定证据 | 未标定 SOC 会报假低电/假满电，影响低电策略与升级电量门槛；接近零的放电也可能被算成有外电，需上机区分；不可把原厂的电压算法当作适合所有电池的实测真值 | 高 | 拷 `bq27220_gauge.{h,cc}`、保留 HAL 读数和现有电源服务；原厂曲线注明电芯/负载条件，后续同电芯校准；外电推断单独验证 |
| C04 RTC 初始化与时间可信性 | `U:sensor/pcf8563.cc:28-67,88-169`：清 CTRL1 STOP；VL 置位拒绝灌系统时间；检查月日时分秒，用本地时间转换；`U:periph/metalio_rtc.cc:54-79` | `drivers/chips/pcf8563/pcf8563.c:35-43` 只读日历，无 STOP 初始化和字段校验；`ports/esp32s31/s31_board.c:136-160` 固定 UTC+8，VL 时只要换算后在 2025..2099 仍接受 | 我们不一样；VL 变通及 UTC+8 有既有试用观察（代码注释），不是本次验证，也不证明损坏/停振情况安全 | STOP 遗留会让 RTC 一直不走；VL 或非法字段仍可变成看似合理的日期，离线启动时系统时间和闹钟不可信；固定时区未贯通设置已属既知限制 | 中 | 拷 `pcf8563.{h,cc}` 的初始化、校验和清标志；保留 `hal_rtc_*` 及系统时间服务；可信/未可信分开，不能以一次时钟正确抹去 VL 含义 |
| C05 RTC 闹钟与中断 | `U:sensor/pcf8563.cc:174-197` 只提供 IRQ 标志清除/系统写回；`U:periph/metalio_rtc.cc:33-49,86-98` 对 P16 采边沿并清芯片中断 | `drivers/chips/pcf8563/pcf8563.c:57-77` 写分钟/时/日闹钟、禁星期、清 CTRL2；`ports/esp32s31/s31_board.c:160-164` 接 HAL；`boards/metalio_plus/metalio_board.c:75-88` 增加 P16 唤醒 | 我们新增，原厂无等价闹钟设置路径；不得宣称直接拷贝即可补齐 | 分钟精度且按“每月的某日”匹配，具体一次性/秒级语义须由系统时间服务兑现；唤醒源扩展需与原厂只留 P17 的睡眠流程一起核对 | 中 | 保留 HAL 和系统闹钟业务，底层其余部分复用原厂；闹钟扩展保留独立的设备验证条目 |

| C06 S31 浅睡内核、MPLL/PSRAM 交接 | `U:power_policy/s31_sleep/s31_sleep.{cc,h}`、`s31_sleep_sdk.cmake`；原厂 PMU/MPLL 包装和时钟补丁 | `ports/esp32s31/vendor/s31_sleep/`；`s31_sleep_bridge.cc:4-10`、`s31_sleep_sdk_check.cmake:1-9` | 一样/编译适配等价：【实测】头文件和 CMake 字节一致；cc 仅拆 volatile 连续赋值、诊断改多源唤醒 API | 内核补丁没有发现新的寄存器/时钟次序偏差；这不证明 HAL 外围序列正确 | 低 | 保留已拷的原厂模块、MIT 与 SDK 哈希保护；不再重写 |
| C07 睡眠停放与准入 | `U:power_policy/README.md:61-90`、`power_hw.cc:426-461`：等待文件/启动/网络音频真正空闲，卸载 SD、释放 CNNT、停屏总线并保持 | `ports/esp32s31/hal_power_s31.c:80-84,126-167,195` 明写 SD/屏总线停放未接；仅查显示、震动、相机和音频；`ports/esp32s31/CMakeLists.txt:6` 默认 OFF | 我们缺；开关关闭时不执行此睡眠路径，无本次 ON 实机证据 | 未来打开浅睡时，挂载卡/在途 IO 和 LCD 总线未停放，可能睡不稳、醒后卡失败或外围反向供电；不能把现有默认清醒状态报成已发生的浅睡故障 | 高（启用后） | 拷 `power_hw.cc` 的硬件停放/恢复及 `metalio_sd.cc` 片段，接现有存储/电源准入；保持默认关闭，补齐再验 |
| C08 P14、电源恢复与定时唤醒 | `U:power_policy/README.md:26-28,78-90`、`power_hw.cc:531-547`：待机保留 P14；定时醒只恢复按键；只有复位蓝牙才做 500 ms 马达放电、P14 低累计 1 s | `boards/metalio_plus/metalio_board.c:93-105,130-139` 在 `keep_shared_power=false` 时切 P14 并放电 500 ms；`hal_power_s31.c:157-163,189-198` 每次都恢复轨、加速度计和部分驱动 | 我们不一样；原厂待机和复位的两种流程被混用；无本次对应实机证据 | 关闭保模组睡眠会多一次共享轨复位，且没有原厂 1 s 最短断电；定时切片重复拉轨/复位增加耗电并可能残留 BH 状态；保留模组分支不切 P14 | 中（浅睡 ON） | 拷原厂 `sleep_cut_rails/sleep_restore_rails` 与独立 BT reset 顺序；保留 `hal_power_sleep` 结果接口，不把复位序列套到普通待机 |
| C09 唤醒源与 USB 串口恢复 | `U:power_policy/README.md:84-90,125-127`、`power_hw.cc:142-179,472-524`：只留 P17 输入、GPIO2 低唤醒；SD 恢复后恢复 USB PHY | `hal_power_s31.c:86-114,130-155,178-198`、`boards/metalio_plus/metalio_board.c:75-88` 增加 RTC/运动/模组唤醒；USB 另加关 PHY 150 ms，纯定时醒跳过；SD 恢复未接 | 我们不一样；扩展唤醒是项目能力，不能直接丢弃；USB 基于旧原厂改写 | 共享 INT 上 P16/运动仍低时可能反复拒睡或唤醒；接入 CNNT 后若 USB 恢复仍发生在 SD 前，串口可能再次失联；扩展唤醒需单独验，不拿原厂支持代证 | 中 | 复用最新版 SD→USB 恢复顺序和电源键基线；保留项目唤醒掩码并逐源验证 |
| C10 开机次序、跨复位 GPIO hold 与共享电源 | `U:main/app_main.cc:17-32` 最先释放 hold；`U:metalio_e_ink4_plus_board.cc:59-96` 先板屏/电源/RTC，再 SD、按键与策略，音频初始化执行共享电源复位 | `ports/esp32s31/main/main.c:14-25` 先资产/存储，再板级；`s31_board.c:77-102`、`boards/metalio_plus/metalio_board.c:18-29` 上电高后等 80 ms，无启动共享电源完整复位、无全板 hold 释放 | 我们缺/不一样；存储先于 I²C 本身不是故障证据 | 从留有 GPIO hold 的复位状态启动可能保留屏/电源旧电平；MCU 重启但 BH 仍有旧模式时只等稳定/发命令，未执行原厂完整断电放电流程，不能保证与冷启动同态 | 高 | 拷 `boot_release_holds`、`power_hw_bt_power_reset` 及板级电源复位；在触发蜂窝/音频使用前接入既有启动装配，保留项目 NVS 不写的试用约束 |
| C11 启动崩溃保护与硬件关机 | `U:main/app_main.cc:17-35` 无本项目 RTC 三次回退；`U:power_policy/power_hw.cc:385-405` 等外围/显示后持续重复关机脉冲；`U:display/epd_board_metalio_eink4_plus.c:600-612` 15 组 100 ms 高/低 | `ports/esp32s31/s31_boot.c:11-49` RTC 60 s 内连续三次 fault 选 ota_0；`boards/metalio_plus/metalio_board.c:153-159` 脉冲参数一致但只打一轮，I²C 失败忽略；`hal_power_s31.c:205-211` 返回后只记日志 | 我们新增启动保护；关机正常一轮等价、失败恢复不等价 | 回退不恢复重用的 font_data（已知布局限制）；关机某个脉冲丢失可能仍开着、上层却已进入关机完成态；不能把选槽称完整回出厂 | 中 | 保留 RTC 防循环与 HAL；关机复用原厂脉冲/返回状态处理；全镜像回厂按既定恢复文档执行，不在此审计更改 |
| C12 panic 屏幕安全收尾 | `U:display/epd_board_metalio_eink4_plus.c` 的正常电源顺序及 `U:bus/metalio_i2c1.c:55-83` 总线恢复；原厂无同名 panic hook | `ports/esp32s31/s31_panic.c:14-66` IRAM 下直接 GPIO bit-bang，XOE/MODE 低→PWRUP/VCOM 低→WAKEUP 低，再进 IDF panic | 我们新增；正常驱动序列的紧急适配，非原文复用 | 无 RTOS/I²C 锁依赖是正确方向；坏总线只作 best effort，ACK 失败无法保证屏轨关闭；它发生在异常之后，不能据此解释先前 Cache error | 中 | 保留专用紧急入口和内部 RAM/ROM 约束；不要在 panic 中直接拷会锁/调度的普通驱动；后续单独验证真正断轨 |

| C13 内部闪存与 LittleFS | 原厂分区 `U:partitions/v1/16m.csv:3-12` 保留 model；原厂运行数据主要走 NVS/SD，无本项目 LittleFS 对应实现；项目既定布局 `docs/hardware/flash-layout.md:3-15` 授权重用原厂空 model | `ports/esp32s31/hal_storage_s31.c:39-51,94-101,124-163`：400 KiB、100×4096，缓存/文件对象内部 RAM，经 `esp_partition_*`；`LFS_ERR_CORRUPT` 无条件格式化 | 我们新增；不是原厂有等价 LittleFS | 已用 `/user` 损坏或挂载不识别，重启就自动格式化，设置/成员/数据都可能丢失；首次空区与已有损坏区没有区分；首次重用 model 本身已有布局授权，不能误报成擅改分区 | 高 | 保留 LittleFS 和 HAL（原厂无可拷替代）；把自动格式化与首次初始化/显式恢复分开交主会话决定，审计不改 |
| C14 TF 卡接线、DMA 与挂载 | `U:storage/SdCardManager.cc:27-60,83-101,116-143`：4 位 SDMMC、CLK24/CMD25/D0..3=20..23，4096 B 内部 DMA 中转、最多8扇区、失败不格式化；`U:periph/metalio_sd.cc:67-98` 解 hold 与休眠时卸载/释放 CNNT | `ports/esp32s31/hal_storage_s31.c:52-111` 同接线、4096 B 内部对齐 DMA/8扇区、12句柄/16 KiB分配单位、不格式化；自行每秒事件查卡，拔卡等待句柄归还才卸载；没有 CNNT 停放 | 我们重写但正常传输设置等价；睡眠部分缺（见 C07） | DMA 大缓冲没有误放 PSRAM，此项不是新坑；开机异步挂载与查卡串在默认事件循环，慢卡会拖延同循环其他事件；后者需实测延迟，不能据此判必死锁 | 中 | 拷 `metalio_sd.cc` 和 `SdCardManager` 的底层挂载/DMA/停放；保留 `/content`、`/cache` 与 HAL 热插拔接口，不照搬原厂目录和 UI |
| C15 分区表及回原厂兼容性 | `U:partitions.csv:3-12` 与 `partitions/v1/16m.csv`：16 MiB、双5 MiB OTA、400 KiB model/resources、5 MiB font_data、64 KiB coredump | `ports/esp32s31/partitions.csv:2-10` 数值相同；`docs/hardware/flash-layout.md:10-21` 复用 model/font_data 为用户区、资源和日志，保留 ota_0 | 一样（物理边界）；内容格式不一样且有既定试用决策 | 仅切回 ota_0 不会还原原厂字体和 model；这已在布局文档说明，不作为新发现；64 KiB coredump 配合最多8任务可能不能保存每个任务，不能保证获得完整故障现场 | 中 | 保留既定分区和恢复约定；拷驱动不写分区表，不把 OTA 回退当整片恢复 |
| C16 GC2145 寄存器与便携驱动 | `U:camera/gc2145_vga_settings.h` 是 Apache-2.0 的 586 对 VGA/RGB565_BE/20 MHz 表；`U:camera/dvp_camera.cc:95-141` MCLK先开、等10 ms、分读 PID/选page0，拍照每次重载表 | `ports/esp32s31/vendor/gc2145/gc2145_vga_settings.h` 【实测】586对全部相同；`hal_camera_s31.c:94-123` 等价主次序；`drivers/chips/gc2145/gc2145.c:13-43` 是另一份便携调用，S31 HAL 不调用它 | 表一样；S31 HAL 为 C 改写但正常初始化等价；便携驱动缺page0/MCLK编排但不是当前S31路径 | 不能用便携驱动单测证明 S31 硬件路径，也不能把它的问题报成 S31 已在用；两个表后续独立改动易漂移 | 低 | 保留已拷寄存器表与 Apache/MIT 声明；优先原文复用 `dvp_camera.{cc,h}` 的硬件会话，保留 HAL 图像返回接口 |
| C17 GC2145 取帧、共享屏资源与失败恢复 | `U:camera/dvp_camera.cc:54-70,84-94,147-173,201-239`：屏/Flash guard、共享复位租约、8次预热请求后只把目标帧交DMA一次、PSRAM帧缓冲；成功停DMA，清理后释放 | `ports/esp32s31/hal_camera_s31.c:48-53,77-92,114-188` 主DMA次序/614400 B PSRAM帧/内部ISR上下文相同，用显示锁和busy替租约；`171-178` 首次检测结果永久缓存 | 我们重写但正常取帧等价，探测失败恢复不一样；没有新增拍照实机证据 | 第一次探测因显示锁超时、I²C瞬态或P5尚未恢复失败，就永久缓存“无相机”，之后不会再试，重启前拍照一直不可用；原厂低层 Detect 每次重试可重新探测，但上层也可能缓存，不能声称整机原厂一定自动重试 | 中 | 拷硬件会话/资源租约；HAL只保留适配与明确的重探语义；正常PSRAM DMA是原厂有意设计，不应一律搬内部RAM |
| C18 音频 I²S、采样格式与DMA | `U:audio/bt_audio_codec.cc:66-131,138-185`，`U:config.h`：I²S0 slave、BCLK42/WS43/OUT44/IN45、32位立体声Philips、4 DMA描述符，采样右移12、输出复制双声道；BH提供时钟 | `ports/esp32s31/hal_audio_s31.c:54-87,105-125,148-163` 参数等价，固定16 kHz、240帧×4=60 ms软件runway；临时PCM在PSRAM，DMA由IDF分配；采集任务核0，ISR溢出回调默认非cache-safe | 我们重写但正常总线格式等价；软件缓存/任务节拍不同 | Flash忙时非cache-safe音频IRQ会延后，60 ms是缓冲预算而非声学保证；长擦除可能录音溢出/播放断音；不能把PSRAM临时PCM误判为直接DMA到不可用内存 | 中 | 拷 `bt_audio_codec.{cc,h}` 的通道生命周期、格式和错误处理，HAL保留录放回调和资源服务，不搬原厂应用线程 |
| C19 BH音频模式、功放与启动恢复 | `U:periph/metalio_audio.cc:24-27,94-126,135-178,202-235`：P14完整复位、恢复后700 ms、RX=2→700 ms→MODE=1、等ACK1 s，首次失败后最多3次重试；`U:power_policy/power_hw.cc:242-265` 配合PA/蜂窝 | `ports/esp32s31/hal_bt_s31.c:74-102`、`drivers/chips/bh1098g/bh1098g.c:18-24` 同命令/700 ms/1 s但按调用同步等待，无原厂开机复位和固定一轮最多4次恢复；`hal_audio_s31.c:30-34,98-123` 就绪后才开I²S和PA | 我们不一样；协议常量一致，完整恢复策略没有原文复用；无对应故障实机证据 | MCU软重启且BH留在旧蓝牙模式/临时丢ACK，首个录放请求可能失败；调用线程会同步等待稳定/命令/ACK，是否阻塞UI取决于现有执行器，不能笼统声称UI必阻塞 | 高 | 拷 `metalio_audio.cc` 底层模式与电源通知和板级复位，桥接已有媒体/蓝牙执行器；头戴设备路由仍保留项目HAL，不把原厂启动/UI/NVS全搬进来 |

## 最可能出事的前 10 个坑

待逐模块核查后排序。

## 直接拷贝清单

待补；默认复用原厂底层实现，仅保留 HAL 接口及必要安全检查，逐项核实许可证。

## 上机先核对清单

本任务不碰设备；仅记录供后续获授权的实机验证使用的项目。

## 没做到的

- 按审计任务书要求，不运行全量测试、ASan、S31 编译和 gcccheck，不修改代码。不得据此声称通过运行或实机验收。

## 和设计不一致的

待核查。

## 改过的已有测试

无。

## 要用户定的

待核查；本任务不作产品决定。

## 顺手发现

待核查。

## 要改的文档

本任务不改共享文档，待列出需要主会话同步的事实。

## 新增的机制

无。产品代码改动 0 行。

## 最终检查

待补范围、链接、行号、隐私与提交检查。
