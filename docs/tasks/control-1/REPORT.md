# control-1 报告

由 Claude 子代理执行（基线 `8ff500c1`，分支 `worktree-agent-a290b4d0920d4b35d`，13 个提交，最后 `c84820c8`，只在本地）。子代理不能写报告文件，本文由主会话照它交回的报告存档。全量 ctest（清空 build/test-out 并重新配置后，跑在 c84820c8 上）原话：`98% tests passed, 11 tests failed out of 448`（`Total Test time (real) = 1077.83 sec`）。

## 验收表

| # | 结论 | 证据 | 命令 |
|---|---|---|---|
| 1 照 AOSP 走一遍 | 做到 | 触摸走 104 项、AI 走 112 项全过。下拉第一段出 4 格和通知；再拉第二段出全部格子和亮度、暖光两条；点 Wi-Fi 第二行先“连接中”再变网络名；连失败一次写“没连上”；长按进 Wi-Fi 设置页再返回；亮度点三下 +；飞行模式开了再关，面板自己收起。AI 走同一条路结果一样。每步有截图 walk-*.png | `python3 tests/control_center.py --build build`（ctest：control-center） |
| 2 家人和孩子 | 做到 | kid 19 项全过。孩子只看到 Wi-Fi、蓝牙、刷新屏幕和亮度、暖光、音量三条；大人关掉的 Wi-Fi 写“受家人限制”，点了不生效，AI 调也被拒 | 同上加 `--case kid` |
| 3 刷新面积 | 做到 | 显示记录断言：切一格只刷 1 次、格子外 0 次；新的一分钟只刷 1 次、时间外 0 次；改成整面板重画则失败 | 同上；手动 `tests/control_center_reverse.py` |
| 4 状态栏只画真实状态 | 做到 | bar 28 项全过。蓝牙没连没图标、连上才有；飞行模式下飞机图标替换 4G 和信号格；下拉入口点按高度不小于 108、不和页面顶上的返回键重叠 | `--case bar` |
| 5 改前改后截图 | 做到 | docs/tasks/control-1/，每张 22–44 KB：改前 before-panel-1/3、before-statusbar-1/3、before-kid；改后 statusbar、stage1、stage2、empty 各两张（-1 默认字号，-3 最大字号）；孩子只有 kid-1；另有 walk-*-1 | `control_center.py --save docs/tasks/control-1` |
| 6 测试和固件 | 部分 | 全量 11 个失败，10 个在基线上同样失败，autotest-cap 见下；ASan 107 全过；S31 宿主机 78/78；build.sh 在基线和本分支都卡在 today_edit.c:258 | 见下 |

反向检查 10 条在最终代码上每条 `failed_as_expected: true`，跑完 `restored: true, walks_pass_again: true`：整面板重画、分钟也重画图标、蓝牙总是画出来、飞行模式不替换网络、关飞行后面板不收、孩子看到移动网络格、没有“受家人限制”、+ 不按格走、只有一段、没通知时写“没有通知”。

## 测试和检查

- 全量失败的 11 个：file-index-regressions、integration-update、scenario-standby-metalio-plus、scenario-powercut-during-save、gui-smoke、power-pages、weather、ai-history-photo、perf-latency-2、games2 在基线 8ff500c1 上同样失败（单独编基线跑过）。autotest-cap：基线过、本分支不过，相机场景 26 秒按的 HOME 没收到。原因：相机取景每 1.62 秒重写一遍 GC2145 寄存器（588 次、约 0.39 秒），它和盖板触摸键的触摸芯片在同一条 I2C 上，这段时间触摸读不上，80 ms 的短按丢了；本分支开机时状态栏少刷两次整条，相机节拍挪了 0.54 秒，26 秒那一下落进写寄存器那段。在基线上把按键挪进写寄存器时间（25.4 秒）同样失败。是真问题：相机取景时按 HOME 约四分之一没反应，不在本任务范围，测试脚本没改。
- ASan（-DINK_SANITIZE=ON，-DINK_WERROR=ON）：control-center、lock-cover、cell-status、calendar、comm-ux2、ui-scale、ui-scale-regressions、全部 ui-scene-*、ui-shell*、semantics-shell*、ui-snapshot-regressions-*、i18n-shell*、i18n-literals 共 107 个，`100% tests passed out of 107`。
- S31 宿主机：`100% tests passed out of 78`。S31 固件：日志有 `factory screen output layer @690a023`、没有“没有屏幕输出层”；基线和本分支都在 today_edit.c:258 报 format-truncation（临时改 `%.23s` 量大小后还原，未提交）。固件 +5,216 字节（.text +4,472、.rodata +744，上限 15 KB），内部 RAM 不变（状态放 PSRAM）。
- gcccheck：11 个改过的 C 文件 `11 compiled, 0 with findings`。隐私检查无结果；没有任务外文件。
- 产品代码约 448 行（上限 450）：control_center.c +303/−125，status_bar.c +119/−70，notification_list.c +41（约 27 行原样挪来），notification_list.h +11，navigation.c +1；另 CMake 2 行、测试注册 6 行、en.tsv 7 行。没有新能力（用现有 shell.more、shell.close 和各设置页的能力），capabilities.md 不变。

## 没做到的

1. 移动网络格第二行写“4G / 无服务 / 无卡”，不写运营商：系统里没有运营商名称状态。
2. 勿扰格只写开/关（起步分支里 notify-svc-1 已撤回，没有原因和到几点的状态）。
3. Wi-Fi 格的“没连上”只在明确发起的连接失败后出现，后台自动重连失败不显示。
4. 英文在最大字号下没逐页看过；默认字号下名字太长的格子改成占整行。
5. 孩子的面板只有一套截图（孩子的字号固定比中间大一档，“文字大小”对孩子不起作用）。
6. 没上真机。

## 和设计不一致的

1. 第一段两行两列四格，不是一行四格（一行四格每格不到 160 px，名称加值两行字放不下）。
2. 头行是家人（点了换人）、日期、设置齿轮；时间和电量在正上方状态栏，不再画一遍。
3. “展开 / 收起”用文字按钮（图标表里没有向上的箭头）。
4. 大人的面板没有音量条（任务书只要亮度和暖光），孩子照旧带上限标记；原来常驻的“正在做的事”按钮去掉，有作业时活动卡上有“查看”。
5. 移动网络格点了进移动网络页；刷新屏幕格点了先收起面板再整屏清残影。
6. 孩子的头行点了不换人。
7. 状态栏的 Wi-Fi 图标只在真连上时画。
8. −/+ 一格一格走只在控制中心里做（设置页的滑杆是共用控件，没有）。
9. 面板只有内容那么高（照 Kindle）；最大字号时下沿那条线可能切过下面页面的一行字（stage1-3.png）。

## 改过的已有测试（只改定位和页面结构，另有两处时序）

- tests/ui_shell_scenes.c、tests/ui_shell_test.c：先展开到第二段再点亮度，按面板里控件的实际位置点。
- tests/i18n_shell_remaining.c：Control-en 截图改等 “Bluetooth”。
- tests/ui_shell_regressions.c：第一段放得下两条通知，断言改成“屏幕和快照一致，最新那条一定在”。
- tests/i18n_literals.py：快照函数白名单加 ink_notification_list_snapshot。
- tests/calendar/run.py：分钟刷新从“等于整条状态栏”改为“落在状态栏里面”（现在只刷时间标签）。
- tests/lock_cover.c（时序）：“空间关了要藏起下一件事”改成最多等 3 秒再看（member.authed 要等成员服务的定时器才发出）。
- tests/comm_ux2/comm_ux2.c（时序）：截图前 1 ms 里有后台变化就等稳了重拍，最多 3 次，仍要求截到稳定画面。
- 新测试：tests/control_center.c 和 .py（control-center）；tests/control_center_reverse.py 手动跑，跑完还原源码。

## 要用户定的

1. 大人的面板要不要音量条。
2. 移动网络格要不要写运营商名（要得在蜂窝服务里加状态）。
3. 孩子能看到哪些格子（现在是 Wi-Fi、蓝牙、刷新屏幕，加三条滑杆）。
4. 第一段保持两行两列，还是一行四格（只能缩字或去掉第二行）。

## 顺手发现

1. 相机取景挤掉触摸键（见上 autotest-cap），真机上大概率也有。
2. S31 固件在基线上编不过：today_edit.c:258。
3. 状态监听表 80 格，lock-cover 场景已用到 77 格，本任务加 1 格。
4. navigation.c 里 refresh() 重建状态栏那段循环（N.status[]、status_time）现在空转，可删。
5. LVGL 9.5 的新对象布局前会先在父对象左上角算一块脏区，所以格子、滑杆、日期、活动卡和状态栏图标都放在固定容器里重画。
6. tools/icongen/check.py：转发图标名的参数叫 name 这种常见名时检查会指数级变慢（卡过 12 分钟），本任务把参数改名叫 icon。
7. 改了 assets/i18n/en.tsv 要重新 cmake 配置。
8. metalio-plus 配置没有返回键，测试里返回要点页面上的返回按钮。
9. 原来关飞行模式后只有触摸路径会收起面板，AI 关的不收；现在按状态收，两条路一样。

## 新机制和接口

- 状态栏自己订阅状态（ink_shell_bar_init，navigation.c 加的那一行），每条状态栏记住上次画的状态，变了才重画图标那块；时间标签单独刷。
- 面板根对象的点按范围扩到整屏，点面板外面只收起面板。
- 给 notify-ui-1 的通知区接口在 notification_list.h：`int ink_notification_list_draw(lv_obj_t *parent, int y, int limit)`（从 y 起画当前家人最新的通知，最多 limit 行，每行点了走 shell.notify_open，返回最后一行下面的 y，没有通知什么都不画）；`int ink_notification_list_snapshot(char *buf, size_t cap, bool read)`（写进快照 `通知[id]=标题，正文；`，read 为真时每条带 `已读=0/1；`）。控制中心只在第一段调用，按这几行的 id 变没变决定要不要整块重画。
