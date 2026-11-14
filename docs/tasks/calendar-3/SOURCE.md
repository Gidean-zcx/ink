# calendar-3 的来源

## 为什么没有整文件拷的原文（D-140 第 11 节“搬代码”不适用）

两个参照都是安卓应用，在 S31 上不能编译也不能链接：

- Fossify Calendar（`github.com/FossifyOrg/Calendar`，提交 `a437b8f2a354e0487043fd6b98734f9cfeb93933`，GPL-3.0）：事件模型和重复计算依赖 Room 数据库和 Joda-Time（`models/Event.kt:5-37` 的 import，`helpers/EventsHelper.kt` 全文经 `eventsDB`），ICS 读写依赖 `java.io` 和 `DateTimeZone`（`helpers/IcsImporter.kt`、`helpers/IcsExporter.kt`）。另外它是 GPL，按总规则第 4 条本来也不能拷进来。
- Etar（`github.com/Etar-Group/Etar-Calendar`，提交 `82ff55ea1fc035177054ce4b4aa8e0dd91059ef1`，GPL-3.0）：`calendarcommon2/` 依赖 `android.text.format.Time` 和 ContentProvider。
- Fossify Commons 7.1.3（提交 `227e7d3cdea2343679eb0863bb597fc7a9200b4d`）：只看了提醒选项。

所以做法是照参照的算法逐段用 C 重写，下面每一段写对应的参照文件和行号（Fossify 在 `app/src/main/kotlin/org/fossify/calendar/` 下，Etar 在 `app/src/main/java/com/android/calendar/` 下）。已经在仓库里的 C 原文只调用、不拷：Mogo 老黄历（`system/services/time/vendor/mogo-calendar/`，经 `ink_almanac_date()`），我们的农历和节气（`system/services/time/lunar.c`）。

## 时间服务（`system/services/time/events.c`、`ics.c`、`time_caps.c` 的提醒部分）

| 部分 | 参照 | 照做 | 按设备改和原因 |
| --- | --- | --- | --- |
| 事件记录 `ink_event_t` | Fossify `models/Event.kt:40-71`（3 个提醒、间隔、规则、结束、例外、parent_id、import_id、last_updated） | 3 个提醒、间隔、结束（日期或次数）、例外日期、母系列 id、UID、修改时间 | 固定大小的 C 记录（约 370 字节），例外最多 16 个放在记录里（满了明说“请改成这次和以后”），存在新的设备级集合 `time.events`，闹钟记录一个字节不动 |
| 每天、每周 | `Event.kt:76-110`、`:215-231`（周从周一算，每 N 周） | 同样按周一开头的周算“第几周” | 按“周期”一次展开一周里的几天，不像 Fossify 一天一天往后加 |
| 每月同一天 / 第几个星期几 / 最后一个星期几 / 最后一天 | `Event.kt:88-94`、`:146-180` | 四种都有；第 5 个星期几不存在的月不出现 | — |
| 31 号、2 月 29 号 | `Event.kt:112-144` | 没有那一天的月或年不出现（`by_day()` 返回“没有”） | — |
| 每年同一天 / 第几个月的第几个星期几 | `EventActivity.kt:986-1010`、`Event.kt:88-94` | 照做 | — |
| 农历每年 | 无（我们自己的） | 照旧的规则：闰月多数年份落在平月，短月的三十落在最后一天（原 `time_service.c:266-277` 的做法） | 用 `lunar.c` 的 `ink_lunar_to_solar` |
| 重复展开 | `EventsHelper.kt:608-758`（到某天或一直 / 共几次） | 到某天（含当天）、共几次（例外也算次数，同 RFC 5545） | 不数次数时直接跳到查询范围附近（`skip()`），不从第一次一路算；一次查询一个事件走一遍 |
| 例外过滤 | `EventsHelper.kt:525-529` | 例外日期按“那一次原来的本地日期”记 | — |
| 只改这一次 | `EventsHelper.kt:242-278`；Etar `EditEventHelper.java:398-412` | 母系列记例外日，改过的那一次另存一条单次事件，带母系列 id 和原来的日期、时刻 | — |
| 这次和以后 | `EventsHelper.kt:280-305`、`:391-416`；Etar `:414-456`、`:1101` | 原系列在这一次之前结束；“共几次”按已经过去的次数拆开（有次数时只拆次数，不再加到期日）；这一次就是第一次时整组直接改（Etar `:432-438`） | 新系列把这一次以后的例外日带过去 |
| 全部 | `EventsHelper.kt:229-240`、`:307-323`；Etar `:458-470` | 保留系列原来的开始日，套上新时间（按打开的那一次移动的天数平移） | — |
| 删掉这一次 / 这次和以后 / 全部 | `EventsHelper.kt:428-452`；Etar `DeleteEventHelper.java:73-75` | 三种范围；删全部连改过的那几次一起删（Fossify 同） | 能撤销：删之前把受影响的记录存一份（最近两次，PSRAM），新能力 `calendar.restore` 原样写回（Fossify 删了找不回，按 D-105 和撤销规则改） |
| 提醒时刻 | `extensions/Context.kt:196-226`；`Event.kt:187-200` | 时刻 = 这一次开始 − 提前分钟；全天事件从当天 0 点算；全天的负数是“当天几点后多少分钟”（Commons `Activity.kt:1676-1679`） | 一个时间服务定时器统一执行；每个提醒各触发一次 `reminder.fired`，参数加 `at`、`lead_min`、`full_title` |
| 单次事件响过就删 | 原 `time_service.c:456` | 没有时长的单次事件，最后一个提醒响过、而且开始时刻也过了才删 | 开始前（例如“提前 1 天”响过）不删 |
| 时钟往回调 | — | — | 已经响过的不再响（按 `done_s` 往后排）；关机期间到点的照旧报“错过”（按 `done_s` 补报） |
| 旧接口投影 | 无 | — | `ink_alarm_next(INK_REMINDER)` 给 `ink_alarm_t` 投影（`next_s` 是下一次开始，标题按字截到 47 字节以内）；`ink_alarm_occurrence` 对提醒按 id 查事件表，例外算进去 |
| 迁移 | 无（D-138） | — | 开机读 `time.alarms`，`kind = INK_REMINDER` 的记录（含没有 `silent` 的旧长度）转成事件写进 `time.events`，id 不变，写成了再删旧的；已经在 `time.events` 里的直接删旧的（断电重来一样） |
| id 不撞 | 无 | — | 事件的新 id 也从 `time.alarms` 的计数器取；开机把计数器推过最大的事件 id |

## iCalendar（`system/services/time/ics.c`）

| 部分 | 参照 | 照做 | 按设备改和原因 |
| --- | --- | --- | --- |
| 折行、BEGIN/END | `IcsImporter.kt:110-130` | 续行（空格或 Tab 开头）接到上一行 | 1 KB 一块流式读，行缓冲 1 KB，都在 PSRAM |
| SUMMARY、UID、DTSTART、DTEND、DURATION、全天、TZID | `IcsImporter.kt:144-175`、`:478-520`；`Parser.kt:220-250` | `VALUE=DATE` 是全天；`TZID` 是我们 9 个时区之一就按那个时区换算，否则按设备时区（同 Fossify） | UID 超过 63 字节存 FNV-1a 摘要（`h-` 加 16 位十六进制） |
| RRULE 读 | `IcsImporter.kt:176-181`；`Parser.kt:17-71` | FREQ（DAILY/WEEKLY/MONTHLY/YEARLY）、INTERVAL、COUNT、UNTIL、BYDAY、BYMONTHDAY、BYMONTH、WKST | **看不懂的部分整条跳过并计数**（BYSETPOS、BYHOUR、BYMINUTE、BYYEARDAY、BYWEEKNO、多个 BYMONTHDAY、带序号的 BYDAY 不止一个、HOURLY 等）：Fossify 会忽略不认识的部分，可能算错；加 `RSCALE=CHINESE`（RFC 7529）读农历每年，`BYMONTH=4L` 是闰月 |
| RRULE 写 | `Parser.kt:118-216` | 同样的字段 | 有次数只写 COUNT，否则写 UNTIL；农历写 `RSCALE=CHINESE;FREQ=YEARLY;BYMONTH=8;BYMONTHDAY=15` |
| EXDATE | `IcsImporter.kt:255-279`；`IcsExporter.kt:144-196` | 读写例外日 | 改过的那一次不写 EXDATE，写成自己的 VEVENT |
| RECURRENCE-ID | `IcsImporter.kt:280-289` | 读：挂到同 UID 的母系列（等全部系列读完再挂） | 写：照 RFC 5545 写同 UID 带 `RECURRENCE-ID` 的一条（Fossify 不写，再导入例外会丢） |
| VALARM | `IcsImporter.kt:182-195`、`:290-296`；`IcsExporter.kt:87-115` | `ACTION:DISPLAY`/`AUDIO` 的 `TRIGGER`（相对开始）；最多 3 个 | `RELATED=END` 和绝对时刻的提醒不认（跳过那个提醒）；`EMAIL` 不认 |
| 去重 | `IcsImporter.kt:305-318` | 同 UID（改过的那一次再加原来的日期）而且不比已有的新，算“已存在”；更新的就替换 | — |
| 结果 | `ImportEventsDialog.kt:135-137` | 导入几件、跳过几件、已存在几件、日程满了几件 | 结果是一页（`ImportResult`），不是几秒消失的提示 |
| 导出 | `IcsExporter.kt:48`；`ExportEventsDialog.kt:50-122` | 全部（含过去的） | 不问：一键写到 `/me/日历导出-YYYYMMDD.ics`，重名加 `-2` 到 `-9`；没卡、卡借给电脑、空间不够照文件服务的错误说 |

## 页面（`system/apps/calendar/`）

| 页面 | 参照 | 照做 | 按设备改和原因 |
| --- | --- | --- | --- |
| 月（`Calendar`，`cal_month.c`） | `helpers/MonthlyCalendarImpl.kt:15`、`:37-73`；`fragments/MonthDayFragment.kt:115-162` | 固定 42 格含前后月；点前后月的日子切过去；下面列所选那天 | 整屏宽七等分、格高 108，最大字号也不分页；农历和节气用 `lunar.c`；标记按形状分（空心=日程，实心=会响）；今天数字下加粗线；选中反白；选一天只重画旧格、新格和下面一块；盖板键翻月；跳到日期用共用选择器（只选年月日） |
| 议程（`Agenda`，`cal_lists.c`） | `fragments/EventListFragment.kt:103-254`；Etar `agenda/AgendaByDayAdapter.java:213-238` | 从今天起按日期排；“今天/明天 + 日期 + 星期” | 分页列表加页码和盖板键，不无限滚动；日期写在每天第一行的灰字里（一整行标题会让一页少一半）；闹钟照旧列进来；“更早的”看前 90 天 |
| 搜索 | `MainActivity.kt:1369-1450` | 先前后两年，不够 30 件再全部 | 1 个字起；按键盘“搜索”键才搜；只按标题 |
| 当天（`Day`） | `fragments/DayFragment.kt:47-127` | 前一天、后一天、这天的列表 | 写字的按钮；农历、干支、节气、宜、忌全文；主按钮“加一件”；这天的闹钟也列出 |
| 老黄历详情（`Almanac`） | Mogo `main/ui/ui_calendar.c:196-283`（只看内容和顺序） | 冲煞、三神方位、吉神宜趋、凶煞宜忌、值神（黄道/黑道）、建除、宜忌 | 不搬 Mogo 的画法；不印 almanac 的 `lunar_text`（2097-08-07 到 09-05 和 `lunar.c` 差一天，农历只用 `lunar.c`）；最后一行“这些是民俗说法” |
| 只读详情（`EventInfo`） | Etar `EventInfoFragment.java:1149-1152` | 先看信息，再编辑或删除 | 写字的“编辑”“删除”；在别处删了写“这件事已经删掉了” |
| 编辑（`ReminderEdit`、`ReminderTime`） | `activities/EventActivity.kt:817-1010`、`:1591-1610`、`:274-301` | 标题、全天、开始、结束、提醒、重复；标题空、结束早于开始的提示；离开时问保存 | 选择器用共用的 `ink_ui_datetime_picker`，按 + − 只改那一行的字（照 `memo.c:647-662` 找到行标签），按钮参数写成“加一/减一”，不用重绑；全天藏时、分两行；改开始时结束跟着平移（保持时长，同 Fossify） |
| 重复、提醒、范围、离开 | `dialogs/RepeatRuleWeeklyDialog.kt`、`CustomEventRepeatIntervalDialog.kt`、`RepeatLimitTypePickerDialog.kt`、`EditRepeatingEventDialog.kt`、`DeleteEventDialog.kt`；Commons `Activity.kt:1631-1690` | 选项内容照参照 | 都是整页的共用列表行，不弹框（底部面板 dialogs-1 没合）；提醒选项去掉“1 分钟”、直接给“1 天”，自定义用 − + 加单位，不弹键盘；每月的选项只列适用的几项（`EventActivity.kt:1012-1016`） |
| 设置（`CalendarSettings`） | `activities/SettingsActivity.kt:501-506`、`:747-751`、`:1294-1382` | 一周从哪天开始、新建时的提醒、导入、导出 | 只给周一、周日；新建时的提醒是一个（不是 Fossify 的三个），点一下换下一个 |
| 导入结果（`ImportResult`） | `ImportEventsDialog.kt:135-137` | 四个数 | 留着的一页 |

## 没照搬的

- 周视图、日视图的时间格、年视图、CalDAV 同步和下拉刷新、参与人、颜色和日历分类、每件事的时区、地点和备注、节假日、生日、打印、小组件、复制、分享、邮件提醒、提醒铃声和稍后再响：见对照表“不做”的行，进报告“要用户定的”。
