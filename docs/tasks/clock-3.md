# clock-3：时钟照 Fossify Clock 重写，InfiniTime 的秒表控制器和计时器分秒输入拷原文

本任务做的事：
- 四个页签：时钟（本地时间加世界时钟）、闹钟、秒表、计时器。
- 多个闹钟，按星期几重复。
- 多个计时器，每个有名字，按秒设，可以同时走。
- 秒表计次能同时看单圈和累计，能排序。
- 世界时钟跨日写“明天”“昨天”，城市可以改名。
- 走秒时只刷数字。

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：本任务是“照着重写”，另有一部分“片段可搬”。
1. 第一步把对照表 `docs/tasks/clock-3/对照表.md` 单独提交（草稿随任务书给出，开工时核对后提交）。
2. 第二步单独提交 InfiniTime 原文的原样拷贝。
3. 然后才写接线和页面。
4. 参照安卓的地方，按第 11 节为这台设备改。做完逐行验收。

## 为什么

- 用户 2026-11-12：计时器“其他项目没有现成的时钟 app 移植吗 没有的话你就改”。
- D-129：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”。
- D-131：先把外壳和现有应用做到成熟可用。
- D-140：照计划用成熟产品，“注意设备特性 不要生搬硬套”。
- 交付计划 v4（`docs/tasks/delivery-plan.md` 第 3 节“时钟”一行）写的是：Fossify Clock，分秒输入和计次照 InfiniTime，照着重写，预算 500。
- 调研（`docs/tasks/replan-sysapps/APPS.md` A04）：搬 Fossify 四类工作的交互，借 InfiniTime 的分秒输入和计次展示，保留我们的时间和闹钟服务。参照路径是：
  - “1 分 05 秒 → 暂停 → 继续 → 到点”；
  - “运行秒表 → 计次 → 暂停精读”；
  - “异地跨日城市”。

## 现状（2026-11-14 按 `claude/system-layer` 核对，行号都对过）

已合入：design-lang-2、home-1、lock-1、control-1、settings-1、settings-d137、notify-svc-1、graft-mogo-4、reader-one-1、auto-provision-1、font-pack-1、camera-touch-bus、tap-loss-1，以及第 2a 波（almanac-2、lock-styles-1、music-3、phone-3、memo-3、files-3、gallery-3）。这些任务都没改 `system/apps/clock/`。时钟最后一次改动是 design-lang-2 的 `fb398681`。

**应用：`system/apps/clock/clock.c`，一个文件 564 行，应用 id `clock`，系统应用，孩子可用（第 564 行）**

- **页面**：页签不是一个页面，点了是推进一个新页面。
  - `Clock`（闹钟列表，第 95-132 行）。上面四个分段“闹钟 计时 秒表 世界时钟”（第 109-119 行），点后三个就推进 `ClockTimer`、`ClockStopwatch`、`ClockWorld`。
  - `ClockAlarm`（编辑闹钟，第 133-185 行）。
  - `ClockTimer` 页上的分段只有“闹钟 计时”两个（第 307-308 行），和首页的四个对不上。
- **闹钟列表**：
  - 每页 3 行（第 127 行）。行上有时间、名字和重复、开关（第 69-94 行）。
  - 顶上一行“下一个闹钟：7:00”只有时间，不写哪天、还有多久（第 120-125 行）。
  - 底部主按钮“+ 加闹钟”（第 129-131 行）。
  - 顺序是时间服务表里的插入顺序（`time_service.c:730-742`，`ink_time_alarm_next_for` 的游标遍历；公开接口 `ink_alarm_next(&cursor, kind, &out)` 是按游标取下一条，不是“下一次响铃”。下一次响铃用记录里的 `next_s`）。
- **编辑闹钟**：
  - “时- 时＋ 分- 分＋”四个按钮，到头回卷（第 151-157 行）。每按一下都 `ink_ui_redraw()` 整页重画（第 512 行），不符合 D-127。
  - 名字输入框（第 159-165 行）。
  - 快捷“只响一次 每天 工作日”（第 166-172 行）。
  - 七天按钮排成 4+3（第 174-177 行），周日在前。
  - 开启或关闭、删除、保存（第 148-149、179-184 行）。
  - 保存失败只分“名字太长”和“没能保存”（第 523 行），表满（`alarm.set` 返回 `too many alarms`，`time_caps.c:131-132`）也写“没能保存”。
  - **没有贪睡一项**：`ink_alarm_t` 没有这个字段（`system/include/ink/sys/time.h`）。响铃卡在 `notify_service.c:636-659`，写死“这次不再响”和“再睡 10 分钟”（第 657 行）。
- **计时**（第 205-346 行）：
  - 每位家人只有**一个**计时（`timers[INK_MEMBERS_MAX]`，第 213 行），默认 5 分钟（第 226 行），不保存，没有名字。
  - 页面上有快捷“1、5、15、30 分钟”，“分－ 分＋ 秒－ 秒＋”带进位，加上开始或继续、暂停、取消（第 312-337 行）。
  - 剩余时间向上取整（第 235-239 行）。在走时每秒只改数字标签，`INK_REFRESH_FAST`（第 240-251、311 行）。
  - 到点用服务截止时间（`ink_svc_at`，第 256-273 行；`system/include/ink/sys/svc.h:38`，每个截止时间一个结构，多个计时器可以各挂各的）。打断卡标题“计时结束”，正文“设定的时间到了”，**不写是哪个**；两个按钮“关闭”“知道了”都是 `clock.timer_dismiss`（第 265-269 行）。
  - 在走时持有唤醒锁 `INK_NEED_AWAKE`（第 293-294 行），到点或停下时放（第 270、294 行）。
- **秒表**（第 348-461 行，和世界时钟共用 `extra_create`）：
  - 每位家人一个，不保存，最多 99 次计次（第 349-351 行）。
  - 走时整秒，暂停显示到 0.1 秒（第 370-374 行）；显示格式是 `h:mm:ss.t`，计次行文字“第 n 次”（第 448-449 行；`clock_extras.c:85`、`timer_seconds.c:129-130` 按这个格式解析）。
  - **计次只存累计**，每行“第 n 次 + 累计”，没有单圈（第 448-449 行）。
  - 每页 3 行，走秒只刷数字（第 389-400、430 行）。
- **世界时钟**：
  - 按家人存的集合 `clock.world`，最多 6 个（第 352-353 行），记录只有 `{"zone":z}`（第 483-484 行）。
  - 城市表有 9 个（`time_service.c:76-89`）。**夏令时已经按美国、欧洲规则算**（`time_service.c:137-152`），`clock_extras.c` 测过切换那一分钟。
  - 行上写“HH:MM · +d 小时 · 第二天/前一天/同一天”（第 381-387 行）。
  - 加城市是同一页切成“挑选”状态，行上“加入”“已选择”按钮立刻写（第 440-446 行）。
  - 删除是每行一个按钮。不能改城市名。整分时只刷时间（第 402-403、562 行）。
- **能力**（第 525-552 行），共 25 个：
  - `clock.prev/next/new/open/draft/focus/save`
  - `clock.timer/timer_set/timer_start/timer_pause/timer_cancel/timer_dismiss`
  - `clock.stopwatch/stopwatch_start/stopwatch_pause/stopwatch_lap/stopwatch_reset`
  - `clock.world/world_pick/world_add/world_remove`
  - `clock.extra_prev/extra_next`

  闹钟本身的能力 `alarm.set/list/update/cancel/dismiss/snooze` 在时间服务里（`time_caps.c:372-393`），本任务不改。`clock.timer_dismiss` 的 `id` 上限是 7（第 537 行），`clock.timer_set` 的秒数是 1-86400（第 539 行）。
- **共用控件的限制**：`ink_ui_datetime_picker`（`system/ui/theme.c:34-50`，声明在 `ui_theme.h:88`）固定画年、月、日、时、分五行，不能只画时和分，也没有秒。所以不能拿它选闹钟时间或计时时长。
- **和服务有关的两条事实**（写垫片和接线前必读）：
  - `ink_svc_at`、`ink_svc_cancel`（`system/services/res/svc.c`）以及 `ink_interrupt_raise/update/end`（`system/services/notify/interrupt.c`）内部都自己拿 `ink_svc_lock`，这把锁**不可重入**。所以它们都**不能**在持有 `ink_svc_lock` 时调用。截止时间的回调是在锁外调用的。
  - 打断卡表只有 4 个位置（`interrupt.c` 的 `MAX_INT 4`），满了 `ink_interrupt_raise` 返回 -1，没有别的提示。
- **构建**：
  - 根 `CMakeLists.txt:320` 一行加入 `clock.c`；`:469` 的 `INK_I18N_SOURCES` 列了 `clock.c`。
  - S31 的 `components/ink_system/CMakeLists.txt:2` 自动收 `system/*.c`，`:35` 的词表来源也列了 `clock.c`。
  - C++ 的先例是 gallery-3：根 `CMakeLists.txt:2378-2386`，`-fno-exceptions -fno-rtti`，链接 C++ 库；S31 `main/CMakeLists.txt:145-146`。根和 S31 都是 C++20（根 `:329`，S31 `main/CMakeLists.txt:35`）。按文件加头文件目录的先例是 memo-3（S31 `components/ink_system/CMakeLists.txt:17-19`）。
- **用到时钟的测试**：
  - 在 CTest 里：`clock_alarm.c`（`clock-alarm`）、`clock_extras.c` 和 `.py`（`clock-extras`）、`timer_seconds.c` 和 `.py`（`timer-seconds`）、`sound_clock.c`（`sound-clock`、`sound-clock-input`）、`i18n_apps.c`、`design_lang_1.c` 和 `.py`、`textfield_forms.c`、`kid_apps.c`、`integration/ai_surface.c`。
  - 按页面名 `Clock` 判断的：`home_one_page.c:58-59`、`kid_2.c:100`、`settings_1.c:236`。
  - 手动跑、靠改源码字符串的反向对照：`clock_extras_reverse.py`、`clock_alarm_negative.py`、`sound_clock_negative.py`。

**旧任务书里不对、这次改了的**

- 加城市“能搜”：Fossify 没有搜索，删掉。
- 秒表“最快、最慢用文字标出”：两份来源都没有，换成 Fossify 的按列排序。
- “夏令时没处理”：已经处理了。
- 闹钟时间“用共用日期时间选择器”：做不到。
- “编辑页有贪睡”：没有这个字段。
- 页签顺序：Fossify 是 时钟、闹钟、秒表、计时器。
- 预算：重估了，见“范围和预算”。
- 共享文件：照第 2a 波的做法。

## 成熟来源（已定，不重新选）

- **Fossify Clock**（只照着做，GPL-3.0，不拷代码）：`github.com/FossifyOrg/Clock`，提交 `0511f508368ada82cdb587b0bf8df9d56cf9e6ad`，路径 `app/src/main/kotlin/org/fossify/clock/`。在临时目录或 `work/` 里只读浅克隆，不进仓库。
  - SOURCES.md 点名的行号都核过：
    - `dialogs/EditAlarmDialog.kt:110-156`（星期重复）；
    - `dialogs/EditTimerDialog.kt:19-29`、`:97-102`（时长显示和回传秒数；0 秒变 10 秒在 `:99`）；
    - `fragments/StopwatchFragment.kt:247-285`（计次列表和时间更新）；
    - `dialogs/AddTimeZonesDialog.kt:11-31`（多选时区，没有搜索）。
  - 另外照着读的有：
    - `MainActivity.kt`（页签）；
    - `fragments/ClockFragment.kt`、`AlarmFragment.kt`、`TimerFragment.kt`；
    - `adapters/AlarmsAdapter.kt`、`TimerAdapter.kt`、`StopwatchAdapter.kt`、`TimeZonesAdapter.kt`、`SelectTimeZonesAdapter.kt`；
    - `dialogs/EditTimeZoneDialog.kt`、`MyTimePickerDialogDialog.kt`；
    - `helpers/Stopwatch.kt`、`Config.kt`、`Constants.kt`；
    - `App.kt`（计时器的开始、暂停、到点）；
    - `extensions/Context.kt:161-185,381-489`；
    - 中文词条 `res/values-zh-rCN/strings.xml`。
- **InfiniTime**（拷原文，GPL-3.0）：`github.com/InfiniTimeOrg/InfiniTime`，提交 `9ad16304f5fd526b781a760cf1313813d232645e`，同样只读浅克隆。它的闹钟调度（`AlarmController.cpp:56-90`）不搬，响铃照用我们的时间服务。

## 要搬的代码（第 11 节）

**放哪、怎么编译**

- 原文放 `system/apps/clock/vendor/infinitime/`，保留原来的相对路径（`src/components/...`、`src/utility/...`），这样原文里的 `#include "components/stopwatch/StopWatchController.h"` 不用改。
- 旁边放原项目根目录的 `LICENSE`（GPL-3.0，SHA-256 `8b1ba204bb69a0ade2bfcf65ef294a920f6bb361b317dba43c7ef29d96332b9b`），并在 `third_party/licenses/catalog.json` 末尾加一条（照 `flipper-archive`）。
- 原文件**没有**逐文件的版权头，没有可保留的版权声明，不要自己加，SOURCE.md 写明这一点。
- 编译方式：
  - 按 **C++20** 编译，`-fno-exceptions -fno-rtti`，照 gallery-3。
  - 只有一个编译单元 `system/apps/clock/clock_infinitime.cpp`：它 `#include` 拷来的几个 `.cpp`，再给 C 端提供 `extern "C"` 接口 `clock_infinitime.h`。
  - 根 `CMakeLists.txt` 末尾加这一个文件和它的按文件选项。S31 在 `main/CMakeLists.txt` 末尾 `target_sources(${ink_system_lib} ...)` 加这一个文件。
  - 垫片目录 `system/apps/clock/vendor/infinitime/port/` 用按文件的 `INCLUDE_DIRECTORIES` 排在最前，照 memo-3。
  - 原文在 gcc `-Werror` 下如果出警告（例如 `StopWatchController` 的 `startTime` 没初始化），**不改原文**：给这一个编译单元加按文件的警告选项，并在 SOURCE.md 写明是哪几条、为什么。
- **S31 上的同名头文件和符号**：原文 `#include <FreeRTOS.h>`、`<timers.h>`，IDF 也有同名头文件和同名函数。
  - 垫片头文件里定义 `INK_INFINITIME_SHIM`，`clock_infinitime.cpp` 开头 `#ifndef INK_INFINITIME_SHIM #error`，保证用的是我们的。S31 构建要证明这一点。
  - 垫片里的所有函数（`xTimerCreate`、`xTimerStart`、`xTaskGetTickCount` 等）和类型一律 `static inline` 或放匿名命名空间，**不许有外部链接**，否则和 IDF 自带的同名符号在链接时重复定义。S31 构建后用 `nm` 检查 `clock_infinitime.cpp.obj` 里没有导出这些名字。
  - 垫片 `FreeRTOS.h` 只包含 `<stdint.h>`，不包含 `ink/osal.h` 或任何 IDF 头；`configTICK_RATE_HZ` 等宏加 `#ifndef` 保护。时钟来源由 `clock_infinitime.cpp` 在包含垫片之前注册（函数指针或一个 `static` 变量），垫片里的 `xTaskGetTickCount()` 读它，等于 `(uint32_t)(osal_now_us() / 1000)`。
- **构造时机**：C++ 对象不做全局构造。存储放在静态 PSRAM（`INK_PSRAM_BSS`，对齐好），第一次用到时 placement new。

**拷哪些**（整文件或整函数，原文照拷）

| 原文（提交 `9ad16304`） | 拷哪些 | 原文 SHA-256 | 我们用在哪 |
| --- | --- | --- | --- |
| `src/components/stopwatch/StopWatchController.cpp` | 整个文件（72 行，含 SOURCES 点名的 11-59） | `a48c950a…c5960a1d` | 秒表：开始、暂停、清零、计次、累计 |
| `src/components/stopwatch/StopWatchController.h` | 整个文件（68 行） | `f21b29f0…ea2bb7c0` | 同上 |
| `src/utility/CircularBuffer.h` | 整个文件（51 行） | `ed65afa6…e60d27e4` | 计次记录 |
| `src/utility/DirtyValue.h` | 整个文件（39 行） | `259e7186…f798d495` | 计时器显示：秒数变了才改字 |
| `src/components/timer/Timer.cpp`、`Timer.h` | 整个文件（47 + 36 行） | `9d5bf76d…ad46d189`、`4fc78e15…eb6e3b9d` | 每个计时器一个 `Controllers::Timer`：开始、停、剩余、到点 |
| `src/displayapp/screens/Timer.cpp` | 整函数：`Refresh`（118-147，按下面改动去掉 137-145 和 131-134）、`DisplayTime`（149-156）、`ToggleRunning`（181-196）、`Reset`（198-202），放进 `timer_screen_part.cpp` | `7fd3fd9d…edf2820b` | 剩余显示、暂停和继续、重置、到点停声 |
| `src/displayapp/widgets/Counter.cpp` | 整函数：`UpBtnPressed`（35-45）、`DownBtnPressed`（47-57）、`SetValue`（59-62），放进 `counter_part.cpp` | `d1f83129…2d25a05a` | 闹钟的时、分，计时器的时、分、秒加减（到头回卷） |

表里的哈希是缩写，SOURCE.md 写全文（开工时用 `shasum -a 256` 再核一遍）。

**原文要改的行**（逐行记进 SOURCE.md，别的行不许动）

1. `StopWatchController.h:59`：`histSize = 4` 改成 `99`。保留现有的 99 次计次；第 100 次由接线层在调用前拒绝，原文的回卷不会被触发。
2. `screens/Timer.cpp` 的 `Refresh` 到点分支（`:121-134`）：
   - `:125` 的 `10000` 改成 `60000`：照 Fossify 的最长提醒 60 秒（`Config.kt:53-54`，默认 `DEFAULT_MAX_TIMER_REMINDER_SECS = 60`）。
   - `:131-134`（“60 秒后 `Reset()`”）删掉：卡片留着等人点，不自动消失。
   - `wakeLock.Release()` 由垫片接到 `ink_power_release(INK_NEED_AWAKE)`（垫片的 `WakeLock` 要像原文一样记住自己是否持有，`Lock`/`Release` 幂等）。
3. `screens/Timer.cpp:137-145`：长按滑出 Reset 的分支删掉（`:146` 的 `}` 要留着，它闭合 `:135` 起的 `else if`；只删 137-145，不是 137-146）。长按加动画遮罩，换成看得见的“重置”按钮。
4. `screens/Timer.cpp` 的 `DisplayTime`（`:149-156`）：
   - `:150-151`：`duration_cast` 改成 `std::chrono::ceil`，开始时显示设定的时长，到点那一刻才是 0:00（Fossify 是四舍五入，现有是向上取整）。
   - `:153-154`：原文只有分和秒，分最大 59；Fossify 的时是 0-23。加 `hourCounter`，三行改成：时 = 秒数 / 3600，分 = 秒数 / 60 % 60，秒 = 秒数 % 60。
5. `screens/Timer.cpp` 的 `ToggleRunning`：`:190` 的条件加上 `hourCounter.GetValue()`（否则 1 小时 0 分 0 秒点开始没反应）；`:191` 的时长加上 `std::chrono::hours(hourCounter.GetValue())`。
6. `Counter.cpp` 三个函数不改；构造函数（`:32`）和 `UpdateLabel` 不拷（`UpdateLabel` 用 LVGL 7 的 `lv_label_set_text_static` 和十二小时、月份模式），由垫片和接线层写。

**我们写的垫片**（算进产品代码预算，放在 `port/`）

- `FreeRTOS.h`：
  - `TickType_t` 是 `uint32_t`，`configTICK_RATE_HZ 1000`；
  - `xTaskGetTickCount()` 读注册进来的毫秒时钟；
  - `pdMS_TO_TICKS`、`pdTRUE`、`pdFALSE`。
  - 1000 小时回卷（`elapsedTimeBoundary` 是 3.6e9 毫秒）放得进 32 位。
- `timers.h`：FreeRTOS 软件定时器接到 `ink_svc_at`。
  - 要实现的接口：`TimerHandle_t`、`xTimerCreate`、`xTimerChangePeriod`、`xTimerStart`、`xTimerStop`、`xTimerGetExpiryTime`、`xTimerIsTimerActive`、`pvTimerGetTimerID`、`TimerCallbackFunction_t`。
  - 句柄来自一个固定池（每位家人 6 个，共 48 个，PSRAM），每个句柄一个 `ink_svc_deadline_t`。`xTimerChangePeriod` 只记周期不挂截止时间，`xTimerStart` 才挂（原文 `StartTimer` 先 Change 再 Start）。
  - 回调在服务定时任务上跑，回调进入时先把句柄标成“不在走”再调原文的回调（和 FreeRTOS 一致，原文 `GetTimerState` 靠这个判断“已到点”）。
  - **锁**：句柄的“在走/到期时间”字段由垫片自己用 `ink_svc_lock` 短暂保护（UI 任务读、服务任务写）；`ink_svc_at`、`ink_svc_cancel` 在锁外调用（它们自己拿锁）。接线层在调用拷来的 `ToggleRunning`、`Reset`、`StopRinging` 时**不得**持有 `ink_svc_lock`。
- `displayapp/screens/Timer.h`：我们自己声明的、和原文同名的 `Pinetime::Applications::Screens::Timer` 类，没有 LVGL。
  - 只有拷来的函数要的成员：`timer`、`motorController`、`wakeLock`、`hourCounter`、`minuteCounter`、`secondCounter`、`displaySeconds`。
  - `SetTimerRunning`、`SetTimerStopped`、`SetTimerRinging` 由接线层实现：改这一行的状态字和按钮字；`SetTimerRinging` 弹打断卡、持有唤醒锁。
  - 每个计时器一个这样的对象。
- `displayapp/widgets/Counter.h`：同名类，有构造函数 `Counter(min, max)`（我们写，没有字体参数）、`min`、`max`、`value`、`userData`、`GetValue`、`UpdateLabel`（接线层：改那个数字标签，只刷它）、`ValueChangedHandler`。
- `components/motor/MotorController.h`：`StartRinging`（空）、`StopRinging`、`IsRinging`。`StopRinging` 接到 `ink_interrupt_update(id, NULL, NULL, 0, false)`，`IsRinging` 是这张卡还在不在响；`StopRinging` 要在锁外调用。
- `systemtask/WakeLock.h`：`Lock`、`Release` 接到 `ink_power_hold` 和 `ink_power_release`（`INK_NEED_AWAKE`），幂等，记住自己是否持有。

**搬不动的和证据**（只照着做，写进 SOURCE.md 和对照表）

- `Timer.cpp` 的构造、析构和遮罩（20-116 行）：
  - 用 LVGL 7 的 `lv_objmask_*`、`lv_task_create`、`lv_obj_set_style_local_*`，我们是 LVGL 9（`build-s31/lvgl/lv_version.h`），这些接口都不存在；
  - 另外要 `MotorController`、`SystemTask` 的整套。
- `SetTimerRunning`、`SetTimerStopped`、`SetTimerRinging`（158-179 行）：都是改按钮颜色（绿、红）和 LVGL 7 的样式调用，颜色在灰阶上也没意义。
- `Counter::Create`、`ShowControls`、`HideControls`（64-78、122-187 行）：LVGL 7 的控件搭建。
- `StopWatch.cpp` 页面：LVGL 7。它的显示格式（`ConvertTicksToTimeSegments` 10-18 行、`RenderLaps` 173-197 行）照着写，不拷。
- **停下的规矩**：哪个垫片要改原文函数的控制流程才能接上，就停下，把文件、行号、为什么写进“要用户定”，不许悄悄换成自己写。

**到点显示和 60 秒停声由谁驱动**（拷来的函数管不到的两处，必须由接线层补，写进 SOURCE.md 的“接线层补的”一节）

- **已到点的行显示 0:00**：拷来的 `DisplayTime` 在已到点状态下算的是“到点后过了多久”，不能拿来显示。接线层对状态是“已到点”的行直接写 0:00 和“已到点”，不读计数器，也不在到点后每秒刷数字。
- **60 秒停声不依赖页面**：`Refresh` 只有页面在前台每秒调用才会跑。每个计时器在到点时，接线层再挂一个“到点后 60 秒”的 `ink_svc_at`，到时调拷来的 `Refresh`（锁外），所以离开页面、锁屏、在别的应用里也照样停声。
- **打断卡满了**：`ink_interrupt_raise` 返回 -1（卡表只有 4 个位置）时，这个计时器照样进入“已到点”，列表那一行写“已到点”，带“关闭”和“再计一次”，唤醒锁照放，并 `ink_probe_count` 记一次；报告写这个上限。

**SOURCE.md**（`system/apps/clock/vendor/infinitime/SOURCE.md`）写这些：
- 项目、URL、固定提交、许可（GPL-3.0，原文件没有逐文件版权头）；
- 每个文件或函数的原路径、行号范围、SHA-256；
- 上面 1-6 条改动逐行写原因；
- 垫片清单；
- “接线层补的”一节（上一段的三条）；
- 没拷的部分和证据。
- 再写一节“Fossify 照着做的页面对应”：每页对应 Fossify 的哪个文件、哪几行，哪些没照做、为什么（和对照表一致）。
- 原文单独提交（原样拷入的那一个提交只有原文和 LICENSE），之后的改动用 `git diff <那个提交> -- system/apps/clock/vendor/infinitime/` 能看到。
- 核对脚本 `tests/clock_3/source_check.py`（CTest `clock-3-source`）：照 `tests/gallery_3/source_check.py`，逐行比原文，只允许上面列的改动；没有克隆时返回 77，CI 里显示为跳过。
- 原仓库这几个文件的测试：`tests/` 下只有别的组件，没有秒表和计时器的，SOURCE.md 写明。我们为拷来的函数写宿主测试 `tests/clock_3/infinitime.c`（CTest `clock-3-infinitime`），从 C 通过接口调：暂停累计、计次差值、99 次、回卷、计数器回卷、暂停继续、到点 60 秒停声（页面不在前台也停）、向上取整、小时进位。

## 要做的（逐项对应对照表）

### 1. 外框和页签（对照表 A1-A7）

- 一个页面 `Clock`，上面四个分段“时钟 闹钟 秒表 计时”，下面是内容区。换页签只重画分段和内容区，刷 1 次。根页面名保持 `Clock`，主屏、孩子、设置的测试不用改。
- 按家人记住上次的页签（PSRAM）。第一次打开、重启后落在“闹钟”（理由见 A3）。
- 编辑类的页面照旧推进新页面：
  - `ClockAlarm`（编辑闹钟）；
  - `ClockTimerEdit`（编辑计时器）；
  - `ClockWorldPick`（加城市）；
  - `ClockCity`（改名、删城市）。
- 打开编辑页时键盘不弹（memo-3 的经验）。

### 2. 时钟页签（对照表 B1-B16）

- 本地时间（只到分）、日期、“下一个闹钟：周一 7:00”（没有就不画）。
- 每个城市一行：
  - 名字（改过的名字优先），右边时间；
  - 第二行写时差：“比这里晚 13 小时”（城市的钟面比这里早到的反过来写“比这里早 N 小时”，同一时间写“和这里同一时间”），跨日时前面加“明天”或“昨天”，同一天不写。例：北京早上 8 点，纽约一行写“昨天 · 比这里晚 13 小时”，时间 19:00（夏令时期间“晚 12 小时”）。
- 整分时只刷时间那一列和本地时间那一行；跨日再刷日期。
- 底部“加城市”打开 `ClockWorldPick`：9 个城市分页，点一行加或删、立刻存、只刷那一行，选中写“已选”；满 6 个时写“最多 6 个城市”；“完成”返回。
- 点城市打开 `ClockCity`：名字输入框（留空就用默认名）、“保存”、“删除这个城市”（不弹确认）。名字写进 `clock.world` 记录的 `title`。旧记录没有这个字段，照样读，不用迁移。
- 状态：
  - 时间未设置：写“时间尚未设置”。
  - 读不出城市：写“没能读取城市，请返回重试”。
  - 没有城市：只画上面三行和“加城市”。

### 3. 闹钟页签和编辑（对照表 C1-C12、D1-D12、E1-E6）

- **列表**：
  - 顶上常驻“下一个闹钟：明天 7:00，还有 9 小时 12 分钟”，代替 Fossify 保存后的提示（用记录里的 `next_s` 算）。一小时以上只写小时，文字变了才刷这一行。
  - 每行：大字时间、开关；第二行“名字 · 重复”（“每天”“工作日”“周六、周日”“今天”“明天”“未设置”）。
  - 拨开关只刷这一行。分页，盖板键能翻。底部主按钮“+ 加闹钟”。
- **编辑**：
  - 时、分两行加减，用拷来的 Counter，按钮字保持“时-”“时＋”“分-”“分＋”，每按一下只刷那个数字。
  - 快捷三个（只响一次、每天、工作日）。
  - 七天按钮周一在前，选中用主按钮样式（反白）；一天都没选时写“只响一次 · 明天”。
  - 名字、开关（已有的闹钟）、“删除闹钟”、主按钮“保存闹钟”。
  - 失败原因写在表单上：名字太长（上限取 `alarm.set` 的 `maxLength`，文案里的字数按这个上限拼，不写死 15）、闹钟已满（全家最多 64 个闹钟和提醒）、没能保存。
- 存取照旧走 `alarm.set`、`alarm.update`、`alarm.cancel`，不碰时间服务。
- 响铃、贪睡、错过照现有的时间服务和 notify 打断卡（勿扰时闹钟照响），本任务不改。

### 4. 秒表页签（对照表 F1-F14）

- 控制器用拷来的 `StopWatchController`，每位家人一个。
- 走时整秒，每秒只刷数字那一块；暂停后显示到 0.01 秒。
- 显示格式：`h:mm:ss.cc`，一小时内省略小时位（“12.34 秒”写成 `00:12.34`，一小时以上写 `1:02:50.50`）。
- 三个按钮：主按钮“开始、暂停、继续”，“计次”（只在走时），“清零”（不是清零状态时）。
- 计次每行“第 3 圈 · 单圈 00:12.34 · 累计 01:02.50”（最大字号放不下就换行）。单圈用相邻两次的累计相减。
- 列表上面“圈次 / 单圈 / 累计”三个排序按钮，当前那个后面写“↓”或“↑”，默认圈次降序。
- 暂停时第一行是“第 4 圈（未完）”。
- 最多 99 次，第 100 次拒绝、不丢数据。分页。
- 离开页面、锁屏照样计时、不刷屏。不持有唤醒锁（和现在一样）。

### 5. 计时器页签（对照表 G1-G25）

- **数据**：
  - 新集合 `clock.timers`（按家人，schema 1，`max_records = 6`，JSON `{"title":"泡面","seconds":65}`），只存名字和时长。
  - 运行状态（`Controllers::Timer`、显示对象）放 PSRAM，最多 8 人 × 6 个。
  - 第一次进页签、这个家人从来没有过计时器时，建一条“5 分钟”的。“第一次”要有按家人的“已建过”标记；全删了再进页签不重建，用共用空状态“还没有计时器”。现有存储做不到这个标记时，停下来写进“要用户定”，不要悄悄选其中一种。
- **列表**：
  - 每行：名字、剩余时间大字、状态字（“正在计时”“已暂停”“已到点”）。
  - 按钮：“开始、暂停、继续”或“再计一次”，加“重置”（空闲时不画）。
  - 点名字或时间进编辑。
  - 在走的几行每秒只刷数字（`DirtyValue` 控制，秒数没变不改字）。
  - 底部主按钮“+ 加计时器”，满 6 个写“最多 6 个计时器”。
- **编辑 `ClockTimerEdit`**：
  - 时、分、秒三行加减（拷来的 Counter，到头回卷），快捷 1、5、15、30 分钟。
  - 名字（最多 15 个字）、主按钮“保存”、“删除这个计时器”（编辑已有的才有）。
  - 0 秒存成 10 秒。
  - 在走或暂停的计时器改了时长，保存后变成新时长的空闲状态；只改名字不停。
- **逻辑**：暂停、继续是拷来的 `ToggleRunning`；重置是拷来的 `Reset`，接线层随后把设定时长写回计数器。截止时间走 `timers.h` 垫片，也就是 `ink_svc_at`，所以多个计时器各挂各的，时间服务不用改。
- **到点**：
  - 打断卡标题“计时结束”，正文写名字（没有写“计时器”）和“14:05 到点”。
  - 两个按钮：“关闭”（回到设定时长的空闲状态）和“再计一次”。
  - 响到 60 秒停声，卡片留着；停声由到点后 60 秒的服务截止时间驱动，不依赖页面在前台（见“到点显示和 60 秒停声由谁驱动”）。
  - 两个同一秒到点就各一张卡；另一个在走的不受影响。打断卡表最多 4 张，满了的处理见上；报告写这个上限。
  - 已到点的行显示 0:00，不显示“过了多久”。
- **唤醒锁**：有计时器在走就持有，全停了才放（引用计数）。报告写对待机的影响：和现在一样，有计时就不进深睡。
- 重启后名字和时长都在，运行状态不留。

### 6. 能力和快照（对照表 I1-I2）

- **现有名字全部保留**（AI 定闹钟、定计时都在用），只加可选参数：
  - `clock.prev`、`clock.next`：翻当前页签或当前页面的列表。旧的 `clock.extra_prev`、`clock.extra_next` 照样能用，做同一件事。
  - `clock.timer`、`clock.stopwatch`、`clock.world`：切到对应页签，在应用里任何页面都能调。
  - `clock.timer_set {seconds, id?}`：在编辑页设草稿；在列表上没给 `id` 时，作用于“当前计时器”（快照里的 `current_timer`：这个家人最近设过或开始过的那个；没有就是第一个空闲的，再没有且不满 6 个就新建一个）。秒数上限 86400 改成 86399（时 0-23），列进报告。
  - `clock.timer_start`、`clock.timer_pause`、`clock.timer_cancel` 加可选 `id`。`clock.timer_cancel` 的意思是“重置”。
  - `clock.timer_dismiss {id}`：`id` 现在是家人槽位（上限 7），以后是计时器句柄（上限 47），列进报告。
  - `clock.world_pick`：打开或关上加城市页。
- **AI 现有的流程要照样能用**：“`clock.timer` → `clock.timer_set {seconds}` → `clock.timer_start`”；“`clock.new` → `clock.draft` → `clock.save`”。
- **新能力**（孩子可用）：
  - `clock.tab {tab: world|alarm|stopwatch|timer}`
  - `clock.timer_new`
  - `clock.timer_edit {id}`
  - `clock.timer_name`（名字框聚焦）
  - `clock.timer_save`
  - `clock.timer_delete {id}`
  - `clock.timer_restart {id}`（卡片上的“再计一次”，锁屏时也能用：`while_locked`）
  - `clock.lap_sort {by: lap|lap_time|total}`
  - `clock.world_edit {zone}`
  - `clock.world_name`（名字框聚焦）
  - `clock.world_title {zone, title}`
- **快照**：
  - `Clock` 写 `tab`。
  - 闹钟页签：行、`next_alarm`（带哪天和还有多久）。
  - 时钟页签：本地时间、每个城市的 `zone`、名字、时间、跨日、时差。
  - 秒表：`elapsed_ms`、`elapsed_seconds`、`running`、`laps`（圈次、单圈、累计）、`sort`、`page_number`、`total`。
  - 计时器：每个的 `id`、`title`、`seconds`、`remaining_seconds`、`state`，以及 `current_timer`。
  - 编辑页：各字段；键盘在用时动作清空（现有做法，`clock.c:196-199`）。
- 新能力进 `tests/integration/ai_surface.c` 的两张表，只在末尾追加：给 AI 的写清必填参数，不给 AI 的写理由。

### 7. 原地重写

- `system/apps/clock/` 拆成这些文件，旧页面代码删掉，不留两套：
  - `clock.c`（外框、页签、能力表、初始化）
  - `clock_alarm.c`
  - `clock_world.c`
  - `clock_stopwatch.c`
  - `clock_timer.c`
  - `clock_infinitime.cpp` 和 `.h`
  - `vendor/infinitime/`
- 每个新的 `.c` 都要加进根 `CMakeLists.txt` 的 `ink_system` 源文件和 `INK_I18N_SOURCES`，以及 S31 `components/ink_system/CMakeLists.txt:35` 的词表来源列表。这几行写进报告，冲突由主会话解。
- 页面文件头照 `clock.c:2` 写 `ui-scale-runtime` 的说明。
- 合不合入、新旧怎么换，等用户看预览后由主会话定。

## 共同要求（页面任务都一样）

- **来源**：功能、页面、状态、出错处理照对照表逐行做，不自己发挥；少做的列进“要用户定”。
- **按设备改**（第 11 节）：
  - 不用动画、滚动、转圈、闪烁和一直在变的数字。走秒只在秒表和计时器页上、只刷数字。
  - 长列表按页翻，写页码，盖板的上一页、下一页能翻。
  - 一次操作只刷一次屏，只刷变了的那一块。
  - 只用灰阶，选中、状态都写成字或反白。
  - 能点的不小于 108 px，全是写着字的按钮，没有长按、滑动删除、拖拽排序。
  - 不用会自动消失的提示、浮动按钮、汉堡菜单。
  - 大块数据（计次、计时器、城市）放 PSRAM，不后台轮询。
- **新设计语言**：
  - 只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的按钮、输入框、列表行、开关、分段、空状态、翻页条）。
  - 一屏最多一个主按钮。不画粗框和圆圈（七天按钮也不用圆圈）。没内容的区块不画，整页空才用共用空状态。
  - 不写死像素和颜色（`tools/ui_scale_check.py`）。
- **文字大小**：跟系统“文字大小”一起缩放（`ink_ui_px`）。放不下就分页或换行，不缩字、不裁按钮。四个分段在最大字号下也要放得下（字是两个字）。
- **手指按着时**（tap-loss-1）：
  - 不是这个人做的变化要等手指抬起：被动的重建用 `ink_ui_redraw`，部分重建用 `ink_ui_after_press`。这些变化包括：计时器到点、别处删了闹钟、整分、换家人、AI 改了数据。
  - 只改数字标签的不重建。
  - 页面显示的内容真变了才调 `ink_ui_content_changed`。
- **不变的规矩**：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作（本任务没有）、按下反馈 D-31、名字不进日志。
- **共享文件**（照第 2a 波已合入任务的做法，只在末尾追加，冲突由主会话解）：
  - 新文案的英文加进 `assets/i18n/en.tsv` 末尾。构建时 `i18n_gen.py --sources` 要求每条 `TR` 都有英文，所以不能只写 I18N-ADD。
  - `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交。
  - `tests/integration/ai_surface.c` 末尾追加。
  - `third_party/licenses/catalog.json` 末尾加 InfiniTime 一条。
- **截图**：放 `docs/tasks/clock-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。默认字号、最大字号、孩子身份各一套，加上改前的对照。

## 范围和预算

- **只改**：
  - `system/apps/clock/`（可以拆文件，新增 `vendor/infinitime/`）；
  - 时钟相关测试：`tests/clock_alarm.c`、`clock_alarm_negative.py`、`clock_extras.c` 和 `.py`、`clock_extras_reverse.py`、`timer_seconds.c` 和 `.py`、`sound_clock.c`、`sound_clock_negative.py`、`i18n_apps.c` 的时钟几行、`design_lang_1.c` 和 `.py` 的时钟几行；新的 `tests/clock_3/`；
  - 根 `CMakeLists.txt`、S31 `main/CMakeLists.txt`、S31 `components/ink_system/CMakeLists.txt` 里新文件的那几行；
  - 上面列的共享文件末尾，以及 `docs/tasks/clock-3/`。
- **不碰**：
  - `system/services/time/`：calendar-3 在改提醒、记录格式和 `time_caps.c`。需要的接口写进报告。
  - `system/services/notify/`（notify-ui-1）、`system/ui/shell/interrupt_card.c`（dialogs-1）：响铃卡照现有接口用；打断卡表只有 4 个位置也不归本任务改，写进报告。
  - `system/ui/theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`（dialogs-1；`ink_ui_datetime_picker` 归 calendar-3 改）。要“能选部件的共用时间选择器”，写进报告。
  - 日历应用、状态栏和控制中心（control-1）、主屏（home-1）、锁屏（lock-1、lock-styles-1）。
- **预算**：
  - **产品代码不超过 1000 行**（测试代码不计）：新写的、改的、垫片和接线都算；拷来的 InfiniTime 原文（约 260 行）不算；删掉的旧代码不抵。
  - 计划里的 500 不够，因为这次多了：
    - 多个计时器的存储和编辑页，约 200 行；
    - 城市改名页，约 60 行；
    - 计次排序和单圈，约 50 行；
    - C++ 垫片和 `extern "C"` 接口（FreeRTOS 定时器接 `ink_svc_at`、三个同名类声明、60 秒停声的截止时间），约 200 行；
    - 11 个新能力、四个页签的快照、闹钟顶栏“还有多久”，约 100 行。
    - 页面本身约 410 行（旧代码 564 行里含能力表和快照，新页面把这两块单列了）。
  - 主会话要相应改交付计划。超了先砍城市改名（B10）和圈次排序（F7），写进“没做到的”；还超就停下报告，不压缩功能硬塞。
  - **固件增量不超过 20 KB**（计划写 15 KB）：C++ 的 `std::optional`、`chrono` 都在头文件里展开，C++ 库 gallery-3 已经链上，多出来的主要是两个新页面和计时器存储。应用分区现在约剩 10%（约 500 KB），放得下。
  - 内部内存不增加：新数组都放 `INK_PSRAM_BSS`。
- **起步**：当前的 `claude/system-layer`（3baaf0e2 或更新）。

## 验收

1. **对照表逐行**：每行配截图或测试名。报告写做到了几行、哪几行没做。
2. **照参照路径走一遍**（`APPS.md` A04，模拟器脚本，时间注入，每一步截图并断言）：
   - **计时**：
     1. 新建“泡面”1 分 05 秒，开始；
     2. 30 秒时暂停，停 10 秒再继续；
     3. 运行满 1 分 05 秒时响铃，卡片写“泡面”，行上写“已到点 0:00”；
     4. 同时开着的另一个“3 分钟”照走、不受影响；
     5. 响 60 秒后停声、卡片还在（此时应用在别的页面、或者锁屏，也要停）；点“再计一次”又从 1:05 开始。
   - **秒表**：开始 → 计次 3 次（间隔不同）→ 暂停。三圈各自的单圈和累计都对（≤0.01 秒），按“单圈”排序后最快的在第一行。
   - **闹钟**：
     1. 新建 7:00“工作日”；
     2. 周五晚上看到闹钟页签写“下一个闹钟：周一 7:00，还有 … 小时”，时钟页签写“下一个闹钟：周一 7:00”；
     3. 周一 7:00 响 → 贪睡 10 分钟 → 7:10 再响。
   - **世界时钟**：在加城市页点“美国 · 纽约”。北京时间早上 8 点时，纽约那一行写“昨天”、时间 19:00 和“比这里晚 13 小时”（夏令时期间 12 小时）。再加“日本 · 东京”看到“比这里早”写法，“中国 · 上海”（和本地同时区）看到“和这里同一时间”。改名“老妹家”后列表写新名字；清空名字恢复默认。

   同一组操作用能力调用（来源是 AI）再走一遍，结果一样。
3. **刷新**（模拟器数刷新区域，报告写实测）：
   - 秒表运行 10 秒：刷新只覆盖数字那一块，每秒不超过一次。
   - 两个计时器同时在走：只覆盖两行的数字。
   - 一个计时器到点后 60 秒内：不再每秒刷数字。
   - 反向对照：改成每秒整页重画，这条必须失败。
   - 各主要操作刷几次：换页签 1、加减 1、开关 1、开始或暂停 1、计次 1、排序 1、勾城市 1、翻页 1、保存返回 1。
4. **家人隔离**：A 的计时器、城市、秒表、上次的页签，B 和孩子都看不到；A 的计时器到点时，B 在用设备也照样响（现有规则）。
5. **InfiniTime 原文**：`clock-3-source` 逐行比对通过（没有克隆时跳过）；`clock-3-infinitime` 覆盖每个拷来的函数；SOURCE.md 的哈希和原文一致。
6. **截图**（默认、最大字号、孩子身份各一套，加改前对照）：
   - 四个页签（各自有内容和空的两种）；
   - 编辑闹钟、编辑计时器（时分秒）、加城市、城市页；
   - 计时到点的卡片、时间未设置、读不出数据。
7. **构建和测试**：
   - 先清掉 `build/test-out` 再跑主工程普通全量 ctest，要过（含 `ui-scale`、`i18n-literals`）。
   - ASan 跑时钟相关测试和 `ui-scene-*`，过。
   - S31 宿主机测试过。`ports/esp32s31/build.sh` 编译通过，没有新警告，并证明用的是我们的 FreeRTOS 垫片（`INK_INFINITIME_SHIM` 的 `#error` 保护 + `nm` 检查没有导出同名符号）。
   - 报告写：固件增量、产品代码行数（不算原文）、`gcccheck` 结果。
   - **改过的已有测试逐条列出**。只改定位方式和页面结构：页签从推进页面改成同一页、按钮名和 `id` 参数，暂停精度 0.1 秒改成 0.01 秒（只能更严），秒表显示格式从 `h:mm:ss.t` 改成 `h:mm:ss.cc`，计次行文字“第 n 次”改成“第 n 圈”，名字太长的提示文案按上限拼。业务断言不改，三个改源码的反向对照按新代码重写，意图和严格程度不变。
   - 报告还要写：改名或删除的能力（预期没有删除，只有 `timer_set` 上限和 `timer_dismiss` 的 `id` 范围变了），运行计时对待机的影响，以及打断卡表 4 张的上限。

## 要用户定（开工不等，合入前由主会话问）

1. **InfiniTime 的许可**：GPL-3.0，和总规则第 4 节“不用 GPL”冲突（Fossify 也是 GPL，但只照着做、不拷代码）。
   - 自己用不受影响。
   - 以后要把固件给别人，要么整个按 GPL 发布（files-3 的 Flipper、memo-3 的 ZSWatch 也是同样情况），要么删掉 `vendor/infinitime/`、换成同样行为的改写。
   - 默认：照 D-140 拷，隔离、单独提交。
2. **这次不做的参照功能**（默认不做）：
   - 新建闹钟、计时器沿用上次的设置（C4、G5）；
   - 每个闹钟单独的铃声和振动（D5、D6、G9）；
   - 快响前“跳过这一次”的通知（E5，要时间服务支持）；
   - 闹钟和计时器的导入导出（H1）；
   - 默认页签、一周起始日、贪睡时长、最长响多久这些时钟设置（H1）；
   - 计时器在走时的常驻提示（G20，归状态栏或通知）。
3. **保留的现有功能**（Fossify 没有，默认保留）：
   - 闹钟的“只响一次、每天、工作日”快捷（D9）；
   - 计时器的“1、5、15、30 分钟”快捷（G10）；
   - 世界时钟的时差（B4）。
4. **同时到点的计时器最多 4 张卡**（打断卡表的上限，不归本任务改）：默认超出的只在列表上显示“已到点”，不响第 5 张。要不要放宽归 notify-ui-1 或 dialogs-1。

## 和本批其他任务的边界

- **calendar-3**：
  - 它改时间服务的提醒和记录格式，并承诺“闹钟记录和行为完全不变”。本任务只用 `time.h` 的公开接口（`ink_alarm_next` 的游标遍历、记录里的 `next_s`、`alarm.*`），字段长度一律用 `sizeof`，不写死 15、48；标题会放宽到 30 字，`alarm.set` 的 `maxLength` 以代码为准，错误文案和对照表里的“15”都跟着上限走。
  - 它改 `ink_ui_datetime_picker`，本任务不用这个函数。
  - 后合入的一方重跑 `clock-alarm`、`timesvc`。
- **keyboard-2**：名字框只用共用单行输入框，不碰键盘文件。
- **small-apps-3、sms-3、contacts-3**：没有共同文件。共享文件（`en.tsv`、`ai_surface.c`、`capabilities.md`、`catalog.json`、CMake 列表）只在末尾追加。
- **home-1-check、settings-1-check、control-1-check**：根页面名保持 `Clock`，主屏常用里的“时钟”和 `shell.launch {"id":"clock"}` 照旧打开它（`home_one_page.c:58-59`、`settings_1.c:236`）。设置里的闹钟铃声（`sound_clock.c:76`）不动。秒表和计时器在走时的状态栏标记只写进报告，不改 `status_bar.c`。
- **dialogs-1、notify-ui-1**：只调用 `ink_interrupt_raise`、`ink_interrupt_update`、`ink_interrupt_end`，卡片样子和闹钟卡的文字不归本任务；卡表 4 张的上限只写进报告。
