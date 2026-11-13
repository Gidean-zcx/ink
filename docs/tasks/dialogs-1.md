# dialogs-1：弹窗换新——警告框、确认卡、动作菜单、底部面板、提示条照 iOS HIG 和 Material 3 重做，电源菜单改成动作菜单

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先：本任务不碰硬件，这两节只在要查电源键行为时用得上）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/dialogs-1/对照表.md`（参照产品每一页、每个操作和状态，外壳看 `docs/tasks/shell-research-1.md` 和交付计划这一行的来源出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12：“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”；D-131 把对话框列为要先做到成熟可用的外壳部件；D-135 定了采用新设计语言。现在弹窗的问题（主会话看过 `docs/tasks/design-lang-1/Confirm-1.png`、`PowerMenu-1.png`，也读过代码）：

- 各种弹窗都用同一个底部卡 `shell_card_*`（`system/ui/shell/card.c`），没有按用途分容器。iOS 和 Material 3 都按用途分：要立刻做决定的用警告框，人自己点“更多”出来的用动作菜单，跟当前页有关的一件短事用底部面板，操作结果用提示条。我们的“没有做 · 请家长来操作”这种只是告知的事，也弹成了要点掉的模态卡（`airplane_card.c` 的 `Refused`）。
- 确认卡的标题和按钮常常写成泛泛的“继续操作”：`confirm_card.c` 的 `answer_label()` 没对上的能力一律落到“继续操作”，标题也用这个动词，服务给的真实标题（例如“AI 想把这条备忘发出去”）却挤在三行灰字的第三行。要确认的能力（`docs/system/capabilities.md` 里“是否确认”一列，现在 25 个，另有 `call.dial`、`sms.*`、`hub.confirm` 这类按情况现场要求确认的）大半会落到“继续操作”，比如加家人、改孩子能用的应用、忘掉 Wi-Fi。
- 粗框和圆圈：确认卡左上角是黑底圆圈里的 AI 图标，面板里又套了一个带框的说明盒；电源菜单三个大圆图标（`power_menu.c:98-131`）；来电、闹钟、低电量页都是大圆圈图标，来电的接听和拒接也是圆圈（`interrupt_card.c:113`、`:137`）。
- 刷新：`card.c:49` 每次改字都让整块面板闪一下，确认卡翻页也跟着闪，不符合 D-127（换页、文字都不闪，只刷变了的那块）。
- 电源菜单：没有标题；“记录结果”固定空出三行（`power_menu.c:119-120`）；“记录问题”“导出日志”绕过页面动作直接调能力（`:84-87`）；打开电源菜单没有能力，AI 打不开它。
- 飞行模式卡和拒绝卡的快照是手写的一段话（`airplane_card.c:20-24`），按钮不在页面动作里，AI 按不到“关闭飞行模式”以外的按钮，也看不出页面结构。
- 共用的提示条没有：备忘自己画了一个“已删除一条备忘 · 撤销”（`system/apps/memo/memo.c:463`），AI 回执页自己画撤销按钮（`ai_pages.c:162`），各写各的。

外壳调研：`docs/tasks/shell-research-1/DESIGN.md` 第 5.1 节（容器怎么搬）、第 4 节“电源菜单”一行、第 6 节（边界），`REUSE.md` 的“5 对话框/底部面板”一行。主线里还没有这个目录，读 `$HOME/Documents/Workspace/ink-codex/shell-research-1/repo/docs/tasks/shell-research-1/`。

## 成熟来源（照着重写，片段可搬）

- iOS HIG（`SOURCES.md` S11）：[Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts)、[Action sheets](https://developer.apple.com/design/human-interface-guidelines/action-sheets)、[Sheets](https://developer.apple.com/design/human-interface-guidelines/sheets)。取这几条：警告框只用于要立刻做决定的事，标题直接说发生什么，按钮写具体动词；取消在左、动作在右；破坏性动作不做默认按钮；动作菜单的取消单独一行；底部面板有明确的“完成/收起”。
- Material 3（S12）：[Dialog.md](https://github.com/material-components/material-components-android/blob/master/docs/components/Dialog.md)、[BottomSheet.md](https://github.com/material-components/material-components-android/blob/master/docs/components/BottomSheet.md)、[Compose snackbar](https://developer.android.com/develop/ui/compose/components/snackbar)。取这几条：模态面板和普通面板分开；提示条一行字加最多一个动作，不挡住页面主要操作。**不搬**“几秒后自动消失”。
- MuditaOS（墨水屏手机），提交 `2cd0e4721405c0bf263f311f27d38aadae6ac89f`：`module-apps/apps-common/windows/Dialog.cpp:30-62`、`:65-95`（标题、内容、动作的装配，同时只有一个活动对话框）。实体键“是/否”不搬。
- ESP-Brookesia，提交 `65d66c810ad706c9e2f82fe9177b11816acee546`：`system/brookesia_system_super/src/shell_message_dialog.cpp:153-192`（对话框参数装配、完成回调）。它的自动关闭计时器不搬。
- AOSP 电源菜单，`android-16.0.0_r1`：`packages/SystemUI/src/com/android/systemui/globalactions/GlobalActionsDialogLite.java:638-710`（电源项按语义组成列表）。只借列表的组织方式，Binder 和电源实现不搬。

以上只读克隆到本任务的 `work/`（已忽略），不进仓库。第三方截图不进仓库。

## 要做的

1. **四种共用容器**（新文件，例如 `system/ui/dialog.c`，声明放进 `ui_theme.h`；尺寸全用令牌，数值照 DESIGN 5.1，`ink_ui_px` 缩放）：
   - **警告框**（居中）：标题、正文、一到两个按钮。左右边距 `INK_MARGIN`，圆角用面板令牌，一圈细边；标题 Bold、正文 Regular；按钮高 ≥108（主要按钮 120）。两个按钮放不下就上下排，不缩字。
   - **确认卡**（贴底）：给第 3 项用，见下文。
   - **动作菜单**（贴底）：可选标题，下面每行 ≥108 的文字行（图标不套圆圈，行间细分隔线），取消单独一行，和上面隔开一段空白。破坏性的项写清楚动词，不靠颜色。
   - **底部面板**（贴底）：内容区由调用方填；有一个明确的“收起”（命中 ≥108），不只靠一条拖拽条；固定展开和收起两档高度，整块切换，不跟手；打开时记住原处，关闭时回原处。面板上不再叠第二层面板，复杂的转整页。`ai-panel-1` 接着用这个面板，接口写进报告。
   - **提示条**：一行短字（Note 字号）加最多一个动作（命中 ≥108），在页面底部占 108 高的一条；键盘开着时放在键盘上方，需要键盘顶边高度就在 `keyboard.c` 旁加一个只读的取高度函数（增量，列进报告，不改它的行为），拿不到就不放在键盘上方而是不显示动作条之外的位置，不许遮住键盘。不模态：不挡页面其他地方的点按，不接按键；只刷这一条。**不另加自动消失的计时器**：带撤销的，到回执里的撤销期限（`ink_receipt_t.undo_ms`）就把撤销按钮去掉；不带动作的，换页或下一次点按时收掉。点按穿透走现有触摸路径，不改 `ui_task.c` 的分发逻辑；`camera-touch-bus` 在改触摸，两边先对一次接口再动手。
   - 每种容器同时只开一个；模态的照 D-97 现行做法淡化背后（`card.c:15`），出现和收起各刷一次；之后改字、翻页只刷变了的那块，不闪（D-127）。
   - 现有的 `shell_card_*`（`card.c`、`shell_card.h`）接口保留、改成用新容器实现：`member_pages.c`、`power_pages.c`、`update_pages.c`、`kid_pages.c`（`shell-sweep-1` 的）和 `off_picture.c`（`lock-1` 的，`lock-styles-1` 也可能在改）都在用它，这次不改那些文件，它们自动换上新样式。
2. **飞行模式卡和拒绝提示**（`airplane_card.c`）：
   - 飞行模式挡住电话、短信时，用警告框：标题“飞行模式已打开”，正文写被挡下的事，按钮“取消”和“关闭飞行模式”（`net.set_airplane`）；孩子只看到“请家长关闭飞行模式”和“知道了”。
   - 策略拒绝（“请家长来操作”“今天的时间用完了”“请先解锁”）只是告知，不需要做决定：改用提示条，写“没有做：原因”。照旧“不能没反应”。
   - 两种都改成用 `ink_ui_page_spec_t` 出快照，按钮走 `ink_ui_semantics_action`，和点按同一条路。
3. **确认卡**（`confirm_card.c`）：
   - 标题用服务给的这件事本身（`ink_confirm_view_t.title`，例如“把这条备忘发给 example.com”）；正文是要核对的说明；身份、来源、有效期压成一行灰字放在正文下面。快照里这些字段照旧都有。
   - 按钮是“取消”加具体动词。要确认的每个能力（按能力表的确认策略取，现在 25 个，不手写名单）都要有自己的动词（例如“清空通知”“忘掉这个网络”“加这位家人”“删除这位家人”“借给电脑”），落不到“继续操作”“确定”这类泛泛的词。动词写在能力自己那里：在 `ink_cap_t`（`system/include/ink/sys/cap.h`）加一个 `verb` 字段，每个要确认的能力声明里只加这一行，不改别的；`settings.set` 是一个能力管很多设置，动词放进 `ink_setting_def_t`（同样增量加字段）；`call.dial`、`sms.*`、`hub.confirm` 这类现场要求确认的，由 `ink_confirm_hold` 的调用方给动词，缺了就在 `ink/sys/confirm.h` 的 `ink_confirm_view_t` 加 `verb` 字段。确认卡不再自己维护 `answer_label()` 那张 `if` 表。碰到别的任务的文件只改那一行声明，逐行列进报告；之后任何任务新增要确认的能力都必须带动词，第 2 项验收的测试会卡住它们。
   - 去掉 AI 圆圈和说明盒的边框；破坏性动作照旧不做主要按钮（现在的规则保留）。
   - 原有机制一条不改：翻完说明才能确认、首次显示后才算读过、过期卡、家长 PIN 页（`pinpad.c` 不改，只改它上面的排版）、换家人、锁屏、来电时卡片让路、返回和主页键等于取消、AI 不能替人确认（`confirm.accept` 只由人在设备上按）。
4. **电源菜单**（`power_menu.c`）改成动作菜单（AOSP 和 DESIGN 第 4 节）：标题“电源”；第一组“关机”“重启”“锁屏”三行；第二组灰字小标题“帮助排查”，下面“记录问题”“导出日志”两行；最后单独一行“取消”。不加二次确认、不倒计时（这些都是人明确选的普通动作，D-105）。记录结果不再占三行空白：菜单收起，用提示条显示“已记录问题 #n”或者失败原因（例如没插卡导不出日志，要说清原因，D-138）。所有按钮都走页面动作。另外登记一个打开电源菜单的能力（例如 `shell.power_menu`），长按电源键和 AI 用同一个入口；锁屏和来电、闹钟时照旧不开。
5. **系统打断页**（`interrupt_card.c`：来电、闹钟、低电量）：去掉图标外面的大圆圈，只留线形图标；来电的接听和拒接改成两个大按钮（接听是主要按钮，拒接是次要按钮），高 ≥120，左右分开；闹钟、低电量用同一套标题、正文、按钮层次。行为、按钮顺序、按键规则、`ink_ui_cause` 的打点都不改（`races_interrupt`、`ui_fix_g` 那几个测试管着）。
6. **提示条接口给别人用**：提供一个函数，按回执显示（有 `undo_ms` 和对象就带“撤销”，走 `system.undo`）。这次只用在本任务自己的文件里（第 2、4 项）。备忘、文件、AI 回执页里各自画的撤销行改用它，由那些文件的负责任务做，写进报告（位置见上面“为什么”）。
7. **快照和能力**：每种容器出快照时写清容器类型、标题、正文、每个按钮和对应的能力；提示条不是模态，但它的字和动作要出现在当前页面的快照里（`ui_semantics.c` 或 `ui_task.c` 缺一个接口就增量加，列进报告）。打开的面板、菜单都能用 `shell.close` 或 `shell.back` 关掉，和返回键一样（这两个能力在 `navigation.c`，新容器接到现有的覆盖层关闭路径上；如果必须改 `navigation.c`，只加最少的几行，列进报告）。现有快照里的页面名（`confirm`、`ConfirmExpired`、`ConfirmParent`、`StressConfirm`、`AirplaneOn`、`power_menu`、`interrupt`）保持不变。
8. **测试用的刷新记录**：仿真端现在没有“每次刷新的区域和是否闪”的记录。加一个只给测试用的小记录（放在仿真端口或 `ui_task.c` 的刷新提示处，不进固件路径），列进报告；第 3 项验收靠它。

## 共同要求（页面任务都一样）

- 来源：容器的分工、按钮层级、退出方式照上面的来源一个不落，不自己发挥。`docs/tasks/dialogs-1/SOURCE.md` 写每一部分对应来源的哪个文件和行号（HIG、M3 写到页面小节），哪些没照搬、为什么（例如不搬自动消失、不搬跟手拖拽）。
- 新设计语言：只用令牌和共用控件（`ink_ui_pill`、`ink_ui_text_button`、`ink_ui_list_row`、`ink_ui_group_divider`、`ink_ui_panel`、`ink_ui_notice` 等）；能点的不小于 108 px；每个任务区只有一个主要按钮；图标不套圆圈；不加粗框，只有模态边界画一圈细边；没内容的区块不画。不写死像素和颜色（`tools/ui_scale_check.py`；运行时才算出的尺寸照现有写法加 `ui-scale-runtime` 注释）。
- 不变的规矩：D-105（确认卡只给高风险清单，排版统一不能变成“每个动作都先弹确认”，也不能去掉本来要的确认）；家人隔离（换家人、锁屏时上一位的卡片、面板、提示条都收掉，不带给下一位）；孩子模式；D-31 按下反馈（默认不反色，结果就是反馈）；D-97 淡背景保留（改不改要用户在真屏上比，不在本任务里定）；密码不给 AI、不进日志；D-138：任何弹窗都不能出现空白，缺了东西就写出原因。
- 文字大小：跟系统“文字大小”一起缩放，默认和最大一档各出一套截图；放不下就分页（确认卡已有分页）、换行或按钮改竖排，不缩字、不裁按钮。
- 原地重写，旧画法删掉，不留两套。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由）；新文案的英文加进 `assets/i18n/en.tsv`。这两个文件都只在末尾追加本任务的行，合并冲突由主会话处理。
- 截图放 `docs/tasks/dialogs-1/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。只出验收第 5 项列的那些场景，不多拍。

## 范围和预算

- 只改：`system/ui/shell/confirm_card.c`、`card.c`、`shell_card.h`、`interrupt_card.c`、`power_menu.c`、`airplane_card.c`（`update_card.c` 只是转发，不用动）；`system/ui/theme.c`、`system/ui/ink/ui_theme.h`（今天归本任务，交付计划第 6 节；别的任务只能在末尾追加令牌，本任务先合入）；新的 `system/ui/dialog.c`；相关测试；CMake 里新文件的那一行；`docs/tasks/dialogs-1/`。增量加字段：`system/include/ink/sys/cap.h` 的 `ink_cap_t.verb` 和 `ink_setting_def_t` 的动词、`ink/sys/confirm.h` 的 `verb`、`system/policy/confirm.c` 用到它；能力声明里只加 `.verb` 一行；`keyboard.c` 的只读取高度函数；测试用的刷新记录（第 8 项）。这些都列进报告。
- 不碰：`member_*.c`、`pinpad.*`、`power_pages.c`、`update_pages.c`、`kid_pages.c`、`boot_flow.c`（`shell-sweep-1`）；`off_picture.c`、`lock_mark.c`（`lock-1`、`lock-styles-1`）；`ai_panel.c`、`ai_chat.*`、`ai_pages.*`（`ai-panel-1`）；`shell_home.c`、`today_edit.c`；`status_bar.c`、`control_center.c`、`notification_list.c`（`notify-ui-1`）；设置页文件；`system/apps/`、`apps/`。`ui_list.c`、`textfield.c` 这次不要改，确实要改就先写进报告。
- 能力登记：新能力在本任务自己的文件里用 `ink_cap_register` 登记；`navigation.c` 不派任务，确实要在它的初始化里加一行调用就只加这一行，列进报告。
- 产品代码不超过 1100 行（删掉的旧代码不抵扣）；固件增量不超过 20 KB。超了先停下报告，由主会话决定是否拆成 dialogs-1a（容器、飞行模式、提示条、电源菜单、打断页）和 dialogs-1b（确认卡和动词）。
- 起步：主会话给的分支（当前主线，已含 `design-lang-1`、`home-1`、`lock-1`、`settings-1`、`control-1`）。`ai-panel-1`、`shell-sweep-1` 排在本任务合入以后；新增要确认的能力的任务（`settings-2`、`settings-d137`、`remote-2`、`camera-3`、`gallery-3`、`notify-ui-1`）合入时必须带 `.verb`。

## 验收

1. **每种容器走一遍**（模拟器脚本，每步截图并断言；长按电源键用 `tests/integration/power_menu.c` 现有的按键注入方式）：
   - 拨号时飞行模式开着 → 警告框，按钮是“取消”和“关闭飞行模式” → 点“关闭飞行模式” → 警告框关闭，飞行模式已关。
   - 孩子点一个家长才能用的按钮 → 底部出现提示条“没有做：请家长来操作” → 点页面别处照样有反应（不模态） → 换页后提示条消失。
   - AI 发起“删除文件” → 确认卡标题写这件事，按钮写“删除这个文件”一类的具体动词 → 没翻完说明时确认按不了 → 翻完以后按确认生效。再来一次按“取消”，没有执行。
   - 长按电源键 → 动作菜单：标题“电源”、两组、取消单独一行 → 点“记录问题” → 菜单收起，提示条显示“已记录问题 #n”。
   - 来电 → 打断页的接听和拒接都是大按钮，没有圆圈 → 挂断后回到原页，画面逐字节一致（`tools/ui_panel_check.py` 照旧通过）。
   同一条路径用能力调用（来源是 AI）再走一遍：打开电源菜单、关掉飞行模式、通过 `shell.close` 关掉面板，结果一样；AI 调 `confirm.accept` 照旧被拒。
2. **确认卡的动词**：加一个测试，遍历所有要确认的能力（按能力表的确认策略取，不手写名单），每个出卡时的按钮文字都不是“继续操作”“确定”“好”这类泛泛的词，也不是空的；`settings.set` 对每个要确认的设置都查。反向对照：故意把其中一个能力的动词去掉，测试必须失败。
3. **刷新面积**（用第 8 项的刷新记录断言）：确认卡翻一页只刷说明区，不闪；提示条出现和消失只刷那一条；模态容器出现和收起各一次。反向对照：把提示条改成整屏刷新，这条必须失败。
4. **快照**：每种容器的快照写出容器类型、标题、按钮和对应能力；提示条在时，当前页面的快照里有它的字和动作；页面名照旧。
5. **改前改后截图**：警告框（大人、孩子）、确认卡（普通、多页、家长 PIN、已过期）、动作菜单（电源菜单）、底部面板（测试里的示例）、提示条（有撤销、没撤销）、来电、闹钟、低电量，默认和最大文字大小各一套；最大一档里没有被裁掉的字和按钮。
6. 主工程普通全量 ctest 过（含 `ui-scale`）。下面这些照旧通过，断言不改：`tests/ui_fix_a/confirm.c` 之外的 `tests/confirm_pages/boundary.c`、`tests/semantics_shell/confirm.c`、`tests/integration/confirm.c`、`tests/integration/races_interrupt.c`、`tests/ui_fix_g/`（interrupt、battery、off、scan）、`tests/airplane_mode.c`、`tests/design_lang_1.*`。下面这些因为本任务有意改了结构，**允许改，且只改到下面写的程度**，每个在报告里写原因：
   - `tests/ui_fix_g/menu.c`：它直接用 `M.card.panel`、`M.result`、`M.say`，并查“结果不压住取消”；改成查动作菜单的行、取消单独一行、结果出现在提示条里。
   - `tests/integration/power_menu.c`：按钮数从 5 变成 3+2+取消；“已记录问题 #”改成在提示条或当前页面快照里找。
   - `tests/ui_fix_a/confirm.c`：不再有 `answer_label()` 的 `if` 表和固定的“删除”“修改设置”等字，改成查每个场景的按钮是该能力自己的动词，且不是泛泛的词；`C.pages` 的分页断言保留。
   - `tests/confirm_pages/pages.c`：默认场景不能再按“继续操作”，改成按该能力的动词（取自能力声明）；翻页、AI 不能确认的断言保留。
   - `tests/ui_fix_g/airplane.c`：改用新警告框的内部引用，“取消”“知道了”和“孩子不能关飞行模式”断言不变。
   - `tests/i18n_shell.c:131`、`tests/audit12_fix/device.c:78-84`：现在认的是模态的 `Refused`，改成认提示条；“不能没反应”（快照里能看到原因、什么都没改）照样严格。
   ASan 跑外壳相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。
7. 报告写清：固件增量、产品代码行数、`ink-codex/tools/gcccheck.py` 结果；改过的每个已有测试和原因；给 `ai-panel-1` 的底部面板接口说明、给 `shell-sweep-1` 的 `shell_card_*` 说明；各任务该给确认能力加 `.verb` 的清单；所有碰到别的任务文件的那几行（能力声明、`keyboard.c`、`navigation.c`、刷新记录）；还没改用共用提示条的地方（备忘、文件、AI 回执页等）和对应的负责任务。
