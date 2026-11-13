# tap-loss-1 实施计划（复核修订后的版本）

任务书：[../tap-loss-1.md](../tap-loss-1.md)。四份调查（真机事实和模拟器模型、按着时的重画和键盘、ai_page_version 的丢键和“在听”标记、perf-latency-2 的断音）对过以后定下这份计划。初稿的第 7 步（键盘按下反馈只刷一个键）在复核时拿掉了，列进“要用户定的事”。

## 结论

| 丢法 | 真原因 | 修在哪 |
|---|---|---|
| 19.5 s“下一页”丢了 | 模拟器的触摸模型：寄存器在 1.8 ms 传输**结束**时才取样；手指一抬，寄存器立刻空。实测读取 19.579106 s 开始、19.580 s 抬起、总线等待 0、monitor 抽到 58.9 ms | 模拟器，不改产品 |
| 屏幕上电时读不到触摸 | 排除：原厂 tps65185.c:18-43、epd_board_metalio_eink4_plus.c:775-847，加上我们 S31 实际用的 boards/metalio_plus/metalio_epd_power.c：每读写一个寄存器单独拿锁，等待都在锁外 | 只加防回归场景 |
| 按着时重画删掉被按住的控件 | 没有框架规则。来源有 ink_ui_redraw、lv_obj_clean、settle 钩子，还有 lv_timer 回调（比如 shell_home.c 的 cards_due） | ui_task.c |
| 按着时别处的改动让带版本控件被拒 | 全局内容版本同时给回执和远程过期检查用（cap.c:141、441），所以不能推迟这个计数 | 触摸控件改成看按下这一刻，全局计数不动 |
| 写闪存时丢点按 | 擦一个块两个核都停（典型 50 ms，最长 240 ms），I2C 锁也被拿着（hal_i2c_s31.c:74）。IDF 不给厂商 0x85 开挂起（spi_flash_chip_generic.c:675-718） | take_job |
| “在听”标记多刷两次 | 状态是异步发布的 | status_bar.c，只提前合并“开” |
| S31 触摸中断要经 irq 任务中转 | irq 任务优先级 10，刷新期间跑不到；现在靠 20 ms 轮询兜底 | s31_irq.c（收益有限） |

键盘按下反馈改成只刷一个键：不在本任务里做（任务书只要切换的刷新；切换已经只刷一帧），列进“要用户定的事”。

## 第 1 步：模拟器触摸模型（产品代码 0 行）

1. 一帧数据的取样时刻，改成 TD_STATUS 字节从线上出来的时刻（hal_i2c_sim 把起始时刻交给芯片模型）。这一条按 I2C 时序就能推出来，不靠估算。
2. 触摸的 monitor 延迟改用触摸自己的随机数流。加“最坏情况”开关：monitor 取最大值。
3. 抬起滞后（可选，标【估算】）：active 周期（0x88）原厂没写，不能拿原厂主机 10 ms 的续采周期当依据。要加就只保持上一次扫描的接触帧（event=2），不模拟 event=1（驱动 ft6336u.c:30 丢掉抬起帧）。
4. 模型缺口：S31 和原厂空闲时都每 20 ms 读一次触摸（hal_input_s31.c，metalio_touch.c:224），模拟器只等 INT。在模拟器里补上，并量它对总线占用的影响。
5. 测试：selftest 加取样时刻的断言。scenario-ai_page_version、sound-clock-input 不改时间。换流后如果因为抽数失败，停下来报告，不挑种子、不调流。种子 1 到 19 都扫一遍。反向对照：去掉第 1 项要失败。全量 ctest 的预算只能确认，不能放宽。

## 第 2 步：屏幕上电期间点按的场景（0 行）

在上电的各个阶段按 60 ms 和 80 ms 的键，事先碰一下，让芯片离开 monitor 状态。TPS 模型加一个只给测试用的“上电全程占总线”开关，打开它场景要失败。touch_pause_during_power_us 只在说明里写清“TPS 路径不用”，不改名。和设计不一致：原厂在上电、断电期间停触摸采样（690a023:main/display/epd_refresh.cc 第 230-239、303-308 行），我们不停，列进上机核对。

## 第 3 步：框架规则——按着时推迟同页的被动改动（约 95 行，接线超过 60 行就停下来写原因）

1. 处理输入时置 UI.in_input，处理输入中途的调用立刻执行。
2. ink_ui_redraw()：按着、又不在处理输入时，只记“待重画”，同时记下新加的 UI.page_serial（只在 show_top 里加一）。
3. 新增 ink_ui_after_press(slot, fn, arg)，8 项小表，同一个 slot 去重。
4. settle 不整个跳过：ink_graft_settle 的脱离和暂停、phone 的 resume_call 照常执行；首页的 settle 改走 ink_ui_after_press。
5. lv_timer 回调里有删控件的也接到 ink_ui_after_press；lv_obj_clean 同样过一遍。“重画后马上改控件”的调用点（比如 contacts.c）改成先写状态再重画。
6. 收尾：手指抬起后的那一轮，依次跑小表回调、待重画（page_serial 变了就丢掉）。按着超过 2 s【估算】就照常执行，计数 ui.passive_forced；UI 循环的超时要算上这个到期时刻。
7. 探针 ui.press_lost_to_redraw：在 LV_EVENT_PRESSED 里记下被按的对象，给它挂 LV_EVENT_DELETE；按着时它被删了就计数。
8. 备份页改用 ink_ui_after_press，删掉 B.stale。
9. 测试：用真实指针按住，按着期间分别注入页面重画、cards_rev、alarm、通话横幅、备份回复（同页的和切页的），另加“按着时来浮层、浮层消失”。反向对照：去掉推迟，场景要失败。

## 第 4 步：带版本的触摸控件只看按下那一刻（约 25 行）

全局内容版本不动。控件按下时记下 page_serial、浮层计数和自己对象的 revision；抬起时 ui_semantics 对 INK_SRC_TOUCH 比这几样，不比全局版本。确认卡和打断卡保住“按着时卡片变了就拒”。测试：键盘按键碰上别处的 content_changed 或 text_presenter_kick、首页图标，都要打出来；按着时确认卡换了内容要拒；按着时远程调用改了内容，hub 按回执准备的下一次调用照样被接受。ai_page_version 照旧通过。

## 第 5 步：闪存擦除避开交互（约 35 行）

先用 trace 查清 kid-1 那次擦除是从哪条路放行的。take_job：手指按着时，或者离上次抬起不到 TAP_GAP（300 ms【估算】），后台写不开始；前台写最多推迟 1 s【估算】，量 storage.wait_for_idle_ms 前后对比。紧急写入不受限制。新增 ink_input_touch_held() 和计数 storage.erase_near_input。hal_storage_sim 照 s31_flash_begin 加 I2C 锁。整个落在一次已经开始的擦除里的点按救不回来，单独一项，期望值写“丢”并计数，交给用户定。kid-1、files-app、dictation 照旧通过。

## 第 6 步：“在听”标记和 AI 面板合成一帧（约 18 行）

只提前合并“开”：开始听的那一轮把标记画进面板这一帧，所有 bar 的 B.shown 一起更新。结束听时等真实状态来了再刷。测试：开始说话时状态栏不再单独刷一次；在采集真正停止之前标记一直在。

## 第 7 步：S31 触摸中断直接叫醒输入任务（约 25 行）

ISR 直接通知输入任务。确认 esp_timer_get_time 在 IRAM，写出处。边沿时刻只在比上一次读取新时才用。报告写清：同级任务不抢占，刷新期间最多还要等一个 tick；ISR 不在 IRAM，擦闪存期间不会执行。vendor_irq.c 加测试，S31 构建无新警告。上机要用户看屏有没有条纹。

## 第 8 步：报告

docs/tasks/tap-loss-1/REPORT.md。

## 全量 ctest

perf-latency-2 现在就失败。本任务不为它加大缓冲、不放宽预算。“全量 ctest 过”能不能以另开的那个任务为前提，先请用户定。
