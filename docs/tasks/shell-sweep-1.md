# shell-sweep-1：其余外壳页面照 iOS 和 AOSP 收齐。包括开机、换家人、密码键盘、全局搜索（接上设置）、正在做的事、备份更新恢复、电量页

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/shell-sweep-1/对照表.md`（参照产品每一页、每个操作和状态，外壳看 `docs/tasks/shell-research-1.md` 和交付计划这一行的来源出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12 说：“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”（D-129、D-131：外壳要先做到成熟可用）；D-135 定了“都按推荐推进”。到现在，主屏（home-1）、锁屏（lock-1）、控制中心（control-1）、设置首页（settings-1）都换成了新设计语言。剩下的外壳页面还是旧样子，交付计划第 3a 节最后一行把它们归给本任务：开机、换家人、密码键盘、全局搜索、“正在做的事”、备份更新恢复、电量页。要照 iOS 和 AOSP 的对应流程，主要是套新样式。试用版 22 要带上它们（交付计划第 2 节）。

现在的缺口（【实现】读代码和截图得到）：

- **密码页乱**：`docs/tasks/lock-1/pin-1.png` 可以看到，“紧急呼叫”按钮压住了名字“我”；“输入密码”在屏上出现了三次（标题、圆点占位、状态行）；“四到八位数字”飘在按钮下面；数字键是 163 px 的粗框格子。密码键盘还**有两份**：解锁和设密码用的是 `member_pages.c:170-183` 自己画的，确认卡里输密码用的是 `pinpad.c:40-60`（`confirm_card.c:118` 调用）。两份长得不一样。
- **全局搜索没接设置注册表**：`search.c:76-82` 只对大人遍历旧的 `ink_setting_at` 定义表，用不上注册表的别名、路径、当前值和可用状态。打开设置结果时，`search.c:270` 靠 id 里有没有 “audio.”、“display.” 来猜进哪一页，猜不中就回设置首页。孩子完全搜不到设置，和 D-137 不一致（孩子能改亮度、暖光、音量、勿扰）。settings-1 报告“没做到的”第 1 条也记了这件事。另外，一页固定只放 3 条（`search.c:51`）；没结果时画一个大圆圈（`:216`）；返回键是自己画的圆按钮（`:204`）；搜索页和空页之间切换会整页出栈再入栈（`:173`）。
- **“正在做的事”只有后台作业**：`activity_card.c` 只列通话、播放和 `job.list`。手机上的“最近用过”（AOSP Recents、iOS 应用切换器）它没有。shell-research-1 的 DESIGN.md 第 4 节和第 7 节 P2 都写了这是缺口，“不能只做个空页面交差”。可是应用管理本来就知道哪些应用还在后台或已冻结（`app_registry.c` 的 `state` 和 `left_at`），不用另建记录。页面上还有 3 处套圆圈的图标（`:114`、`:120`、`:143`），作业行也自己描边。
- **换家人多一步**：现在先弹一张“换人之前，先看看”（`member_pages.c:129-136`），再到选人页。选人页用圆形大头像，带锁的角标是小圆圈（`member_gate.c:110`），相机按钮另加描边（`:138`），和 lock-1 锁屏上方角头像的样子不一致。AOSP 的换用户是直接列出用户、点了就换。
- **电量页**：低电量页用的是圆圈电池图标（`power_pages.c:80`）；充电锁屏页画了一个整圈圆环（`:103`）；每次变化 `render()` 都先关掉整页再重开（`:71-72`），电量每涨 10% 就整页重画，不符合 D-127。
- **备份、更新、恢复、清空、开机、孩子页**：状态图标都是大圆圈（`backup_pages.c:106`、`update_pages.c:188`、`factory_pages.c:77`、`:105`、`boot_flow.c:155`、`:218`、`kid_pages.c:55`）。更新页的返回键是自画的圆按钮（`update_pages.c:142`），进度条和四个步骤用带框的方块拼成（`:165-177`）。

## 来源（照着重写，不搬代码）

- **shell-research-1**：`$HOME/Documents/Workspace/ink-codex/shell-research-1/repo/docs/tasks/shell-research-1/` 下的 `DESIGN.md` 第 4 节（返回、主页、多任务、搜索、换人、首次开机这张表）、第 6 节（必须保留的边界）、第 7 节 P2 和 P3；`REUSE.md` 中“按六项任务找代码”的第 4 行（Pebble `window_stack.c:296-334`、AOSP `GlobalActionsDialogLite.java:638-710` 的职责划分；**不要搬 Brookesia 的 go_home**），以及“我们现有代码”那张表里 `activity_card.c`、`search.c`、`member_gate.c`、`boot_flow.c` 几行。
- **AOSP**（`android-16.0.0_r1`，只读克隆到 `work/`）：
  - 锁屏密码：`packages/SystemUI/src/com/android/keyguard/` 下的 `KeyguardPINView.java`、`NumPadKey.java`、`PasswordTextView.java`，以及 `res-keyguard/layout/keyguard_pin_view.xml`。要照的是：3×4 等大数字键，最后一行“删除、0、确定”；输入的位数只在一行密码框里用圆点显示；错误提示只出现在一处；紧急呼叫放在键盘下方（`EmergencyButton.java:114-143`）。
  - 低电量：`packages/SystemUI/src/com/android/systemui/power/PowerNotificationWarnings.java`。要照的是：标题“电量低”，一行“剩余 N%”，一个“省电”动作，一个关闭。
- **LineageOS**（只读克隆）：`android_packages_apps_SetupWizard` 的欢迎页，第一页就能改语言、打紧急电话，之后一页只做一步；`android_packages_apps_Updater` 的更新页，有版本、更新说明、下载进度、“现在安装”、失败后重试。
- **iOS**（只看 HIG 和 Apple 官方说明，没有源码）：Spotlight 搜索结果按类别分组；“软件更新”页；“iCloud 云备份”页，有“立即备份”和“上次成功备份：时间”；“从 iCloud 云备份恢复”，先选备份，再看进度。
- 上面 AOSP 和 Lineage 的文件名、行号要在克隆里核对，写进 `docs/tasks/shell-sweep-1/SOURCE.md`。找不到的就写“没找到”，不要编。

## 要做的

按下面的顺序做，每做完一项提交一次。预算或时间快用完时，按这个顺序停下，在报告里写清做到哪一项。

1. **密码键盘只留一份**（`pinpad.c`、`pinpad.h`、`member_pages.c`）
   - 解锁、设或改密码、家长验证都改用 `pinpad.c`。`member_pages.c` 里自己画的那份删掉。`confirm_card.c` 调的 `shell_pinpad()` 签名不变，它自动就是新样子。
   - 版式照 AOSP 锁屏：最上面一行标题（谁的密码，或“请家长输入密码”）；下面一行密码框，只显示已输入位数的圆点，空的时候灰字写“四到八位数字”；再下面一行状态，“密码不对”“请等 N 秒再试”只出现在这里；3×4 等大数字键，用共用按钮的次要样式，不套粗框，每个键不小于 108；“忘记密码”是一个文字按钮。
   - 紧急呼叫和相机放在键盘下面或标题行两端，**和标题、键盘都不相交**。`ink_shell_emergency_button`、`ink_shell_camera_button` 在 `phone.c`、`camera.c` 里，不改它们的实现；位置不合适就用它们的 `strip` 参数，放进自己的容器里摆。实在摆不下写进报告。
   - 数字照旧只在本机，不进快照、不进日志，AI 不能代输。`shell.pin_submit` 照旧只给本人（`INK_EXPOSE_OWNER`）。
2. **全局搜索接上设置注册表**（`search.c`）
   - 设置结果改用 settings-1 的 `ink_sr_search()` 和 `ink_sr_value()`。名称、别名、分组都能搜到。每条写路径和当前值（“显示和阅读 > 暖光 · 3”，连接符和 settings-1 一样用“ > ”）。点开调 `settings.open {"id":…}`，和设置里的搜索进同一页、定位到同一行。`search.c:270` 猜页面的那句删掉。旧的 `ink_setting_at` 遍历也删掉，不留两份。
   - 孩子：按注册表的可用状态来。孩子能改的（D-137）能搜到也能进；只给大人的项只写“家长管理”，不写值。
   - 结果照 Spotlight 按类别分组：应用、设置、联系人、备忘、文件。组名是灰字标题行，和 settings-1 首页一样，不画框。一页放几条按可用高度和行高算，不再固定 3 条，大字号下照样分页。`search.query` 可以加一个可选的 `limit` 参数，AI 不传就照旧。
   - 没结果用共用空状态 `ink_ui_empty`，不画大圆圈；返回键用共用顶栏 `ink_shell_topbar`；搜索页和空页之间切换不再出栈重建，只重画结果区。
   - 联系人、备忘、文件这三类的搜索和成员检查都不改。
3. **“正在做的事”分两段**（`activity_card.c`、`activity_card.h`）
   - 照 shell-research-1 DESIGN.md 第 4 节，同一页上下两段。
   - “正在进行”：通话（回到通话）、播放（暂停或接着放）、作业（能取消的给“取消”），和现在一样。
   - “最近用过”：应用管理里还没停掉的应用（`INK_APP_BACKGROUND`、`INK_APP_FROZEN`），离开得最晚的排最前，最多 6 个，只列当前家人能用的。每行是图标加应用名，点了走 `shell.launch {"id":…}`，和点主屏图标是同一个入口，回到离开时的那一页。
   - 不做截图卡片、不做“清理内存”、不做划掉关闭。
   - 要读离开时间，就在 `app_registry.c` 和 `appreg.h` 里加一个只读函数（例如 `ink_app_left_at`，不超过 10 行，算进预算，列进报告），别的地方不动。
   - 两段都空时，整页只有一个共用空状态；有一段空时，这一段不画。不写“还没有正在做的事”这类占位行。
   - `ink_activity_card_draw`、`ink_activity_home_draw`、`ink_activity_card_snapshot` 的签名和返回值的含义不变：主屏和控制中心都在用它们。小卡里的圆圈换成不带圆底的图标，去掉描边。
4. **换家人**（`member_gate.c`、`member_pages.c`）
   - 照 AOSP，去掉“换人之前，先看看”这一步：点头像（控制中心的 `shell.members`）直接到选人页。
   - 正在通话或放声音时，选人页顶上一行灰字写“换人后正在放的声音会停下”，平时不写。`shell.member_choose` 能力保留，改成直接打开选人页。这是有意改变的行为，相关测试照新流程改，列进报告。
   - 头像照 lock-1 锁屏的画法（`lock_mark.c:200-220`：108 的圆角方块写名字第一个字，下面写名字），用同样的令牌。有密码的人在名字后面加一个锁图标，不用小圆圈。当前是谁要看得出来（黑底）。
   - 选大人走第 1 项的密码页；选孩子直接换。D-95 两分钟内直接回原页的规矩照旧。
5. **电量页**（`power_pages.c`）
   - 低电量照 AOSP：标题“电量低”，一行“剩余 N%”，大人才有“省电”开关（`power.set_saving`），一个“知道了”。图标不套圆圈。
   - 充电锁屏页：去掉圆环，换成电池图标加大号百分比加“正在充电”。
   - 电量变了只改百分比那一个标签，不再关掉整页重开；模式变了（低电量、存东西、充电）才重建。
   - 关机页照旧是静态文字，写明“已关机”，不留会走的时钟。
   - AOSP 和 iOS 是在锁屏上加一行“正在充电 80%”，不另换一整页。要改成那样得动 `lock_mark.c`（不归本任务），所以只写进报告“要用户定的”。
6. **备份、更新、恢复、清空**（`backup_pages.c`、`update_pages.c`、`factory_pages.c`）
   - 版式照 iOS 的“软件更新”和“iCloud 云备份”、Lineage 的 Updater：顶栏用共用返回；状态一行大字加一行说明；图标不套圆圈；进度条用滑杆的轨道令牌（`INK_SLIDER_TRACK`、`INK_SLIDER_FILL`）画，不用带框方块拼；四个安装步骤改成文字行，当前那步加粗。
   - 进度变化只刷进度条和百分比。
   - 每个状态都有字：Backup、BackupFail、Restore、RestoreRun、RestoreDone、RestoreFail、Update、UpdateReady、UpdateBlocked、UpdateFail、UpdateInstall、UpdateDone、BootRecovery、FactoryReset、FactoryDone，任何一个都不能是空白页（D-138）。
   - 清空页的“会清除”“不会动”两组改成灰字分组标题加列表行，不再是小圆圈图标卡片。“验证并清空”照旧走 D-105 的确认。
7. **开机**（`boot_flow.c`）
   - 第一页照 Lineage SetupWizard 和 iOS 的“你好”：一行“语言”和一行“文字大小”，点了改，走 `settings.set`，和设置里是同一个入口。老人一开机就能把字调大。
   - “先用起来”在第一页一步可达（交付计划附录验收要点），“在设备上设置”“用手机设置”照旧。之后一页只做一步，每页的状态图标换成不带圆底的图标或去掉。
   - 恢复页照 iOS：先写备份的日期和条数，再“放回来”。
   - 配对、恢复、示例的逻辑都不改。
8. **孩子的时间页和放行页**（`kid_pages.c`）：去掉大圆圈，按钮用共用样式，其余照旧。

**所有页面都一样**：

- 只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`ui_list.c`、`textfield.c` 里的按钮、输入框、列表行、开关、空状态、分页条）。能点的不小于 108；一屏最多一个主按钮；行间用细分隔线，组间留白；图标不套圆圈（本任务的文件里 `ink_ui_circle` 应该一个不剩，留下的要在报告里说明理由）；没有内容的区块不画。不写死像素和颜色，`tools/ui_scale_check.py` 要过。
- 跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图。放不下就分页或换行，不缩字、不裁按钮。
- 每个用户操作都是能力，按钮和 AI 调同一个；页面快照写明这一页显示了什么：搜索的分组和每条的路径与值、最近用过的列表、电量百分比、更新的状态和进度。
- 少刷新（D-127，只刷变了的那块）；按下反馈照 D-31；家人隔离、孩子模式照旧；确认卡只给 D-105 的高风险动作；密码不给 AI、不进日志；紧急呼叫不用解锁（D-119）；锁屏相机只能拍、只能看这次拍的（D-121）。
- 原地重写，旧代码删掉，不留两套。界面文字守 D-98 的词表。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力或改了参数的能力，在 `tests/integration/ai_surface.c` 的两张表末尾追加；新文案的英文追加到 `assets/i18n/en.tsv` 末尾。合并冲突由主会话处理。
- 截图放 `docs/tasks/shell-sweep-1/`，单张不超过 300 KB。先看干净的整页图，不能有调试框。

## 范围和预算

- 只改：`system/ui/search.c`（必要时加 `system/include/ink/sys/search.h`）、`system/ui/shell/activity_card.c/.h`、`pinpad.c/.h`、`member_gate.c`、`member_pages.c/.h`、`boot_flow.c`、`backup_pages.c`、`update_pages.c`、`factory_pages.c`、`power_pages.c`、`kid_pages.c`；`system/ui/app_registry.c` 和 `system/include/ink/sys/appreg.h` 只加第 3 项那个只读函数；还有相关测试、根目录 `CMakeLists.txt` 里 `INK_SHELL_SCENES` 那一行（只追加本任务的场景）、`tests/design_lang_1.py` 的场景表（只追加）、`docs/tasks/shell-sweep-1/`。
- 不碰：
  - `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`、`card.c`、`shell_card.h`、`confirm_card.c`、`power_menu.c`、`interrupt_card.c`、`airplane_card.c`、`update_card.c`，归 dialogs-1。要新的共用控件（例如进度条）就写进报告，排给下一个负责的任务。
  - `lock_mark.c`、`off_picture.c`（lock-1）；`shell_home.c`、`today_edit.c`（home-1）；`status_bar.c`、`control_center.c`（control-1）；`settings_*.c`（settings-1、settings-2）。注册表只读着用 `ink_sr_*`。
  - `ai_panel.c`、`ai_chat.*`、`ai_pages.*`（ai-panel-1）；`system/apps/phone/phone.c`、`system/apps/camera/camera.c`。
  - `navigation.c` 拆完以后不派任务，确实要加一行初始化就只加这一行，列进报告。
- 主屏不加放大镜。全局搜索照旧从“全部应用”页最后一行“搜索全部”进，AI 走 `search.show`。觉得入口不够明显，就写进报告。
- 产品代码新增不超过 450 行（删掉的旧代码不抵扣）。交付计划给的是 400，多出的 50 行给“搜索接注册表”和“最近用过”这两处功能缺口。快超了就按“要做的”顺序停下，写原因。固件增量不超过 10 KB。
- 新增的机制只有第 3 项那个只读函数，和 `search.query` 的可选 `limit` 参数。不加新任务、新文件格式、新的持久记录或缓存。
- 起步用主会话给的分支：已合入 dialogs-1 的主线。dialogs-1 还没合时就从当前主线起步，但仍然不改它的文件；合并排在 dialogs-1 之后。

## 验收

1. **搜索**（模拟器脚本，每步截图并断言）：
   - 走一遍：“全部应用”→“搜索全部”→ 输“暖光”→ 结果在“设置”组下，写着“显示和阅读 > 暖光 · 当前值”→ 点进去，是显示页并定位到暖光那一行，和 `settings.open {"id":"display.warmth"}` 打开的完全一样。搜别名“色温”也能找到。
   - 用能力调用（来源是 AI）走 `search.show`、`search.open` 再走一遍，结果和打开的页面都一样。
   - 孩子：搜“音量”能找到并能进。`tests/search/search.c:146` 原来断言孩子搜“音量”结果为 0，按 D-137 有意改成“能找到音量”，同时加一条：孩子搜“自动锁屏”，只看到“家长管理”，看不到值。改后同样严格，列进报告。
   - 联系人、备忘、文件的原有断言一条不动。
   - 反向对照：把设置结果换回旧的 `ink_setting_at` 遍历，“色温”和孩子那两条测试必须失败。
2. **正在做的事**：
   - 先打开备忘、再打开阅读，回主页 → 正在做的事 →“最近用过”依次是阅读、备忘 → 点阅读，回到离开时的那一页。
   - 播放音乐时，“正在进行”有暂停；一个能取消的作业，取消以后变成“已取消”。
   - 什么都没有时整页只有共用空状态。换成孩子，只列孩子能用的应用。
   - 同样几步用 `shell.launch`、`media.pause`、`job.cancel` 再走一遍，结果相同；快照里列出两段的每一行。
3. **换家人和密码**：
   - 控制中心点头像，直接到选人页（没有中间那一步），头像和锁屏的一样。
   - 选有密码的大人 → 密码页：“输入密码”这几个字在屏上只出现一次；紧急呼叫、相机、标题、密码框、键盘的矩形两两不相交（布局断言）；12 个键一样大，每个不小于 108。
   - 输错写“密码不对”；连错要等时有倒数；忘记密码进家长验证。
   - 确认卡里输密码的样子和密码页一样（截图对照）。
   - 密码数字不出现在快照和日志里（现有测试照过）。
4. **电量**：低电量、存东西、充电锁屏、没电关机，各一张图。电量从 41% 变到 42%，只刷百分比那一块（显示统计断言）；反向对照：恢复成整页重开，这条要失败。
5. **备份、更新、恢复、清空、开机、孩子页**：第 6 项列的每个状态各一张图，开机从 FirstBoot 到 BootHello 每页一张，都不是空白页。更新下载进度变化只刷进度条那块。开机第一页能改语言和文字大小（走 `settings.set`），“先用起来”一步可达。
6. **设计检查**：
   - 本任务的页面加进 `INK_SHELL_SCENES`，`ui-scene-*`、`ui-scene-big-*`、`ui-scene-child-*` 都过。
   - `tests/design_lang_1.py --check-targets` 下，本任务各页不足 108 的点按对象为 0。
   - 本任务的文件里 `grep -c ink_ui_circle` 为 0，或在报告里逐处说明理由。
   - `ui-scale` 过。
   - 改前改后的截图（搜索结果、正在做的事、选人页、密码页、低电量、充电、更新、备份、开机第一页），默认和最大字号各一套，并排放。
7. **全量**：主工程普通全量 ctest 过；ASan 跑本任务相关测试（search、activity-card、power-pages、update-pages、backup-pages、factory-pages、boot-flow、kid-1、ui-scene-*）过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。
8. **报告**写：
   - 固件增量、产品代码行数、`gcccheck` 结果；
   - **改过的已有测试**，逐条写原因，至少包括 `tests/search/search.c:146`，以及换家人少了一步牵动的 `tests/kid_1.c`、`tests/ui_shell_regressions.c`、`tests/semantics_shell2/member.c`、`tests/i18n_shell_remaining.c` 里相关的那几处；只改流程定位和页面结构，业务断言一条不放宽；
   - 新增的机制；
   - 给 dialogs-1 或后续任务的共用控件需求（进度条等）；
   - 要用户定的：充电是否改成锁屏上的一行；“正在做的事”要不要给 HOME 长按入口；全局搜索要不要在主屏有入口。
