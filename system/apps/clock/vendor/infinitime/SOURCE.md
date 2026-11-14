# InfiniTime 原文（clock-3）

- 项目：InfiniTime，<https://github.com/InfiniTimeOrg/InfiniTime>
- 固定提交：`9ad16304f5fd526b781a760cf1313813d232645e`
- 许可：GPL-3.0，原项目根目录的 `LICENSE` 放在本目录（SHA-256 `8b1ba204bb69a0ade2bfcf65ef294a920f6bb361b317dba43c7ef29d96332b9b`），`third_party/licenses/catalog.json` 的 `infinitime-clock` 一条。
- 原文件**没有逐文件的版权头**，没有可保留的版权声明，我们也没有加。
- 原样拷入的那一个提交只有原文和 LICENSE（`357957ba`）；之后的改动用 `git diff 357957ba -- system/apps/clock/vendor/infinitime/` 能看到。
- 核对：`tests/clock_3/source_check.py`（CTest `clock-3-source`）逐行比原文，只允许下面列的改动；没有 `work/infinitime` 只读副本时返回 77（跳过）。

## 拷了哪些

| 原文（`src/` 下） | 拷了什么 | 原文 SHA-256 | 放在 |
| --- | --- | --- | --- |
| `components/stopwatch/StopWatchController.cpp` | 整个文件，1-72 行 | `a48c950a05e80733958a16fc94ce8ce890e41f237d4223c295688ef4c5960a1d` | 同路径 |
| `components/stopwatch/StopWatchController.h` | 整个文件，1-68 行 | `f21b29f0fe35ed73fbb995c58af59919622b4c9d5efaef36aa42e830ea2bb7c0` | 同路径 |
| `utility/CircularBuffer.h` | 整个文件，1-51 行 | `ed65afa603890ab0866345aa770a9cbc8f5d66c5b7d2e3556461875fe60d27e4` | 同路径 |
| `utility/DirtyValue.h` | 整个文件，1-39 行 | `259e7186768091a8f4413449e1cae17ba0669c9af932bda2619d8fadf798d495` | 同路径 |
| `components/timer/Timer.cpp` | 整个文件，1-47 行 | `9d5bf76d3a43b887052d85a9883d171a07347b7e6af17d1aa0461949ad46d189` | 同路径 |
| `components/timer/Timer.h` | 整个文件，1-36 行 | `4fc78e15585bfac0e1df48a6e105737fdcc27f21891f96b321510e9ceb6e3b9d` | 同路径 |
| `displayapp/screens/Timer.cpp` | 第 7 行（`using namespace`），整函数 `Refresh` 118-147、`DisplayTime` 149-156、`ToggleRunning` 181-196、`Reset` 198-202 | `7fd3fd9dc13f8002776b25d5619e085d3570b18ed1093172dba8f81cedf2820b` | `src/displayapp/screens/timer_screen_part.cpp` |
| `displayapp/widgets/Counter.cpp` | 第 5 行（`using namespace`），整函数 `UpBtnPressed` 35-45、`DownBtnPressed` 47-57、`SetValue` 59-62 | `d1f83129bcac55b218de4f3ce83a2fbe750ce0c902ab72ac6e14d10c2d25a05a` | `src/displayapp/widgets/counter_part.cpp` |

函数之间各空一行，别的都是原文。原仓库的 `tests/` 下没有秒表、计时器、计数器的测试（只有别的组件的），所以没有原文测试可带；我们为每个拷来的函数写了宿主测试 `tests/clock_3/infinitime.c`（CTest `clock-3-infinitime`）：暂停累计、计次差值、99 次和第 100 次被拒、1000 小时回卷、计数器回卷、暂停继续、到点后 60 秒停声（没有页面调用 `Refresh`）、向上取整、小时进位、卡表满。

## 原文改了的行（别的行没动）

1. `StopWatchController.h:59`：`histSize = 4` 改成 `99`。保留现有的 99 次计次；第 100 次由接线层（`ck_watch_lap`）在调用前拒绝，原文环形缓冲的回卷不会被触发。
2. `screens/Timer.cpp` 的 `Refresh` 到点分支：
   - `:125` 的 `10000` 改成 `60000`：照 Fossify 的最长提醒 60 秒（`helpers/Config.kt:53-54`，`DEFAULT_MAX_TIMER_REMINDER_SECS = 60`）。旁边原文注释“10 seconds”没改（不许动别的行）。
   - `:131-134`（“1 分钟后 `Reset()`”）删掉：卡片留着等人点，不自动消失。
   - `wakeLock.Release()` 由垫片接到 `ink_power_release(INK_NEED_AWAKE)`。
3. `screens/Timer.cpp:137-145`：长按滑出 Reset 的分支删掉，`:146` 的 `}` 留着（它闭合 `:135` 起的 `else if`）。长按加动画遮罩换成看得见的“重置”按钮。
4. `screens/Timer.cpp` 的 `DisplayTime`：
   - `:150-151`：`duration_cast` 改成 `std::chrono::ceil`：开始时显示设定的时长，到点那一刻才是 0:00（Fossify 四舍五入，我们原来也是向上取整）。
   - `:153-154`：原文只有分和秒（分最大 59），Fossify 的时是 0-23：两行改成三行，时 = 秒数 / 3600，分 = 秒数 / 60 % 60，秒 = 秒数 % 60。
5. `screens/Timer.cpp` 的 `ToggleRunning`：`:190` 的条件加上 `hourCounter.GetValue()`（否则 1 小时 0 分 0 秒点开始没反应）；`:191` 的时长加上 `std::chrono::hours(hourCounter.GetValue())`。
6. `Counter.cpp` 三个函数没改；构造函数（`:32`）和 `UpdateLabel` 没拷（`UpdateLabel` 用 LVGL 7 的 `lv_label_set_text_static` 和十二小时、月份模式），由垫片和接线层写。

在 gcc `-Werror` 下原文没有出警告（`gcccheck` 和 S31 构建都过），所以没有加按文件的警告选项。

## 怎么编译

- 只有一个编译单元 `system/apps/clock/clock_infinitime.cpp`，C++20，`-fno-exceptions -fno-rtti`：先 `#define INK_INFINITIME_NOW_US`（毫秒时钟的来源，`osal_now_us`），再包含垫片 `FreeRTOS.h`、`timers.h`，然后 `#ifndef INK_INFINITIME_SHIM #error`，再包含拷来的四个 `.cpp`。
- 两个整函数文件各自包在自己的命名空间里包含（`namespace Pinetime::Applications::Screens { #include ... }`）：几个原文的 `using namespace` 同在一个编译单元里会让 `Timer` 有歧义，包一层就不会，原文不用改。
- 头文件目录：`port/` 在前，`src/` 在后（根 `CMakeLists.txt` 和 S31 `main/CMakeLists.txt` 按文件的 `INCLUDE_DIRECTORIES`）。垫片 `FreeRTOS.h`、`timers.h` 用的是 FreeRTOS 自己的头文件保护宏（`INC_FREERTOS_H`、`TIMERS_H`），SDK 的同名头文件在它之后不会再被读进来。
- 垫片里的函数全是 `static inline`，池子是 `static`：S31 上 `nm` 看 `clock_infinitime.cpp.obj` 不导出 `xTimerCreate`、`xTimerStart`、`xTaskGetTickCount` 这些名字（报告里有 `nm` 结果）。
- C++ 对象不做全局构造：计时器、秒表、编辑用的计数器都放在静态 PSRAM（`INK_PSRAM_BSS`，按类型对齐），第一次用到时 placement new。

## 我们写的垫片（`port/`）

| 文件 | 做什么 |
| --- | --- |
| `FreeRTOS.h` | `TickType_t`（`uint32_t`，毫秒；1000 小时 = 3.6e9 毫秒放得下）、`configTICK_RATE_HZ 1000`、`pdMS_TO_TICKS`、`pdTRUE`、`pdFALSE`、`xTaskGetTickCount()`（读注册进来的时钟）。只包含 `<stdint.h>`。 |
| `timers.h` | FreeRTOS 单次软件定时器接到服务截止时间 `ink_svc_at`：`TimerHandle_t`、`xTimerCreate`（固定池 48 个，PSRAM）、`xTimerChangePeriod`（只记周期）、`xTimerStart`（挂截止时间，落在到期那一毫秒）、`xTimerStop`、`xTimerGetExpiryTime`、`xTimerIsTimerActive`、`pvTimerGetTimerID`。回调在服务定时任务上跑，进入时先把句柄标成“不在走”再调原回调（和 FreeRTOS 一样，原文 `GetTimerState` 靠这个判断到点）。“在走/到期时间”用 `ink_svc_lock` 短暂保护，`ink_svc_at`、`ink_svc_cancel` 在锁外调用。另外：过了到期那一毫秒、服务任务还没来得及跑回调的这一小段，句柄也算“不在走”，所以剩余时间不会算成负数（FreeRTOS 在这一小段里会算出一个很大的数）。 |
| `displayapp/screens/Timer.h` | 我们自己声明的 `Pinetime::Applications::Screens::Timer`，没有 LVGL：只有拷来的函数要的成员（`timer`、`motorController`、`wakeLock`、`hourCounter`、`minuteCounter`、`secondCounter`、`displaySeconds`），加上原文没有的 `hourCounter`。`SetTimerRunning`（持有唤醒锁）、`SetTimerStopped`（放唤醒锁）、`SetTimerRinging`（弹打断卡、持有唤醒锁、挂 60 秒停声）在接线层。每个计时器一个。 |
| `displayapp/widgets/Counter.h` | 同名类：`Counter(min, max)`、`min`、`max`、`value`、`userData`、`GetValue`、`UpdateLabel`（接线层：编辑页只刷那个数字；计时器的计数器只记“变了”，那一行的数字由页面刷）、`ValueChangedHandler`。 |
| `components/motor/MotorController.h` | `StartRinging`（空）、`StopRinging`（`ink_interrupt_update(id, NULL, NULL, 0, false)`，卡片留着）、`IsRinging`。在锁外调用。 |
| `systemtask/WakeLock.h` | `Lock`、`Release` 接到 `ink_power_hold`、`ink_power_release`（`INK_NEED_AWAKE`），记住自己是否持有，重复调用不重复计数。 |

## 接线层补的（拷来的函数管不到的）

- **已到点的行显示 0:00**：拷来的 `DisplayTime` 在到点以后算的是“过了多久”。接线层对“已到点”的计时器直接显示 0:00 和“已到点”（`ck_timer_shown`），不读计数器，到点后也不再每秒刷那一行。
- **60 秒停声不依赖页面**：`Refresh` 只在页面每秒调用时才跑。接线层在到点时再挂一个“到点后 60.001 秒”的 `ink_svc_at`（`quiet`），到时在 UI 任务上调拷来的 `Refresh`，所以离开页面、锁屏、在别的应用里也照样停声（`clock-3-infinitime` 和 `clock-3` 都测了）。
- **打断卡满了**：`ink_interrupt_raise` 返回 -1（卡表 4 张）时，计时器照样进入“已到点”，列表那一行写“已到点”、带“再计一次”和“重置”，唤醒锁放掉，`ink_probe_count("clock.timer_card_full")` 记一次。
- **状态**：InfiniTime 不分“空闲”和“暂停”（都只是“没在走”），接线层自己记：在走时点了就是暂停，到点后的点按是“关闭”或“再计一次”，重置是拷来的 `Reset` 之后把设定时长写回计数器（Fossify 的重置语义）。在走时点“重置”先调一次拷来的 `ToggleRunning`（停下），再 `Reset`。
- **到点在 UI 任务上处理**：定时器回调只把事情交给 UI 任务（`ink_exec_post`），`SetTimerRinging`、`Refresh`、计数器都只在 UI 任务上碰。

## 没拷的部分和证据

- `screens/Timer.cpp` 的构造、析构和遮罩（20-116 行）：用 LVGL 7 的 `lv_objmask_*`、`lv_task_create`、`lv_obj_set_style_local_*`，我们是 LVGL 9，这些接口都不存在；还要 `MotorController`、`SystemTask` 的整套。
- `SetTimerRunning`、`SetTimerStopped`、`SetTimerRinging`（158-179 行）：都是改按钮颜色（绿、红）和 LVGL 7 的样式，颜色在灰阶上没意义。由接线层写。
- `Counter::Create`、`ShowControls`、`HideControls`（64-78、122-187 行）：LVGL 7 的控件搭建。
- `displayapp/screens/StopWatch.cpp` 页面：LVGL 7。它的显示格式（`ConvertTicksToTimeSegments` 10-18 行的百分之一秒、`RenderLaps` 173-197 行）照着写在 `clock_stopwatch.c`，没拷。
- 闹钟调度 `AlarmController.cpp:56-90`：不搬，闹钟照用我们的时间服务。
- 没有哪个垫片需要改原文函数的控制流程才能接上。

## Fossify Clock 照着做的页面对应（只照着做，GPL-3.0，不拷代码）

提交 `0511f508368ada82cdb587b0bf8df9d56cf9e6ad`，路径前缀 `app/src/main/kotlin/org/fossify/clock/`。逐行的决定见 `docs/tasks/clock-3/对照表.md`。

| 我们的页面 | Fossify | 照做的 | 没照做的、为什么 |
| --- | --- | --- | --- |
| `Clock` 四个页签 | `activities/MainActivity.kt`、`helpers/Constants.kt:69-76` | 顺序“时钟 闹钟 秒表 计时”，按家人记住上次的页签 | 不滑动切换（只点分段）；第一次落在“闹钟”；没有排序菜单和默认页签设置（A2-A4、H1） |
| 时钟页签 | `fragments/ClockFragment.kt:43-121`、`adapters/TimeZonesAdapter.kt:84-131` | 本地时间、日期、下一个闹钟，城市名和时间，跨日写明天、昨天 | 只到分钟、整分才刷；保留我们的时差那一行（B1、B4） |
| `ClockWorldPick` | `dialogs/AddTimeZonesDialog.kt:11-31`、`adapters/SelectTimeZonesAdapter.kt:9-26` | 全部城市的勾选列表 | 点一行立刻存、只刷那一行、没有“取消”；分页；没有浮动按钮（B7、B8） |
| `ClockCity` | `dialogs/EditTimeZoneDialog.kt` | 改显示的名字，留空恢复默认 | 是一页而不是对话框；删除也放在这里，不用长按多选（B10、B11） |
| 闹钟页签 | `fragments/AlarmFragment.kt`、`adapters/AlarmsAdapter.kt:143-223` | 一行时间、开关、名字和重复（每天、几个星期几、今天、明天、未设置），拨开关就存 | 保存后的提示改成顶上常驻的“下一个闹钟：…，还有…”；没有拖动排序和长按（C6、C9） |
| `ClockAlarm` | `dialogs/EditAlarmDialog.kt:56-250` | 时间、星期几（周一在前，选中反白）、一天没选写“只响一次 · 明天”、名字 | 时、分用 InfiniTime 的加减；没有铃声、振动（系统设置管）；保留“只响一次、每天、工作日”快捷（D1、D5、D6、D9） |
| 秒表页签 | `fragments/StopwatchFragment.kt:61-287`、`adapters/StopwatchAdapter.kt:63-85`、`helpers/Stopwatch.kt` | 开始暂停二合一、计次只在走时、清零；三列（圈次、单圈、累计）；按列排序，新列先降序、再点切换 | 走时只到整秒、暂停后到 0.01 秒；走时不画跳动的“正在走的这一圈”，暂停后才写“第 n 圈（未完）”（F2、F3、F8） |
| 计时器页签 | `fragments/TimerFragment.kt`、`adapters/TimerAdapter.kt:126-200`、`App.kt` | 多个计时器，名字、剩余大字、开始暂停继续、重置（空闲时不画）、到点后再计一次；第一次自动建一个 | 状态写成字；到点是打断卡，60 秒停声、卡片留着；没有常驻通知（G2、G16、G20） |
| `ClockTimerEdit` | `dialogs/EditTimerDialog.kt:19-102`、`dialogs/MyTimePickerDialogDialog.kt` | 时（0-23）、分、秒，0 秒存成 10 秒，名字 | 用 InfiniTime 的加减代替滚轮；保留“1、5、15、30 分钟”快捷；没有铃声、振动（G6、G9、G10） |
