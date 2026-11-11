# driver-audit-1b：屏幕与输入驱动差异审计

审计日期：2026-11-11。基线：`87bf48655ee4820837b0f54a43c8f5aed37278c2`。

状态：源码审计完成，交付核查见文末。只读源码，不改产品代码、测试及构建文件，不操作设备。

证据标记：【实现】为源码核查；【实测】仅表示本次在电脑上执行命令所得结果，不代表真机验证；【估算】为尚待验证的风险和改动量。按总任务说明，不运行构建、全量测试、ASan、S31 编译和 gcccheck。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1：全部模块与流程、双方出处 | 完成【实现】 | 模块表 18 行，双方文件与行号；[屏幕全部差分和迁移说明](DISPLAY-CHANGES.md) | `nl -ba`、`git show`；附录覆盖 LCD 源 22 个、头 2 个差分块，逐字节核对 3 个未变文件 |
| 2：前十风险、直接拷贝清单 | 完成【实现／估算】 | 下方前十风险、9 项复用清单及上机清单；风险不冒充真机故障 | 源码交叉核查、相间时序整数算式复算 |
| 3：仅新增本任务目录文件 | 完成【实测】 | 仅本目录 2 个 Markdown 文件；产品、测试、构建修改均为 0 | `git diff --name-status`、`git diff --stat` 与基线比较；收尾核查见下 |
| 补充：原厂主分支是否更新 | 完成【实测】 | `main=874538e`，compare identical，无更晚公开源码 | GitHub commits/main 与 compare API；下方来源链接 |
| 构建／测试豁免 | 按任务未运行 | 全量、ASan／UBSan、S31 编译、gcccheck 均无执行结果，不写通过 | `docs/tasks/driver-audit-1.md` 开头明确禁止本轮运行 |

## 来源与复现约定

- 本项目行号均取指定基线。`U:` 表示原厂 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7` 的 `main/boards/metalio-e-ink4-plus/`；`U0:` 表示同路径的 `690a023c66862af103a3187464373ee7c5efcd14`。用 `git -C "$INK_UPSTREAM" show 874538e:<路径>` 配合 `nl -ba` 复核，不移动原厂 HEAD。
- 【实测】GitHub 公开 `main` API 返回 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，提交日期 `2026-11-07T11:32:56Z`，标题 v2.0.77；compare 返回 `identical`、ahead/behind 均为 0。网页烧录列表的 2.0.78 不等于已公开源码。来源：[main 提交 API](https://api.github.com/repos/CloudZao/Metalio-E-INK4-Plus/commits/main)、[指定提交](https://github.com/CloudZao/Metalio-E-INK4-Plus/commit/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)，查阅 2026-11-11。
- `V:` 表示同一原厂新提交的仓库根目录（用于 `main/display/`、`main/boards/common/` 和 `sdkconfig`）。ESP-IDF 核查版本：`fff9895c82d744c7237be8847347bdd1b07c6643`。下文 `IDF:` 为该检出下的相对路径；配置默认值不等于本次检查过最终 ELF/map。
- 表中省略目录的本项目 `hal_*_s31.c`、`s31_*.c/.h` 在 `ports/esp32s31/`；`metalio_epd_power.c`、`metalio_board.c` 在 `boards/metalio_plus/`；`ft6336u.c`、`tca9555.c`、`sc7a20h.c` 在 `drivers/chips/<同名芯片>/`。上游省略前缀的 board 相对路径沿用 `U:`。

## 模块记录

以下事实均为【实现】，硬件后果为条件性风险，未在本次上机复现。

| 模块 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑（场景及后果） | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| 屏幕输出／完成与中止 B01 | `U:display/output_lcd/lcd_driver.c:247–283,564–641,1263–1299`：LCD/GDMA 同核；完成先屏蔽 EOF；中止在 IRQ 核等待旧 ISR 退出、reset 并清 pending；中止不回调完成 | `ports/esp32s31/display_upstream.cmake:2,19–23` 固定旧版；`hal_display_s31.c:67–71,190–204` 先清回调再 abort，成功后直接重置队列 | 我们缺：新输出层的同步与 EOF 清理 | 超时／guard 中止或连续相切换时，旧 EOF 可能继续读旧队列、补旧 bounce，或污染新帧；旧 abort API 本身会走完成回调；当前 HAL 先清回调避免了直接伪通知，但没等已进入的 ISR 退出。当前初始化通常同核，不能把跨核风险当作每次必现 | 高 | 直接取新版 `lcd_driver.c/.h` 与原版队列；HAL 改为先在任务中 abort 返回，再清回调／复用队列，见迁移说明 |
| LCD 中断与 IRAM B02 | `U:display/output_lcd/lcd_driver.c:915–926` 无条件 `ESP_INTR_FLAG_IRAM`；`247–265` 完成路径调 `gdma_stop`；原厂 `V:sdkconfig:2170` 显式开启 `GDMA_CTRL_FUNC_IN_IRAM=y`（旧版配置也开启）；多批路径调 `start_ckv_cycles:340` 未显式标 IRAM | 旧版同类调用仍在；`ports/esp32s31/sdkconfig.defaults:1–42` 漏掉该配置，IDF 默认 n；`hal_display_s31.c:147–164` 回调标 IRAM 不改变 GDMA 控制函数放置 | 我们缺：原厂配套 IRAM 配置；源码入口标记相同不足以等价 | 若缓存关闭时恰好结束一帧，按本项目默认值在 Flash 的 `gdma_stop`／HAL 调用可能触发 cache error；当前 684 行不会进入大于 1000 行的 CKV 续批分支，后者不列作当前必现故障 | 高 | 复用原厂源码及配套配置，核对最终 map、控制函数及其依赖；未编译前不能断言实际 ELF 放置。D-36 互斥仍须保留 |
| 送行队列／DMA 内存 B03 | 两版本 `display/output_common/line_queue.c` 完全相同：`26` 用普通 `calloc` 分配指针表，`32` 行缓冲显式 INTERNAL；`lcd_driver.c:818–847` bounce/描述符为 INTERNAL\|DMA | `hal_display_s31.c:342` 两条 128 项队列；`sdkconfig.defaults:10` 普通分配仅 16 字节以内优先内部内存；指针表约 512 字节／条 | 一样：行数据内部，不保证指针表内部；新版也未修 | IDF 默认分配策略优先把该指针表放 PSRAM（`IDF:components/heap/heap_caps.c:107–124`）；若将 GDMA ISR 改成 cache-safe，却只检查行缓冲，会在 `lq_read:75` 访问外存指针表 | 中 | 原厂队列可直接复用，但其 ISR 可达元数据须显式放内部内存；该必要安全适配不能靠单改 IRAM 配置替代 |
| 扫描几何／CKV B04 | `U:display/output_lcd/lcd_driver.c:212–220,787–805,1136–1151,1328–1335`：S31 原生高度、0 dummy byte、精确 RMT tick，SPV 后省一行等待 | 固定旧版会把 684 向上补成 688，8 位总线每行加 1 dummy byte，周期取整微秒；`hal_display_s31.c:335–339` 参数可启用新版原生扫描分支 | 我们缺 | 首尾像素错位、额外扫描和 gate/source 相位偏差；换版后物理驱动时长会变，不能直接声称 PQ-BN 的灰阶标定仍等效 | 高 | 拷新版输出传输；保持本项目 PQ-BN/HAL 接口，核对相间节拍与原厂 SPV 时间戳，逐项上机验证 |
| TPS65185 寄存器与上电 | `U:bus/tps65185.c:15–75` 地址 0x68、VCOM 10 mV 步进；`display/epd_board_metalio_eink4_plus.c:811–978` WAKEUP→50 ms→REVID/UPSEQ E4/55→PWRUP/VCOM→50 ms→GPIO PG→ENABLE 3F/VCOM→PG FA→XOE；失败最多重试三次后重启 | `drivers/chips/tps65185/tps65185.c:3–29`；`boards/metalio_plus/metalio_epd_power.c:45–126` 主时序和数值一致，额外检查 P05、超时重试；失败关屏并停驱动，没有整机重启 | 我们重写但主流程等价；错误策略我们不一样（本次无真机证据）；读事务不同，见 I2C 行 | repeated START 代替原厂 STOP 分隔，PMIC 超时 50 ms 代替 1000 ms；设备／总线暂忙时可能被判断为配置失败。不能把原厂的“200 次 PG 轮询”称作 200 ms：HZ=100 时 `pdMS_TO_TICKS(1)` 为 0 | 中 | 拷原厂 `bus/tps65185.c/.h` 与板级 `poweron_once/poweroff` 原文，HAL 只转换句柄／错误码；保留有明确安全目的的错误返回，重启策略单列决策 |
| 屏幕关电／保护 B05 | `U:display/epd_board_metalio_eink4_plus.c:218–244,986–1001` 输出有 desired/confirmed/valid，写失败不认为已确认；XOE 先低至少 12 µs，再 PWRUP/VCOM 低→一 tick→WAKEUP 低；`V:main/display/epd_flash_guard.h:5–18` 是 Flash 互斥，并非停扫看门狗 | `metalio_epd_power.c:18–22,129–136` pins 只计错误；`hal_display_s31.c:63–73,92–98,119–145,347–350` 独立 2 s guard、自测；关电后无条件 `powered=false` 并撤防；panic 见 `ports/esp32s31/s31_panic.c:31–61` | 我们不一样（guard 自测有历史上机说明，本次未复验）；失效处理弱于已确认状态 | 运行中 I2C 写失败时，XOE/MODE 已拉低但高压轨未必关闭；软件却撤防，不再据硬件确认。panic 位操作也是 best effort，固定地址 0x20，不能宣称断轨得到保证 | 高 | 拷原厂已确认输出写入及断电原文，保留本项目保护接口和自测；关电结果须如实反映失败。无原厂等效 guard 可整块替换，保护仍需单独验收 |
| 屏幕 Flash／相机互斥 | `V:main/display/epd_flash_guard.h:5–18` 整个波形共用可重入锁；`U:display/output_lcd/lcd_driver.c:818–847` bounce 和描述符内部 DMA 内存 | `system/services/display/display_service.c:296–306`→`hal_display_s31.c:405–415`；`system/services/storage/storage_service.c:342–345,514–522` 包读写；`ports/esp32s31/hal_camera_s31.c:158–166` 同一显示访问门 | 我们重写但所查入口等价；锁不是原厂递归锁，接口契约不同 | 正常经服务的 Flash 操作已串行，不能把 B02 直接说成此次“屏幕空闲时核 1 Cache error”的根因。没证明所有 IDF／第三方 Flash 入口都受门保护 | 中 | 保留 HAL 门接口，原文复用 `EpdFlashGuard` 的互斥范围；对 Flash 入口做后续专项核查，不能因换新版输出就删 guard |
| FT6336U 初始化／复位 B06 | `U:display/metalio_touch.c:273–285` 初始化先复位；`display/epd_board_metalio_eink4_plus.c:615–627` 在共享复位互斥内 P12 低 10 ms、高后等 300 ms | `ports/esp32s31/s31_board.c:78–98` 只初始化扩展器、等电源稳定 80 ms 后单次读 ID／版本；`drivers/chips/ft6336u/ft6336u.c:9–18` 没有复位与就绪等待，失败后将触摸指针清空，本次启动不再恢复 | 我们缺 | CPU 软重启而触摸仍留在异常／Bootloader 状态，或上电尚未准备好，首次 ID 读取失败即整个启动无触摸／盖板键。既有记录只证明曾读到 ID 64、FW03（`docs/hardware/2026-11-10-selftest.md:30`），没有证明这些复位条件 | 中 | 原文复用原厂复位与初始化，在其它共享 P12 使用者启动前执行；不得在相机使用中随意补脉冲；保留 HAL 事件接口 |
| FT6336U 帧、坐标和采样 B07 | `U:display/metalio_touch.c:85–106` 可识别两种轴布局；`163–216` 读第一触点；`229–241` 即使空闲也每 20 ms 补采，按住每 10 ms；`131–159` 为 LVGL 保留短按回放 | `ft6336u.c:21–35` 两点、13 字节、按 ID；`ports/esp32s31/s31_input_logic.h:70–93` 先把 raw 判成 684×1216 竖屏／盖板坐标再旋转；`hal_input_s31.c:103–113` 空闲无定期补采，仅按住／错误后 10 ms；`s31_irq.c:40–48,73` 已有低线重查与启动补采 | 我们不一样（多点是扩展，没证据证明第二种布局等价）；缺原厂轴布局兼容与固定空闲补采 | 若固件实际报横屏 raw_x≥684，会在旋转前被当成屏外而丢弃；若确实丢 IRQ 且线已恢复高，空闲不再补采。GPIO2／5 仍低时已有补救，不能误报“所有丢沿都永久失效” | 中 | 直接复用 `metalio_touch.c` 的芯片采样、映射与暂停原文，只把发布结果适配 HAL；是否保留两点按 HAL 契约确认，不能照搬原厂第一点就删掉现有能力 |
| FT6336U 固件升级 B08 | `U:display/metalio_touch_upgrade.cc:33,38–53` 在采样前独占 I2C；`ft6336u_upgrade_protocol.h:44–134` 应用 ID 64/26 最多 5 次间隔 200 ms；AA→10 ms→55→80 ms；Boot ID 79/1C 最多 30 次；擦除后 500 ms，再读状态；32 字节包逐包状态／XOR，复位后 300 ms，最多两轮 | `drivers/chips/ft6336u/ft6336u.c:9–18` 只读版本，不升级；本项目无上述协议、固件或恢复流程 | 我们缺 | 版本 03 或损坏应用不会自动变成原厂 V04／恢复，不能宣称与最新固件同等；原厂 README 没写 V04 具体修复什么，不据版本号推断某次触摸故障一定能修 | 中 | 需升级时直接拷原厂升级 `.cc/.h` 与协议头，保留完整 ID、CRC、包状态、退出 Bootloader 和重试时序；本任务不执行任何芯片擦写；供应商固件授权／面板型号先确认 |
| TCA/PCA9555 方向、锁存、地址 | `U:bus/pca9555.c:54–98` 寄存器读写；`display/epd_board_metalio_eink4_plus.c:187–244,523–592,652–685` 输出与输入分开、写后确认、候选地址 20/24/21…27；正常方向 P0=C0、P1=E3；睡前全输出，键锁存为高；恢复后读双口清 INT | `drivers/chips/tca9555/tca9555.c:10–54` out→polarity→dir，单锁下修改 out，写失败 stale、下次读回输出寄存器；`boards/metalio_plus/metalio_board.c:9–28,57–86,116–127` 方向和关键电平一致，地址仅 0x20 | 我们重写但正常电平等价；失效状态与地址范围我们不一样（目标外的地址无本次真机证据） | timeout 后原厂保留 desired 并重新写，本项目只读回硬件，未自动保留上次失败的功能请求；不能把 out 当已证实的物理电平。地址非 0x20 的板整片扩展器不可用，panic 地址也要同时匹配 | 中（目标板地址差异为低） | 直接拷 `pca9555.c/.h` 与原厂板级输出合并、初始化／睡眠恢复段；只保留 `metalio_board_*` 接口和本项目 RTC 极性等已测适配，不把输入口用于输出读改写 |
| GPIO IRQ、消抖、长按与电源键 | `U:display/epd_board_metalio_eink4_plus.c:93–105` ISR 只通知；`input/metalio_keys.cc:24–27,81–97,147–171,226–245` BOOT 双沿、30 ms 防抖、50 ms 兜底采样、POWER 3 s 发关机请求；唤醒恢复等 20 ms；`config.h:118–121` P13 15 次、100 ms 高低脉冲 | `ports/esp32s31/s31_irq.c:29–33,53–74` 所有线下降沿、非 cache-safe GPIO 服务、低线重查；`hal_input_s31.c:67–113` BOOT 按住仍轮询；`s31_input_logic.h:38–46` 全键掩码稳定 20 ms；`system/services/input/input_service.c:21–23,175–224` 1.5 s 电源菜单；`metalio_board.c:153–158` 关机脉冲数量／时长相同 | 我们重写但电气／中断任务化相符；防抖与空闲补采不同；长按为产品策略差异 | 多个键在 20 ms 内交替变化会重置整组稳定时间，原厂按键独立节流；没有原厂 50 ms 空闲兜底。未证明实际会漏键。GPIO handler 标 IRAM 不代表注册为 cache-safe：这里 `gpio_install_isr_service(0)`，不能列作 Flash 中继续跑的 IRAM ISR | 中 | 直接复用原厂独立采样／电气防抖和恢复段接 HAL 边沿；保留输入服务长按→电源菜单及能力调用，禁止直接拷原厂 3 s 自动断电覆盖项目 UI 合同 |
| 触感马达 B09 | `U:input/metalio_vibe.cc:16–17,64–78,136–180` 20 ms 软 PWM，强度 0/1/2/3 对应 0/70/90/100% 占空比，脉冲由 timer 结束，可持续；`V:main/haptic_feedback.cc:23–25` 用户反馈另选 35/50/70 ms | `ports/esp32s31/hal_input_s31.c:56–64,153–162` P03 整段全开，由输入任务约 10 ms 轮询关电，请求只延长；`system/services/audio/audio_service.c:1237–1240` 保留 35/50/70 ms 三档但不调占空比；`metalio_board.c:93–104` 独立 500 ms 放电脉冲 | 我们不一样（脉宽相同不等于马达驱动相同）；缺软 PWM／连续驱动接口 | 同名“弱”反馈仍全功率，触感、声响和耗电不能按原厂推定；输入任务被 I2C 卡住时停止输出会晚于请求时间。原厂 timer 回调也经 I2C，不能声称它保证硬实时 | 中 | 直接拷 `metalio_vibe.cc/.h` 驱动引擎，HAL pulse 接 PulseFor；保留系统设置、AI 录音时抑制反馈和放电独立于用户设置的策略，不再按另一套 PWM 重写 |
| 前光 B10 | `U:periph/frontlight.h:44,159` 默认 20 kHz；`.cc:17–24,246–278,311–323` 双路 10 bit、最大输出 10%，混色每路 `min(100,2×比例)`；每 5 ms 亮度变化 1%，关灯不拆 LEDC 矩阵 | `ports/esp32s31/s31_board.h:36–39` GPIO6/7、20 kHz 相同；`s31_board.c:64–73,169–180` 10 bit，但 1000 permille→1023；`system/services/display/light.c:20–33,49–55` 总亮度线性分给两路、已有 5 ms 渐变和保持 LEDC 的关灯 | 我们不一样（最大占空比和混色均无本次真机依据），频率／引脚／分辨率一样 | 亮度 100、暖光 0 时我们冷路 1023、原厂 102；暖光 50 时我们每路 511、原厂每路 102，分别约 10 倍和 5 倍，占空比与功耗／观感基准不符；不据此断言硬件一定过流 | 高 | 原文拷 `frontlight.cc/.h` 的硬件输出映射与混色段接 `hal_light_set`，系统设置与渐变继续归服务；避免把 NVS／UI 整块搬进 HAL |
| SC7A20H 正常采样／共享中断 B11 | `V:main/boards/common/sensor/sc7a20h.cc:21–36,53–96,170–180` CTRL 57/01/40/88/08/03、AOI 2A/10/00，±2g 高分辨率、右移 4 为 mg，AOI1_SRC 清锁存；`U:periph/metalio_accel.cc:86–96,104–117` 地址 0x19 后 0x18，初始化失败仍有 SilenceSharedInt 兜底 | `drivers/chips/sc7a20h/sc7a20h.c:14–47` 正常配置、换算和清锁存相同；`ports/esp32s31/s31_board.c:46–61,86–89` 仅 0x19，失败置 NULL；`hal_input_s31.c:96–99` GPIO2 同时清 sensor 和 P0/P1 | 我们重写但正常模式等价；缺备用地址及“不 ready 仍释放共享线”；严格 ID／错误返回优于原厂 best effort，不能删 | 若实际地址 0x18 或初始化中途失败却已打开 AOI1，后续指针清空便不再清 INT；GPIO2 可能常低，侧键后续变化无新下降沿，只能靠低线回查维持采样，睡前也可能因线不空闲而拒睡 | 中 | 拷 `common/sensor/sc7a20h.cc/.h` 和 `metalio_accel.cc` 的地址探测／共享线收尾，HAL 保留读数与运动计数，保留本项目明确失败状态，不照搬 Configure 失败仍 ready 的漏洞 |
| SC7A20H 休眠／唤醒 | `V:main/boards/common/sensor/sc7a20h.cc:90–105` 关闭 AOI、路由／锁存，读源，再 1 Hz 轴关、CTRL6=0；`U:periph/metalio_accel.cc:47–53,104–117` 睡前释放、需要时重新 Begin | `sc7a20h.c:42–55` quiet 路径寄存器基本一致，源读取移到调用方；新增 10 Hz 低功耗、阈值 08、持续 01 的 motion_watch；`s31_board.c:53–61` 恢复重设正常模式；`hal_power_s31.c:145–155,197–198` 睡前清源／查共享线 | 我们不一样（motion_watch 无原厂对应实现、无本次真机证据）；正常静默重排可追踪 | 【估算】100 ms 采样／持续门槛未校准，可能漏抬起或被震动误唤醒；阈值不是已验证指标。`ports/esp32s31/CMakeLists.txt:6` 浅睡默认关闭，不能把这些分支说成当前试用版正发生的故障 | 中 | 正常模式／静默流程直接拷原厂；保留 HAL 的运动唤醒契约，原厂没有的低功耗参数只列为待上机核对，不能冒称已完成或未经选择删功能 |
| I2C 总线、超时与恢复 B12 | `U:bus/metalio_i2c1.c:49–107,110–164,203–207` 外层总线锁、Flash 前 drain 并静默中断，启动 bit-bang 恢复（最多 9 个 SCL、5 µs 边沿与 STOP），100 kHz、16 设备、glitch=7／内部上拉；每设备 `scl_wait_us=10000` | `ports/esp32s31/hal_i2c_s31.c:20–57,61–77` GPIO0/1、100 kHz、16 设备、glitch／上拉相同；仅创建设备持锁，传输依赖 IDF 单事务锁；没有 Flash drain、引脚级恢复或显式 SCL stretch 预算；`s31_board.h:16` 传输一律 50 ms | 我们缺：原厂总线护栏和 10 ms clock-stretch 配置 | IDF S31 默认 stretch 为 2500 µs（`IDF:components/esp_hal_i2c/esp32s31/include/hal/i2c_ll.h:88`、`esp_driver_i2c/i2c_master.c:1194`），原厂特意兼容同总线电量计大于 4 ms 拉伸；故可能共享总线超时，进一步拖慢键／触摸／guard。IDF 自有 FSM／clear-bus（同文件:56,111,550,598），不是“完全没有恢复”；缺的是原厂启动引脚级恢复和 Flash 协调 | 高 | 直接拷 `metalio_i2c1.c/.h`，HAL 仅映射 bus 0→I2C1、设备和错误码，复制原有 Flash drain 接入点，不另写总线恢复算法；50/100/1000 ms 应保留各设备原厂预算 |
| I2C 寄存器读语义 B13 | `U:bus/pca9555.c:62–63`、`bus/tps65185.c:21–22` 调 `metalio_i2c1_read_reg_stop_locked:257–262`，在同一总线锁内写寄存器→STOP→读；传感器 `V:main/boards/common/power/power_i2c.c:40–60` 先 repeated START，失败后间隔 100 µs 再试 STOP 分隔；触摸使用 repeated START | `hal_i2c_s31.c:72–77` 统一 combined transmit_receive；`tca9555.c:31–32,52–54`、`tps65185.c:5–10`、`sc7a20h.c:15–16,25–38` 全部如此；TPS 仅重复 timeout，不切换读语义 | 我们不一样（目标芯片对此等价无本次真机依据） | 单把 50 ms 加长不会恢复原厂 STOP／fallback 语义；在对 repeated START 不兼容／偶发失败的芯片状态下，PMIC／扩展器／传感器仍可能读失败。不能把原厂存在 STOP 就当成已经证明本机 repeated START 必坏 | 中 | 原文复用原厂各芯片读 helper 和共同总线锁；HAL 接口如需表达 STOP 分隔，单列最小适配，禁止简单把统一接口改成两次无共同锁的调用 |

### 屏幕输出核查补充

- `epdiy.c` 与 `line_queue.c/.h` 在两个提交之间无差异。`highlevel.c:125–129` 的全屏更新不再受 dirty 掩码限制；`206–250` 新增黑色均衡模式全屏处理，防止“图没变就不刷”跳过黑色补偿。本项目用 PQ-BN，不编这些高层文件，不能把它们记成本项目已获得的修复。
- `gdma_stop` 的证据是 `IDF:components/esp_driver_dma/Kconfig:3–9`（默认 n）、`linker.lf:9–13,46–72`（控制函数与 HAL 条件放 IRAM）、`src/gdma.c:687–701`（调用 HAL）。未编译，最终放置待 map 核实。
- GDMA 的缓存安全与 LCD 是两回事：原厂 `lcd_driver.c:593–597` 仅在 `CONFIG_LCD_RGB_ISR_IRAM_SAFE` 下设 `isr_cache_safe`；IDF `src/gdma.c:1132–1134` 才据此加中断 IRAM 标志。入口写 `IRAM_ATTR` 不自动让中断在 Flash 操作期间运行，也不证明数据可访问。

## 最可能出事的前十个坑

按影响、触发条件和本次证据强弱排序；代码差异已核实，后果仍需真机验证。B 编号指上表及屏幕差分附录。

1. **B01／高**：超时中止或相切换后旧 EOF 仍可能补数／读队列，本项目没有原厂新版屏蔽 pending 和等待 ISR 退出的边界，会使下一帧花屏、欠载或误处理完成。
2. **B02／高**：Flash 关闭缓存时若撞上 LCD 完成中断，项目 defaults 漏掉原厂 GDMA 控制函数入 IRAM 配置，存在取 Flash 指令的条件；最终配置／map 未编译核实。
3. **B05／高**：保护关电遇 I2C 写失败，软件仍撤销 powered／armed，而高压轨可能尚未关闭；XOE 拉低不等于高压已断。
4. **B12／高**：共享 I2C 的默认 SCL 拉伸预算约 2.5 ms，未复制原厂 10 ms 配置，同总线从机超过 4 ms 拉伸时可能超时，牵连输入和关屏。
5. **B04／高**：旧版每行占位／688 行扫描／SPV 额外等待不同于新版原生时序，搬到新版后不核对 PQ-BN 物理节拍，可能换来像素错位或灰阶变化。
6. **B10／高**：亮度拉满时输出占空比达到原厂同设置的约 5～10 倍，原厂的观感与耗电结论不再适用。
7. **B11／中**：加速度计初始化中途失败或板上用备用地址时，不 ready 就不清共享 INT，GPIO2 可能持续低、侧键依赖重查，睡眠可能被拒绝。
8. **B06／中**：软复位或触摸未就绪时，缺原厂复位／300 ms 等待，单次 ID 读失败就整次启动无触摸和盖板键。
9. **B13／中**：把原厂 TPS／TCA 的 STOP 分隔读及 sensor fallback 统一替换为 repeated START，遇不兼容状态时重试同一读法不能恢复。
10. **B07／中**：触摸固件如报原厂支持的横屏轴布局，当前在旋转前就判屏外，部分触点会被丢；本机是否用该布局未证实。

B03 的外存指针表在开启 GDMA cache-safe 后才成为直接禁用缓存风险；B08 的 V04 缺失、B09 的全功率触感及运动唤醒校准仍见模块表，没有因未进前十而判定无需建设。

## 直接拷贝清单

优先取 `874538e` 的原文，保留 CloudZao 根 [MIT 许可证](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/LICENSE)、两条版权及来源提交；未见此处需要引入 GPL 的理由，不从供应商 Linux GPL 驱动移植协议。下表为后续实施量【估算】，按源文件／函数范围估计，未做实现。本次复制进产品的代码仍为 0 行。

| 优先级 | 拷原厂哪些原文 | 我们保留的接口／策略 | 估计改动量与边界 |
| --- | --- | --- | --- |
| P0 | `display/output_lcd/lcd_driver.c/.h`、`output_common/line_queue.c/.h` 及匹配的传递头；原厂 `sdkconfig` 的 GDMA IRAM 配置 | `hal_display_*`、PQ-BN 计划／行转换、guard／panic 对外入口、显示访问门 | 原文约 1.5k 行，胶水约 15～35 行、版本／配置约 5～15 行；须按 [完整迁移清单](DISPLAY-CHANGES.md#hal_display_s31c-升级到新版的具体适配) 处理 abort 合同，不移动公用上游 HEAD |
| P0 | `bus/metalio_i2c1.c/.h`，`pca9555.c/.h`／`tps65185.c/.h` 的原厂读 helper，`common/power/power_i2c.c/.h` 的 fallback | HAL bus 0→IDF I2C1、设备句柄／错误码，现有服务调用方式 | 总线原文约 300～350 行，胶水约 20～40 行；Flash drain 接原有存储入口，STOP 分隔读必须共同持锁 |
| P0 | `display/epd_board_metalio_eink4_plus.c` 的输出锁／确认状态、初始化、TPS on/off、mask／restore、P12 复位及相关头定义；`bus/tps65185.c/.h` | `metalio_board_*`、`epd_power_*`、安全返回及本机已测的 RTC 极性；guard 继续单独验收 | 原文约 500～700 行，胶水约 20～50 行；共享 P0/P1 只留一个所有者，不在调用方各补时序 |
| P0 | `periph/frontlight.cc:246–278,311–323` 的硬件初始化、10% 输出上限和混色，及关联定义 | `hal_light_set`、系统 `display.light/warmth` 设置与现有渐变 | 原文／关联定义约 60～100 行，胶水约 10～20 行；不重复引入原厂 NVS 或第二套渐变定时器 |
| P1 | `display/metalio_touch.c/.h` 的复位初始化、寄存器采样／映射／暂停逻辑，板级共享复位段 | `hal_input` 边沿、系统手势／成员策略、多点 HAL 契约；LVGL 仍只在 UI 层 | 原文相关段约 250～400 行，胶水约 20～50 行；不能把原厂 LVGL 短按回放直接叠加到已有事件队列 |
| P1 | `common/sensor/sc7a20h.cc/.h` 与 `periph/metalio_accel.cc` 的探测、配置、读数和静默恢复段 | `hal_accel_read`、运动计数／唤醒配置、严格错误返回；自动旋转的产品策略 | 原文相关段约 200～300 行，胶水约 15～30 行；不复制 Configure 失败仍 ready 的宽松语义；运动低功耗参数单独验证 |
| P1 | `input/metalio_keys.cc/.h` 的独立按键采样、消抖和恢复，板级 P13 关机脉冲原文 | HAL key edges、系统 1.5 s 菜单／能力调用、锁屏与音量语义 | 原文相关段约 120～200 行，胶水约 15～30 行；原厂 3 s 直接断电不覆盖本项目产品策略 |
| P2 | `input/metalio_vibe.cc/.h` 引擎及板级 motor_set | `hal_haptic_pulse`、反馈开关／三档脉宽、AI 采音抑制、放电脉冲不受普通触感设置影响 | 原文约 220～260 行，胶水约 10～25 行；复用原厂驱动定时器，不另写 PWM 引擎 |
| P2（待选择） | `display/metalio_touch_upgrade.cc/.h`、`ft6336u_upgrade_protocol.h`；固件只在授权／面板确认后单独处理 | 显式受控升级入口或经确认的启动流程，HAL 采样接口 | 协议／适配原文约 200 行、镜像 47176 字节；胶水约 10～25 行，不是本轮设备写入许可 |

复制较大文件的原因是保留原厂已跑过的成套时序、状态及取消边界；上表不授权顺带更换 PQ-BN 或产品交互，也不要求本任务做任何大规模实现。

## 上机先核对清单

本任务未碰设备；以下留给后续获准的实机任务，先确认错误可观测，再宣称驱动通过。

- 新构建的 sdkconfig／map：LCD、GDMA 实际核号和优先级；`gdma_stop` 及 HAL／RTOS 依赖的段；队列指针表、bounce／描述符的内存区域。不能只检查 callback 入口。
- 屏幕首末像素、原生 684 行、CKV 高／低、XCL/XLE/XSTL 与 SPV 的对齐，以及实际 SPV 相间隔；比较同一灰阶图、相同温度的 PQ-BN 输出。16.9 µs 是既有参数源码复算【估算】，未量引脚。
- 普通更新、连续相、自然欠载／超时和取消后重试：旧回调不能在复用之后消费新队列，abort 不报正常完成。不要为审计人为卡线程。
- 上电与关电：GPIO PG 和寄存器 PG 的先后；XOE 先低至少 12 µs；I2C 失败时如实保留“未确认关电”，不能把 NACK 或浮高 PG 当成物理断轨证明。
- 共享 I2C：保留 100 kHz，核对 10 ms 拉伸；软重启时 SDA 挂低后的原厂恢复是否奏效；核对 TPS／TCA STOP 分隔和 sensor fallback；Flash 前 drain 是否覆盖所有合法入口。
- 触摸：P12 共享对象与互斥、10 ms 低／300 ms 高后等待；冷启、软重启的 ID／版本就绪；两种轴布局、盖板坐标、两点同时及短按；空闲漏沿／INT 已低的恢复。
- V04 固件：先核面板 `LT0470GF0430M_V1.1`、镜像大小／SHA256 和分发授权，再选择是否升级；没有原厂变更说明，V04 对具体触摸故障的效果仍未知。不可顺手刷写。
- 前光 0／50／100 亮度、冷暖两端与中间：比较 duty 和实测电流／观感；先复用原厂 10% 上限，不能把“频率相同”当等效。
- GPIO2 两个中断源都能释放；加速度计初始化失败、备用地址和低功耗唤醒阈值【估算】；本机 RTC 极性已测差异保留，睡眠启用状态必须注明。
- 马达弱／中／强的驱动占空比与结束延迟；放电 500 ms 独立于反馈开关，普通反馈不篡改共享电源复位的放电过程。

## 没做到的

- 按任务边界不做真机验证、修复、构建及测试；不据源码审计宣称硬件通过。
- 无最终 ELF/map 的证据，所以 B02 是默认配置遗漏与条件性运行风险；没有把源码标注或历史测试当成本轮实机证明。
- 未拿到 2.0.78 源码／具体触摸 V04 修复说明；原厂没有的运动唤醒参数列入上机清单【估算】。未检查任务 1a／1c 所属蜂窝、完整睡眠或其它驱动，不重复本任务总说明中已修的 4G 问题。

## 和设计不一致的

- **相对原厂**：输出传输版本／abort 合同、GDMA IRAM 配置、前光输出上限／混色、总线 STOP／stretch／恢复、触摸复位与固件升级、sensor 地址兜底、马达 PWM 均见表。除正常寄存器／电平相同部分外，没有本次真机证据支持偏离原厂；不改设计文件来迁就实现。
- **产品策略与原厂不同且有本项目依据**：电源键长按打开菜单（`docs/product/system-ui-spec.md:125–132`），不照搬原厂 3 s 自动断电；保持 4G 在线的睡眠策略是 D-37；RTC 极性差异是板级头中标出的历史实测。本次不重新判定产品范围，也不删这些能力。
- **源码注释与证据边界**：`hal_display_s31.c:1` 仍写 Compile-only，而仓库已有试用记录；该注释不能当成没有上机或已全部验证的证据。

## 架构与 AI 原生审视（R-061）

- 划分：芯片寄存器／电气时序归驱动及板级，HAL 暴露设备接口，长按菜单、反馈开关和渐变归系统服务。复用原厂不能把原厂 UI、NVS、自动断电策略一起塞进 HAL；当前共享 TCA 单实例／锁边界有合理依据，重点是恢复原厂底层行为而非增建抽象层。
- 可调用路径：【实现】亮度／暖光定义于 `system/services/display/light.c:77–81`，反馈设置在 `system/services/audio/audio_service.c:803–804`；触控 slider 经 `system/ui/shell/navigation.c:999–1002` 调 `settings.set`，能力注册与类型契约在 `system/policy/settings.c:418–431`。AI 与按钮已有同一路径；设置页快照 `navigation.c:1698–1706` 可见这些数值。
- 对 AI 的边界：所查用户设置已有能力入口；芯片 raw I2C／DMA／复位和触摸固件擦写是内部机制，不因“AI 原生”直接开放。传感器 raw 数值目前由电源服务内部读取（`system/services/power/power_service.c:242–244`）；本任务未查到面向用户的 raw 数值页面，不据此新造一个能力或宣称全系统能力覆盖已审完。

## 改过的已有测试

无。

## 要用户定的

1. 触摸固件：建议先采用**单独受控升级**，核面板／授权／镜像后再执行；另一个方案是原厂式每次启动自动检查并升级。当前保守做法为只报告缺项、维持设备现状，不能把“没升级”解释为首版不需要。
2. 屏幕换版后 PQ-BN 与原厂时序／85 Hz 的关系：建议先保留 PQ-BN 接口，仅迁原厂传输和取消边界并复核标定；若考虑完整换成原厂 render/highlevel，是另一项算法和体验选择。本审计不替用户决定。
3. TPS 多次失败：建议保留向上报告失败／关屏的接口；原厂重启整机作为另一个恢复策略。两者没有本次真机比较，不能在本轮以“等价移植”名义偷偷更换。

这些选择不阻碍源码审计交付，本轮没有任何依赖其许可的设备动作。

## 顺手发现

- 原厂不是所有路径都更稳：SC7A20H `Begin` 在 Configure 失败后仍返回 ready（`V:main/boards/common/sensor/sc7a20h.cc:164–167`）；队列指针表也没有强制内部内存（B03）。复用时保留本项目严格错误返回和必要安全检查。
- V04 原厂 README 说协议参考供应商驱动，但明确没有直接复制 Linux 驱动代码；触摸固件二进制来自供应商，本次未找到单独分发授权文字，不能只凭仓库 MIT 推断供应商镜像的授权范围。
- 仅查到公开主分支 v2.0.77；未取到已发布 2.0.78 的对应源码或变更说明，不能列它相对 874538e 的驱动修复。
- 【实测】项目公开 GitHub API 的 open issues／pulls 各为 0（各取最多 100 条，均为空）；本地任务、基线日志和现有报告已检查，没有重报任务总说明中已经修好的 4G 问题。

## 新增的机制

无。产品代码改动 0 行。

## 要改的文档

待主会话统一维护，本任务不修改：`docs/tasks/backlog.md` 录入 B01～B13 与受控迁移；`docs/system/ui-dependencies.md` 区分模拟等价与本次原厂差分；相关硬件／显示说明记录新版取消合同、原生时序与核查结果。`docs/requirements.md`、`docs/ideas.md`、`docs/decisions/` 和生成的 capabilities 文档均未动。

## 收尾核查

【实测】电脑上的文档／差分核查均退出 0：只新增本目录 `REPORT.md`、`DISPLAY-CHANGES.md`；当前差分的隐私规则匹配 0 项；本地 Markdown 链接可解析，表格列数一致；原厂检出 HEAD 前后均为 `690a023`。未新增机制、依赖、线程、定时器或配置，产品代码 0 行。

- 全量测试结果原话：**未运行（审计任务明确豁免）**。
- 内存检查：**未运行 ASan／UBSan**；S31：**未编译**；`ink-codex/tools/gcccheck.py`：**未运行**。本次只做文件、引用、差分与隐私检查。
- 分模块里程碑已本地提交；未 push、未开 PR、未写远端，未声称远端已保存。收尾提交后的 `git diff <基线>..HEAD`／工作区状态再次核对。

复核命令（`INK_DEPS` 由任务环境提供，所有原厂操作只读）：

```sh
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
epd_dir=main/boards/metalio-e-ink4-plus/display
git -C "$INK_UPSTREAM" rev-parse HEAD
git -C "$INK_UPSTREAM" diff --stat 690a023 874538e -- "$epd_dir"
git -C "$INK_UPSTREAM" diff --unified=3 690a023 874538e -- "$epd_dir/output_lcd/lcd_driver.c" "$epd_dir/output_lcd/lcd_driver.h"
git -C "$INK_UPSTREAM" diff --exit-code 690a023 874538e -- "$epd_dir/output_common/line_queue.c" "$epd_dir/output_common/line_queue.h" "$epd_dir/epdiy.c"
git diff --name-status 87bf48655ee4820837b0f54a43c8f5aed37278c2..HEAD
git diff --stat 87bf48655ee4820837b0f54a43c8f5aed37278c2..HEAD
git diff --check 87bf48655ee4820837b0f54a43c8f5aed37278c2..HEAD
git status --short --branch
```

隐私核查以总规则的完整正则过滤上述已提交 diff；`grep` 退出 **1（无匹配）** 才是预期结果。报告未抄录本机路径、设备身份信息或真实通信数据；固件 SHA／版本及寄存器值为公开源码参数。
