# tap-loss-1 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

# tap-loss-1 报告：点按和按键不再丢

（docs/tasks/tap-loss-1/REPORT.md 写不进去，正文放在这里。PLAN.md 已经提交了。）分支从 claude/system-layer 1240cb3a 开始，按步骤提交，一共 8 个提交，最后一个是 6c85460c。证据分三种：【实测】是本次命令跑出来的，【实现】是读代码得到的，【估算】是没有出处、要上机核对的。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
|---|---|---|---|
| 1 屏幕上电期间按下 | 通过 | 【实测】上电分 7 个阶段：0-60 ms 待机等待、61-65 ms 配 PMIC、66-90 ms PWRUP 等待、91 ms 查 PWR_GOOD、120-125 ms 写 ENABLE 和 VCOM、140 ms 查 PG。每个阶段按一下 60 ms 和一下 80 ms 的键，monitor 取最坏，14 下全部送到，每下一次。反向：--pmic-hold-bus 下丢 6 下 | ctest -R selftest-power-tap |
| 1 闪存擦除期间按下 | 部分通过 | 【实测】擦除按手册最长的 240 ms 算。间隔小于 300 ms 连点 16 下，后台写排满，全部送到；排着前台写时按住 600 ms，期间不开始擦除，这一下送到；擦除中途按下、擦完后抬起的一下送到一次，坐标对；**整个落在已经开始的擦除里的 30 ms 快点会丢**，要用户定。反向：去掉闸门，16 下只到 4 下，按住期间开始了擦除 | ctest -R tap-erase |
| 1 按着时重画 | 通过 | 【实测】真实手指按住期间注入：页面重画、首页卡片 app.cards_rev、通话横幅刷新、别处的内容变化、浮层来了又走、键盘候选到达、键盘键碰上别处的 content_changed。每下只送一次，ui.press_lost_to_redraw 不增加；按住 2.5 s 时推迟的重画照常执行，计数 1，这是设计如此。反向：去掉推迟，11 项失败；去掉第 4 步，2 项失败 | ctest -R tap-held |
| 1 键盘切换后 | 通过（原来就不丢） | 【实测】ime-keyboard1：四种组合在切换后 300、1000、2000 ms 按键都能打出来；切换只画一帧 | ctest -R ime-keyboard1 |
| 2 相关测试 | 通过 | scenario-ai_page_version-metalio-plus 和 sound-clock-input 不改时间通过；kid-1、integration-dictation、files-app、ui-fix-a-backup、contacts、comm-ux2 也通过 | ctest |
| 2 全量 ctest | 471/472 | 只有 perf-latency-2 失败，原因在上一轮调查里已查明，见“全量测试” | ctest --test-dir build -j2 |
| 2 S31 构建 | 通过，无警告 | 日志里有 factory screen output layer @690a023，没有“没有屏幕输出层” | ports/esp32s31/build.sh |
| 2 gcccheck | 我改的地方没有发现 | 30 个文件编过，1 个文件有一处 2026-11-02 就在的旧警告（见“顺手发现”） | gcccheck.py build … |
| 3 刷新次数前后对比 | 见下表 | | |

### 原来失败的两个测试

- 【实测】在 1240cb3a 上两个都失败，失败点和调查一致。
- 【实测】改完模型后两个都通过。sound-clock-input 输出 first_touch_read_us 19563675、key_up_us 19580000、dispatch_us 17、keys_delivered 8；换成触摸自己的随机流以后，monitor 抽到 43.7 ms。
- 【实测】种子 1 到 19 全部通过：monitor 最大值在 5.5 到 53.2 ms 之间，每次都是 8 个键、stale 1。没有挑种子，也没有调随机流。
- 【实测】加 --touch-worst 时 ai_page_version 失败：检测 20 ms 加 monitor 60 ms 正好等于 80 ms 的键长，芯片还没察觉手指就抬起了，和总线、CPU 都无关。这两个数都是【估算】，要上机核对。

## 每种丢法：原因、修法、场景、反向对照

| 丢法 | 原因 | 修在哪 | 场景 | 反向对照 |
|---|---|---|---|---|
| 19.5 s“下一页”丢了 | 模拟器模型有两处不对：一帧要等 1.8 ms 传输结束才取样（真芯片在 TD_STATUS 字节出线时就定了）；monitor 延迟和闪存、网络共用一个随机流 | hal_input_sim.c、hal_i2c_sim.c：在第三个字节之后取样；按下要等芯片察觉（t_seen）才出现在寄存器里；monitor 用触摸自己的随机流；--touch-worst 取满 | selftest-touch-timing；ai_page_version；sound-clock-input | 改回“传输结束”取样，3 项失败 |
| 屏幕上电时读不到触摸 | 排除：原厂和我们都是每读写一次寄存器单独拿锁，两次 50 ms 等待都在锁外 | 不改产品 | selftest-power-tap | --pmic-hold-bus 下丢 6 下 |
| 按着时重画删掉了被按住的控件 | 框架没有统一规则 | ui_task.c：手指按着、又不在处理输入时，ink_ui_redraw 只记“待重画”，ink_ui_after_press 记进 8 项小表；抬起后、画下一帧之前补做，和点击结果合成一帧；中间换过页，待重画就丢掉；按着超过 2 s【估算】就照常执行，计数 ui.passive_forced；加探针 ui.press_lost_to_redraw | tap-held | 去掉推迟，11 项失败 |
| 按着时别处改了内容，带版本的控件就拒收 | 全局内容版本会被别处的被动改动带着变 | 新增 ink_ui_touch_current：触摸控件比的是按下那一刻的版本（同一屏、同一浮层）。全局版本、回执、远程过期检查都不动；确认卡和打断卡另外比自己的 revision，卡片自己变了照样拒 | tap-held（首页图标、键盘键） | 去掉，2 项失败 |
| 写闪存时丢点按 | 擦一个块时两个核都停（典型 50 ms，最长 240 ms）；IDF 6.1 不给 Puya 闪存开挂起 | storage_service.c 的 take_job：手指或按键按着、或者离上次松开不到 300 ms【估算】，后台写不开始，连点时一直等，最长 10 s；前台写只在手指按着时等，最长 1 s；按住超过 2 s 就不再挡；计数 storage.write_near_input | tap-erase（--flash-timing max） | 去掉闸门：16 下只到 4 下；按住期间开始了擦除 |
| “在听”标记多刷 | 麦克风状态是音频任务异步发布的 | status_bar.c 加 ink_shell_bar_note_mic：AI 开麦那一轮就把标记画进面板这一帧 | listen-mark | 去掉调用，18.422 s、29.422 s 又各单独刷一次 |
| S31 触摸中断要经过 irq 任务转一手 | irq 任务优先级 10，刷新期间轮不到 | s31_irq.c：s31_irq_watch_task，ISR 直接通知输入任务并记边沿时刻；被触摸中断叫醒时先读触摸再采键 | unit-s31-vendor-irq | ISR 改回中转，断言失败 |

## 真机事实和出处

- 触摸芯片是 FT6336U，地址 0x38，INT 接 GPIO5。I2C1 只有一条 100 kHz 总线，上面挂着触摸、TPS65185、TCA9555、电量计、充电芯片、RTC、加速度计，还有摄像头的 SCCB。出处：原厂 main/boards/metalio-e-ink4-plus/config.h:73-99、bus/metalio_i2c1.c:151-152。
- 总线锁每次读写拿一次。TPS65185 和 TCA9555 的每个寄存器单独加锁：原厂 bus/metalio_i2c1.c:28-53、bus/tps65185.c:18-43、bus/pca9555.c:35-48；我们 boards/metalio_plus/metalio_epd_power.c:18-36。
- 上电序列的两次 50 ms 等待在 display/epd_board_metalio_eink4_plus.c:775、815，查 PWR_GOOD 的循环在 831-847；原厂 sdkconfig 里 CONFIG_FREERTOS_HZ=100。模拟器量到一次上电 133-142 ms【实测】。
- 原厂在屏幕上电和断电期间会停掉触摸采样：git show 690a023:main/display/epd_refresh.cc 第 230-239、303-308 行。我们不停。
- 原厂把中断模式设成 0x00，按着时每 10 ms 采一次，空闲时 20 ms 兜底采一次：metalio_touch.c:131-161、219-244。抬起帧（event=1）原厂和我们都扔掉：原厂 metalio_touch.c:188-191，我们 drivers/chips/ft6336u/ft6336u.c:30。原厂从来不写 0x86 到 0x89，所以 monitor 模式多久进入、多久扫一次查不到【估算】。
- 闪存是 Puya PY25Q128HA，4 KiB 擦除典型 50 ms、最长 240 ms（docs/hardware/2026-11-03-phase0.md）。IDF 6.1 的 generic 驱动只给 XMC 和 FM 开挂起：~/esp/esp-idf/components/spi_flash/spi_flash_chip_generic.c:675-718。S31 的 sdkconfig 没开 CONFIG_SPI_FLASH_AUTO_SUSPEND（build-s31/sdkconfig:3604）。
- S31 每次 LittleFS 读、写、擦都要先拿 I2C1 锁：ports/esp32s31/hal_i2c_s31.c:74-77、hal_storage_s31.c:41-57。
- esp_timer_get_time 在 IRAM 里：CONFIG_ESP_TIMER_IN_IRAM=y（build-s31/sdkconfig:2114），实现在 esp_timer/src/esp_timer_impl_systimer.c:68-75。GPIO 中断服务安装时 flags=0，不是 IRAM 安全的，所以擦闪存期间 ISR 要等擦完才执行。
- S31 的输入任务空闲时每 20 ms 读一次触摸和扩展芯片（hal_input_s31.c 里的 wait）；原厂空闲时也是 20 ms 兜底读一次（metalio_touch.c:224）。

## 刷新次数前后对比

| 操作 | 之前 | 之后 | 证据 |
|---|---|---|---|
| AI 一问一答（ai_page_version，三轮） | 一共派发 38 次，其中只刷状态栏的 6 次 | 36 次，只刷状态栏的 4 次。开始说话那一下和面板合成一次 GL16；结束说话那一下照旧单独刷，要等音频任务报告采集真的停了 | 【实测】trace。ai.press_to_listening_visible_ms 从 680.8 变成 709.4 ms（合成后这一帧面积大了）；ai.release_to_question_visible_ms 从 1804.5 变成 1802.0 ms |
| 键盘点一个键 | 2 到 3 次（按下反馈会用 GL16 刷整块键盘） | 没改，要用户定 | 调查时的临时探针 |
| 中英切换 | 1 帧 | 1 帧 | 【实测】ime-keyboard1 |
| 按住时碰上被动改动 | 被动重画 1 次，这一下丢了，用户只好再按 | 抬起后和点击结果合成一帧 | 【实现】run_after_press 在 lv_timer_handler 之前执行；【实测】tap-held 里页面只重建一次 |

## 新增的机制

- ink_ui_after_press(fn, arg)、8 项小表、ink_ui_redraw 的推迟和 2 s 兜底：计划第 3 步要求的。
- ink_ui_touch_current(version)：计划第 4 步要求的。
- ink_input_touch_held()、ink_input_last_release_us()：计划第 5 步要求的。都是原子量，只由输入任务写。
- 存储取任务的闸门，用到 300 ms、1 s、10 s、2 s 四个估算常量：计划第 5 步要求的。
- ink_shell_bar_note_mic(mic)：计划第 6 步要求的。
- s31_irq_watch_task、s31_irq_edge_us、s31_touch_frame_at：计划第 7 步要求的。
- 模拟器：命令行开关 --touch-worst、--pmic-hold-bus、--flash-holds-i2c，profile 里的 input.touch_idle_poll_ms，触摸自己的随机流；测试用的接口 sim_flash_last_erase。计划第 1、2、5 步要求的。计划写的是“在 profile 里加最坏情况开关”，我做成了命令行开关，因为场景里用起来更方便。
- 计数：ui.press_lost_to_redraw、ui.passive_forced、ui.after_press_full、storage.write_near_input。计划里写的名字是 erase_near_input；存储服务看不出一次写里有没有擦除，所以改名为 write_near_input。

## 没做到的

1. 整个落在已经开始的擦除里的点按救不回来（tap-erase 第 4 项的期望值就是“丢”）。闸门只能管“擦除不开始”，擦除一旦开始就停不下来。要救只有三条路：上机证明芯片会留下抬起帧，再做补报；预先擦好空闲块；或者开闪存挂起。
2. 前台写，也就是有人在等的写，多半是这一下点按自己的结果（比如保存表单）。所以前台写只在手指按着时等，松开后不再等 300 ms。原因是一开始 300 ms 也挡前台写，结果 contacts、comm-ux2 的保存晚了，阅读器在老化闪存上打开一本书超出预算（1611 > 1500 ms）。代价是：如果前台写在连点的空隙里开始了，下一下点按仍然可能落进擦除。
3. 模拟器里空闲时每 20 ms 读一次触摸，这个功能默认没开。我做了 touch_idle_poll_ms，设成 20 时【实测】ai_page_version 的总线占用从 1047 ms 涨到 3982 ms（37.5 s 里从 2.8% 涨到 10.6%），PMIC 等待 p99 从 0.007 ms 涨到 0.62 ms，送键延迟不变。但全量测试里有 7 项变了：ui-scene-ImeVoice、big-ImeVoice、AiListen、big-AiListen 的内部内存余量各少了 8 字节；audit7-b7-04 的改名等待到了 803 ms；还有 voice-name-assist、accel-use。这些要逐项核对，不能放宽预算，所以先关着，另开任务。
4. 模拟器里“写闪存时拿 I2C 锁”默认也没开（计划要求开），做成了 --flash-holds-i2c。打开后【实测】触摸读等总线最长 546 ms，p99 282 ms：模拟器的互斥量释放时不交给正在等的任务，两次闪存操作之间又不花时间，于是 LittleFS 一长串读、写、擦一直占着总线，擦完后才按下的那一下也丢了。真机上释放锁到下次拿锁之间有几到几十微秒，输入任务（优先级 24）多半能抢到，但没有证据【估算】。已列进上机核对。
5. 没加“抬手后芯片再保留一帧”的滞后，也不模拟 event=1：0x88 原厂没写，依据不够。
6. lv_timer 回调和 lv_obj_clean，只接了那些会删可点控件的被动来源：首页卡片和 cards_due（经过 settle）、通话横幅、控制中心、状态栏引起的控制中心重画、键盘同步、备份回复、AI 记录行、联系人回复、备忘保存回复。其余的多数是在处理输入时调用，会立刻执行；或者清的是不可点的容器。这些没有逐个写测试。
7. 计划里的两项测试没做：“按着时确认卡换了内容要拒”（只在代码里加了 revision 比较），以及 storage.wait_for_idle_ms 前后分布的对比。
8. 键盘按下反馈只刷那一个键：复核时从本任务拿掉了，要用户定。

## 和设计不一致的

1. 原厂在屏幕上电、断电期间停触摸采样，我们不停。场景证明不停也不丢点按，但真机上一边上电一边读触摸，TPS 会不会报 PG 故障、多出重试，没有证据。照 §10 列进上机核对。
2. 按住时把被动重画推迟到抬起：原厂没有这条规则。原厂只补“采样看到了、LVGL 没来得及读到”的短按（metalio_touch.c:151-157、364-379）。
3. 点按附近不开始写闪存：原厂也没有。
4. 0x86 到 0x89 照旧不写，和原厂一致。

## 改过的已有测试

- tests/ui_shell_regressions.c 的 settled() 多等一个条件：输入服务看到手指抬起（!ink_input_touch_held()）。原因是模型改成按 TD_STATUS 取样后，松开最多晚一个 10 ms 轮询才报上来；原来的 settled() 在松开还没送到时就判定已经空闲，于是 ui-slider-regressions 的 16 项、ui-confirm-regressions 的 2 项读到了旧值。现在是等一个明确的状态，不是加等待。
- ports/esp32s31/tests/vendor_board.c 的桩跟着改了：GPIO5 改由 s31_irq_watch_task 订阅。
- ports/esp32s31/tests/vendor_irq.c 加了“ISR 直接通知”的断言。
- 没有改任何时间、预算或种子。

## 要用户定的

1. 整个落在已经开始的擦除里的快点会丢。要不要做“预擦空闲块”，或者写 Puya 的挂起驱动？后者要烧录，得先有整片备份并经你同意。
2. 键盘按下反馈要不要改成只刷那一个键（现在每个键 2 到 3 次，其中一次是整块键盘的 GL16）？或者干脆不显示按下反色？
3. 写闪存时拿 I2C 锁，会不会让触摸等很久？要等上机数据。如果真会，要不要在 s31_flash_begin 里先看触摸 INT，有按下就先让一拍？
4. 要不要另开任务，核对那 7 项测试后把模拟器的 20 ms 空闲读取默认打开？
5. perf-latency-2 跟点按无关（TTS 抖动缓冲只有约 0.5 s 余量），本任务没为它加大缓冲、没放宽预算。“全量 ctest 过”能不能以另开的那个任务为前提？
6. 下面这些数都是估算，要等试用后再调：按住超过 2 s 才放行被动改动；松开后 300 ms 内不开始后台写；前台写最多让 1 s；后台写最多等 10 s。

## 顺手发现

- gcccheck 用的是 RISC-V gcc，在那里 uint32_t 是 long。所以 ports/sim/hal/hal_storage_sim.c:263 的 "%x" 配 uint32_t（2026-11-02 的旧代码）会报错。Linux CI 的 x86 gcc 不受影响，这次没改。
- 写闪存时的总线锁可能会让触摸读等很久，见“没做到的”第 4 条。

## 全量测试

【实测】最后一次全量（清空 test-out、重新 configure，-j2）：472 项里 471 项过，memo-save-busy 本来就是 Disabled。失败的只有 perf-latency-2（ai_ask-metalio-plus-00，种子 42，audio.speaker.underruns=1、断音 45 ms），和上一轮调查查到的一样。中途那次全量有 5 项失败，都是本任务引起的，已经在 6c85460c 修好。ASan 构建里跑了 8 项相关测试，全部通过。S31 主机测试 78 项全部通过。

## S31 构建

【实测】rc=0，日志里 factory screen output layer @690a023 出现 2 次，“没有屏幕输出层”0 次，warning 0 个；ink-s31.bin 大小 0x422900，app 分区还剩 17%。

## gcc 检查

【实测】30 个文件编过。只有 hal_storage_sim.c:263 一处，是上面说的旧代码。S31 的两个文件不在主机构建里，被跳过；它们在 S31 构建和 build-s31-host（-Werror）里都编过，没有警告。

## 产品代码行数

【实测】加 303 行，删 58 行，含注释，没超过 400 行。分步见上。

## 隐私检查

【实测】git diff 1240cb3a..HEAD 里搜本机路径和 MAC 地址的模式，没有结果。

## 上机先核对清单

- FT6336U：读 0x86 到 0x89（包括 0x88）的默认值；放置 10 s 以上再点，量第一下从触碰到 INT 拉低的延迟。用量到的值换掉 profile 里的 20 ms、10 s、60 ms。只读。
- 快按一下（短于 50 ms），每 2 ms 读一次原始 13 字节：看抬手后有没有 event=1 的抬起帧、保持几帧，抬手后多久寄存器变空、INT 变高。用来决定要不要加抬起滞后、要不要补报抬起帧。只读。
- 一边给屏幕上电一边连续点触摸：看 TPS 有没有 PG 故障、有没有多出重试（原厂上电时停触摸，我们不停）。
- 第 7 步装上以后，在 GC16 刷新期间反复点：要用户看屏上有没有条纹；再看按下到送达的延迟。
- 试用构建里看 storage.write_near_input 的计数，以及真实擦除时长的分布（Puya PY25Q128HA）。
- 写闪存时（大文件拷贝、备忘连续保存）触摸读要等总线多久：对比日志里 input: touch down 的时间和实际按下的时间。模拟器在 --flash-holds-i2c 下最长 546 ms。
- 按下到按下反馈排进显示队列的延迟；首页卡片更新撞上点按时有没有丢；试用日志里 ui.press_lost_to_redraw 是不是 0。
- SMP 下 PMIC 连续 7 次传输时，触摸要等多久（模拟器 4.6 ms）。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

## 结论

两条复核意见都查了。第 1 条属实，已修好。第 2 条说的丢点按确实存在，但在本任务的范围内修不了；“测试把丢当成通过”这一点我只认一半，测试已改成明确的“已知丢失”标记。

- worktree：.claude/worktrees/wf_0c5217bf-27b-7
- 分支：worktree-wf_0c5217bf-27b-7（没有 push）
- 最后一个提交：**9fe3820d**（上一个是 6c85460c）

全量 ctest 原文：

```
99% tests passed, 2 tests failed out of 472
Total Test time (real) = 729.56 sec
```

- 失败的两项：
  - perf-latency-2：原来就失败，和点按无关。
  - unit-s31-uhci-negative：4G 模组的主机计时测试，和这次改的代码无关。单独重跑通过（23.09 s）。
- memo-save-busy 本来就是 Disabled。
- gcccheck：5 个文件都编过，0 处问题。
- S31 构建：rc=0；“factory screen output layer @690a023”出现 2 次，“没有屏幕输出层”0 次，warning 0 个。ink-s31.bin 大小 0x422ac0，app 分区还剩 17%。
- build-s31-own 已删除。build-asan 这次没有建，没跑 ASan。

## 修了什么（第 1 条，keyboard.c，属实）

**为什么属实**（【实现】读代码）：手指按着时，keyboard 的 sync 被推迟了（ink_text_set_presenter(sync_later)），但下面这些照常发生：

- text_service.c 的 assist_show 先写 AS.words，再调 kick；
- on_dict_ready 把 cands_valid 设成 false。

于是会话里的候选列表已经换了，屏幕和 K.revision 却都没变。ink_ui_touch_current 放行之后，pick() 按序号调 ink_text_pick(index)，取的是新列表里的词。改之前，sync 会立刻让 K.revision 加一，这一下被判过期。

**反向对照证实了**【实测】：把 keyboard.c 退回去，新场景里会提交屏幕上没有的词：

- 按的是“一起看书”，写进去的是“今天下雨”；
- 按空格时屏幕上第一个是“你”，写进去的是“泥”；
- 按第二个候选“你”，写进去的是“泥”。

**同一类问题还有两处，一并修了**：

- 空格键提交的是“当前列表的第一个”，不是屏幕上的第一个。
- 按着时别处把焦点换到另一个输入框，键会打进那个看不见的框。改之前也会被判过期。

**修法**（产品代码 +39/-13，本任务累计 +341/-70，没超过 400 行）：

- keyboard.c：
  - sync 时记下候选按钮上显示的内容（K.cand[]），以及它对应的输入会话（K.session）。
  - 触摸选候选时，提交“按下时看到的那个词”（新接口 ink_text_pick_shown）。
  - 触摸按空格、有拼音在输入、屏幕上有候选时，提交显示的第一个。
  - 触摸时如果会话已经换了，判过期（may_edit 里比 K.session 和 ink_text_target_session）。
  - AI 或远程调用 keyboard.pick 照旧按当前列表的序号，不变。
- text_service.c：把 ink_text_pick 的提交部分抽成 pick_cand。接话词照旧整段写入；拼音词照旧学词频、去掉用掉的字母，另加一条：用掉的字母数不能超过正在输入的长度。
- ink_text_target_session 从 text_internal.h 挪到公开头文件 textinput.h。

**测试**（tap-held，【实测】通过，test.pass 88）：

- 新增 tests/tap_loss/held_text.c：把 text_service.c 编进测试，由它提供两种“列表自己变了”的方式：
  - 一条接话按真实的回复路径（assist_show）送到；
  - 拼音列表前两个对调，再通知键盘，代替“输入中途词库加载完”。真实的加载时机没法卡进一次按压里，这一点已在注释里写明。
- held.c 新增四项：
  - 接话到达时按候选：按的那个词写进去，只写一次。前提是先让模拟 hub 连上（ink_hub_demand），打开 text.assist，等真实的接话出现在键盘上。
  - 拼音列表重排时按空格。
  - 拼音列表重排时按第二个候选。
  - 按键时别的输入框拿到焦点：这一键不进那个框，下一键正常进。
- 反向对照：去掉 keyboard.c 的改动，6 个 CHECK 失败。
- 相关测试都过了：ime-keyboard1、semantics-shell2-keyboard、voice-name-assist、integration-dictation、kid-1、scenario-ai_page_version-metalio-plus。

## 拒掉了什么（第 2 条，tap-erase 第 4 项）

**丢点按属实，但本任务修不了**。证据：

- 擦一个 4 KiB 块时两个核都停。原厂驱动直接扔掉抬起帧（ink-hw/upstream/Metalio-E-INK4-Plus/main/boards/metalio-e-ink4-plus/display/metalio_touch.c:188-191，event 不是 DOWN 或 CONTACT 就 return）。所以擦完后再读，读到的是“没有触点”，位置也不知道。
- IDF 的通用驱动只给 XMC、FM 开挂起（~/esp/esp-idf/components/spi_flash/spi_flash_chip_generic.c:675-718），不包括 Puya。
- 要救只有两条路：给 Puya 写挂起驱动，或者用芯片可能保留下来的抬起帧补报。两条都要先拿到真机事实（总规则第 9、10 节）。挂起驱动还要烧录，得先有整片备份并经用户同意。
- 已经审过的计划第 5 步本来就写明：这一项期望值写“丢”，交给用户定。

**“没有一旦修好就会失败的反向检查”这句不对**：原来的 CHECK 判的是“没送到”，一旦送到它就失败。

**认下的那一半**：原来的提示看起来像一项通过。现在改成：

- CHECK 提示开头写“KNOWN LOSS, not fixed”，并写明“如果现在送到了，说明修好了，要改这里和预算”；
- 只有真丢了才计 test.tap_erase.lost_inside_erase；
- erase.budget.json 给它加了 {"min": 1, "max": 1, "why": ...}，预算报告里会单独列出这项已知丢失；修好后预算故意失败。

没有放宽、跳过或改时间。tap-erase【实测】通过，test.pass 17，lost_inside_erase=1，预算 ok。

---

# tap-loss-1 报告：点按和按键不再丢（复核后更新版）

docs/tasks/tap-loss-1/REPORT.md 写不进去，正文放在这里；PLAN.md 已经提交了。分支从 claude/system-layer 1240cb3a 开始，按步骤提交，一共 9 个提交，最后一个是 9fe3820d。证据分三种：【实测】是本次命令跑出来的，【实现】是读代码得到的，【估算】是没有出处、要上机核对的。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
|---|---|---|---|
| 1 屏幕上电期间按下 | 通过 | 【实测】上电分 7 个阶段：0-60 ms 待机等待、61-65 ms 配 PMIC、66-90 ms PWRUP 等待、91 ms 查 PWR_GOOD、120-125 ms 写 ENABLE 和 VCOM、140 ms 查 PG。每个阶段按一下 60 ms 和一下 80 ms 的键，monitor 取最坏，14 下全部送到，每下一次。反向：--pmic-hold-bus 下丢 6 下 | ctest -R selftest-power-tap |
| 1 闪存擦除期间按下 | 部分通过 | 【实测】擦除按手册最长的 240 ms 算。间隔小于 300 ms 连点 16 下，全部送到；排着前台写时按住 600 ms，期间不开始擦除，这一下送到；擦除中途按下、擦完后抬起的一下送到一次，坐标对。**整个落在已经开始的擦除里的 30 ms 快点会丢**，现在作为“已知丢失”标出并计数，要用户定。反向：去掉闸门，16 下只到 4 下，按住期间开始了擦除 | ctest -R tap-erase |
| 1 按着时重画 | 通过 | 【实测】真实手指按住期间注入：页面重画、首页卡片 app.cards_rev、通话横幅刷新、别处的内容变化、浮层来了又走、键盘候选到达、键盘键碰上别处的 content_changed。复核后加了四项：接话到达时按候选、拼音列表重排时按空格、重排时按第二个候选、按键时别的输入框拿到焦点。每下只送一次，送的是按下时看到的那个；ui.press_lost_to_redraw 不增加。按住 2.5 s 时推迟的重画照常执行，计数 1，这是设计如此。反向：去掉推迟，11 项失败；去掉第 4 步，2 项失败；去掉复核修复，6 项失败 | ctest -R tap-held |
| 1 键盘切换后 | 通过（原来就不丢） | 【实测】ime-keyboard1：四种组合在切换后 300、1000、2000 ms 按键都能打出来；切换只画一帧 | ctest -R ime-keyboard1 |
| 2 相关测试 | 通过 | scenario-ai_page_version-metalio-plus 和 sound-clock-input 不改时间通过；kid-1、integration-dictation、files-app、ui-fix-a-backup、contacts、comm-ux2、voice-name-assist、semantics-shell2-keyboard 也通过 | ctest |
| 2 全量 ctest | 470/472，另 1 项单独重跑通过 | perf-latency-2 失败，原因见“全量测试”；unit-s31-uhci-negative 在全量里失败、单独重跑通过 | ctest --test-dir build -j2 |
| 2 S31 构建 | 通过，无警告 | 日志里有 factory screen output layer @690a023，没有“没有屏幕输出层” | ports/esp32s31/build.sh |
| 2 gcccheck | 通过 | 复核改到的 5 个文件 0 处问题；之前 30 个文件的结果见“gcc 检查” | gcccheck.py build … |
| 3 刷新次数前后对比 | 见下表 | | |

### 原来失败的两个测试

- 【实测】在 1240cb3a 上两个都失败，失败点和调查一致。
- 【实测】改完模型后两个都通过。sound-clock-input 输出 first_touch_read_us 19563675、key_up_us 19580000、dispatch_us 17、keys_delivered 8；换成触摸自己的随机流以后，monitor 抽到 43.7 ms。
- 【实测】种子 1 到 19 全部通过：monitor 最大值在 5.5 到 53.2 ms 之间，每次都是 8 个键、stale 1。没有挑种子，也没有调随机流。
- 【实测】加 --touch-worst 时 ai_page_version 失败：检测 20 ms 加 monitor 60 ms 正好等于 80 ms 的键长，芯片还没察觉手指就抬起了，和总线、CPU 都无关。这两个数都是【估算】，要上机核对。

## 每种丢法：原因、修法、场景、反向对照

| 丢法 | 原因 | 修在哪 | 场景 | 反向对照 |
|---|---|---|---|---|
| 19.5 s“下一页”丢了 | 模拟器模型有两处不对：一帧要等 1.8 ms 传输结束才取样（真芯片在 TD_STATUS 字节出线时就定了）；monitor 延迟和闪存、网络共用一个随机流 | hal_input_sim.c、hal_i2c_sim.c：在第三个字节之后取样；按下要等芯片察觉（t_seen）才出现在寄存器里；monitor 用触摸自己的随机流；--touch-worst 取满 | selftest-touch-timing；ai_page_version；sound-clock-input | 改回“传输结束”取样，3 项失败 |
| 屏幕上电时读不到触摸 | 排除：原厂和我们都是每读写一次寄存器单独拿锁，两次 50 ms 等待都在锁外 | 不改产品 | selftest-power-tap | --pmic-hold-bus 下丢 6 下 |
| 按着时重画删掉了被按住的控件 | 框架没有统一规则 | ui_task.c：手指按着、又不在处理输入时，ink_ui_redraw 只记“待重画”，ink_ui_after_press 记进 8 项小表；抬起后、画下一帧之前补做，和点击结果合成一帧；中间换过页，待重画就丢掉；按着超过 2 s【估算】照常执行，计数 ui.passive_forced；加探针 ui.press_lost_to_redraw | tap-held | 去掉推迟，11 项失败 |
| 按着时别处改了内容，带版本的控件就拒收 | 全局内容版本会被别处的被动改动带着变 | 新增 ink_ui_touch_current：触摸控件比的是按下那一刻的版本（同一屏、同一浮层）。全局版本、回执、远程过期检查都不动；确认卡和打断卡另外比自己的 revision，卡片自己变了照样拒 | tap-held（首页图标、键盘键） | 去掉，2 项失败 |
| 键盘被推迟重画时，候选列表或焦点换了（复核发现） | 键盘不重画，会话里的列表却换了；按序号选会选到没看到的词，空格会取到新的第一个；焦点换了，键会打进别的框 | keyboard.c 记下按钮显示的候选和会话；触摸提交看到的那个（ink_text_pick_shown），空格取显示的第一个，会话换了就判过期 | tap-held 新增四项 | 去掉，6 项失败 |
| 写闪存时丢点按 | 擦一个块时两个核都停（典型 50 ms，最长 240 ms）；IDF 6.1 不给 Puya 闪存开挂起 | storage_service.c 的 take_job：手指或按键按着、或者离上次松开不到 300 ms【估算】，后台写不开始，连点时一直等，最长 10 s；前台写只在手指按着时等，最长 1 s；按住超过 2 s 就不再挡；计数 storage.write_near_input | tap-erase（--flash-timing max） | 去掉闸门：16 下只到 4 下；按住期间开始了擦除 |
| “在听”标记多刷 | 麦克风状态是音频任务异步发布的 | status_bar.c 加 ink_shell_bar_note_mic：AI 开麦那一轮就把标记画进面板这一帧 | listen-mark | 去掉调用，18.422 s、29.422 s 又各单独刷一次 |
| S31 触摸中断要经过 irq 任务转一手 | irq 任务优先级 10，刷新期间轮不到 | s31_irq.c：s31_irq_watch_task，ISR 直接通知输入任务并记边沿时刻；被触摸中断叫醒时先读触摸再采键 | unit-s31-vendor-irq | ISR 改回中转，断言失败 |

## 真机事实和出处

- 触摸芯片是 FT6336U，地址 0x38，INT 接 GPIO5。I2C1 只有一条 100 kHz 总线，上面挂着触摸、TPS65185、TCA9555、电量计、充电芯片、RTC、加速度计，还有摄像头的 SCCB。出处：原厂 main/boards/metalio-e-ink4-plus/config.h:73-99、bus/metalio_i2c1.c:151-152。
- 总线锁每次读写拿一次。TPS65185 和 TCA9555 的每个寄存器单独加锁：原厂 bus/metalio_i2c1.c:28-53、bus/tps65185.c:18-43、bus/pca9555.c:35-48；我们 boards/metalio_plus/metalio_epd_power.c:18-36。
- 上电序列的两次 50 ms 等待在 display/epd_board_metalio_eink4_plus.c:775、815，查 PWR_GOOD 的循环在 831-847；原厂 sdkconfig 里 CONFIG_FREERTOS_HZ=100。模拟器量到一次上电 133-142 ms【实测】。
- 原厂在屏幕上电和断电期间会停掉触摸采样：git show 690a023:main/display/epd_refresh.cc 第 230-239、303-308 行。我们不停。
- 原厂把中断模式设成 0x00，按着时每 10 ms 采一次，空闲时 20 ms 兜底采一次：metalio_touch.c:131-161、219-244。
- 抬起帧（event=1）原厂和我们都扔掉：原厂 metalio_touch.c:188-191（复核时又核对过），我们 drivers/chips/ft6336u/ft6336u.c:30。
- 原厂从来不写 0x86 到 0x89，所以 monitor 模式多久进入、多久扫一次，查不到【估算】。
- 闪存是 Puya PY25Q128HA，4 KiB 擦除典型 50 ms、最长 240 ms（docs/hardware/2026-11-03-phase0.md）。IDF 6.1 的 generic 驱动只给 XMC 和 FM 开挂起：~/esp/esp-idf/components/spi_flash/spi_flash_chip_generic.c:675-718（复核时又核对过）。S31 的 sdkconfig 没开 CONFIG_SPI_FLASH_AUTO_SUSPEND（build-s31/sdkconfig:3604）。
- S31 每次 LittleFS 读、写、擦都要先拿 I2C1 锁：ports/esp32s31/hal_i2c_s31.c:74-77、hal_storage_s31.c:41-57。
- esp_timer_get_time 在 IRAM 里：CONFIG_ESP_TIMER_IN_IRAM=y（build-s31/sdkconfig:2114），实现在 esp_timer/src/esp_timer_impl_systimer.c:68-75。GPIO 中断服务安装时 flags=0，不是 IRAM 安全的，所以擦闪存期间 ISR 要等擦完才执行。
- S31 的输入任务空闲时每 20 ms 读一次触摸和扩展芯片（hal_input_s31.c 里的 wait）；原厂空闲时也是 20 ms 兜底读一次（metalio_touch.c:224）。

## 刷新次数前后对比

| 操作 | 之前 | 之后 | 证据 |
|---|---|---|---|
| AI 一问一答（ai_page_version，三轮） | 一共派发 38 次，其中只刷状态栏的 6 次 | 36 次，只刷状态栏的 4 次。开始说话那一下和面板合成一次 GL16；结束说话那一下照旧单独刷，要等音频任务报告采集真的停了 | 【实测】trace。ai.press_to_listening_visible_ms 从 680.8 变成 709.4 ms（合成后这一帧面积大了）；ai.release_to_question_visible_ms 从 1804.5 变成 1802.0 ms |
| 键盘点一个键 | 2 到 3 次（按下反馈会用 GL16 刷整块键盘） | 没改，要用户定 | 调查时的临时探针 |
| 中英切换 | 1 帧 | 1 帧 | 【实测】ime-keyboard1 |
| 按住时碰上被动改动 | 被动重画 1 次，这一下丢了，用户只好再按 | 抬起后和点击结果合成一帧 | 【实现】run_after_press 在 lv_timer_handler 之前执行；【实测】tap-held 里页面只重建一次 |

## 新增的机制

- ink_ui_after_press(fn, arg)、8 项小表、ink_ui_redraw 的推迟和 2 s 兜底：计划第 3 步要求的。
- ink_ui_touch_current(version)：计划第 4 步要求的。
- ink_text_pick_shown(cand)、键盘记下显示的候选和会话、ink_text_target_session 公开：复核后加的，让推迟重画时的点按作用在看到的内容上。
- ink_input_touch_held()、ink_input_last_release_us()：计划第 5 步要求的。都是原子量，只由输入任务写。
- 存储取任务的闸门，用到 300 ms、1 s、10 s、2 s 四个估算常量：计划第 5 步要求的。
- ink_shell_bar_note_mic(mic)：计划第 6 步要求的。
- s31_irq_watch_task、s31_irq_edge_us、s31_touch_frame_at：计划第 7 步要求的。
- 模拟器：
  - 命令行开关 --touch-worst、--pmic-hold-bus、--flash-holds-i2c；
  - profile 里的 input.touch_idle_poll_ms；
  - 触摸自己的随机流；
  - 测试用的接口 sim_flash_last_erase；
  - 测试文件 tests/tap_loss/held_text.c。
  
  计划写的是“在 profile 里加最坏情况开关”，我做成了命令行开关，因为场景里用起来更方便。
- 计数：ui.press_lost_to_redraw、ui.passive_forced、ui.after_press_full、storage.write_near_input、test.tap_erase.lost_inside_erase。计划里写的名字是 erase_near_input；存储服务看不出一次写里有没有擦除，所以改名为 write_near_input。

## 没做到的

1. 整个落在已经开始的擦除里的点按救不回来。tap-erase 第 4 项标成“已知丢失”：只在真丢时计数，预算要求正好 1 次；修好后检查和预算都会失败，提醒更新。闸门只能管“擦除不开始”，擦除一旦开始就停不下来。要救只有三条路：
   - 上机证明芯片会留下抬起帧，再做补报；
   - 预先擦好空闲块（只是把擦除挪到空闲时，空闲时来的点按照样可能撞上，治标）；
   - 开闪存挂起。
2. 前台写，也就是有人在等的写，多半是这一下点按自己的结果（比如保存表单）。所以前台写只在手指按着时等，松开后不再等 300 ms。原因：一开始 300 ms 也挡前台写，结果 contacts、comm-ux2 的保存晚了，阅读器在老化闪存上打开一本书超出预算（1611 > 1500 ms）。代价是：如果前台写在连点的空隙里开始了，下一下点按仍然可能落进擦除。
3. 模拟器里空闲时每 20 ms 读一次触摸，这个功能默认没开。我做了 touch_idle_poll_ms，设成 20 时【实测】：
   - ai_page_version 的总线占用从 1047 ms 涨到 3982 ms（37.5 s 里从 2.8% 涨到 10.6%）；
   - PMIC 等待 p99 从 0.007 ms 涨到 0.62 ms；
   - 送键延迟不变。
   
   但全量测试里有 7 项变了：ui-scene-ImeVoice、big-ImeVoice、AiListen、big-AiListen 的内部内存余量各少了 8 字节；audit7-b7-04 的改名等待到了 803 ms；还有 voice-name-assist、accel-use。这些要逐项核对，不能放宽预算，所以先关着，另开任务。
4. 模拟器里“写闪存时拿 I2C 锁”默认也没开（计划要求开），做成了 --flash-holds-i2c。打开后【实测】触摸读等总线最长 546 ms，p99 282 ms。原因是模拟器的互斥量释放时不交给正在等的任务，两次闪存操作之间又不花时间，于是 LittleFS 一长串读、写、擦一直占着总线，擦完后才按下的那一下也丢了。真机上释放锁到下次拿锁之间有几到几十微秒，输入任务（优先级 24）多半能抢到，但没有证据【估算】。已列进上机核对。
5. 没加“抬手后芯片再保留一帧”的滞后，也不模拟 event=1：0x88 原厂没写，依据不够。
6. lv_timer 回调和 lv_obj_clean，只接了会删可点控件的被动来源：首页卡片和 cards_due（经过 settle）、通话横幅、控制中心、状态栏引起的控制中心重画、键盘同步、备份回复、AI 记录行、联系人回复、备忘保存回复。其余的多数在处理输入时调用，会立刻执行；或者清的是不可点的容器。这些没有逐个写测试。
7. 计划里的两项测试没做：“按着时确认卡换了内容要拒”（只在代码里加了 revision 比较），以及 storage.wait_for_idle_ms 前后分布的对比。
8. 键盘按下反馈只刷那一个键：复核时从本任务拿掉了，要用户定。
9. 键盘推迟重画时，其他会改列表的来源（引擎设置切换、成员切换）走的是同一条“提交看到的那个”路径，没有逐个写场景。词库真实加载的时机卡不进一次按压，场景里用“列表重排再通知键盘”代替。

## 和设计不一致的

1. 原厂在屏幕上电、断电期间停触摸采样，我们不停。场景证明不停也不丢点按；但真机上一边上电一边读触摸，TPS 会不会报 PG 故障、多出重试，没有证据。照 §10 列进上机核对。
2. 按住时把被动重画推迟到抬起：原厂没有这条规则。原厂只补“采样看到了、LVGL 没来得及读到”的短按（metalio_touch.c:151-157、364-379）。
3. 点按附近不开始写闪存：原厂也没有。
4. 0x86 到 0x89 照旧不写，和原厂一致。

## 改过的已有测试

- tests/ui_shell_regressions.c 的 settled() 多等一个条件：输入服务看到手指抬起（!ink_input_touch_held()）。原因是模型改成按 TD_STATUS 取样后，松开最多晚一个 10 ms 轮询才报上来；原来的 settled() 在松开还没送到时就判定已经空闲，于是 ui-slider-regressions 的 16 项、ui-confirm-regressions 的 2 项读到了旧值。现在是等一个明确的状态，不是加等待。
- ports/esp32s31/tests/vendor_board.c 的桩跟着改了：GPIO5 改由 s31_irq_watch_task 订阅。
- ports/esp32s31/tests/vendor_irq.c 加了“ISR 直接通知”的断言。
- tests/tap_loss/erase.c 第 4 项的提示和计数改成“已知丢失”，erase.budget.json 加了它的预算项（min 1、max 1）。判定条件没变。
- 没有改任何时间、预算上限或种子。

## 要用户定的

1. 整个落在已经开始的擦除里的快点会丢。要不要做“预擦空闲块”，或者写 Puya 的挂起驱动？后者要烧录，得先有整片备份并经你同意。
2. 键盘按下反馈要不要改成只刷那一个键（现在每个键 2 到 3 次，其中一次是整块键盘的 GL16）？或者干脆不显示按下反色？
3. 写闪存时拿 I2C 锁，会不会让触摸等很久？要等上机数据。如果真会，要不要在 s31_flash_begin 里先看触摸 INT，有按下就先让一拍？
4. 要不要另开任务，核对那 7 项测试后把模拟器的 20 ms 空闲读取默认打开？
5. perf-latency-2 跟点按无关（TTS 抖动缓冲只有约 0.5 s 余量），本任务没为它加大缓冲、没放宽预算。“全量 ctest 过”能不能以另开的那个任务为前提？
6. 下面这些数都是估算，要等试用后再调：按住超过 2 s 才放行被动改动；松开后 300 ms 内不开始后台写；前台写最多让 1 s；后台写最多等 10 s。

## 上机先核对清单

- FT6336U：读 0x86 到 0x89（包括 0x88）的默认值；放置 10 s 以上再点，量第一下从触碰到 INT 拉低的延迟。用量到的值换掉 profile 里的 20 ms、10 s、60 ms。只读。
- 快按一下（短于 50 ms），每 2 ms 读一次原始 13 字节：看抬手后有没有 event=1 的抬起帧、保持几帧，抬手后多久寄存器变空、INT 变高。用来决定要不要加抬起滞后、要不要补报抬起帧（也决定擦除中的快点能不能救）。只读。
- 一边给屏幕上电一边连续点触摸：看 TPS 有没有 PG 故障、有没有多出重试（原厂上电时停触摸，我们不停）。
- 第 7 步装上以后，在 GC16 刷新期间反复点：要用户看屏上有没有条纹；再看按下到送达的延迟。
- 试用构建里看 storage.write_near_input 的计数，以及真实擦除时长的分布（Puya PY25Q128HA）。
- 写闪存时（大文件拷贝、备忘连续保存）触摸读要等总线多久：对比日志里 input: touch down 的时间和实际按下的时间。模拟器在 --flash-holds-i2c 下最长 546 ms。
- 按下到按下反馈排进显示队列的延迟；首页卡片更新撞上点按时有没有丢；试用日志里 ui.press_lost_to_redraw 是不是 0。
- SMP 下 PMIC 连续 7 次传输时，触摸要等多久（模拟器 4.6 ms）。

## 顺手发现

- gcccheck 用的是 RISC-V gcc，在那里 uint32_t 是 long。所以 ports/sim/hal/hal_storage_sim.c:263 的 "%x" 配 uint32_t（2026-11-02 的旧代码）会报错。Linux CI 的 x86 gcc 不受影响，这次没改。
- 写闪存时的总线锁可能会让触摸读等很久，见“没做到的”第 4 条。
- unit-s31-uhci-negative（4G 模组的主机计时测试）在 -j2 全量里失败过一次，单独重跑通过。这台 Mac 上同时有别的构建在跑，像是负载下的计时抖动，和本任务无关。

## 全量测试

【实测】复核后最后一次全量：清空 test-out、重新 configure、-j2。

- 结果原文：`99% tests passed, 2 tests failed out of 472`。memo-save-busy 本来就是 Disabled。
- 失败的两项：
  - perf-latency-2（ai_ask-metalio-plus-00，种子 42，audio.speaker.underruns=1、断音 45 ms），和上一轮调查查到的一样；
  - unit-s31-uhci-negative，单独重跑通过（23.09 s）。
- 复核前那一轮：中途那次全量有 5 项失败，都是本任务引起的，已经在 6c85460c 修好；ASan 构建里跑了 8 项相关测试，全部通过；S31 主机测试 78 项全部通过。复核这轮没有重跑 ASan。

## S31 构建

【实测】复核后：rc=0。日志里 factory screen output layer @690a023 出现 2 次，“没有屏幕输出层”0 次，warning 0 个。ink-s31.bin 大小 0x422ac0，app 分区还剩 17%；DIRAM 用了 161030 字节（31.97%）。build-s31-own 已删除。

## gcc 检查

【实测】复核改到的 5 个文件（keyboard.c、text_service.c、held.c、held_text.c、erase.c）都编过，0 处问题。之前那轮 30 个文件也都编过，只有 hal_storage_sim.c:263 一处，是上面说的旧代码。S31 的两个文件不在主机构建里，被跳过；它们在 S31 构建和 build-s31-host（-Werror）里都编过，没有警告。

## 产品代码行数

【实测】git diff --numstat 1240cb3a，统计 system、apps、boards、drivers 和 ports/esp32s31（不含 tests）：加 341 行，删 70 行，含注释，没超过 400 行。复核修复这一个提交占加 39、删 13。

## 隐私检查

【实测】git diff 里搜本机路径和 MAC 地址的模式，没有结果。

## 复核修复

两位复核人提了两条，结论如下。

1. **keyboard.c：按着候选时，候选列表已经换了，抬手后按序号提交了屏幕上没有的词**——**属实，已修**（9fe3820d）。
   - 原因：sync 被推迟，但 assist_show、on_dict_ready 照样改了会话里的列表；K.revision 没变，touch_current 又放行，于是 ink_text_pick(index) 取到新列表里的词。改之前这一下会被判过期。
   - 同类问题还有两处，一并修了：空格取“当前第一个”；按着时焦点换到另一个输入框，键会打进去。
   - 修法：键盘记下候选按钮显示的内容和对应的会话。触摸提交看到的那个（新接口 ink_text_pick_shown）；空格取显示的第一个；会话换了就判过期。AI 和远程调用照旧按当前列表的序号。
   - 场景：tap-held 新增四项（接话到达时按候选、拼音重排时按空格、重排时按第二个候选、按键时别的框拿到焦点）。去掉修复，6 个 CHECK 失败；加上修复，全部通过。
2. **tap-erase 第 4 项：整个落在已经开始的擦除里的点按还是丢，测试把“丢”写成期望值**——**丢失属实，但本任务不修；测试的说法认一半**。
   - 证据：擦除时两个核都停；原厂驱动扔掉抬起帧（metalio_touch.c:188-191）；IDF 不给 Puya 开挂起（spi_flash_chip_generic.c:675-718）。救它要先有真机事实，或者要烧录挂起驱动，计划第 5 步本来就交给用户定。
   - “修好后也不会失败”这句不对：检查判的是“没送到”，一旦送到就失败。
   - 改了：提示写明“KNOWN LOSS, not fixed”；只有真丢才计数；erase.budget.json 加了 test.tap_erase.lost_inside_erase {"min": 1, "max": 1}，修好后预算故意失败。判定没有放宽，也没改时间。

复核后重跑：tap-held、tap-erase、ime-keyboard1、semantics-shell2-keyboard、voice-name-assist、integration-dictation、kid-1、scenario-ai_page_version-metalio-plus 都通过；全量 `99% tests passed, 2 tests failed out of 472`（perf-latency-2 原来就失败；unit-s31-uhci-negative 单独重跑通过）；gcccheck 0 处问题；S31 构建通过、无警告。

相关文件：

- .claude/worktrees/wf_0c5217bf-27b-7/system/ui/shell/keyboard.c
- .claude/worktrees/wf_0c5217bf-27b-7/system/services/text/text_service.c
- .claude/worktrees/wf_0c5217bf-27b-7/system/include/ink/sys/textinput.h
- .claude/worktrees/wf_0c5217bf-27b-7/tests/tap_loss/held.c
- .claude/worktrees/wf_0c5217bf-27b-7/tests/tap_loss/held_text.c
- .claude/worktrees/wf_0c5217bf-27b-7/tests/tap_loss/erase.c
- .claude/worktrees/wf_0c5217bf-27b-7/tests/tap_loss/erase.budget.json
