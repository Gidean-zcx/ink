# calendar-3：日历照 Fossify Calendar 和 Etar 重写——整月一屏、议程、多个提醒、重复和例外（“只改这一次”“这次和以后”“全部”）、导入导出 ICS；农历和节气用我们自己的，宜忌接 almanac-2

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：交付计划这一行写的是“照着重写”（`docs/tasks/delivery-plan.md` 第 72 行）。第一步把 `docs/tasks/calendar-3/对照表.md` 单独提交（主会话已经起草了一版，见本任务附带的对照表；动手前逐行核对出处行号，补漏，有改动在提交说明里写清），之后才写代码；做完逐行验收，每行配截图或测试名，报告写做到了几行、哪几行没做。参照安卓的地方照第 11 节为这台设备改（墨水屏、灰阶、108 px、盖板键、内存、离线、家人和孩子）。

**第 11 节的“搬代码”这里不适用，原因写进 `SOURCE.md`**：两个参照都是安卓应用，不能在 S31 上编译或链接——Fossify 的事件模型和重复计算依赖 Room 数据库和 Joda-Time（`models/Event.kt:5-37` 的 import、`helpers/EventsHelper.kt` 全文经 `eventsDB`），ICS 读写依赖 `java.io` 和 `DateTimeZone`（`helpers/IcsImporter.kt`、`helpers/IcsExporter.kt`）；Etar 的 `calendarcommon2/` 依赖 `android.text.format.Time` 和 ContentProvider。所以没有要整文件拷的原文。我们要做的是：**照参照的算法逐段重写**（重复步进、例外、三种范围、全天提醒时刻、RRULE 和 VALARM 的读写），`SOURCE.md` 给每一段写对应的参照文件和行号、哪里照做、哪里按设备改及原因。已经搬进来的 C 原文只调用、不再拷：Mogo 老黄历（`system/services/time/vendor/mogo-calendar/`，经 `ink_almanac_date()`）、我们自己的农历和节气（`system/services/time/lunar.c`）。

## 为什么

用户 2026-11-12（D-129）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；D-131：先把外壳和现有应用（日历在其中）做到成熟可用；D-140：照成熟产品做，按设备改。交付计划 v4 第 3b 节“日历”一行：照 Fossify Calendar 和 Etar 重写，议程、多个提醒、重复和例外、“只改这一次”；农历用我们自己的；宜忌用 almanac-2；进试用版 22。交付计划第 8 节：“两个日期时间选择器没统一”并给了本任务和 settings-2。

调研（`docs/tasks/replan-sysapps/APPS.md` A06、`CURRENT.md` A06）：搬 Fossify 的事件和议程工作流、Etar 的重复系列和例外处理；ICS/RRULE 源码是完整实现的参照，不是能直接链接的库；保留我们的农历显示和统一的提醒执行；LVGL 自带的 calendar 只是控件。**参照路径**：新建跨日事件 → 设多个提醒 → 每周重复 → 只改某一次 → 导出再导入，例外还在。墨水屏四项（APPS.md A06）：月和周切换不滑动；类别写名字不靠颜色；七列月格实测触控并给议程入口；切月和改事件只局部刷新；选日和事项分区局刷。

## 现状和缺口（以 main `3baaf0e2` 的代码为准，逐行核过）

- 应用在 `system/apps/calendar/calendar.c`（439 行，一个文件，自写）：应用 id `upcoming`，名字“接下来”（`:430`）；四个页面 `Upcoming`、`Calendar`、`ReminderEdit`、`ReminderTime`（`:213-214`、`:370-371`）。旧“接下来”页把闹钟和提醒混在一起列（`:56-58`）。
- **数据就是时间服务里的提醒**：`system/include/ink/sys/time.h:77-94` 的 `ink_alarm_t`，闹钟和提醒共用一张固定表（PSRAM，`time_service.c:53-70`），全家合计 64 条（`time_service.c:38` `MAX_ALARMS`），存成设备级集合 `time.alarms` schema 1（`:46`），读入时按记录长度认旧格式（`:839`，没有 `silent` 字段的旧长度）。每条只有一种重复（`ink_repeat_t`：每周几、每月同一天、每年、农历每年），**没有间隔、没有结束条件、没有例外、不能只改其中一次、只有一个提醒（就在开始时刻）**；标题 48 字节，AI 参数写着最多 15 个字（`time_caps.c:348` `REMINDER_FIELDS`），编辑页的错误提示也写“最多15个字”（`calendar.c:410`）；“日程”就是不响铃的提醒（`silent`，到点不触发，`time_service.c:446`）。
- 单次、没有时长的提醒响过就从表里删掉（`time_service.c:456`）；`reminder.fired` 事件只带 `id`、`title`、`owner`（`time_service.c:368-379`），**不带开始时间**。
- 编辑页的重复只给 4 个选项（`calendar.c:16` `REPEAT_PRESETS = 4`），每月、每年、农历每年只有 AI 能设（`reminder.draft` 的 `repeat` 到 6，`calendar.c:419`）；“每周”只取开始那天是星期几（`time_caps.c:209-212`）。保存失败只有一句笼统的提示（`calendar.c:410`）。
- **删除没有撤销**：`reminder.cancel`（`time_caps.c:322-332`）不登记反操作；`tests/services_finish/undo_schedules.c` 只验了“新建能撤销”。原任务书说“用现有的撤销”，这条不成立，要新做（见第 5 项）。
- 月历在最大字号下分页（`calendar.c:124-152`），页面上两套翻页，当天的事每页只放 1 条（`:176`）；见 `docs/tasks/design-lang-2/calendar-six-weeks-max-1.png`。翻页和前后月用 `ink_ui_circle`（`:112-113`、`:140`）。
- 刷新：选一天、翻月格、日期时间选择器每按一下都整页重画（`calendar.c:224`、`:235`、`:402`），不符合 D-127。提醒表一变（`alarm.revision`）月历整页重画（`:431-435`）。
- 没有议程视图、搜索、ICS 导入导出、节气、宜忌、只读的详情页（点一件事直接进编辑页）。
- 农历来自 `system/services/time/lunar.c`（1901–2100），显示在月格第二行（`calendar.c:42-49`）。**节气我们也有**：`lunar.c:207` `ink_lunar_solar_term()`（1901–2100，cnlunar 数据，MIT），锁屏月历样式在用（`system/ui/lock_faces/faces.c:20`、`:178`，用的是文件里自己写的声明，`time.h` 里没有）。almanac-2 把我们的农历和 Mogo 在 2000–2099 逐日比过：36,495 天一致，**2097-08-07 到 2097-09-05 共 30 天差一天**（`docs/tasks/almanac-2/LUNAR-DIFF.md`，已在 main）。
- **宜忌已合入**（`fd04fe44`、`f8ff0b78`）：`system/include/ink/sys/almanac.h` 的 `ink_almanac_date()`（2000–2099，约 700 B 的结果，别放小栈上）、能力 `time.almanac`（`time_caps.c:351`，孩子可用）。表编进固件（约 44 KB），**不在卡上，没有“数据没装上”这种状态**（`vendor/mogo-calendar/SOURCE.md` 末段）；能失败的只有超出 2000–2099 和无效日期。宜忌词里有一个字“磑”（“安碓磑”）不在 GB2312，界面正文字体（`CMakeLists.txt:372`，`ascii,punct,pinyin,gb2312`）里没有它，直接显示会缺字（主会话用 Python 编码核过：数据里 382 个汉字，只缺这一个）。
- **别的模块也在读提醒**，改数据模型时它们必须照样对，而且都不归本任务改：
  - 锁屏下一件事 `system/ui/lock_mark.c:120-136`（`next_text`），`:130` 对会响、没有时长的提醒用 `a.next_s`，`:133-134` 用 `%.47s` 截标题；
  - 勿扰的“日历忙” `system/services/notify/notify_model.c:78-86`，靠 `ink_alarm_next` 加 `ink_alarm_occurrence`；
  - 提醒到点的通知 `system/services/notify/notify_service.c:661-670`（`on_missed`，`reminder.` 开头的事件都走这里）：标题“提醒”，正文只有标题，`title[48]` 按字节截（30 个字的标题会截在字中间）——**提醒到点不是中断卡片**，原任务书写的 `interrupt_card.c` 不对；
  - 能力 `calendar.list`、`reminder.list`（`time_caps.c:243-317` `list_kind`）；
  - 测试直接用 `ink_alarm_add(kind=INK_REMINDER)` 和 `time.alarms` 集合：`tests/time_settings.c:143-146`、`tests/calendar_events/events.c:137-145`（旧长度记录升级）、`tests/integration/timesvc.c:466`，另有 `tests/integration/notify.c`、`races.c`、`tests/lock_cover.c`、`tests/notify_model.c`、`tests/services_finish/{member_caps,removal_alarm,undo_schedules}.c`。
  - 主屏卡片 `apps/home/home.c:8-16` 只读**闹钟**，标题写死“接下来”，id `upcoming`；改名后卡片标题照旧是“接下来”（不归本任务，写进报告给 home-1-check）。
- 共用日期时间选择器 `ink_ui_datetime_picker`（`system/ui/theme.c:34-51`，声明 `ui_theme.h:87-89`）现在有三个调用方：日历（`calendar.c:307`）、时间设置（`system/ui/shell/time_settings.c:129`，归 settings-2）、**备忘的到期时间**（`system/apps/memo/memo.c:876`）。备忘已经只刷一行：它靠“减号按钮前面那个是数值标签”的子对象顺序找到标签，自己改字（`memo.c:647-662`）。`theme.c` 11-13 起归 dialogs-1（还没合）。
- tap-loss-1 已合：被动更新用 `ink_ui_redraw`（按着时推迟）和 `ink_ui_after_press`，`ink_ui_content_changed` 只在页面内容真变了才调（`system/include/ink/sys/ui.h:85-92`、`:158-168`）。
- 共用件都在：分页列表 `ink_ui_list_*`（`system/include/ink/sys/ui_list.h`，带页码和盖板键）、分段 `ink_ui_segments`、空状态 `ink_ui_empty`、列表行 `ink_ui_list_row`、搜索框 `INK_FIELD_SEARCH`（`textfield.h:17`）、文件选择器 `ink_file_picker_open`（`file_picker.h:9`；`.ics` 在文件服务里算 `INK_FS_OTHER`，`file_service.c:1664`）、按家人存的设置 `per_member`（`settings.h:39`）。“删除后页面留一个写着‘撤销’的按钮到下一次操作”有现成做法：相册 `gallery.c:566`、`:933`。dialogs-1 的底部面板没合。

## 成熟来源（照着重写，选型已定，不重新调研）

- **Fossify Calendar**（第一参照），`github.com/FossifyOrg/Calendar` 提交 `a437b8f2a354e0487043fd6b98734f9cfeb93933`，`app/src/main/kotlin/org/fossify/calendar/`（主会话已浅克隆核过行号）：
  - 事件模型 `models/Event.kt:47-55`（3 个提醒、间隔、规则、结束、例外）、`:76-180`（每月同一天 / 第几个星期几 / 最后一个星期几 / 最后一天的步进）、`:187-200`（提醒列表；全天事件开始算当天 0 点）、`:215-231`（每 N 周）、`:242-248`（例外日期）；
  - 重复展开 `helpers/EventsHelper.kt:608-758`（到某天或一直 / 共几次），例外过滤 `:525-529`；三种范围 `:242-278`（只改这一次）、`:280-305`（这次和以后）、`:307-323`（全部）、`:391-416`（截止到前一次）、`:428-452`（删掉这一次）；
  - 编辑页 `activities/EventActivity.kt:846-1010`（多个提醒、重复频率、重复细项、结束条件）、`:1591-1610`（标题空、结束早于开始）、`:1530-1561`（删除）、`:1732-1801`（保存时问范围）；
  - 提醒时刻 `extensions/Context.kt:196-226`（开始减提前分钟数，全天从 0 点算）；提醒选项来自 Fossify Commons 7.1.3（提交 `227e7d3cdea2343679eb0863bb597fc7a9200b4d`）`commons/src/main/kotlin/org/fossify/commons/extensions/Activity.kt:1631-1690`：关、准时、1、5、10、30、60 分钟、自定义，全天事件多一项“当天几点”（存成负数）；
  - 月视图 `helpers/MonthlyCalendarImpl.kt:15`、`:37-73`（固定 42 格，含前后月）；月加当天 `fragments/MonthDayFragment.kt:115-162`；日视图 `fragments/DayFragment.kt:47-127`（前一天、后一天，列表）；议程 `fragments/EventListFragment.kt:103-254`；搜索 `activities/MainActivity.kt:1369-1450`；回到今天 `:619`；
  - ICS：`helpers/IcsImporter.kt:143-190`（属性和 RRULE）、`:250-276`（EXDATE、RECURRENCE-ID）、`:305-318`（按 UID 和 LAST-MODIFIED 去重）；`helpers/Parser.kt:17-71`（RRULE 读）、`:118-216`（RRULE 写）、`:220-250`（时长）；`helpers/IcsExporter.kt:87-115`、`:144-196`；结果 `dialogs/ImportEventsDialog.kt:135-137`；
  - 设置 `activities/SettingsActivity.kt:501-506`（一周从哪天开始）、`:747-751`（默认提醒）、`:1102-1108`、`:1294-1382`（导入导出入口）。
- **Etar**，`github.com/Etar-Group/Etar-Calendar` 提交 `82ff55ea1fc035177054ce4b4aa8e0dd91059ef1`，`app/src/main/java/com/android/calendar/`：
  - `calendarcommon2/EventRecurrence.java:31-92`（频率、次数、到期、星期、每月第几）；
  - `event/EditEventHelper.java:159-162`、`:398-412`（只改这一次：另存一条，带原系列和原来的开始时刻）、`:414-456`（这次和以后：第一次就整组改，否则原系列截止、次数拆开）、`:458-470`（全部）、`:922`（`updatePastEvents`）、`:1101`（`isFirstEventInSeries`）；
  - `recurrencepicker/RecurrencePickerDialog.java:81`（间隔最多 99）、`:84`（次数最多 730）、`:248-312`（结束方式、每月按日期或按第几个星期几）；
  - `EventInfoFragment.java:1149-1152`（先看只读详情，再点编辑或删除）；`DeleteEventHelper.java:73-75`（删除的三种范围）；`agenda/AgendaByDayAdapter.java:213-238`（分组标题“今天/明天 + 星期”，过去的日子变灰）、`agenda/AgendaWindowAdapter.java:241-243`、`:578-643`（加载提示、搜索）。
- **Mogo 老黄历的字段和顺序**（只参考显示哪些内容，不搬画法，画法归已合的 lock-styles-1）：`ink-hw/upstream/oss/mogo-e4p` 提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`，`main/ui/ui_calendar.c:196-283`（宜忌、三神吉位、吉神宜趋、凶煞宜忌、值神、建除、冲煞；节气当天在月格下画线 `:278-282`）。
- **数据来源不换**：农历和节气用我们自己的 `lunar.c`；宜、忌、冲煞、神位、值神、建除只用 `ink_almanac_date()`；不改这两处的文件。

## 要做的

1. **数据：在时间服务里把提醒补成完整的日程**（提醒照旧由时间服务统一执行，一个定时器、一套重算，不另建提醒服务）。
   - **另开一张日程表，闹钟一个字节都不动**：新集合 `time.events`（设备级，每条带主人，schema 1，表在 PSRAM，至少 128 条），新的公开结构 `ink_event_t` 和 `ink_event_*` 接口写进 `time.h` 提醒那一节。理由：要放 30 个字的标题、3 个提醒、间隔、结束条件、例外，记录必然变长；如果直接加长 `ink_alarm_t`，闹钟的存储记录跟着变，违反“闹钟记录和行为完全不变”（clock-3 依赖）。`time.alarms` 以后只放闹钟，闹钟的可用条数因此从“和提醒合计 64”变成“闹钟 64”，写进报告。
   - **旧接口照旧能用，读提醒的模块不改**：`ink_alarm_add/next/cancel/occurrence` 对 `INK_REMINDER` 转到新表，`ink_alarm_next(INK_REMINDER)` 返回一份投影：`id` 不变；`next_s` 是**下一次开始的时刻**（不是提前提醒的时刻）；标题按字的边界截到 47 字节以内；`duration_s`、`all_day`、`silent`（没有提醒就是 true）照旧。`ink_alarm_occurrence()` 对提醒按 id 查新表、把例外算进去（删掉的那一次不出现，改过的那一次出现在新时间）。这样 `lock_mark.c`、`notify_model.c` 和上面那些测试不用改就对；`lock_mark.c` 原则上不碰，真要改只许改 `next_text`（`:120-136`）里的一两行，列进报告。
   - 重复（照 Fossify `Event.kt:76-231` 和 Etar `EventRecurrence`）：每天、每周（选几天，`workdays` 是周一到周五）、每月（同一天 / 第几个星期几 / 最后一个星期几 / 最后一天）、每年（同一天 / 第几个月的第几个星期几）、农历每年（照旧：闰月和三十的落法照 `time_service.c:266-277`）；间隔 1–99（Etar `:81`）；结束三种：一直、到某天为止、共几次（1–730，Etar `:84`）。每月 31 号、2 月 29 号的落法照 Fossify：没有那一天的月或年不出现（`Event.kt:112-144`），写进 `SOURCE.md`。
   - 例外（照 Fossify `EventsHelper.kt:242-305`、`:391-452` 和 Etar `EditEventHelper.java:398-456`）：删掉某一次（母系列记下这一天）；只改某一次（母系列记下这一天，改过的那一次另存一条，带母系列 id 和它原来的开始时刻）；这次和以后（原系列在这一次之前结束，“共几次”按已经过去的次数拆开；如果这一次就是第一次，整组直接改，Etar `:432-438`）。每个系列的例外日期最多 16 个（固定数组，放在记录里）；满了明说“这组重复的例外已满 16 个，请改成‘这次和以后’”，不悄悄丢。
   - 多个提醒（照 Fossify 和 Commons `Activity.kt:1631-1690`）：每件事 0～3 个，每个是“提前多少分钟”。选项：准时、5、10、30 分钟、1 小时、1 天（Fossify 把“1 天”放在自定义里；这里直接给，因为自定义要多步操作，按设备改）、自定义（数字加单位：分钟、小时、天，用共用的加减步进，不弹键盘）；全天事件多一项“当天几点”（存负数，照 Commons `:1676-1679`），提醒时刻 = 当天 0 点减提前量（`Context.kt:206-221`），写进 `SOURCE.md`。没有提醒就是现在的“日程”（到点不响）；旧的 `ring` 参数照旧能用：`ring=true` 没给提醒就等于“准时”，`ring=false` 就是没有提醒。
   - 一件事有几个提醒，就在每个提醒时刻触发一次 `reminder.fired`；开始时刻没到就不能把单次事件删掉（现在 `time_service.c:456` 响一次就删，要改）。事件参数在现有 `id`、`title`、`owner` 之外加：`at`（这一次本地开始时间“YYYY-MM-DD HH:MM”，全天写日期）、`lead_min`、`full_title`；`title` 照旧按字的边界截到 47 字节以内，旧的接收方不会截出半个字。通知里要写开始时间，归通知的负责任务（notify-ui-1），写进报告。
   - 标题放宽到 30 个字（存 96 字节）；AI 参数的 `maxLength` 改成 30。导入的事件记下原来的 UID（最多 64 字节，更长的存摘要），导出时原样写回；自己建的生成固定 UID。
   - 容量：日程至少 128 条（报告写记录大小和表的总大小，PSRAM 增量）；满了明说“日程已满（最多 N 条）”。
   - **旧数据自动迁移**（D-138）：开机时把 `time.alarms` 里 `kind = INK_REMINDER` 的记录（含没有 `silent` 字段的旧长度）转成新记录写进 `time.events`，**保留原 id**（对象名 `reminder:<id>` 和撤销都不变，新 id 不和它们撞），写成功后再从 `time.alarms` 删掉；中途断电重来结果一样（幂等）。时间、重复、响不响、全天、结束都不变；闹钟记录一条不碰。
   - 家人退出时（`ink_time_forget_member`，`time_service.c:703`）新表一起清；核对备份恢复（`system/services/backup/`）对设备级集合的处理，新集合和 `time.alarms` 一样对待，不一样写进报告。
2. **月视图**（照 Fossify 月视图 `MonthlyCalendarImpl.kt:15` 和“月加当天” `MonthDayFragment.kt:115-162`）：
   - 固定 42 格（6 周），每个月都是 6 行，前后月的日子用浅灰字（位置和灰度区分，另在快照写清“上月/下月”），点前后月的日子就切到那个月并选中它。**整月一屏，最大字号也不分页**，去掉月格翻页和页码点。
   - 高度账（屏 684×1216，状态栏 108）：月格 6×108 = 648；星期一行；顶上要放标题“2026年11月”、前一月、后一月、“今天”、月和议程的分段切换（宽度算下来一行放不下：标题约 280 px，加四个 108 宽的按钮超过 608 px 的可用宽度，所以按两行 108 排或另想办法，原任务书“合成一行”作废）；底下所选那一天的区块至少一行文字加一行事件。默认字号剩约 309 px，最大字号（120%）约 192 px。**先做这一项并出最大字号和孩子身份的截图**，放不下就停下写清。
   - 每格：日期数字，下面一行最多两个字：我们的农历日（初一到三十），初一写月名（正月、二月……十月、冬月、腊月，闰月写“闰四”），节气当天写节气名（`ink_lunar_solar_term`）。这样最大字号下也是两个字（约 74 px，格宽约 97 px）。格高固定 108，按字形实际高度排两行（最大字号下两行的行高加起来 126 px，比 108 高；字形本身约 43+37 px，放得下），不缩字、格子不长高；还放不下就停下写清。今天：数字下加粗线（不靠颜色）；选中：整格反白。有事的格子打标记：日程（没有提醒）一个空心小方块，会响的一个实心小方块（形状区分，不靠颜色）。
   - 下面写所选那天：“11月13日 周五 · 十月廿四 · 宜 祭祀 出行”（宜取前两个词；2000–2099 以外不写宜），点它进当天页；放得下就列这天的事，放不下就一行“这天 3 件事 ›”。
   - 选一天只刷旧格、新格和下面那一块（D-127）；盖板的上一页、下一页键翻月；提醒表变化（`alarm.revision`）时用 `ink_ui_after_press` 只重画受影响的格子和下面那一块，按着屏的时候不动（tap-loss-1）。
   - 一次算整个 42 天窗口：每件事从窗口开头往后走它自己的重复，不要 42 天 × 每件事各算一遍；报告写 S31 模型上打开月视图的耗时，不加转圈。
3. **议程视图**（照 Fossify `EventListFragment` 和 Etar 议程）：从今天起按日期分组，分组标题只有灰字（“今天 · 11月13日 周五”“明天 · …”“11月20日 周四”，Etar `AgendaByDayAdapter.java:213-238`）；每行写时间（或“全天”）、标题，第二行灰字写“每周”“已改这一次”“提前 10 分钟”（有几个提醒写最早的）。闹钟照旧列进来（旧“接下来”页就是这样，`calendar.c:56-58`），灰字标“闹钟”，点开走 `clock.open`；要不要去掉放进“要用户定的”。Fossify 的无限滚动和“先取 6 个月、不够 30 件再加 3 个月”（`:103-125`）改成共用分页列表（页码、盖板键翻页），往前看用“更早的”按钮，不往回滚。空的时候用共用空状态“还没有接下来的事”加一个“加一件”。
   - 搜索：顶上共用搜索框（`INK_FIELD_SEARCH`），按标题搜，结果也按日期分组、写页码。Fossify 要至少 2 个字（`MainActivity.kt:1386-1387`），中文 1 个字就有意义，改成 1 个字起；按键盘的“搜索”键才搜，不是每打一个字刷一次屏（按设备改：墨水屏）。范围照 Fossify 先前后两年，不够再扩大到全部（`:1388-1404`）。没结果写“没有找到‘xx’”。
   - 月视图和议程用共用分段切换，按家人记住上次的视图；“今天”一点就回到今天（月视图选中今天、议程回第一页）。
4. **当天页和老黄历**（照 Fossify `DayFragment`，内容参考 Mogo `ui_calendar.c:196-283`）：
   - 顶上“前一天”“后一天”两个写字的按钮（Fossify `DayFragment.kt:47-70`；也是月格窄于 108 的补偿，见下面“月格例外”）；日期、我们的农历、干支（年月日）、节气；宜、忌两行全文（长了换行）；一个主按钮“加一件”；下面是这天的事（共用分页列表）。
   - “更多老黄历 ›”进详情页：冲煞（“冲{clash} · 煞{sha}”）、三神方位（财神、喜神、福神）、吉神宜趋、凶煞宜忌、值神（带黄道或黑道）、建除，最后一行灰字“这些是民俗说法”。
   - 日历通过自己代码里的一个小包装函数调用 `ink_almanac_date()`（测试可以替换它；不改 almanac 的文件）。2000 年以前、2099 年以后：那一行写“老黄历只有 2000–2099 年”，农历、节气照常。包装函数返回失败（测试里模拟）：那一行写“这天的老黄历查不到”，页面其余照常，**不白屏、不崩**。（原任务书的“数据没装上，插电脑会补上”删掉：表在固件里，没有这种状态。）
   - 2097-08-07 到 09-05 那 30 天：农历照 `lunar.c` 显示，页面不印 almanac 自带的 `lunar_text`。节气只用 `lunar.c` 的；加一个测试把 2000–2099 每天的 `ink_lunar_solar_term` 和 `ink_almanac_date().solar_term` 比一遍，有差异列进报告，不当失败。
   - “磑”：加一个测试，检查当天页和详情页显示的宜忌、神煞每个字都在界面字体里；已知只缺“磑”，本任务不改字体文件，报告请主会话在 `CMakeLists.txt:372` 的字集后面加这一个字（font-pack-1 之后字体分区还剩约 2 MB）；加之前这条测试先登记成已知例外并写明原因，加字后去掉例外。
5. **详情页、编辑页和三种范围**：
   - **点开一件事先看只读详情**（照 Etar `EventInfoFragment.java:1149-1152`；memo-3 的教训：带着键盘的编辑器不是阅读页）：标题、时间（跨天写两行）、重复说明（“每 2 周的周一、周三，到 12月31日为止”）、提醒列表、“已改这一次”；两个写字的按钮“编辑”“删除”。
   - 编辑页（照 Fossify `EventActivity`）：标题（共用输入框）、全天开关、开始、结束（可以跨天）、提醒（最多 3 个，“加一个提醒”“删掉”都是写字的按钮）、重复（选频率后出对应细项：每周选哪几天用 7 个 108 px 的开关按钮；每月选哪种；间隔和结束条件），农历每年时显示对应的农历日期。重复细项和提醒选项各是一页共用列表（Fossify 是弹框，按设备改：底部面板 dialogs-1 没合，用整页列表行，不自己造对话框）。保存失败说清是哪一项：“标题不能空”“结束要晚于开始”“到期日要晚于开始”“日程已满（最多 N 条）”“这组重复的例外已满”。
   - 对重复的事保存或删除时先问范围：一页三行共用列表行“只改这一次 / 这次和以后 / 全部”（删除时是“只删这一次 / 删掉这次和以后 / 全部删掉”，Etar `DeleteEventHelper.java:73-75`）。Fossify 的删除还要再点一次“是”（`DeleteEventDialog.kt`），按 D-105 不算高风险，不弹确认卡。
   - **删除能撤销（新做）**：`reminder.cancel` 登记反操作（例如新的 `reminder.restore`，只给 `system.undo` 用，进 `ai_surface.c` 的不给 AI 表并写理由）：删全部恢复整条（含改过的那几次），删这一次去掉那个例外，删这次和以后恢复原来的结束条件。撤销时限和新建一样（`time_caps.c:18` `UNDO_MS`）。页面照相册的做法（`gallery.c:566`、`:933`）留一个写着“撤销”的按钮，直到下一次操作，不用几秒自动消失的提示条。
   - Fossify 离开编辑页时有“没保存，要丢掉吗”（`EventActivity.kt:274-301`）：照做成一页两行“保存”“不保存”，不用弹框。
6. **日期时间选择器**（交付计划第 8 节）：日历只用共用的 `ink_ui_datetime_picker`，不自己写。**这次不改 `theme.c`**（它归 dialogs-1，还没合；三个调用方都靠它现在的子对象顺序）。照备忘的做法（`memo.c:647-662`）在日历这边只刷一行：按 + 或 − 只改那一行的数字标签并只刷它；同时更新这一行两个按钮的参数（现在参数是算好的绝对值，`calendar.c:277-286`）；改月份使“日”超过当月天数时，“日”那一行也一起刷。全天事件只选年月日：日历把时、分两行藏起来（不画、不能点、快照里没有）。AI 调 `reminder.draft` 也只刷变了的行。`tests/small_gaps_2/ui.c`（两个调用方用同一套步进）、`tests/memo_3/` 照样过。把“选择器自己提供只刷一行和只选日期”的建议写进报告，交给 dialogs-1；`time_settings.c` 的整页重画归 settings-2，本任务不碰。
7. **导入导出 ICS**（照 Fossify `IcsImporter`、`IcsExporter`）：
   - 导出：当前家人的全部日程（含过去的；Fossify 让人选要不要过去的，`ExportEventsDialog.kt:50-52`，这里不问，少一步）写成一个文件到他自己的空间 `/me/日历导出-YYYYMMDD.ics`（重名加“-2”；核对文件服务对中文文件名的处理，不行就用英文名并写进报告）。改过的那一次照 RFC 5545 写成同 UID 带 `RECURRENCE-ID` 的一条（Fossify 没写这个，再导入会乱，按设备改：验收要求导出再导入例外还在）；农历每年照 RFC 7529 写成 `RRULE:RSCALE=CHINESE;FREQ=YEARLY;BYMONTH=8;BYMONTHDAY=15`（闰月写 `BYMONTH=4L`），读回来还原。没卡、卡借给电脑、空间不够都写清原因（`INK_FS_ERR_NO_CARD`、`_USB`、`_NO_SPACE`）。
   - 导入：用系统文件选择器（`ink_file_picker_open`，只调用；`.ics` 属于 `INK_FS_OTHER`），流式按行读（折行照 Fossify `IcsImporter.kt:115-118`），缓冲放 PSRAM。认 `VEVENT` 的 `SUMMARY`、`UID`、`DTSTART`、`DTEND`、`DURATION`、全天（`VALUE=DATE`）、`TZID`（我们 9 个时区里有的照用，没有的按设备当前时区，和 Fossify 一样）、`RRULE`（第 1 项支持的那些，加 `RSCALE=CHINESE`）、`EXDATE`、`RECURRENCE-ID`、`VALARM` 的 `TRIGGER`（`ACTION:DISPLAY`）、`LAST-MODIFIED`（同 UID 而且不比已有的新就算“已存在”，Fossify `:305-318`）。**看不懂的规则不猜**（Fossify 会忽略不认识的部分、可能错算，`Parser.kt:17-71`）：`RRULE` 里有我们不支持的部分（`BYSETPOS`、`BYHOUR`、多个 `BYMONTHDAY`、`FREQ=HOURLY` 等）整条跳过并计数。日程满了停下，后面的计数。
   - 导完给一页结果（Fossify 是一句提示，`ImportEventsDialog.kt:135-137`，按设备改成留着的一页）：“导入 12 件；跳过 2 件看不懂的规则；1 件已存在；日程满了，3 件没导入”。
   - 孩子：照孩子现在的文件权限，能用文件选择器就能导入导出自己的，不能就不显示这两个入口，快照写原因。
8. **日历设置**（只做两项，照 Fossify `SettingsActivity.kt:501-506`、`:747-751`）：一周从周一还是周日开始（默认周一，【估算】国内手机日历和 GB/T 7408 的习惯，放进预览请用户看）；新建时默认的提醒（0～3 个，默认一个“提前 10 分钟”）。按家人存（`per_member`）。系统设置的搜索入口（`settings_registry.c`）不归本任务，写进报告给 settings-2。
9. **应用名和入口**：应用 id 仍是 `upcoming`，名字改成“日历”（en.tsv 已有“日历 Calendar”），打开是月视图、选中今天。旧的“接下来”页并进议程，旧代码删掉。页面名：`Calendar`（月）、`Agenda`（议程和搜索结果，替代 `Upcoming`）、`Day`、`Almanac`、`EventInfo`、`ReminderEdit`、`ReminderTime`（保留，少改测试）、编辑的子页（重复、提醒、范围）、导入结果、日历设置。
10. **能力和快照**（每个用户操作都是能力，按钮和 AI 走同一个入口）：
    - 现有的 `calendar.*`、`reminder.*` 名字不改；`reminder.*` 仍登记在应用 id `clock` 下（不搬，免得和 clock-3 冲突）。`calendar.prev/next` 变成“当前页列表的上一页/下一页”，`calendar.select` 照旧选当月一天，`calendar.month_prev/next` 照旧。
    - `reminder.add`、`reminder.update` 加参数：`reminders`（提前分钟数的数组，最多 3 个，负数是“全天当天几点”）、`interval`、`weekdays`、`monthly`（`day`、`weekday`、`last_weekday`、`last_day`）、`until` 或 `count`；旧的 `repeat` 字符串和 `ring` 照旧认。`reminder.update`、`reminder.cancel` 加 `occurrence`（那一次原来的本地开始时间）和 `scope`（`this`、`following`、`all`）。**对重复的事改或删时不给 `scope` 就拒绝，并说明要给哪个参数**。
    - 新加（应用 id `upcoming`）：切换视图、回到今天、按标题搜索、打开某一天（含老黄历和“更多老黄历”）、前一天/后一天、打开某件事的详情、导出、导入（AI 给 `/me` 下的路径，按钮走文件选择器）、日历设置两项。`calendar.grid_prev`、`calendar.grid_next` 删掉，从 `ai_surface.c` 表里删并列进报告。
    - `calendar.list` 每条加：系列 id、这一次原来的开始时间、重复说明、是否改过、提醒列表。
    - 快照写清：月份、所选日期、农历、节气、宜忌摘要、每格是否有事（日程或会响）；每行的时间、标题、重复、是否改过、提醒；编辑页每一项的当前值；当天页和详情页的老黄历字段和“这些是民俗说法”；导入结果的四个数。
    - 孩子照旧能用（`child_ok`）；家人之间互相看不到、搜不到、导不出。

## 共同要求（页面任务都一样）

- 来源：功能、页面、状态、出错处理照对照表一个不落；`docs/tasks/calendar-3/SOURCE.md` 写每一部分对应参照的文件和行号、哪些没照搬、为什么。已知不做（对照表“不做”行，报告“要用户定的”里列出，不当作“不需要”）：周视图和日视图的时间格、年视图、账户同步（CalDAV）和下拉刷新、参与人、颜色和日历分类、每件事单独的时区、地点和备注（要另做按家人的存储）、节日和法定假日调休（没有订阅的假日日历）、生日和纪念日自动导入、打印、小组件、复制事件、分享事件、提醒用邮件、提醒铃声和稍后再响的设置（稍后再响现在只有闹钟有，提醒归通知）。
- 新设计语言：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c`）；不画粗框和圆圈；能点的不小于 108 px；一屏最多一个主按钮；行间细分隔线；没内容的区块不画，整页空才用共用空状态。不写死像素和颜色（`tools/ui_scale_check.py`，即 `ui-scale` 测试）。
- **月格是唯一的例外**：七列放不下 108 宽（684 px 七等分约 97 px），月格用满整屏宽度七等分，高 108；每一天也能从议程和当天页的“前一天/后一天”到。写进报告，108 的检查里把它登记成例外，不是放宽检查。
- 墨水屏：不用动画、过渡、惯性、转圈；切月、切视图、翻页都是一次刷新；按下反馈只刷按下的控件（D-31）；被动更新（提醒表变化、跨天）按着屏时等手抬起（`ink_ui_after_press`），只在页面内容真变了才 `ink_ui_content_changed`。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作、少刷新（D-127）、密码不给 AI 不进日志。离线：日历全在本机，没有要联网的部分。
- 文字大小：跟系统“文字大小”一起缩放，默认、最大一档、孩子身份各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 原地重写，旧页面代码删掉，不留两套；`calendar.c` 可以拆成几个文件。
- **共享文件（交付计划第 4 节第 3 条）**：不改 `assets/i18n/en.tsv`，新文案的英文写进 `docs/tasks/calendar-3/I18N-ADD.tsv`，由主会话合；不提交 `docs/system/capabilities.md`，只确认 `python3 tools/capabilities.py` 能跑通；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），只在各表末尾追加本任务的行，删掉的两个能力从表里删。
- 截图放 `docs/tasks/calendar-3/`，单张不超过 300 KB，先看干净的整页图。

## 范围和预算

- 只改：
  - `system/apps/calendar/`（可以拆成几个文件）；
  - 时间服务里提醒和日程的部分：`system/services/time/time_service.c`、`time_caps.c`（提醒、`calendar.list`、`reminder.list` 那几段）、`time_internal.h`、`system/include/ink/sys/time.h` 提醒那一节（加 `ink_event_t` 和 `ink_event_*`），外加农历那一节一行 `ink_lunar_solar_term` 的声明（函数在 `lunar.c:207`，签名和 `faces.c:20` 的自写声明一样）；可以新加文件（例如 `system/services/time/events.c`、`recur.c`、`ics.c`）；
  - 不得不改时 `system/ui/lock_mark.c` `next_text`（`:120-136`）里的一两行；
  - 日历和提醒相关测试（`tests/calendar/`、`tests/calendar_events/`、`tests/reminder_edit/`、新的 `tests/calendar_3/` 和夹具）、CMake 里新文件的那几行（只在末尾追加）、`docs/tasks/calendar-3/`。
- 不碰：`system/services/time/lunar.c`；`system/services/time/almanac*` 和 `vendor/mogo-calendar/`（只读着用）；`system/ui/theme.c`、`ui_theme.h`（dialogs-1）；`system/apps/clock/`（clock-3）；`system/apps/memo/`；`system/services/notify/`（勿扰“日历忙”要靠第 1 项自动对，通知里写开始时间归 notify-ui-1，不对就写进报告）；`system/ui/shell/shell_home.c`、`apps/home/home.c`（home-1-check：卡片标题“接下来”写进报告）；`system/ui/lock_faces/*`；`system/ui/shell/time_settings.c`、`settings_registry.c`（settings-2）；`system/ui/file_picker.c`（只调用）；字体和 `CMakeLists.txt:372` 的字集（“磑”写进报告）；`assets/i18n/en.tsv`、`docs/system/capabilities.md`。
- 时间服务的协调：clock-3 不碰 `system/services/time/`（它的任务书第 46 行），日历独占这一块；闹钟的所有现有测试（`tests/clock_alarm.c`、`tests/integration/timesvc.c` 的闹钟部分、`tests/services_finish/undo_schedules.c` 等）必须不改照过。
- 产品代码不超过 **2,000 行**（删掉的旧代码不抵）：页面 1,050，时间服务里的日程表、重复、例外、多个提醒、旧接口投影、迁移和删除撤销 600，ICS 导入导出 350。比原任务书的 1,900 多 100 行：另开日程表和旧接口投影（为了闹钟一个字节不变）、删除撤销（原来以为现成，其实没有）。交付计划写的 700 是按只改页面估的，主会话要改计划。超了停下写清，不砍内容。
- 固件增量不超过 40 KB（almanac 的表已在 main，不算）；内部内存不增加；PSRAM 增量写进报告（预计日程表约 50 KB）。
- 起步：main（已含 design-lang-1/2、home-1、lock-1、control-1、settings-1、settings-d137、notify-svc-1、tap-loss-1、almanac-2、lock-styles-1、memo-3）。

## 和本批其他任务的边界

- clock-3：不碰时间服务；`reminder.*` 能力的登记在日历这边（`calendar.c:412-420`）和 `time_caps.c`，clock-3 不搬它们；`alarm.*` 和闹钟记录本任务一个字节不动。
- keyboard-2：日历只用共用输入框和搜索框，不碰键盘和 `textfield.c`。
- home-1-check：`apps/home/home.c` 卡片标题“接下来”、主屏“今天的事”没有日程，都只写进报告。
- settings-1-check / settings-2：日历设置的搜索入口、`time_settings.c`、`tests/time_settings.c` 里用 `calendar.next` 的那一段（本任务只改它的定位方式，改了列进报告）。
- 共用测试文件（`tests/integration/ai_surface.c`、`tests/design_lang_1.c`、`tests/i18n_shell_remaining.c`、`tests/textfield_forms.c`）别的任务也在改：只动日历那几行，各表只在末尾追加。

## 验收

1. **照参照路径走一遍**（模拟器脚本，时间注入，每步截图并断言）：
   - 新建“出差”11-20 20:00 到 11-22 18:00（跨天），提醒“提前 1 天”和“提前 1 小时”→ 月格 20、21、22 三天都有标记 → 11-19 20:00 触发一次、11-20 19:00 再触发一次；**断言 `reminder.fired` 的参数 `full_title`=“出差”、`at`=“2026-11-20 20:00”**，并截图通知（通知文字归 notify-ui-1，写得不对只进报告）；
   - 新建“周会”每周一 10:00–11:00，提醒提前 10 分钟，到 12-31 为止 → 议程按日期分组列出 → 12-31 之后不再出现；
   - 把 11-23 那一次改到 11-24 15:00（只改这一次）→ 11-23 没有、11-24 15:00 有，别的周一不变，11-24 14:50 触发；
   - 删掉 11-30 那一次 → 那天不触发，`calendar.list`、锁屏的下一件事、勿扰的“日历忙”都没有这一次 → 点“撤销”，三处都回来；
   - 从 12-14 起改成 14:00（这次和以后）→ 12-07 还是 10:00，12-14 起是 14:00；同样的操作在“共 8 次”的系列上，次数拆开后总数不变；
   - 导出 ICS → 删掉全部 → 导入 → 议程和导出前逐条一样（包括删掉的那一次、改过的那一次、三个以内的提醒、到期日、农历每年）；再导入一次，结果页写“已存在”；
   - 新建“妈妈生日”农历每年（2026-09-25 建，即农历八月十五）→ 2027-09-15 出现；闰月和三十的生日照现有规则断言落在哪天；
   - 同一组操作用能力调用（来源是 AI）再走一遍，结果一样；对重复的事改或删时不给 `scope`，必须被拒绝并带说明。
2. **老黄历**：2026-02-17 的当天页宜、忌、冲煞、神位和 `time.almanac` 返回的逐字一致；1999-12-31 写“老黄历只有 2000–2099 年”，农历照常；替换包装函数模拟查询失败，那一行写原因、页面其余照常；2097-08-07 月格和当天页的农历都是 `lunar.c` 的；节气对照表跑完、差异列进报告；“磑”的字体检查按第 4 项。反向对照：把宜忌改成读错一天，这条必须失败。
3. **迁移**（D-138）：用 schema 1 的夹具开机（同一个 `time.alarms` 里有闹钟、新长度提醒、没有 `silent` 字段的旧长度提醒），提醒全部进了 `time.events`、id 不变，时间、重复、响不响、全天、结束都不变；闹钟一条不变（逐字节比记录）；迁移中途断电重来结果一样。反向对照：去掉迁移，这条必须失败。
4. **刷新**（模拟器显示统计断言）：选一天只刷旧格、新格和下面那一块；选择器按一下 + 只刷那一行；跨分钟只刷状态栏的时间（`410cc1dd` 的断言保留）；提醒表变化时月视图只刷受影响的格子；按着屏时被动更新不发生。反向对照：把选一天改回整页重画，这条必须失败。报告写每个主要操作刷几次屏（打开、切月、选日、切视图、翻页、搜索、保存、删除、撤销）。
5. **整月一屏**：默认、最大字号和孩子身份下，2026 年 8 月（周一或周日起都是六周）整月在一屏里，没有月格翻页；月格高 108，宽是整屏七等分；前后月的日子是浅灰并能点。
6. **家人和孩子**：A 的日程 B 看不到、搜不到、导不出；孩子能看自己的、能加。
7. **改前改后截图**：月视图、议程（含搜索结果和分组）、当天页和老黄历详情、事件详情、编辑页（重复细项、多个提醒）、改范围的三选一、导入结果页、日历设置，默认、最大文字大小和孩子身份各一套。
8. 主工程普通全量 ctest 过（含 `ui-scale`），先清空 `build/test-out`；ASan 跑日历、提醒、时间服务、锁屏、通知、备忘相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告；`gcccheck` 过。报告写：固件增量、PSRAM 增量和新记录大小、日程容量、产品代码行数（按三块分开）、对照表做到几行、**改过的每一个已有测试**（预计 `tests/calendar/`（`grid_next` 的三处换成整月一屏的断言）、`tests/calendar_events/`、`tests/reminder_edit/`、`tests/small_gaps_2/ui.c`、`tests/textfield_forms.c`、`tests/i18n_shell_remaining.c`、`tests/design_lang_1.c`/`.py`、`tests/time_settings.c`、`tests/integration/ai_surface.c`；只改页面名、定位方式和因“整月一屏”“应用改名”“议程替代接下来”而变的结构，业务断言一条不放宽）、改名或删除的能力、改过的别的模块的行（`lock_mark.c` 如果改了）、`lunar.c` 和 Mogo 那 30 天差异及节气差异的处理、交给别的任务的事（通知写开始时间和 96 字节标题、主屏卡片标题、设置搜索入口、选择器的建议、“磑”）。
