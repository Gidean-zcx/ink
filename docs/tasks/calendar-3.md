# calendar-3：日历照 Fossify Calendar 和 Etar 重写——整月一屏、议程、多个提醒、重复和例外（“只改这一次”“这次和以后”“全部”）、导入导出 ICS；农历用我们自己的，宜忌接 almanac-2

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/calendar-3/对照表.md`（参照产品每一页、每个操作和状态，应用看 `docs/tasks/replan-sysapps/APPS.md` 的参照路径出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12（D-129）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；D-131：先把外壳和现有应用（日历在其中）做到成熟可用。交付计划 v4（`docs/tasks/delivery-plan.md` 第 3b 节“日历”一行）：照 Fossify Calendar 和 Etar 重写，做议程、多个提醒、重复和例外、“只改这一次”；农历用我们自己的；宜忌用 almanac-2；进试用版 22。交付计划第 8 节还把“两个日期时间选择器没统一”并给了本任务和 settings-2。

调研（`docs/tasks/replan-sysapps/APPS.md` A06、`CURRENT.md` A06、`REPORT.md` 第 16 行）：搬 Fossify 的事件和议程流程、Etar 的重复系列和例外处理；ICS/RRULE 源码是完整实现的参照，不是能直接链接的库；保留我们的农历显示和统一的提醒执行；LVGL 自带的 calendar 只是控件，不能替代日程应用。参照路径：新建跨日事件 → 设多个提醒 → 每周重复 → 只改某一次 → 导出再导入，例外还在。

### 现状和缺口（以代码为准）

- 应用在 `system/apps/calendar/calendar.c`（439 行，一个文件）：应用 id `upcoming`，名字“接下来”；四个页面 `Upcoming`（接下来）、`Calendar`（月历）、`ReminderEdit`、`ReminderTime`。
- **数据就是时间服务里的提醒**：`system/include/ink/sys/time.h` 的 `ink_alarm_t`，闹钟和提醒共用一张固定表（已在 PSRAM，`INK_PSRAM_BSS`），全家合计 64 条（`system/services/time/time_service.c:38` `MAX_ALARMS`），存储 `time.alarms` schema 1（`time_service.c:46`，读入时按记录长度认旧格式，`:838`）。每条只有一种重复（`ink_repeat_t`：每周几、每月同一天、每年、农历每年），**没有间隔（每两周）、没有结束条件、没有例外、不能只改其中一次、只有一个提醒（就在开始时刻）**；标题 48 字节，编辑页的错误提示写着“最多15个字”（`calendar.c:410`）；“日程”就是不响铃的提醒（`silent`）。
- 编辑页的重复只给 4 个选项（`calendar.c:16` `REPEAT_PRESETS = 4`），每月、每年、农历每年只有 AI 能设（`reminder.draft` 的 `repeat` 到 6）；“每周”只取开始那天是星期几（`time_caps.c` `cap_reminder_add`）。
- 保存失败只有一句笼统的提示（`calendar.c:410`）。
- 月历在最大字号下分页（`calendar.c:124-152`），页面上同时有两套翻页，当天的事每页只放 1 条（`calendar.c:176`）；见 `docs/tasks/design-lang-2/calendar-six-weeks-max-1.png`。月份前后还在用 `ink_ui_circle`（`calendar.c:112-113`）。
- 刷新：选一天、翻月格、日期时间选择器每按一下，都是整页重画（`calendar.c:224`、`:235`、`:402`），不符合 D-127。
- 没有议程视图、搜索、ICS 导入导出、节气、宜忌。
- 农历来自 `system/services/time/lunar.c`（1901–2100），显示在月格第二行（`calendar.c:42-49`）。almanac-2 把它和 Mogo 农历在 2000–2099 逐日比过：36,495 天一致，**2097-08-07 到 2097-09-05 共 30 天差一天**（`ink-codex/almanac-2` 分支的 `docs/tasks/almanac-2/LUNAR-DIFF.md`，本分支里没有，以那个分支为准）。
- **别的模块也在读提醒**，改数据模型时它们必须照样对：锁屏下一件事 `system/ui/lock_mark.c:119-133`、勿扰的“日历忙” `system/services/notify/notify_model.c:78-86`、能力 `calendar.list`（`time_caps.c:244-306`）、主屏卡片 `apps/home/home.c:10-12`、日历自己的列表（`calendar.c:59`、`:161`）。它们都靠 `ink_alarm_next` 加 `ink_alarm_occurrence` 算“这一次什么时候”，其中 `lock_mark.c:128` 和 `calendar.c:59` 对没有时长、会响的提醒用 `a.next_s`（下一次响的时刻）。
- 宜忌：almanac-2 已经做完（C 接口 `ink_almanac_date()`，头文件 `system/include/ink/sys/almanac.h`；能力 `time.almanac`；2000–2099；字符串在 flash），**但还没合**（`docs/tasks/backlog.md` “黄历（almanac-2）没合”：固件多约 178 KB，超过 60 KB 预算，在压缩）。本分支里这些文件都还没有，接口名以 almanac-2 分支为准。

## 成熟来源（照着重写）

- **Fossify Calendar**（第一参照），`github.com/FossifyOrg/Calendar` 提交 `a437b8f2a354e0487043fd6b98734f9cfeb93933`（只读克隆到 `work/`），`app/src/main/kotlin/org/fossify/calendar/`：
  - `activities/EventActivity.kt:846-915`：多个提醒和重复的编辑；
  - `helpers/IcsImporter.kt:143-190`：ICS 属性和 RRULE 解析；`:250-276`：EXDATE；
  - 同一提交里的 `models/Event.kt`、`dialogs/EditRepeatingEventDialog.kt`、`dialogs/DeleteEventDialog.kt`、`dialogs/RepeatRuleWeeklyDialog.kt`、`dialogs/RepeatLimitTypePickerDialog.kt`、`fragments/EventListFragment.kt`、`helpers/IcsExporter.kt`。文件名以固定提交为准，用到的行号写进 `SOURCE.md`。
- **Etar**，`github.com/Etar-Group/Etar-Calendar` 提交 `82ff55ea1fc035177054ce4b4aa8e0dd91059ef1`（只读克隆到 `work/`）：
  - `app/src/main/java/com/android/calendar/calendarcommon2/EventRecurrence.java:31-92`；
  - `app/src/main/java/com/android/calendar/event/EditEventHelper.java:307-394`：“只这一次、这次和以后、全部”三种范围；
  - 同一提交里的 `agenda/` 和 `recurrencepicker/`。
- **Mogo 老黄历的字段和顺序**（只参考显示哪些内容，不搬画法；画法归 `lock-styles-1`）：`$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p` 提交 `864f2f9`，`main/ui/ui_calendar.c:196-283`（节气当天在月格下画线，`:262-273`）。
- **数据来源不换**：农历用我们自己的 `lunar.c`；宜、忌、节气等只用 almanac-2 的接口。

## 要做的

1. **数据：把提醒补成完整的日程**（在时间服务里做，提醒照旧由时间服务统一执行，不另建定时器或第二个提醒库）。照 Fossify 的事件模型和 Etar 的 `EventRecurrence`：
   - 重复：每天、每周（可选几天）、每月（同一天 / 第几个星期几 / 最后一天）、每年、农历每年（保留现有规则：闰月和三十的落法照 `time_service.c:266-274`）；加间隔；结束条件三种：一直、到某天为止、共几次。
   - 例外：删掉某一次；只改某一次（母系列记一个例外日期，改过的那一次另存一条，带母系列 id 和它原来的开始时刻）；这次和以后（原系列在这一次之前结束，从这一次起另成一组）。
   - 多个提醒：每件事 0～3 个，每个是“提前多少”（准时、5、10、15、30 分钟、1 小时、1 天、2 天、1 周，加自定义分钟数）；全天事件的提醒时刻照 Fossify 的算法，写进 `SOURCE.md`。没有提醒的就是现在的“日程”（`ring=false`），旧的 `ring` 参数照旧能用。
   - 标题放宽到 30 个字。
   - **例外在 `ink_alarm_occurrence` 里就算好**：删掉的那一次不出现，改过的那一次出现在新时间。“下一次响”和“这一次开始”要分开：锁屏和列表显示的是开始时刻，不是提前提醒的时刻；`lock_mark.c:128` 和 `calendar.c:59` 对会响、没有时长的提醒用 `a.next_s`，要改成开始时刻（lock_mark.c 只改那一两行，列进报告）。
   - 容量：日程至少 128 条（或单独一张表，放 PSRAM，内部内存不增加）；满了明说“日程已满（最多 N 条）”，不悄悄丢。报告写出新记录的大小和表的总大小。
   - **旧数据自动迁移**（D-138）：开机读到 schema 1 的提醒（包括没有 `silent` 字段的旧长度）自动升级成新格式，时间、重复、响不响、全天、结束时间都不变；闹钟记录和行为完全不变（`clock-3` 依赖这一点）。
2. **月视图**（照 Fossify 月视图）：标题“2026年11月”，前后月和“今天”；**整月一屏，六周的月份也不分页**，最大字号也一样，去掉月格翻页和页码点。顶栏和分段切换合成一行省高度。每格：日期数字，下面一行是我们的农历（初一写月名），节气当天写节气名；有事的格子打标记（日程和响铃的提醒能分开，不靠颜色）。下面一行写所选那天：“11月13日 · 十月廿四 · 宜 祭祀 出行”，点它进当天页；放得下就列出这天的事，放不下就只一行“这天 3 件事 ›”。选一天只刷新旧格、新格和下面那一块（D-127）。
   - **先做这一项并出最大字号的六周截图**（屏 684×1216，状态栏 108）：格高固定 108，放不下时缩短农历那一行（月名、节气或省略），格子不长高、不缩字；这样还放不下就停下写清。
3. **议程视图**（照 Fossify `EventListFragment` 和 Etar 议程）：从今天起按日期分组（分组标题只有灰字，例如“今天 · 11月13日 周五”），每行写时间（或“全天”）、标题，重复的写“每周”，改过的那一次写“已改这一次”，有提醒的写最早那个（“提前 10 分钟”）；分页用共用分页；顶上搜索框（共用输入框加放大镜），按标题搜，结果也按日期分组。月视图和议程用共用分段切换，记住上次的视图，回到“今天”一点就到。
4. **当天页和老黄历**：日期、我们的农历、干支、节气；宜、忌两行（全文，长了换行）；“更多老黄历 ›”进详情：冲煞、三神方位（财神、喜神、福神）、吉神宜趋、凶煞宜忌、值神、建除，最后一行灰字“这些是民俗说法”。下面是这天的事，一个主按钮“加一件”。
   - 日历通过自己代码里的一个小包装函数调用 almanac 接口（测试可以替换它，用来模拟数据缺失；不改 almanac 的文件）。
   - 2000 年以前、2099 年以后：宜忌那一行写“老黄历只有 2000–2099 年”，农历照常显示。
   - 老黄历数据不在：那一行写清原因（例如“老黄历数据没装上，插电脑装机时会自动补上”），**不白屏、不崩**（D-138）。
   - 2097-08-07 到 09-05 那 30 天：农历照 `lunar.c` 显示，详情页不再印 almanac 自带的农历文字。
5. **编辑页**（照 Fossify `EventActivity`）：标题、全天开关、开始、结束（可以跨天）、提醒（最多 3 个，可加可删）、重复（选频率后出对应的细项），农历每年时显示对应的农历日期。保存和删除一个重复事件中的某一次时，先问范围：只改这一次 / 这次和以后 / 全部；`dialogs-1` 的底部面板已合入就用它，没合入就用一页三行的共用列表行，不自己造对话框。保存失败说清是哪一项（“标题不能空”“结束要晚于开始”“到期日要晚于开始”“日程已满”）。删除不弹确认卡（D-105 不算高风险），用现有的撤销（`tests/services_finish/undo_schedules.c` 那套）。
6. **日期时间选择器统一**（交付计划第 8 节）：日历只用共用的 `ink_ui_datetime_picker`（`system/ui/theme.c:34`），不自己写。补两处：按 + 或 − 只刷那一行的数字，不整页重画；全天事件只选年月日。**函数签名不变**（不加参数，全天靠传入的部分数或现有机制表达），所以 `system/ui/shell/time_settings.c` 不用改（它 11-13 18:00 起归 `settings-2`）；确实要改调用行，写进报告交给 `settings-2`，本任务不碰。`theme.c`、`ui_theme.h` 11-13 起归 `dialogs-1`：派之前主会话确认，已合入就直接改那一个函数；没合入就只改这个函数，合并冲突由主会话解。`tests/small_gaps_2/ui.c`（两处用同一套步进）照样要过。
7. **导入导出 ICS**（照 Fossify `IcsImporter`、`IcsExporter`）：导出当前家人的全部日程成一个 `.ics` 到他自己的空间；导入用系统文件选择器（`system/ui/file_picker.c`，只调用）。认 `VEVENT` 的 `SUMMARY`、`DTSTART`、`DTEND`、全天（`VALUE=DATE`）、`RRULE`（第 1 项支持的那些）、`EXDATE`、`RECURRENCE-ID`、`VALARM` 的 `TRIGGER`；农历每年照 RFC 7529 写成 `RSCALE=CHINESE`（闰月写 `L`），读回来还原。看不懂的规则不猜：整条跳过并计数。导完给一页结果：“导入 12 件，跳过 2 件看不懂的规则，1 件已存在”。
8. **日历设置**（只做两项）：一周从周一还是周日开始（默认周一，【估算】国内手机日历和国标 GB/T 7408 的习惯，放进预览请用户看）；新建时默认的提醒（默认“提前 10 分钟”）。按家人存。系统设置的搜索入口（`settings_registry.c`）不归本任务，写进报告给 `settings-2`。
9. **应用名和入口**：应用 id 仍是 `upcoming`，名字改成“日历”，打开是月视图、选中今天。旧的“接下来”页并进议程，旧代码删掉。
10. **能力和快照**（每个用户操作都是能力，按钮和 AI 走同一个入口）：
    - 现有的 `calendar.*`、`reminder.*` 名字不改，`reminder.*` 仍登记在应用 id `clock` 下（不搬，免得和 `clock-3` 冲突）；`reminder.add`、`reminder.update` 加参数：`reminders`（提前分钟数的数组，最多 3 个）、重复细项（`interval`、`weekdays`、每月哪种、`until` 或 `count`），旧的 `repeat` 字符串照旧认；`reminder.update`、`reminder.cancel` 加 `occurrence`（那一次原来的本地开始时间）和 `scope`（`this`、`following`、`all`）。**对重复的事改或删时不给 `scope` 就拒绝并说明**。
    - 新加（应用 id `upcoming`）：切换视图、回到今天、按标题搜索、打开某一天（含老黄历）、导出、导入。`calendar.grid_prev`、`calendar.grid_next` 删掉并列进报告。
    - 快照写清：月份、所选日期、农历、节气、宜忌摘要；每行的时间、标题、重复、是否改过、提醒；编辑页每一项的当前值；当天页的老黄历字段和“这些是民俗说法”。
    - 孩子照旧能用（`child_ok`）；家人之间互相看不到。

## 共同要求（页面任务都一样）

- 来源：功能、页面、状态、出错处理照上面的来源一个不落；`docs/tasks/calendar-3/SOURCE.md` 写每一部分对应来源的文件和行号，哪些没照搬、为什么。已知不照搬的：周视图和日视图的时间格（用列表）、账户同步（CalDAV）、参与人、颜色分类、每件事单独的时区、地点和备注（要另做按家人的存储）、节日和法定假日调休（我们没有订阅的假日日历）。这些在报告“要用户定的”里列出，不当作“不需要”。
- 新设计语言：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c`）；不画粗框和圆圈；能点的不小于 108 px；一屏最多一个主按钮；行间细分隔线；没内容的区块不画，整页空才用共用空状态。不写死像素和颜色（`tools/ui_scale_check.py`，即 `ui-scale` 测试）。
- **月格是唯一的例外**：七列放不下 108 宽（684 px 的屏约 97 px），月格用满整屏宽度七等分，高 108；每天也能从议程和当天页的“前一天/后一天”到。写进报告，108 的检查里把它登记成例外，不是放宽检查。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作、少刷新（D-127）、按下反馈 D-31、密码不给 AI 不进日志。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 原地重写，旧页面代码删掉，不留两套。
- **共享文件（交付计划第 4 节第 3 条）**：不改 `assets/i18n/en.tsv`，新文案的英文写进 `docs/tasks/calendar-3/I18N-ADD.tsv`，由主会话合；不提交 `docs/system/capabilities.md`，只确认 `python3 tools/capabilities.py` 能跑通，主会话合并时重新生成。新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），只在末尾追加本任务的行；删掉的两个能力从表里删，列进报告。
- 截图放 `docs/tasks/calendar-3/`，单张不超过 300 KB，先看干净的整页图。

## 范围和预算

- 只改：
  - `system/apps/calendar/`（可以拆成几个文件）；
  - 时间服务里提醒和日程的部分：`system/services/time/time_service.c`、`time_caps.c`（提醒和 `calendar.list` 那几段）、`time_internal.h`、`system/include/ink/sys/time.h` 里提醒那一节，可以新加文件（例如 `system/services/time/recur.c`）；
  - 第 6 项的 `system/ui/theme.c` 里 `ink_ui_datetime_picker` 这一个函数（签名不变，`ui_theme.h` 的声明不动）；
  - 第 1 项不得不改时，`system/ui/lock_mark.c:119-133` 里读下一件事的那一两行；
  - 日历和提醒相关测试（`tests/calendar/`、`tests/calendar_events/`、`tests/reminder_edit/`、新的 `tests/calendar_3/` 和夹具）、CMake 里新文件的那几行（只在末尾追加）、`docs/tasks/calendar-3/`。
- 不碰：`system/services/time/lunar.c`；`system/services/time/almanac*` 和 `vendor/mogo-calendar/`（`almanac-2`，只读着用）；`system/apps/clock/`（`clock-3`）；`system/services/notify/`（勿扰“日历忙”要靠第 1 项自动对，不对就写进报告）；`system/ui/shell/shell_home.c`、`apps/home/home.c`；`system/ui/lock_faces/*`（`lock-styles-1`）；`system/ui/shell/time_settings.c`、`settings_registry.c`（`settings-2`）；`interrupt_card.c`（`dialogs-1`，提醒响时卡片上要写开始时间和标题，现在写的不对就写进报告）；`system/ui/file_picker.c`（只调用）；`assets/i18n/en.tsv`、`docs/system/capabilities.md`。
- 时间服务的协调：`almanac-2` 在 `time_caps.c` 末尾加了登记 `time.almanac` 的几行，合并冲突由主会话解；闹钟的所有现有测试（`tests/clock_alarm.c`、`tests/integration/timesvc.c` 里的闹钟部分等）必须不改照过。
- 产品代码不超过 **1,900 行**（删掉的旧代码不抵）：页面 1,050，时间服务里的重复、例外、多个提醒、迁移 500，ICS 导入导出 350。（交付计划写的 700 是按只改页面估的，不够；主会话要改计划。）超了停下写清，不砍内容。
- 固件增量不超过 40 KB（不含 almanac-2 的表）；内部内存不增加。
- 起步：主线（已含 `design-lang-1`/`design-lang-2`、`home-1`、`lock-1`、`notify-svc-1`、`settings-1`、`control-1`）。`almanac-2` 合了就带上；没合就先做第 1～3、5～10 项，第 4 项的宜忌最后接，合并排在 `almanac-2` 之后。

## 验收

1. **照参照路径走一遍**（模拟器脚本，时间注入，每步截图并断言）：
   - 新建“出差”11-20 20:00 到 11-22 18:00（跨天），提醒“提前 1 天”和“提前 1 小时”→ 月格 20、21、22 三天都有标记 → 11-19 20:00 响一次、11-20 19:00 再响一次；**断言交给提醒卡片的标题和开始时间是“出差”和 20:00**，并截图卡片（卡片文字归 `dialogs-1`，写得不对只进报告，不算本任务失败）；
   - 新建“周会”每周一 10:00–11:00，提醒提前 10 分钟，到 12-31 为止 → 议程按日期列出 → 12-31 之后不再出现；
   - 把 11-23 那一次改到 11-24 15:00（只改这一次）→ 11-23 没有、11-24 15:00 有，别的周一不变，11-24 14:50 响；
   - 删掉 11-30 那一次 → 那天不响，`calendar.list`、锁屏的下一件事、勿扰的“日历忙”都没有这一次；
   - 从 12-14 起改成 14:00（这次和以后）→ 12-07 还是 10:00，12-14 起是 14:00；
   - 导出 ICS → 删掉全部 → 导入 → 议程和导出前逐条一样（包括例外、改过的那一次、三个以内的提醒、到期日）；
   - 新建“妈妈生日”农历每年（用 2026 年农历八月十五，即 2026-09-25 建）→ 2027-09-15 出现；闰月和三十的生日照现有规则断言落在哪天。
   同一组操作用能力调用（来源是 AI）再走一遍，结果一样；对重复的事改或删时不给 `scope`，必须被拒绝并带说明。
2. **老黄历**：2026-02-17 的当天页宜、忌和 `time.almanac` 返回的一致；1999-12-31 写“老黄历只有 2000–2099 年”，农历照常；替换那个包装函数模拟数据缺失，那一行写原因、页面其余照常；2097-08-07 月格和当天页的农历都是 `lunar.c` 的。反向对照：把宜忌改成读错一天，这条必须失败。（almanac-2 没合时，这一条用包装函数里的替身数据先测，合入后换真接口再跑。）
3. **迁移**（D-138）：用 schema 1 的提醒夹具（含没有 `silent` 字段的旧长度）开机，所有提醒自动变成新格式，时间、重复、响不响、全天、结束都不变；同一夹具里的闹钟一条不变。反向对照：去掉迁移，这条必须失败。
4. **刷新**（模拟器显示统计断言）：选一天只刷旧格、新格和下面那一块；选择器按一下 + 只刷那一行；跨分钟只刷状态栏的时间（`410cc1dd` 的断言保留）。反向对照：把选一天改回整页重画，这条必须失败。
5. **整月一屏**：默认和最大字号下，六周的月份（例如 2026 年 8 月，周一或周日起都是六周）整月在一屏里，没有月格翻页；月格高 108，宽是整屏七等分。
6. **家人和孩子**：A 的日程 B 看不到、搜不到、导不出；孩子能看自己的、能加。
7. **改前改后截图**：月视图、议程（含搜索结果和分组）、当天页和老黄历详情、编辑页（重复细项、多个提醒）、改范围的三选一、导入结果页，默认和最大文字大小各一套。
8. 主工程普通全量 ctest 过（含 `ui-scale`）；ASan 跑日历、提醒、时间服务、锁屏、通知相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写：固件增量、PSRAM 增量和新记录大小、产品代码行数（按三块分开）、`gcccheck` 结果、日程容量、**改过的每一个已有测试**（预计 `tests/calendar/`、`tests/calendar_events/`、`tests/reminder_edit/`、`tests/small_gaps_2/ui.c`、`tests/textfield_forms.c`、`tests/i18n_shell_remaining.c`、`tests/design_lang_1.c`、`tests/integration/ai_surface.c`；只改页面名、定位方式和因“整月一屏”“应用改名”而变的结构，业务断言一条不放宽）、改名或删除的能力、改过的别的模块的行（`lock_mark.c`、`theme.c`），以及 `lunar.c` 和 Mogo 那 30 天差异的处理。
