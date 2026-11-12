# 源码核查与可复用边界

【实现】以下结论来自读取源码，**没有编译、运行或测量这些上游系统**。成熟交互优先于移植便宜；“有源码”与“可直接编进我们的系统”分开判断。本次没有把任何上游产品代码复制进跟踪文件，许可只做来源记录，不据此剔除候选。

## 固定版本

| 上游 | 本次固定版本 | 许可出处 | 本地读取范围 |
| --- | --- | --- | --- |
| AOSP frameworks/base | `android-16.0.0_r1` | 所读两文件头均 Apache-2.0 | GitHub 官方镜像两文件；googlesource 独立下载超时后用连接器读取 |
| MuditaOS | `2cd0e4721405c0bf263f311f27d38aadae6ac89f` | [LICENSE.md](https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/LICENSE.md)，GPL-3.0，另列第三方例外 | 桌面、对话框、状态栏、锁定窗口四文件 |
| PebbleOS | `aa0c27e8330d010e9202f9a706a56002db5861ee` | [LICENSE](https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/LICENSE)，Apache-2.0；逐文件头仍须保留 | MenuLayer、状态栏、窗口栈、Launcher 四文件 |
| InfiniTime | `9ad16304f5fd526b781a760cf1313813d232645e` | [LICENSE](https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/LICENSE)，GPL-3.0 | Tile.cpp、ScreenList.h 两文件 |
| ESP-Brookesia | `65d66c810ad706c9e2f82fe9177b11816acee546` | 所读 C++ 文件头 SPDX Apache-2.0 | Super 的启动器、对话框、导航、启动器布局四文件 |

四份只读克隆放在 `work/upstream/`，提取的十四个文本文件和 SHA-256 清单在 `work/evidence/code/`、`work/evidence/code-manifest.json`；AOSP 连接器原文放 `work/evidence/aosp-emergency.java`、`work/evidence/aosp-global.txt`。四个克隆的 `git status --porcelain` 均为空。没有改 `$INK_DEPS` 或设备原厂副本，也没有下载或执行固件镜像。

## 按六项任务找代码

| 任务项 | 核查文件和行号 | 能搬什么 | 接入时必须保留、不能原样搬的部分 |
| --- | --- | --- | --- |
| 1 设计语言 | Pebble [menu_layer.c:107–118][PB-menu]；[status_bar_layer.c:211–234][PB-status] | 正常/选中前景背景配对、保证文字在选中背景上可读；统一状态栏样式接口 | C 代码可取其绘制小段，但依赖 Pebble 的 Layer/GContext，不能直接链接 LVGL。用我们的主题与单 UI 执行器；不搬动画、滚动调度或主题服务 |
| 2 主屏/全部/Dock | Brookesia [shell_app_launcher.cpp:175–215][EB-launch]、374–418；InfiniTime [Tile.cpp:46–82、101–108][IT-tile] | 前者可见应用集合、列宽/间距计算和按 app_id 启动；后者六格按钮矩阵、禁用项与点击映射 | 代码片段可移植；应用注册、按家人过滤、暂停与恢复继续走我们的应用管理/软总线。InfiniTime 这里是 LVGL 7 风格 API，我们是 LVGL 9.5.0，需适配且加中文文字；不搬它的刷新定时任务 |
| 3 状态栏/锁屏 | Mudita [DesktopMainWindow.cpp:65–77][MD-home]；AOSP [EmergencyButton.java:114–143][AO-emergency] | 大时钟页不重复小时间；紧急入口区别“回到通话”、无线电/SIM/认证状态 | 状态判定可参考，实际紧急号码、飞行模式、锁屏相机权限走既有服务；AOSP 的 Android View/资源/Telecom 不能直接搬。Plus 保持画面不等于自动遮住隐私 |
| 4 返回/主页/任务 | Pebble [window_stack.c:296–334][PB-stack]；Brookesia [system_navigation.cpp:26–69][EB-nav] | push/pop/无动画路径；主页、关应用、打开启动器的入口语义 | **不要搬 Brookesia 的 go_home**：26–34 行可能 stop 当前应用，和我们的“回主页保留任务”不同。Pebble 的栈也不能替代本项目成员生命周期。最近应用的内容快照应由现有应用管理提供；本次未找到可直接插上的整套实现 |
| 4 选人/电源/首次开机 | AOSP [GlobalActionsDialogLite.java:638–710][AO-power]；Mudita 桌面代码 [26–60][MD-home] | 电源项按语义组成列表；菜单和主屏不同职责 | AOSP 把动作交系统服务，借这层职责，不能移植其 Binder/Handler/电源实现。选人沿用我们的 member_gate；首次设置借成熟分步流程，不能把 Kompakt 的指纹、Android 账号搬来 |
| 5 对话框/底部面板 | Mudita [Dialog.cpp:30–62、65–95][MD-dialog]；Brookesia [shell_message_dialog.cpp:153–192、264 起][EB-dialog] | 标题/内容/动作数据装配；一个活动对话框；完成后回调 | 可翻译布局与参数装配；不搬 Mudita 实体键“是/否”、Brookesia 自动关闭计时器。确认权、对象版本、来电让路、到期与拒绝由现有确认服务负责 |
| 5 AI 面板 | 开放的这些仓库中未找到豆包/Gemini/Siri 面板的可直接移植代码 | 官方交互文档与语义协议作参考；继续使用本项目 ai_panel.c 与页面语义 | 不以截图识别替代 R-061 原子能力；不新增另一条 UI 自动点击或权限链 |
| 6 集成顺序 | 上述代码只是来源；本项目集中入口见下一表 | 先公共控件，再内容层次，再导航/锁屏，再 AI | 不为了复用一段代码先替换整套运行时或引入一套新服务总线 |

这里的“可搬”指已读到且有意义的源码片段或布局逻辑，**没有一套上游外壳经本次证明可整套拷贝即用**。AOSP 代码是 Java/Android，MuditaOS 是自有 C++ GUI，PebbleOS 是 Layer，Brookesia 当前 Super 也带自己的 GUI/Core；InfiniTime 虽用 LVGL，也有版本、手表布局和生命周期差异。不能把本表写成已经完成移植。

Brookesia 特别注意：同一固定版本 [system_navigation.cpp:42–54][EB-nav] 中 quick control 和 system monitor 返回不可用，notifications 转到 launcher。**不把函数名字当作功能成熟的证明**；本任务只取启动器与对话框证据，也不替 frameworks-research-1 选择它的通知系统。

## 我们现有代码：复用接点和缺口

以下行号均为任务基线。这里的“有”仅指读到实现，不表示本次跑过。

| 文件/行号 | 【实现】核查结果 | 对调研建议的影响 |
| --- | --- | --- |
| [ui_theme.h:11](../../../system/ui/ink/ui_theme.h#L11)，11–34 | 基本尺寸集中；普通控件 101，输入框 101，Dock 149，卡片圆角 48 | 令牌对照见 DESIGN §1；先统一公共组件，不到页面逐个加补丁 |
| [theme.c:141](../../../system/ui/theme.c#L141)，141–153、270–405 | `INK_SURFACE_STATIC` 的外层即使 border=false 仍自动画框；列表、圆按钮、卡片、公用页码和面板均有入口 | “少画框”涉及表面策略，光把描边从 3 改成 2 不够；不能误写成只改令牌就全系统完成 |
| [navigation.c:355](../../../system/ui/shell/navigation.c#L355)，355–437、705–830 | 状态栏与返回；主屏大时钟和 8 个图标；今日黑色大卡及四小卡，空值仍用卡片呈现 | 当前问题有代码和截图双证据。只改变内容为空时的呈现，不删除原功能 |
| [navigation.c:925](../../../system/ui/shell/navigation.c#L925)，925–976、1808–1836 | 主页快照、导航能力、`shell.search` | 当前有导航与能力基础；重做布局时同步动作与快照，不能只换外观 |
| [activity_card.c:31](../../../system/ui/shell/activity_card.c#L31)，31–101、157–184 | 来电、音频和 `job.list`；“正在做的事”不是最近应用切换器 | 普通应用最近列表仍是缺口，不能把后台工作列表当成手机多任务已齐 |
| [search.c:61](../../../system/ui/search.c#L61)，61–110、187–205 | 有跨应用/设置/联系人/备忘/文件搜索和成员检查 | 缺的是主页明确的全局搜索入口和一致反馈，不是另造搜索后端 |
| [member_gate.c:117](../../../system/ui/shell/member_gate.c#L117)，117–191 | 有选人页、紧急入口、分页；成员选择仍需验证 | 给换人更明显的入口，保留成人验证和成员切换清理 |
| [power_menu.c:110](../../../system/ui/shell/power_menu.c#L110)，110–148 | 关机、重启、锁屏、记录问题、导出日志、取消均有 UI | 重新分组和排版，不能报告成这些入口“全缺” |
| [boot_flow.c:163](../../../system/ui/shell/boot_flow.c#L163)，163–171、217–233 | 首屏已有“先用起来”，有加入、恢复、示例以及服务器未连说明 | 不能沿用旧接续记录宣称没有离线入口；就地试 AI 与回原内容体验仍需后续场景检查 |
| [lock_mark.c:24](../../../system/ui/lock_mark.c#L24)，24–80 | 有紧急/相机动作、语义快照、分钟时间差异更新 | 改摆放时保留受限相机、人来验证和原子能力，不新写休眠/唤醒底层 |
| [ai_panel.c:18](../../../system/ui/shell/ai_panel.c#L18)，18、32–56、112–122、241–290 | 已有随字号计算的高度、展开、来源位置、收起、停止/说完了、分页、面板快照 | 推荐 528/660 是默认字号示意，不能替代现有按行高自适应；优先精简层级和说明当前上下文 |

## 最小复核命令

在任务克隆根目录运行；只读命令不碰设备。下面以一个上游为例，其他仓库同理；克隆由任务授权，均在忽略的 work 下。

```bash
taskpolicy -c utility git clone --depth 1 --filter=blob:none --sparse https://github.com/mudita/MuditaOS.git work/upstream/MuditaOS
git -C work/upstream/MuditaOS rev-parse HEAD
git -C work/upstream/MuditaOS show 2cd0e4721405c0bf263f311f27d38aadae6ac89f:module-apps/application-desktop/windows/DesktopMainWindow.cpp
nl -ba system/ui/ink/ui_theme.h
nl -ba system/ui/shell/activity_card.c
git diff cbbe3227c7579b8c9bb115ddbf9ff8d37ab41a97 --stat
```

已有该目录时跳过 clone，不覆盖。浅克隆未来 HEAD 可能不同；复核应使用表中固定版本，不把新 HEAD 的行号直接套用。AOSP 使用固定 tag 的下列链接；本轮旧路径 `KeyguardBottomAreaView.java` 在此 tag 返回 404，未把旧文件填成现行相机实现。

[PB-menu]: https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/applib/ui/menu_layer.c#L107-L118
[PB-status]: https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/applib/ui/status_bar_layer.c#L211-L234
[PB-stack]: https://github.com/coredevices/PebbleOS/blob/aa0c27e8330d010e9202f9a706a56002db5861ee/pebble/applib/ui/window_stack.c#L296-L334
[MD-home]: https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/application-desktop/windows/DesktopMainWindow.cpp#L26-L77
[MD-dialog]: https://github.com/mudita/MuditaOS/blob/2cd0e4721405c0bf263f311f27d38aadae6ac89f/module-apps/apps-common/windows/Dialog.cpp#L30-L95
[EB-launch]: https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/system/brookesia_system_super/src/shell_app_launcher.cpp#L175-L215
[EB-dialog]: https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/system/brookesia_system_super/src/shell_message_dialog.cpp#L153-L192
[EB-nav]: https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/system/brookesia_system_super/src/system_navigation.cpp#L26-L69
[IT-tile]: https://github.com/InfiniTimeOrg/InfiniTime/blob/9ad16304f5fd526b781a760cf1313813d232645e/src/displayapp/screens/Tile.cpp#L46-L108
[AO-emergency]: https://github.com/aosp-mirror/platform_frameworks_base/blob/android-16.0.0_r1/packages/SystemUI/src/com/android/keyguard/EmergencyButton.java#L114-L143
[AO-power]: https://github.com/aosp-mirror/platform_frameworks_base/blob/android-16.0.0_r1/packages/SystemUI/src/com/android/systemui/globalactions/GlobalActionsDialogLite.java#L638-L710
