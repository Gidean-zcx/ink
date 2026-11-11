# driver-audit-1c 审计报告

日期：2026-11-11。基线：`87bf48655ee4820837b0f54a43c8f5aed37278c2`。

状态：审计完成，结论是风险与复用建议，不是驱动通过实机验收。本报告仅记录源码对照、风险和建议；不修改产品代码、测试或构建文件，不操作设备。

证据约定：【实现】为固定提交的源码核查；【实测】仅指本次执行的只读命令及文档检查；【估算】为尚需实机确认的因果推断。任务书中的既有上机现象不算本次实测。

路径约定：`U:` 指 [原厂固定提交](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)，`I:` 指 [ESP-IDF固定提交](https://github.com/espressif/esp-idf/tree/fff9895c82d744c7237be8847347bdd1b07c6643) 的本机只读源码（v6.1），未加前缀的路径指本项目基线。原厂完整提交为 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；本地克隆 HEAD 保持 `690a023c66862af103a3187464373ee7c5efcd14`。本机 IDF 为 `v6.1`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 完成【实现】 | C01–C23覆盖全部指定驱动/流程；双方源码出处及配置/Cache专节齐全 | [E3](EVIDENCE.md#e3-来源行号复核)；源码引用与行号检查 |
| 2 | 完成【实现/估算】 | 十项风险、七组拷贝建议及上机核对清单；故障候选标注触发前提 | [E1/E2](EVIDENCE.md)；原厂main只读API |
| 3 | 完成【实测】 | 仅新增本目录REPORT.md与EVIDENCE.md；产品/测试/构建改动0行 | `git diff --name-only 87bf4865..HEAD`；`git diff --check 87bf4865..HEAD` |

## 来源核查

【实测】2026-11-11 通过 GitHub 只读 API 查询 [原厂 main](https://api.github.com/repos/CloudZao/Metalio-E-INK4-Plus/branches/main)，返回 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，提交说明 v2.0.77，提交时间 2026-11-07 11:32:56 UTC。公开 main 没有更新的提交；网页烧录有 2.0.78 不等于公开源码已更新。以 `git show 874538e:<路径>` 补齐部分导出缺失的文件，不移动原厂 HEAD。项目相关未结Issue与PR分别检索 `repo:Gidean-zcx/ink is:open driver-audit`，均返回空。

## 模块对照

下列出处和差异均为【实现】；潜在故障与严重度为【估算】，未声称本次在真机复现。`U:power/`、`U:power_policy/`、`U:sensor/`、`U:storage/`、`U:camera/`、`U:audio/` 分别省略前缀 `main/boards/common/`；`U:periph/` 省略 `main/boards/metalio-e-ink4-plus/`。

| 模块 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑 | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| C01 充电初始化与限流 | `U:power/cx25601n.cc:314-337,548-575`；`U:periph/metalio_power.cc:40-80`：VREG 4400 mV、预充 120 mA、终止 60 mA、禁 WDT/DPDM、私有寄存器、ICHG 联动 IINDPM；先 500 mA，再恢复保存值（默认 1000） | `drivers/chips/cx25601n/cx25601n.c:20-42,75-113`；`ports/esp32s31/s31_board.c:77-84`：相同参数和寄存器，直接 1000 mA；初始化失败只记日志，设备指针保留 | 我们重写；主要寄存器与正常结果等价，初始化状态处理不一样；不是原文复用 | 初始化中途 NACK 时，后续仍可能轮询/修改半初始化芯片，无法仅凭“有指针”判定充电策略已就绪；本次无故障实机证据 | 中 | 拷 `cx25601n.{h,cc}` 与 `power_i2c.{h,c}`，以 HAL I²C 作桥接；保留初始化成功状态，电流设置走现有服务接口 |
| C02 再充、欠压 BATFET 与关机寄存器 | `U:power/cx25601n.cc:135-207`：再充关闭 200 ms，重新使能后等 500 ms；欠压无输入且 2500..3100 mV 时写 `0x18=1`，高于 3200 mV 清软件锁存 | `drivers/chips/cx25601n/cx25601n.c:53,115-140` 参数、滞回相同；拆成两次后续轮询；`ports/esp32s31/s31_board.c:40-43` 每次先要求电压/电流/SOC 三读全成功 | 我们不一样；无本次实机证据 | 关闭 EN_CHG 后若电量计下一次读失败，200 ms 的重新使能步骤被提前返回挡住，至少推迟 10 s，持续失败会一直不恢复；SOC 读失败还会阻断本来只需电压/输入状态的欠压保护 | 中 | 复用原厂驱动完整再充操作，避免把必需的收尾绑在下一次独立传感器成功上；不在本任务新增重试或任务 |
| C03 电量计、电量与外部供电判定 | `U:power/bq27220_gauge.cc:22-38,141-233,262-280`；`U:docs/battery-soc-and-charge.md:59-63` 明确不用未经标定的芯片 SOC，按电压曲线、充放电方向、满电 30 s 与滞回计算；地址 `0x55`、电压 `0x08`、有符号电流 `0x0c` | `drivers/chips/bq27220/bq27220.c:3-18` 直接读 SOC `0x2c`；`ports/esp32s31/s31_board.c:117-133` 原样作 percent，使用充电相位或 `ma>=-5` 辅助判有外电 | 我们不一样；寄存器端序及电流符号等价，百分比算法不等价；外电补判已有项目试用记录依据（代码注释），SOC 没有标定证据 | 未标定 SOC 会报假低电/假满电，在启用低电策略时影响低电判断，也影响升级电量门槛；接近零的放电也可能被算成有外电，需上机区分；不可把原厂的电压算法当作适合所有电池的实测真值 | 高 | 拷 `bq27220_gauge.{h,cc}`、保留 HAL 读数和现有电源服务；原厂曲线注明电芯/负载条件，后续同电芯校准；外电推断单独验证 |
| C04 RTC 初始化与时间可信性 | `U:sensor/pcf8563.cc:28-67,88-169`：清 CTRL1 STOP；VL 置位拒绝灌系统时间；检查月日时分秒，用本地时间转换；`U:periph/metalio_rtc.cc:54-79` | `drivers/chips/pcf8563/pcf8563.c:35-43` 只读日历，无 STOP 初始化和字段校验；`ports/esp32s31/s31_board.c:136-160` 固定 UTC+8，VL 时只要换算后在 2025..2099 仍接受 | 我们不一样；VL 变通及 UTC+8 有既有试用观察（代码注释），不是本次验证，也不证明损坏/停振情况安全 | STOP 遗留会让 RTC 一直不走；VL 或非法字段仍可变成看似合理的日期，离线启动时系统时间和闹钟不可信；固定时区未贯通设置已属既知限制 | 中 | 拷 `pcf8563.{h,cc}` 的初始化、校验和清标志；保留 `hal_rtc_*` 及系统时间服务；可信/未可信分开，不能以一次时钟正确抹去 VL 含义 |
| C05 RTC 闹钟与中断 | `U:sensor/pcf8563.cc:174-197` 只提供 IRQ 标志清除/系统写回；`U:periph/metalio_rtc.cc:33-49,86-98` 对 P16 采边沿并清芯片中断 | `drivers/chips/pcf8563/pcf8563.c:57-77` 写分钟/时/日闹钟、禁星期、清 CTRL2；`ports/esp32s31/s31_board.c:160-164` 接 HAL；`boards/metalio_plus/metalio_board.c:75-88` 增加 P16 唤醒 | 我们新增，原厂无等价闹钟设置路径；不得宣称直接拷贝即可补齐 | 分钟精度且按“每月的某日”匹配，具体一次性/秒级语义须由系统时间服务兑现；唤醒源扩展需与原厂只留 P17 的睡眠流程一起核对 | 中 | 保留 HAL 和系统闹钟业务，底层其余部分复用原厂；闹钟扩展保留独立的设备验证条目 |

| C06 S31 浅睡内核、MPLL/PSRAM 交接 | `U:power_policy/s31_sleep/s31_sleep.{cc,h}`、`s31_sleep_sdk.cmake`；原厂 PMU/MPLL 包装和时钟补丁 | `ports/esp32s31/vendor/s31_sleep/`；`ports/esp32s31/s31_sleep_bridge.cc:4-10`、`ports/esp32s31/s31_sleep_sdk_check.cmake:1-9` | 一样/编译适配等价：【实测】头文件和 CMake 字节一致；cc 仅拆 volatile 连续赋值、诊断改多源唤醒 API | 内核补丁没有发现新的寄存器/时钟次序偏差；这不证明 HAL 外围序列正确 | 低 | 保留已拷的原厂模块、MIT 与 SDK 哈希保护；不再重写 |
| C07 睡眠停放与准入 | `U:power_policy/README.md:61-90`、`U:power_policy/power_hw.cc:426-461`：等待文件/启动/网络音频真正空闲，卸载 SD、释放 CNNT、停屏总线并保持 | `ports/esp32s31/hal_power_s31.c:80-84,126-167,195` 明写 SD/屏总线停放未接；HAL仅查显示、震动、相机和音频；上层 `system/services/power/power_service.c:236-239,285-303` 已查待办存储/业务占用，但未卸载卡/归还所有句柄；`ports/esp32s31/CMakeLists.txt:6` 默认 OFF | 我们缺；开关关闭时不执行此睡眠路径，无本次 ON 实机证据 | 未来打开浅睡时，挂载卡/在途 IO 和 LCD 总线未停放，可能睡不稳、醒后卡失败或外围反向供电；不能把现有默认清醒状态报成已发生的浅睡故障 | 高（启用后） | 拷 `power_hw.cc` 的硬件停放/恢复及 `metalio_sd.cc` 片段，接现有存储/电源准入；保持默认关闭，补齐再验 |
| C08 P14、电源恢复与定时唤醒 | `U:power_policy/README.md:26-28,78-90`、`U:power_policy/power_hw.cc:531-547`：待机保留 P14；定时醒只恢复按键；只有复位蓝牙才做 500 ms 马达放电、P14 低累计 1 s | `boards/metalio_plus/metalio_board.c:93-105,130-139` 在 `keep_shared_power=false` 时切 P14 并放电 500 ms；`ports/esp32s31/hal_power_s31.c:157-163,189-198` 每次都恢复轨、加速度计和部分驱动 | 我们不一样；原厂待机和复位的两种流程被混用；无本次对应实机证据 | 关闭保模组睡眠会多一次共享轨复位，入睡取消/很短切片时没有原厂 1 s 最短断电保证；定时切片重复拉轨/复位增加耗电并可能残留 BH 状态；保留模组分支不切 P14 | 中（浅睡 ON） | 拷原厂 `sleep_cut_rails/sleep_restore_rails` 与独立 BT reset 顺序；保留 `hal_power_sleep` 结果接口，不把复位序列套到普通待机 |
| C09 唤醒源与 USB 串口恢复 | `U:power_policy/README.md:84-90,125-127`、`U:power_policy/power_hw.cc:142-179,472-524`：只留 P17 输入、GPIO2 低唤醒；SD 恢复后恢复 USB PHY | `ports/esp32s31/hal_power_s31.c:86-114,130-155,178-198`、`boards/metalio_plus/metalio_board.c:75-88` 增加 RTC/运动/模组唤醒；USB 另加关 PHY 150 ms，纯定时醒跳过；SD 恢复未接 | 我们不一样；扩展唤醒是项目能力，不能直接丢弃；USB 基于旧原厂改写 | 共享 INT 上 P16/运动仍低时可能反复拒睡或唤醒；接入 CNNT 后若 USB 恢复仍发生在 SD 前，串口可能再次失联；扩展唤醒需单独验，不拿原厂支持代证 | 中 | 复用最新版 SD→USB 恢复顺序和电源键基线；保留项目唤醒掩码并逐源验证 |
| C10 开机次序、跨复位 GPIO hold 与共享电源 | `U:main/app_main.cc:17-32` 最先释放 hold；`U:main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc:59-96` 先板屏/电源/RTC，再 SD、按键与策略，音频初始化执行共享电源复位 | `ports/esp32s31/main/main.c:14-25` 先资产/存储，再板级；`ports/esp32s31/s31_board.c:77-102`、`boards/metalio_plus/metalio_board.c:18-29` 上电高后等 80 ms，无启动共享电源完整复位、无全板 hold 释放 | 我们缺/不一样；存储先于 I²C 本身不是故障证据 | 从留有 GPIO hold 的复位状态启动可能保留屏/电源旧电平；MCU 重启但 BH 仍有旧模式时只等稳定/发命令，未执行原厂完整断电放电流程，不能保证与冷启动同态 | 高 | 拷 `boot_release_holds`、`power_hw_bt_power_reset` 及板级电源复位；在触发蜂窝/音频使用前接入既有启动装配，保留项目 NVS 不写的试用约束 |
| C11 启动崩溃保护与硬件关机 | `U:main/app_main.cc:17-35` 无本项目 RTC 三次回退；`U:power_policy/power_hw.cc:385-405` 等外围/显示后持续重复关机脉冲；`U:main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c:600-612` 15 组 100 ms 高/低 | `ports/esp32s31/s31_boot.c:11-49` RTC 60 s 内连续三次 fault 选 ota_0；`boards/metalio_plus/metalio_board.c:153-159` 脉冲参数一致但只打一轮，I²C 失败忽略；`ports/esp32s31/hal_power_s31.c:205-211` 返回后只记日志 | 我们新增启动保护；关机正常一轮等价、失败恢复不等价 | 回退不恢复重用的 font_data（已知布局限制）；关机某个脉冲丢失可能仍开着、上层却已进入关机完成态；不能把选槽称完整回出厂 | 中 | 保留 RTC 防循环与 HAL；关机复用原厂脉冲/返回状态处理；全镜像回厂按既定恢复文档执行，不在此审计更改 |
| C12 panic 屏幕安全收尾 | `U:main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c` 的正常电源顺序及 `U:main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c:55-83` 总线恢复；原厂无同名 panic hook | `ports/esp32s31/s31_panic.c:14-66` IRAM 下直接 GPIO bit-bang，XOE/MODE 低→PWRUP/VCOM 低→WAKEUP 低，再进 IDF panic | 我们新增；正常驱动序列的紧急适配，非原文复用 | 无 RTOS/I²C 锁依赖是正确方向；坏总线只作 best effort，ACK 失败无法保证屏轨关闭；它发生在异常之后，不能据此解释先前 Cache error | 中 | 保留专用紧急入口和内部 RAM/ROM 约束；不要在 panic 中直接拷会锁/调度的普通驱动；后续单独验证真正断轨 |

| C13 内部闪存与 LittleFS | 原厂分区 `U:partitions/v1/16m.csv:3-12` 保留 model；原厂运行数据主要走 NVS/SD，无本项目 LittleFS 对应实现；项目既定布局 `docs/hardware/flash-layout.md:3-15` 授权重用原厂空 model | `ports/esp32s31/hal_storage_s31.c:39-51,94-101,124-163`：400 KiB、100×4096，缓存/文件对象内部 RAM，经 `esp_partition_*`；`LFS_ERR_CORRUPT` 无条件格式化 | 我们新增；不是原厂有等价 LittleFS | 已用 `/user` 损坏或挂载不识别，重启就自动格式化，设置/成员/数据都可能丢失；首次空区与已有损坏区没有区分；首次重用 model 本身已有布局授权，不能误报成擅改分区 | 高 | 保留 LittleFS 和 HAL（原厂无可拷替代）；把自动格式化与首次初始化/显式恢复分开交主会话决定，审计不改 |
| C14 TF 卡接线、DMA 与挂载 | `U:storage/SdCardManager.cc:27-60,83-101,116-143`：4 位 SDMMC、CLK24/CMD25/D0..3=20..23，4096 B 内部 DMA 中转、最多8扇区、失败不格式化；`U:periph/metalio_sd.cc:67-98` 解 hold 与休眠时卸载/释放 CNNT | `ports/esp32s31/hal_storage_s31.c:52-111` 同接线、4096 B 内部对齐 DMA/8扇区、12句柄/16 KiB分配单位、不格式化；自行每秒事件查卡，拔卡等待句柄归还才卸载；没有 CNNT 停放 | 我们重写但正常传输设置等价；睡眠部分缺（见 C07） | DMA 大缓冲没有误放 PSRAM，此项不是新坑；开机异步挂载与查卡串在默认事件循环，慢卡会拖延同循环其他事件；后者需实测延迟，不能据此判必死锁 | 中 | 拷 `metalio_sd.cc` 和 `SdCardManager` 的底层挂载/DMA/停放；保留 `/content`、`/cache` 与 HAL 热插拔接口，不照搬原厂目录和 UI |
| C15 分区表及回原厂兼容性 | `U:partitions.csv:3-12` 与 `partitions/v1/16m.csv`：16 MiB、双5 MiB OTA、400 KiB model/resources、5 MiB font_data、64 KiB coredump | `ports/esp32s31/partitions.csv:2-10` 数值相同；`docs/hardware/flash-layout.md:10-21` 复用 model/font_data 为用户区、资源和日志，保留 ota_0 | 一样（物理边界）；内容格式不一样且有既定试用决策 | 仅切回 ota_0 不会还原原厂字体和 model；这已在布局文档说明，不作为新发现；64 KiB coredump 配合最多8任务可能不能保存每个任务，不能保证获得完整故障现场 | 中 | 保留既定分区和恢复约定；拷驱动不写分区表，不把 OTA 回退当整片恢复 |
| C16 GC2145 寄存器与便携驱动 | `U:camera/gc2145_vga_settings.h` 是 Apache-2.0 的 586 对 VGA/RGB565_BE/20 MHz 表；`U:camera/dvp_camera.cc:95-141` MCLK先开、等10 ms、分读 PID/选page0，拍照每次重载表 | `ports/esp32s31/vendor/gc2145/gc2145_vga_settings.h` 【实测】586对全部相同；`ports/esp32s31/hal_camera_s31.c:94-123` 等价主次序；`drivers/chips/gc2145/gc2145.c:13-43` 是另一份便携调用，S31 HAL 不调用它 | 表一样；S31 HAL 为 C 改写但正常初始化等价；便携驱动缺page0/MCLK编排但不是当前S31路径 | 不能用便携驱动单测证明 S31 硬件路径，也不能把它的问题报成 S31 已在用；两个表后续独立改动易漂移 | 低 | 保留已拷寄存器表与 Apache/MIT 声明；优先原文复用 `dvp_camera.{cc,h}` 的硬件会话，保留 HAL 图像返回接口 |
| C17 GC2145 取帧、共享屏资源与失败恢复 | `U:camera/dvp_camera.cc:54-70,84-94,147-173,201-239`：屏/Flash guard、共享复位租约、8次预热请求后只把目标帧交DMA一次、PSRAM帧缓冲；成功停DMA，清理后释放 | `ports/esp32s31/hal_camera_s31.c:48-53,77-92,114-188` 主DMA次序/614400 B PSRAM帧/内部ISR上下文相同，用显示锁和busy替租约；`171-178` 首次检测结果永久缓存 | 我们重写但正常取帧等价；首次失败缓存与原厂板级入口一样；共享轨租约接口不同，无本次实机证据 | 第一次探测因显示锁超时、I²C瞬态或P5尚未恢复失败，就永久缓存“无相机”，之后不会再试，重启前拍照一直不可用；原厂板级 `U:main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc:111-123` 也永久缓存首次失败，属于两边共有风险 | 中 | 拷硬件会话/资源租约；HAL只保留适配与明确的重探语义；正常PSRAM DMA是原厂有意设计，不应一律搬内部RAM |
| C18 音频 I²S、采样格式与DMA | `U:audio/bt_audio_codec.cc:66-131,138-185`，`U:main/boards/metalio-e-ink4-plus/config.h`：I²S0 slave、BCLK42/WS43/OUT44/IN45、32位立体声Philips、4 DMA描述符，采样右移12、输出复制双声道；BH提供时钟 | `ports/esp32s31/hal_audio_s31.c:54-87,105-125,148-163` 参数等价，固定16 kHz、240帧×4=60 ms软件runway；临时PCM在PSRAM，DMA由IDF分配；采集任务核0，ISR溢出回调默认非cache-safe | 我们重写但正常总线格式等价；软件缓存/任务节拍不同 | Flash忙时非cache-safe音频IRQ会延后，60 ms是缓冲预算而非声学保证；长擦除可能录音溢出/播放断音；不能把PSRAM临时PCM误判为直接DMA到不可用内存 | 中 | 拷 `bt_audio_codec.{cc,h}` 的通道生命周期、格式和错误处理，HAL保留录放回调和资源服务，不搬原厂应用线程 |
| C19 BH音频模式、功放与启动恢复 | `U:periph/metalio_audio.cc:24-27,94-126,135-178,202-235`：P14完整复位、恢复后700 ms、RX=2→700 ms→MODE=1、等ACK1 s，首次失败后最多3次重试；`U:power_policy/power_hw.cc:242-265` 配合PA/蜂窝 | `ports/esp32s31/hal_bt_s31.c:74-102`、`drivers/chips/bh1098g/bh1098g.c:18-24` 同命令/700 ms/1 s但按调用同步等待，无原厂开机复位和固定一轮最多4次恢复；`ports/esp32s31/hal_audio_s31.c:30-34,98-123` 就绪后才开I²S和PA | 我们不一样；协议常量一致，完整恢复策略没有原文复用；无对应故障实机证据 | MCU软重启且BH留在旧蓝牙模式/临时丢ACK，首个录放请求可能失败；调用线程会同步等待稳定/命令/ACK，是否阻塞UI取决于现有执行器，不能笼统声称UI必阻塞 | 高 | 拷 `metalio_audio.cc` 底层模式与电源通知和板级复位，桥接已有媒体/蓝牙执行器；头戴设备路由仍保留项目HAL，不把原厂启动/UI/NVS全搬进来 |

| C20 调频与 CPU boost | `U:power_policy/power_hw.cc:331-380` 定频 min=max、关闭自动浅睡、带 scoped hold；`U:power_policy/power_policy.cc:27,310-313,657` 空闲默认320，可选160/240/320 | `ports/esp32s31/hal_power_s31.c:22-68` 同固定频率/boost语义；`ports/esp32s31/s31_board.h:28` 空闲160；`system/services/power/power_service.c:223,531-534` 随状态及输入切回320 | 我们重写但锁/固定频率思路等价；默认空闲频率不一样 | 160/320会比原厂默认320制造更多切频时点，但二者均用CPLL320分频；仅凭切频次数不能证明Cache error（详见下节） | 中 | 保留HAL资源语义，底层复用原厂定频实现；先对照固定320定位，再决定默认值，不在本任务改频率 |
| C21 SDK 稳定性配置 | `U:sdkconfig.defaults.esp32s31`、`U:sdkconfig`，逐项见下表；包含原厂最终生效值 | `ports/esp32s31/sdkconfig.defaults` 与留存基线构建 `B:sdkconfig`；不能把defaults缺项当n | 部分不一样；关键Cache/PM/中断保护选项多数一样 | 最明确差异是更小内部RAM保留、Wi-Fi/lwIP未优先PSRAM且保留更多Wi-Fi IRAM，以及优化级别变化；资源不足会先表现为创建DMA/任务失败，不等于越界访问Cache | 高（资源） | 对齐原厂资源相关配置前先测本项目余量；不复制厂商整份含应用/凭据的sdkconfig，逐项复用必要配置 |
| C22 Flash操作期间核1路径 | `U:sdkconfig:276,3658,4651` 启用硬件循环、FreeRTOS整库IRAM关闭、Flash自动挂起关闭；无原厂二进制作同等反汇编 | `I:components/spi_flash/cache_utils.c:75-108` 与 `I:components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S:676-686`；`B:ink-s31.elf` 查到中断入口IRAM→`pxPortGetCoprocArea` Flash，细节如下 | 不属于一条已证实的原厂驱动差异；是共享SDK候选缺口 | Cache关闭期间若HWLP dirty分支触发，核1可在IPC等待中被系统IRQ带进Flash；分支前提尚未证实，不能把静态危险路径写成那次崩溃已定位 | 高（条件成立时） | 优先核对故障时PC/返回地址/HWLP状态和链接图，再评估最小SDK映射修补；不得凭IRAM标记/原厂配置直接验收 |
| C23 充电芯片 OTG 能力 | `U:power/cx25601n.cc:458-502` 有关闭充电→配置OTG→等待30 ms，以及退出OTG恢复充电的完整路径 | `drivers/chips/cx25601n/cx25601n.h:1-35` 无OTG API；本块HAL未接此能力 | 我们缺 | 需要主机供电的USB使用场景无法仅凭MSC/TF支持推导为可用；原厂有芯片API也不证明整板USB供电模式已验证 | 中（需要OTG时） | 需要该能力时直接拷原厂OTG路径并接HAL/服务；范围由主会话核对，不自行认定不需要或本期不做 |

## SDK 稳定性配置对照

`B:` 是只读依赖目录中已有的 `build-s31/`，不是本次生成的构建目录。其 `project_description.json` 标记为 `g87bf4865-261111110131`，`INK_S31_LIGHT_SLEEP:BOOL=OFF`；本次检查过相关已跟踪配置/相机/电源文件相对基线无差异。它是额外构建证据，不等于本次从干净目录重建，也不等于故障那次上机用的镜像。其 ELF 和配置哈希及复核命令见 [EVIDENCE.md](EVIDENCE.md)。

| 配置/类别 | 原厂最终值及出处 | 我们与出处 | 稳定性后果 |
| --- | --- | --- | --- |
| 编译优化 | `U:sdkconfig:1288` SIZE（`-Os`） | `ports/esp32s31/sdkconfig.defaults:2` PERF；`B:sdkconfig:1266`（`-O2`） | 改变时序、代码布局和循环生成；可能改变SDK潜在缺口触发率，不能仅凭此认定根因，也不能声称原厂Os一定规避HWLP |
| 内部RAM保留 | `U:sdkconfig.defaults.esp32s31:9`、`U:sdkconfig:2934` 65536 B | defaults未设，`B:sdkconfig:1926` 32768 B；`I:components/esp_psram/Kconfig.spiram.common:117-135` 默认32768 | DMA/内部任务栈专用保留少32 KiB；叠加主任务16 KiB和多路驱动时可能分配失败。不是自动把要求INTERNAL的DMA偷放PSRAM |
| Wi-Fi/lwIP放置 | `U:sdkconfig.defaults.esp32s31:44,62-64` 优先PSRAM，三项Wi-Fi IRAM优化关；`U:sdkconfig:2933,3186-3188` 一致 | defaults未设，`B:sdkconfig:1924,2148-2152` PSRAM优先关、三项IRAM优化开 | 内部内存更紧，可能推高录放/SD/网络同时初始化失败概率；核1故障的根因仍需中断时地址证据 |
| 运行统计、TLS指针 | `U:sdkconfig:3583,3617,3622` TLS=1、统计/格式化开 | `ports/esp32s31/sdkconfig.defaults:12,21` TLS=2、trace开；`B:sdkconfig:2599,2603` 运行统计/格式化关 | TLS多一槽，TCB略增；统计关闭减少开销，不能当故障成因。`USE_TRACE_FACILITY`不等于自定义trace回调已在中断里做IO |
| Cache、IRAM、Flash自动挂起（核对后相同） | `U:sdkconfig:3009-3019,3109,3658,4651,4661` | `B:sdkconfig:1988-1998,2078,2635,3574,3584` | 两边L1 I/D缓存16/64 KiB、64B行；SYSTEM/TIMER/Flash函数IRAM开，FreeRTOS整库IRAM关，自动挂起关。关Cache仍必须完整调用链安全；不能据“FreeRTOS整库IRAM关”推导所有tick函数都在Flash |
| PM、PSRAM（核对后相同） | `U:sdkconfig:2668-2670,2877-2900,2911-2935` | `ports/esp32s31/sdkconfig.defaults:7-11,16`；`B:sdkconfig:1650-1652,1858-1885,1897-1927` | 都是PSRAM OCT200 MHz、允许外栈/指定外部BSS、XIP关；PM/睡眠IRAM与CPU retention/halfsleep同值。`I:components/esp_wifi/Kconfig:364-370` 的SLP_IRAM依赖会选PM_SLEEP_FUNC，因此defaults缺行不表示关。自动tickless两边都关 |
| Flash模式与频率（相同） | `U:sdkconfig:1205,1217,1227` DIO/80 MHz/16 MiB | `ports/esp32s31/sdkconfig.defaults:4` 16 MiB，其余沿SDK默认；`B:sdkconfig:1200,1212,1222` DIO/80 MHz/16 MiB | 未发现QIO、错误Flash频率或容量差异；`I:components/esptool_py/Kconfig.projbuild:39-42` DIO是默认 |
| FreeRTOS、主栈与中断栈（相同） | `U:sdkconfig:3050,3573-3585,3645-3655` 双核/100Hz/主栈16384/ISR2096/idle1536/SYSTIMER level1 | `ports/esp32s31/sdkconfig.defaults:13-15`；`B:sdkconfig:2027,2556-2566,2623-2633` 同值 | 核1在Flash等待中仍会跑系统节拍和公共中断入口；关调度并不关IRAM中断 |
| 看门狗与外设IRQ（相同） | `U:sdkconfig:2156,2225,3062-3079` INT WDT800ms查双核、TASK WDT20s查双idle且不panic；I²S/DVP cache-safe关 | `ports/esp32s31/sdkconfig.defaults:26-34`；`B:sdkconfig:1394,1441,2037-2052` 同值 | 没有更短WDT造成的配置偏差；非cache-safe外设IRQ关Cache时延后。不能为了绕崩溃关WDT，也不能把IRQ入口IRAM当整个外设路径都IRAM |

## 核 1 开机 Cache error：最可能原因与证据边界

**【估算】最值得优先证伪的具体候选是 SDK 公共中断入口的 HWLP 上下文保存调用了 Flash 函数；置信度中低。**【实测】静态调用/段地址存在，尚未取得那次故障的寄存器来证明HWLP dirty条件成立。已有的“核1在Flash等待循环”与此类“等待中被系统中断打断”的机制相容，但不足以单独确诊。核查链：

1. 【实现】`I:components/spi_flash/cache_utils.c:75-108,149-179`：核0通过IPC令核1暂停调度、屏蔽非IRAM中断，再等待核1就绪并关闭Cache；核1在 `spi_flash_op_block_func` 忙等。`I:components/esp_system/port/soc/esp32s31/cache_err_int.c:54-61` 说明S31两个核的总线共用Cache；错误归属核1并不等于核1当时在做存储业务。
2. 【实现】`I:components/freertos/port_systick.c:72,191-227`：tick是IRAM中断，核1调用 `xTaskIncrementTickOtherCores`；暂停调度时该函数早退。`I:components/esp_system/crosscore_int.c:42-65,97-105` 的跨核中断仍可能进来。无须屏幕工作，也无须新增外设IRQ，都会先经 `rtos_int_enter`。
3. 【实现】`I:components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S:676-686` 在 `CSR_HWLP_STATE_REG==3`（dirty）时调用 `pxPortGetCoprocArea`；`I:components/freertos/FreeRTOS-Kernel/portable/riscv/port.c:789-810` 无IRAM属性。`I:components/freertos/linker.lf:159-187` 把portasm及一批临界/ISR函数放IRAM，却没有把此函数纳入；`FREERTOS_IN_IRAM=n` 时仍在Flash。
4. 【实测】留存基线ELF：`rtos_int_enter=0x2f00ef44` 在 `.iram0.text`；其 `0x2f00efde` 调用 `pxPortGetCoprocArea=0x40204c78`（`.flash.text`）。`hwlp_restore_if_used` 也调用它；但核1调度已暂停时不能简单假定会切到别的任务，此分支证据弱于IRQ进入分支。未在IPC准备/等待的几个已查函数中找到实际HWLP指令，**故不能证明等待时dirty，更不能声称此因果已闭环**。
5. 【实测/实现】`SysTickIsrHandler`、`esp_crosscore_isr`、`esp_pm_impl_isr_hook`、当前160/320 CPLL快切函数均在IRAM；关键内部状态也在内部RAM。PM钩子可能随IRQ执行（`I:components/esp_pm/pm_impl.c:1058-1079`），但正常160/320是同CPLL分频（`I:components/esp_hw_support/port/esp32s31/rtc_clk.c:310-324`），本次没有找到正常路径直接跳Flash的调频证据。跨核WDT异常支路另会进Flash（`task_wdt_timeout_abort`）；两边TASK WDT默认不panic，没有现场超时依据，不把它排到主因。

**原厂配置没有显式避开这一SDK风险。**两边都有HWLP、FreeRTOS整库IRAM关闭、Flash自动挂起关闭；原厂使用Os可能改变触发条件，但没有原厂ELF或真机对照证明它已规避。原厂默认空闲320少切频，不会消除公共IRQ入口。把整库改IRAM或改Os都只能是后续受控对照，不能当已选修复；先用故障PC/RA与HWLP状态核对上述地址，再评估单一函数的最小映射及其数据是否完整安全。

其余候选：原厂新增的开机GPIO/UHCI残留清理我们未接（C10），可能只影响早期启动，15秒后IRQ表不能排除早期差异；共享Cache同步/SDK硬件问题需故障地址才能进一步区分。任务书所述littlefs调用链只定位到触发关Cache的操作，不证明littlefs缓冲越界或PSRAM栈就是根因。

## 最可能出事的前 10 个坑

按后果和当前证据排序；以下为风险清单，均未在本次上机复现。C07/C08只在未来启用浅睡时进入，未重复报告总说明中已修的4G样例。

1. **高｜C22**：关Cache期间若HWLP为dirty，系统IRQ公共入口会调用Flash中的协处理器保存函数；这是开机崩溃的具体候选，条件未证实。
2. **高｜C13**：已有LittleFS损坏后重启会走自动格式化，把可恢复的用户资料变成已擦除资料。
3. **高｜C10**：软件复位保留GPIO中断/UHCI/hold时，没有原厂开机清理，可能在本次驱动完成接管前触发异常或锁住引脚。
4. **高｜C19/C10**：MCU重启而BH不断电时缺原厂共享轨完整复位，首个录音/播放请求可能遇到旧模式或丢ACK而失败。
5. **高｜C03**：直接信未经标定SOC，可能误判低电/满电并影响升级门槛；这是原厂明确避开的做法。
6. **高｜C21**：内部专用保留少32 KiB、网络未优先PSRAM、更多Wi-Fi IRAM占用叠加，可能使DMA或内部栈申请失败。
7. **高（浅睡ON）｜C07/C14**：SD/CNNT和屏总线未停放就睡，醒后可能卡不可用、漏电或显示恢复失败。
8. **中（浅睡ON）｜C08**：取消入睡/短切片时共享轨断电不足原厂1 s，且定时醒就拉起外围，可能复位不完整并增加待机功耗。
9. **中｜C02**：再充已关EN_CHG后，下一轮电量计任一读失败会阻止重新使能，充电关闭时间超出原厂200 ms。
10. **中｜C17（双方共有）**：首次相机探测遇到瞬态失败会一直缓存无相机；直接照搬原厂板级入口也保留此风险。

## 直接拷贝清单

全部为后续建议，**本次产品代码改动0行**。下表原始源文件按 `874538e` 固定；复用原文放端口vendor层，适配已有HAL/执行器/资源服务。不得将原厂Application、UI、NVS策略一起替换项目系统。行数为【估算】源码体量与接口适配量，非本任务已写代码或获得新增任务/线程授权。

| 优先级 | 拷哪些原厂文件/流程 | 我们保留的接口/适配 | 改动量估计 | 许可/注意 |
| --- | --- | --- | --- | --- |
| P0 | 板级 `U:main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c:483-520` 的开机清理；同文件 `263-318` 的P14复位及 `U:power_policy/power_hw.cc:242-265` 协调 | `app_main`启动装配、`metalio_board_*`与音频/蜂窝生命周期；不改物理分区和原厂NVS | 原文约120行，集中桥接30–70行 | 原厂MIT；在所有共享轨消费者启动前执行，不能各自复位P14 |
| P1 | `common/power/cx25601n.{h,cc}`、`power_i2c.{h,c}`，含再充/欠压/OTG | `hal_power_charger_poll`、I²C桥接、电源/设置能力 | 原文约0.9–1.1千行，桥接40–80行；可用部分按完整流程迁移 | MIT；原厂带策略任务，接入现有执行器/是否沿用任务需专门评审，不能悄悄新增线程或再次改写时序 |
| P1 | `common/power/bq27220_gauge.{h,cc}` | `hal_power_battery`及电源状态服务 | 原文429行，桥接20–50行 | MIT；曲线是原厂特定电芯/负载估算，不冒充本机校准 |
| P1 | `periph/metalio_audio.{h,cc}`、`common/audio/bt_audio_codec.{h,cc}` 和所需UART薄接口 | `hal_audio_*`、`s31_bh1098`路由、现有媒体/蓝牙执行器；共享电源只一处拥有 | 原文约0.6–0.8千行，桥接60–120行 | MIT；保留项目安全约束/路由/成员隔离；与P0复位合并处理同一根因 |
| P1（浅睡前） | `periph/metalio_sd.{h,cc}`、`common/storage/SdCardManager.{cc,hpp}` 的DMA/卸载/停放；`power_policy/power_hw.cc` 的硬件入睡/退出、`display/epd_board_metalio_eink4_plus.c` 的P5/总线处理 | `hal_fs_*`、`hal_power_sleep`，现有存储准入和句柄管理，继续保留本项目目录/LittleFS | 原文硬件段约0.4–0.7千行，桥接60–120行 | MIT，底层IDF Apache-2.0；完成资源排空再停放，保留RTC/模组/运动扩展验证，不能删除项目能力 |
| P2 | `common/sensor/pcf8563.{h,cc}`、`periph/metalio_rtc.{h,cc}` 的初始化/校验/清标志 | `hal_rtc_*`、系统时区和闹钟；一次性闹钟扩展原厂没有 | 原文约0.3–0.4千行，桥接30–60行 | MIT；VL信任差异要明确处理，不默改产品时间策略 |
| P2 | `common/camera/dvp_camera.{h,cc}` 的Prepare/Grab/清理、`gc2145_vga_settings.h` | `hal_camera_*`原始RGB565接口、显示/电源租约；不拷JPEG上传或原厂AI工具 | 原文硬件会话约220行，桥接30–60行；586对表已相同无需重抄 | 会话MIT，表Apache-2.0；保留版权/版本/哈希；负探测缓存风险双方共有，需另定重探入口 |

已经复用并应保留：`vendor/s31_sleep` 的原厂内核、SDK补丁和许可证；h/CMake不重写。不能靠“拷原厂”解决的：C13用户区损坏后的格式化策略、C22双方共享SDK的IRAM缺口、C17双方共有的首次负探测缓存。对这些先补决定性证据或恢复语义，不为了满足拷贝要求制造新架构。

## 上机先核对清单

以下只是验证计划，本次没有执行；不接设备、不读备份/历史设备日志、不操作4G。

1. **先拿到Cache故障现场**：正确镜像对应的PC、RA、Cache错误地址/核号及HWLP CSR；与E2条件调用地址对照。若dirty条件不成立，应降低此假设优先级，检查早期残留IRQ和SDK缓存同步，不能执意修HWLP。
2. 在独立获授权的测试版本里，比较原配置、固定320、最小IRAM映射；一次只变一个因素，不放宽WDT/预算。确认不是某条异常断言或WDT诊断路径在先。
3. 开机复位路径：冷启动、MCU软复位、留有外设/hold状态的复位，确认原厂GPIO/UHCI清理在新IRQ注册前完成；只核对P14一次完整放电和恢复，不能反复重置模组碰运气。
4. 充电：在关闭EN_CHG后的200 ms间制造明确的合成读失败，再看重新使能时间；低电/插拔外电时比对实际电流、VREG、SOC及UI，不能只看VBUS码。
5. **已有资料的故障恢复先用可丢弃副本**：证明首次空区初始化与已用LittleFS损坏的处理不同；不在真实用户区直接做格式化试验。
6. 浅睡保持关闭，直到SD事务/句柄归还、CNNT/屏总线停放、按键/RTC/运动/模组逐源唤醒和SD→USB恢复链补齐；再测定时切片/取消入睡和长按关机。P14低时长及各电平以原厂值为核对标准，不自行猜参数。
7. 相机/声音：首次探测失败后重探、显示竞争、拍照中取消、录放并行Flash操作、BH丢ACK后的首个请求；测真实DMA溢出和播放间隙，60 ms只作预算。
8. 记录内部DMA可用量/最大连续块，在SD、网络、音频同时初始化时对照原厂65536保留与当前32768。RTC另核对STOP、VL、非法日历及变更时区后的离线启动/闹钟。

原厂对应实现本次均已读到；以上不确定项是本项目行为/故障条件的实机缺口，而非凭空补原厂参数。

## 没做到的

- 按审计任务书要求，不运行全量测试、ASan、S31 编译和 gcccheck，不修改代码。不得据此声称通过运行或实机验收。

## 和设计不一致的

- C03原厂明确不用未经标定SOC，我们直接读取；C10/C19少原厂启动清理与共享轨复位；C07/C08睡眠硬件流程不完整/混用。没有本次实机证据为这些不一致背书，不能按“等价”验收。C04使用VL时间有已有单机试用观察，但未验证停振/损坏场景。
- 物理分区与既定试用布局一致；原厂NVS不写、font_data重用及原厂UI不照搬均有项目约束（`docs/hardware/flash-layout.md:3-23`），不建议为了抄原厂而取消这些边界。
- C23是原厂有而我们未接的硬件能力，是否在既定用户功能中需要它待主会话确认；没有自行取消。

## 改过的已有测试

无。

## 要用户定的

本次审计无阻塞决定，按只读范围完成。后续需要明确：已用LittleFS损坏是否必须保留供恢复（推荐保留并提供显式恢复）；RTC在VL时怎样向系统标记未可信（推荐保留标记、等可靠时间源）；是否需要OTG供电以及相机重探入口。均未修改需求或替用户作取舍。SDK候选尚需故障证据，不能仅靠用户选择认定为根因。

## 顺手发现

- 原厂同样缓存相机首次负探测、同样关闭FreeRTOS整库IRAM；照抄不能自动消除C17/C22。
- 原厂FatFS代码页936，我们437（但两边API均UTF-8）；这是文件名兼容性线索，未纳入稳定性根因，也未扩展修复。
- 64 KiB/最多8任务的coredump不是全系统现场保证；故障归因不能依赖15秒后一次IRQ表替代故障时记录。

## 软总线、原子能力与AI边界

【实现】`system/services/camera/camera_service.c:66` 注册 `camera.capture`，`system/services/power/power_service.c:405-410,466-467` 提供电量/电源状态与关机/省电能力，音频服务 `system/services/audio/audio_service.c:675-678,802-811` 有状态和设置。复用原厂只能替换这些服务下的硬件路径，保持按钮与AI同路、成员/锁屏策略及HAL隔离。未做全系统能力覆盖验收，也不把原厂MCP工具/页面搬来替代本项目软总线；本次未新增可调用行为。

## 要改的文档

由合并主会话将C03、C07/C10/C19、C22证据同步到 `docs/tasks/backlog.md` 与相关硬件/端口记录；避免重复已有“浅睡未接SD/总线”“RTC固定时区”“切槽非整片恢复”条目。后续实际复用驱动后再更新来源版本/许可证和硬件对照；本次不修改共享文档。

## 新增的机制

无。产品代码改动 0 行。

## 最终检查

【实测】E1/E2的两段原样复核命令均退出0；睡眠原文/适配、两份586对相机表、九项分区、许可证与留存基线ELF调用地址可复核。报告23个模块编号唯一；113处显式源码行号均核对文件存在及行号范围；本地文档链接无失效。

- 全量测试结果原话：**未运行（本任务总说明明确“不跑全量测试和ASan”）**。内存检查：未运行；S31：本次未编译，只读留存基线构建；gcccheck：未运行。没有产品修复，故没有反向撤修对照；没有修改已有测试期望。
- `git diff --check 87bf4865..HEAD` 退出0。`git diff --stat 87bf4865..HEAD` 与 `git diff --name-only 87bf4865..HEAD` 仅含本目录 `REPORT.md`、`EVIDENCE.md`；没有混入产品/测试/构建改动或构建产物。产品代码0行，新增机制0项。
- 基线到HEAD的隐私扫描匹配0行，等价grep退出1（无匹配），不含禁止提交的本机路径或设备地址。扫描模式以字符类写路径分隔符，避免把检查命令本身当泄露：`git diff 87bf4865..HEAD | grep -nE '[/]Users/|[/]private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'`。
- 原厂克隆HEAD仍为 `690a023c66862af103a3187464373ee7c5efcd14`。在独立任务分支逐里程碑本地提交，没有push或创建PR。
