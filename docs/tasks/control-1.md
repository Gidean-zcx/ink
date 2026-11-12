# control-1：状态栏和控制中心照 AOSP 快捷设置重做——下拉分两段、开关格写名称和当前值、有亮度条；状态图标照 MuditaOS 只显示真实状态

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

## 为什么

用户 2026-11-12：“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”。主会话看截图的结论之一：“控制中心没有开关格”。D-129：每个功能选最成熟、体验最好的来源，搬不了代码就照它的源码逐页翻译；D-131：状态栏、控制中心和通知是先做到成熟可用的外壳部件。

两份调研一致（`docs/tasks/frameworks-research-1/SHELL.md`；主会话另一份调研 2.2 节）：页面和行为照 AOSP SystemUI——下拉一次是一行开关加通知，再拉出全部开关和亮度条；墨水屏上用两段整块切换代替跟手动画，开关只刷自己那一格。状态栏照 MuditaOS（墨水屏手机）：只画真实连接和故障。外壳调研：`docs/tasks/shell-research-1/DESIGN.md` 第 3 节（状态栏；主线里还没有的话读 `$HOME/Documents/Workspace/ink-codex/shell-research-1/repo/docs/tasks/shell-research-1/DESIGN.md`）；另一路调研（Kindle、文石、Mudita 等）的要点已写进下面“要做的”。

现状（`shell-split-1` 拆完以后）：状态栏在 `system/ui/shell/status_bar.c`，控制中心在 `system/ui/shell/control_center.c`（快捷设置、按家人分流、通知摘要、AI 快照，原来 `navigation.c` 980-1135 行那一段）。已知问题：点“关闭飞行模式”后面板不自动收起。

## 成熟来源（照着重写，片段可搬）

- AOSP SystemUI（Lineage），`github.com/LineageOS/android_frameworks_base` 提交 `22de88b7b8eb979c816669c7963dee66ae4953cd`（只读克隆到 `work/`）：`packages/SystemUI/res/values/config.xml:82-84`（默认开关格）、`packages/SystemUI/src/com/android/systemui/qs/tiles/InternetTileNewImpl.kt:91-118`（点按切换、长按进设置）、`qs/tiles/ModesTile.kt:103-142`（模式切换和次级动作）、`packages/SystemUI/src/com/android/systemui/brightness/ui/compose/BrightnessSlider.kt`（亮度条）。
- MuditaOS，`github.com/mudita/MuditaOS` 提交 `2cd0e4721405c0bf263f311f27d38aadae6ac89f`：`module-gui/gui/widgets/StatusBar.cpp:30-105`（状态栏各图标和手机模式）。
- InfiniTime，`github.com/InfiniTimeOrg/InfiniTime` 提交 `9ad16304f5fd526b781a760cf1313813d232645e`：`src/displayapp/screens/settings/QuickSettings.cpp:54-118`、`:136-166`（LVGL 写的快捷格和状态切换，可搬片段；它的周期刷新任务不搬）。
- LilyPhone，`github.com/dparnell/lilyphone1` 提交 `de10fc3882292b9be1cfcd14632726590fdef262`：`src/ui/ui_phone1.cpp:448-465`（从顶端下拉）、`:4706-4751`（开关行右边写当前值）。它的拖动节流和定期整屏刷新不搬。
- Kindle 快捷面板（外壳调研的参考图）：头行“谁在用 + 时间 + 电量 + 设置”，底部一个上箭头收起，面板下沿一条粗黑线。

## 要做的

1. **下拉两段**（AOSP）：从状态栏下拉一次（或点状态栏）出第一段——头行（当前家人、日期时间、电量、设置入口）、一行 4 个开关格、通知区；再下拉或点“展开”出第二段——全部开关格（两列）、亮度和暖光两条滑杆。段与段之间整块切换，不做跟手动画；底部一个收起按钮，面板下沿一条线和页面分开。
2. **开关格**（用 `design-lang-1` 的开关格样式）：每格第一行名称、第二行当前值，例如“Wi-Fi / 家里的网络名”、“移动网络 / 运营商”、“勿扰 / 关”、“飞行模式 / 关”、“蓝牙 / 耳机名或未连接”，加一个“刷新屏幕”的动作格（文石、华为墨水屏的做法，调用现有的整屏清残影能力；没有现成能力就写进报告）。状态至少分：开、关、不可用（灰字）、连接中、没连上、受家人限制，每种第二行的字不同；“请求已受理”不能显示成“已经开了”。点格切换，长按进对应设置页（AOSP 的做法）。勿扰格第二行先用现有 `notify.dnd`；起步分支里有 `notify-svc-1` 的“因为哪一条、到几点”状态就用上，没有就写进报告。
3. **亮度和暖光**：用共用滑杆（两端 − 和 +，点一下走一格），接现有的 `display.light`、`display.warmth`；孩子的音量、亮度限制照旧。
4. **状态栏**：左边时间，右边网络和电量百分比；只画真实状态（MuditaOS）：蓝牙只在连着时出现，飞行模式替换网络图标，录音和通话中在中间显示；分钟变化只刷时间那一小块。高度用 `INK_STATUS_BAR` 令牌（数值归 `design-lang-1`）；下拉入口的点按范围不小于 108，且不能和页面顶上按钮的点按范围重叠。
5. **通知区**：行为照旧（不重新设计通知行，那是以后 `notify-ui-1` 的事），但把通知摘要的代码原样挪进新文件 `system/ui/shell/notification_list.c`，控制中心只留调用；以后这个文件归 `notify-ui-1`。没有通知时整段不画，面板变短，不写“没有通知”。
6. **修已知问题**：关闭飞行模式后面板自己收起。
7. **能力和快照**：每个开关格和滑杆用现有能力（`net.set_airplane`、`settings.set` 等，以代码里实际有的为准）；新的用户操作（展开第二段、收起、长按进设置）登记成能力，按钮和 AI 调同一个。快照列出每格的名称、当前值、状态和能做的动作。孩子只看到孩子能用的格子（现有分流照旧）。

## 共同要求（页面任务都一样）

- 来源：功能、页面、状态、出错处理照上面的来源一个不落，不自己发挥；`docs/tasks/control-1/SOURCE.md` 写每一部分对应来源的哪个文件和行号，哪些没照搬、为什么。
- 新设计语言（`design-lang-1`）：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的按钮、列表行、开关、开关格、滑杆、分段、空状态）；能点的不小于 108 px；一屏最多一个主按钮；行间细分隔线、组间留白；图标不套圆圈；没内容的区块不画。不写死像素和颜色（`ui-scale` 检查）。旧画板的对照分数（`tools/ui_compare.py`）不作验收。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作、少刷新（D-127：只刷变了的那块）、按下反馈 D-31（默认不反色，结果就是反馈；耗时操作先让被点的控件进入“正在……”状态）、密码不给 AI 不进日志。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 原地重写，旧代码删掉，不留两套。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。
- 截图放 `docs/tasks/control-1/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：`system/ui/shell/status_bar.c`、`system/ui/shell/control_center.c`、新的 `system/ui/shell/notification_list.c`（挪过来的代码）、控制中心和状态栏相关测试、CMake 里新文件的那一行、`docs/tasks/control-1/`。
- 不碰：`theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`（`design-lang-1`）；`system/services/notify/`（`notify-svc-1`）；设置页文件（`settings-1`）；`airplane_card.c` 等弹窗（以后 `dialogs-1`）；`shell_home.c`、`today_edit.c`（以后 `home-1`）。服务缺小接口时可以增量加，列进报告。
- 能力登记：新能力在本任务自己的文件里用 `ink_cap_register` 登记（它可以调用多次）；`system/ui/shell/navigation.c` 拆完以后不派任务，确实要在它的初始化里加一行调用就只加这一行，列进报告，别的地方不改。
- 产品代码不超过 450 行（挪过来的原文不算）；固件增量不超过 15 KB。
- 起步：主会话给的分支（18:30 合并后的主线，已含 `shell-split-1`、`fixes-1112`，再合上 `design-lang-1`）；合并排在 `design-lang-1` 之后。

## 验收

1. **照 AOSP 快捷设置走一遍**（模拟器脚本，每步截图并断言）：下拉 → 第一段 4 个格子和通知 → 再下拉 → 第二段全部格子和两条滑杆 → 点 Wi-Fi 格（模拟器）→ 第二行先“连接中”再变成网络名；让连接失败一次 → 第二行是“没连上” → 长按 Wi-Fi 格进 Wi-Fi 设置页 → 返回 → 亮度点三下 + → 打开飞行模式再关闭 → 面板自己收起。同一条路径用能力调用（来源是 AI）再走一遍，结果一样。
2. **家人和孩子**：换成孩子，只出现孩子能用的格子；大人限制了的显示“受家人限制”，点了不生效。
3. **刷新面积**：切一个开关格只刷这一格（模拟器显示统计断言）；分钟变化只刷状态栏的时间。反向对照：改成整个面板重画，这条必须失败。
4. **状态栏只画真实状态**：蓝牙没连时没有蓝牙图标，连上才有；飞行模式下网络图标被替换。
5. **改前改后截图**：状态栏、第一段、第二段、孩子的面板、没有通知时的面板，默认和最大文字大小各一套。
6. 主工程普通全量 ctest 过（含 `ui-scale`）；ASan 跑外壳相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、产品代码行数、`gcccheck` 结果、改过的已有测试（只改定位方式和页面结构，不改业务断言），以及给 `notify-ui-1` 的通知区接口说明。
