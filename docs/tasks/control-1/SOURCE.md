# control-1 的来源对照

每一部分照的是哪份源码的哪几行，哪些没照搬、为什么。行号：AOSP 和 InfiniTime、LilyPhone 按任务书给的提交在线读（GitHub 原文，2026-11-13），工具给的行号可能差一两行，标“约”；MuditaOS 读的是 shell-research-1 留在本机的证据副本，任务中途那份副本被清理，行号按当时读到的记。

## 控制中心（`system/ui/shell/control_center.c`）

| 部分 | 来源 | 照的是什么 | 没照搬的和原因 |
| --- | --- | --- | --- |
| 两段下拉 | AOSP SystemUI（Lineage `22de88b7`）的 QuickQSPanel / QSPanel 两段；LilyPhone `ui_phone1.cpp:448-465`（从顶边一定范围内下拉才打开） | 第一次下拉：头行、前四个格、通知；再下拉或点“展开”：全部格和亮度、暖光滑杆。从状态栏下拉（系统已有的顶边手势）或点状态栏打开第一段；在面板上往下拉进第二段，往上拉回第一段、再往上收起 | 跟手展开、阴影、回弹不做：墨水屏上两段整块切换（`FLASH_UI` 一次）。往上拉回第一段是本项目自己定的退路（AOSP 新版往上拉直接收起），写在“和设计不一致的” |
| 默认格和顺序 | AOSP `packages/SystemUI/res/values/config.xml` 约 80-98 行：`quick_settings_tiles_default`（internet,bt,dnd,cast,flashlight,airplane,rotation,wallet,alarm,controls,screenrecord,battery）和拆分后的 `quick_settings_tiles_default_split`（wifi,bt,cell,cast,dnd,wallet,airplane,…,battery,…）、大格 `wifi,bt,cell,cast,dnd,wallet` | 取拆分后的顺序，去掉本机没有的（投屏、钱包、手电、旋转、录屏、设备控制、闹钟）：Wi-Fi、蓝牙、移动网络、勿扰（第一段四个，等于大格去掉投屏和钱包），然后飞行模式、省电（AOSP 的 battery 格）；再加本项目原有的“叫名字”和任务书要的“刷新屏幕” | — |
| 格的两行和状态 | AOSP 各 Tile 的 `label` + `secondaryLabel`；InfiniTime `QuickSettings.cpp:136-166`（通知格在 开/关/睡眠 间切换，每种样子不同）；LilyPhone `ui_phone1.cpp:4706-4751`（开关行右边写当前值，切换后立刻按读回的值改字） | 第一行名称、第二行当前值；状态分 开、关、不可用（灰字、点不动）、连接中、没连上、受家人限制、动作，每种第二行的字不同；格子的字一律从服务读回来，不按“点了就算开了”显示 | InfiniTime 用颜色区分状态，墨水屏改为黑底/白底加文字。它的周期刷新任务（`lv_task_create`，约 118 行）不搬：只在状态变了才重画那一格 |
| Wi-Fi 格：点按 / 长按 | AOSP `qs/tiles/InternetTileNewImpl.kt` 约 94-116 行：点按打开 Internet 面板，长按 `getLongClickIntent() = WIFI_SETTINGS` | 长按进 Wi-Fi 设置页（`shell.network`）；第二行：在用的链路是 Wi-Fi 且还没通时“连接中”，通了写网络名（没有名字写“已连接”），Wi-Fi 服务说加入失败时“没连上” | 点按改为开关 Wi-Fi（任务书“点格切换”）；本机没有 Internet 面板 |
| 勿扰格 | AOSP `qs/tiles/ModesTile.kt` 约 111-155 行：点按切换、次级点按、长按进设置；第二行由 mapper 给出模式详情 | 点按切换 `notify.dnd`，长按进声音页（勿扰开关在那页） | “因为哪一条、到几点”：`notify-svc-1` 在本起步分支里已被撤回（提交 `d8ffd8bf`），没有这项状态，第二行只写开/关，写进报告 |
| 亮度、暖光滑杆 | AOSP `brightness/ui/compose/BrightnessSlider.kt`：连续值，松手时定值；受限时滑杆变灰、点了弹出说明 | 用共用滑杆（`ink_ui_slider`），两端 − / + 各点一下走一格（任务书），点在轨道上按松手的位置定值（同设置页）；孩子的音量滑杆照旧带上限标记 | 拖动中不连续写值、不画跟手动画（墨水屏）；BrightnessSlider 的单个轨道内图标改为共用滑杆两端的 − / + |
| 头行 | Kindle 快捷面板参考图：谁在用 + 时间 + 电量 + 设置 | 面板最上面是状态栏（时间、网络、电量），下面一行：家人（点了换人）、日期、设置齿轮 | 时间和电量不在头行重复画一遍：状态栏就在头行正上方 |
| 底部收起、下沿一条线 | Kindle 快捷面板：底部一个上箭头收起，面板下沿一条粗黑线 | 底部“收起”（第一段左边还有“展开”）；面板下沿 3 px 黑线（`INK_STROKE_FOCUS`）；面板只有内容那么高，下面露出原来的页面，点下面只收起面板 | 用文字按钮代替箭头：图标表里没有向上的箭头 |
| 点外面收起 | AOSP：点阴影外收起整个面板 | 面板根对象的点按范围扩到整屏（`lv_obj_set_ext_click_area`），面板下面的点按只收起面板，原页面收不到 | — |
| 正在做的事 | AOSP 在第一段快捷格下面放媒体播放卡 | 第一段格子下面放原有的“正在做的事”卡（播放、通话、后台作业，`activity_card.c`），有事才画 | 原来常驻的“正在做的事”按钮去掉：有作业时卡片上就有“查看”（`shell.tasks`），主屏也还有入口 |

## 状态栏（`system/ui/shell/status_bar.c`）

| 部分 | 来源 | 照的是什么 | 没照搬的和原因 |
| --- | --- | --- | --- |
| 只画真实状态 | MuditaOS（`2cd0e472`）`module-gui/gui/widgets/StatusBar.cpp`：30-105 行的 `Configuration`（各指示器开关、手机模式、蓝牙模式）；约 183-210 行 `configure()`：手机模式是勿扰或离线时不画网络制式；约 278-287 行 `showBluetooth()`：蓝牙模式不是关时才显示 | 飞行模式时用飞机图标代替网络（4G、信号格、无服务都不画）；蓝牙只在耳机连着时画（任务书比 MuditaOS 更严：只看连着）；Wi-Fi 图标只在 Wi-Fi 链路通了时画（原来没通也画一个“Wi-Fi 关”图标，去掉） | MuditaOS 把时间放中间、信号放左边；任务书定为左边时间、右边网络和电量，照任务书。勿扰不进状态栏（任务书没要） |
| 中间的注意事项 | MuditaOS 中间一栏（锁、时间） | 中间只放现在要注意的：通话中、录音、在听（声音正在送出去），只在设备上等人叫名字时一个小话筒 | — |
| 只刷变了的那块 | D-127 | 每条状态栏自己订阅状态：新的一分钟只改时间那个标签（只刷它）；电量、网络等变了只重画时间旁边那个盒子里的图标；没变不刷 | 原来 `navigation.c` 的 `refresh()` 每次状态变化把整条删掉重建；现在状态栏不再登记到它那里（`N.status[]` 空着），它那段循环不再动状态栏 |

## 通知区（`system/ui/shell/notification_list.c`）

原样挪自 `control_center.c` 的 `notifications()` 和两处快照循环（提交 `da9d63cc`），只改了函数名（`ink_notification_list_draw`、`ink_notification_list_snapshot`）。按任务书第 5 条删了“还没有通知”那一行（提交 `df5bbee6`）：没有通知时整段不画，面板变短。

给 `notify-ui-1` 的接口（`notification_list.h`）：

- `int ink_notification_list_draw(lv_obj_t *parent, int y, int limit)`：从 y 起画当前家人最新的通知，最多 `limit` 行（`N.control_items` 只有 2 格），每行点了走 `shell.notify_open`；画了哪些记在 `N.control_items` / `N.control_count`；返回最后一行下面的 y。没有通知什么都不画。
- `int ink_notification_list_snapshot(char *buf, size_t cap, bool read)`：把画出来的行写进页面快照，`通知[id]=标题，正文；`，`read` 为真时每条后面带 `已读=0/1；`。
- 控制中心只在第一段调用它；按“这几行的 id 变没变”决定要不要整块重画（`notes_moved()`），所以以后改行的样子不用动控制中心。
