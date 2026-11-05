# 应用框架调研和来源

查阅日期：2026-11-05。用途：支撑[应用框架设计](../system/app-framework-design.md)，不是移植验收。没有安装、编译或在 S31 上运行本文比较的运行时，也没有下载付费产品或第三方 SDK。

证据：【文档】上游说明；【实现】本地或上游源码可见的行为；【实测】本次实际命令结果；【估算】写明假设的计算和判断。上游宣传的资源数字不当作本设备的预算。`KB/kB` 保留来源原写法，`KiB` 是 1024 字节；不能直接互换。没有同配置数据就写“没查到”，不凭语言名字排性能名次。

## 1. 来源索引

以下每行的日期都是本次实际查阅日期。“分支快照”表示所读页面未给可用于本次比较的稳定发布版，不能把它当作已锁定的移植依赖。后续 spike 必须再固定提交和构建配置。本任务不引入任何第三方代码，故不产生新的第三方分发包或许可附件。

| 编号 | 官方来源链接 | 查阅日期 | 版本或证据范围 | 本次确认的事实及限制 |
| --- | --- | --- | --- | --- |
| S01 | [LVGL 控件目录](https://lvgl.io/docs/open/9.5/widgets/index.html) | 2026-11-05 | 9.5 文档；本项目固定 v9.5.0 | 标签、按钮、文本区、键盘、选择、列表、表格、进度等原语；不等于本项目已有封装 |
| S02 | [LVGL 许可](https://lvgl.io/docs/open/introduction/license)、[XML 调整公告](https://lvgl.io/blog/announcement-lv_xml-removal-from-v9-5)、[维护者说明 #9663](https://github.com/lvgl/lvgl/issues/9663) | 2026-11-05 | 9.5 起的 XML 引擎调整；公告未列本次可用价格 | 开源核心 MIT；运行时 XML 移出核心、商业授权。旧 9.4 的许可和仍可访问的 XML 教程不能证明 9.5 免费提供它；没有询价或购买 |
| S03 | [ESP-Brookesia README](https://github.com/espressif/esp-brookesia/blob/master/README.md)、[组件许可](https://components.espressif.com/components/espressif/esp-brookesia/versions/0.5.0/versions?language=en) | 2026-11-05 | master 自称 v0.8；许可页覆盖 0.5.0 等发布版 | 参考原生应用生命周期和服务接口；当前已扩展为多运行时、JSON UI 和 PC 模拟，不能概括为“只支持 C”。master 列 S31 需 ESP-IDF 6.2；旧组件为 Apache-2.0，不把旧许可页当作全部新组件的逐文件许可证明 |
| S04 | [A2UI 协议](https://a2ui.org/specification/v0.9-a2ui/)、[项目文档](https://github.com/a2ui-project/a2ui/blob/main/docs/public/index.md) | 2026-11-05 | 协议 v0.9；项目主页分支快照 | 自定义 catalog 限定组件，页面和数据更新分开；项目 Apache-2.0。catalog 不会替设备做成员授权、确认或资源检查 |
| S05 | [Lua 头文件与许可](https://github.com/lua/lua/blob/v5.4.8/lua.h) | 2026-11-05 | v5.4.8 | C 嵌入接口、分配器和调试 hook、MIT 许可文字；没有 S31 控件应用的体积和时延实测 |
| S06 | [Berry README](https://github.com/berry-lang/berry/blob/master/README.md) | 2026-11-05 | master 分支快照，未固定发布版 | MIT；官方给 ARM Cortex-M4/Thumb/ARMCC 的内核小例子，支持预构造常量和字节码；不是 S31 端口或 UI 框架 |
| S07 | [MicroPython README](https://github.com/micropython/micropython/blob/master/README.md)、[ESP32 支持列表](https://docs.micropython.org/en/latest/esp32/quickref.html) | 2026-11-05 | master / latest 查阅时快照 | MIT；文档列 C3、C6 等目标，支持解释 Python；本次未确认 S31 板级支持，不能用 S3 的结果代替 S31 |
| S08 | [Moddable SDK](https://github.com/Moddable-OpenSource/moddable/blob/public/readme.md)、[许可逐项说明](https://github.com/Moddable-OpenSource/moddable/blob/public/licenses/readme.md)、[可更新 mods](https://github.com/Moddable-OpenSource/moddable/blob/public/documentation/xs/mods.md) | 2026-11-05 | public 分支快照；许可说明修订 2026-04-20 | XS 执行 JS，Piu 建于 Commodetto，xsbug 调试，有 C3/C6/H2 支持和 mods 路线。许可说明将多数工具列 GPL、运行时列 LGPL，并说明历史 XS Apache 文件的后续修改许可；不能将整套标成 Apache。S31 与本项目 LVGL 接口未验证 |
| S09 | [QuickJS 手册](https://bellard.org/quickjs/quickjs.html)、[下载与许可](https://bellard.org/quickjs/) | 2026-11-05 | 2026-06-04 | MIT；提供运行时内存限制、栈限制、中断回调；裸引擎不提供控件、成员隔离或应用 SDK |
| S10 | [JerryScript README](https://github.com/jerryscript-project/jerryscript/blob/master/README.md)、[官网](https://jerryscript.net/) | 2026-11-05 | master / 无版本官网快照 | Apache-2.0；可预编译 snapshot；小内存目标描述没有给本项目完整 UI 配置或 S31 验证 |
| S11 | [MicroQuickJS README](https://github.com/bellard/mquickjs/blob/main/README.md) | 2026-11-05 | main 查阅时快照，未固定发布版 | MIT；严格的 ES5 附近子集、ROM 常量和紧凑 GC；不是现代浏览器环境，不自带页面框架 |
| S12 | [WAMR README](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/README.md)、[构建目标](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/build_wamr.md)、[ESP-IDF 路径](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/product-mini/README.md) | 2026-11-05 | main 查阅时快照，未固定发布版 | Apache-2.0 WITH LLVM-exception；解释、AOT 和 JIT 是不同配置；明确有 RISCV32 与 ABI 选项，ESP-IDF 示例提及 C3。仍需对 S31 的实际 ABI、IDF 和库调用验证，不能直接选默认浮点 ABI |
| S13 | [TypeScript 手册](https://www.typescriptlang.org/docs/handbook/typescript-in-5-minutes.html) | 2026-11-05 | 在线 Handbook，未标发布版 | TS 的开发检查和转译可留在电脑；选择 TS 写源码不等于设备必须装 TS 编译器或浏览器 |
| S14 | [Xiaomi Vela JS 应用](https://iot.mi.com/vela/quickapp/)、[manifest](https://iot.mi.com/vela/quickapp/en/guide/framework/manifest.html) | 2026-11-05 | 在线 JS 应用文档，未标统一发布版 | 类 Web 开发方式，应用清单声明接口和路由；是成套设备框架，不能据此推断其包可在本设备直接运行 |
| S15 | [OpenHarmony 轻量 JS UI](https://github.com/openharmony/ace_engine_lite/blob/master/README.md)、[组件总表](https://github.com/openharmony/docs/blob/master/zh-cn/application-dev/reference/apis-arkui/Readme-CN.md) | 2026-11-05 | 两个 master 文档快照；ArkUI 总表面向完整系统 | 轻量框架包含数据绑定、JS 运行时和 C++ 桥接；完整 ArkUI 的组件不能自动算成轻量 JS UI 已支持；不能等同整套 ArkTS 框架可搬到 MCU |
| S16 | [Zepp UI 按钮](https://docs.zepp.com/docs/reference/device-app-api/newAPI/ui/widget/BUTTON/)、[app.json](https://docs.zepp.com/zh-cn/docs/reference/app-json/) | 2026-11-05 | v3+ 文档，配置 v3 | JS 模块调用系统控件，manifest 有权限列表；借鉴清单和控件边界，不证明 Zepp OS 可移植或已开源其引擎 |
| S17 | [Pebble 图层指南](https://developer.rebble.io/developer.getpebble.com/guides/user-interfaces/layers/index.html)、[TextLayer API](https://developer.rebble.io/docs/c/User_Interface/Layers/TextLayer/) | 2026-11-05 | Rebble 保存的原 Pebble 开发文档；页面未标 SDK 小版本 | Window/Layer、文字与菜单复用；是原开发文档存档，不是本次新 Pebble 固件实测 |
| S18 | [Connect IQ 核心文档](https://developer.garmin.com/connect-iq/core-topics/)、[Manifest 和权限](https://developer.garmin.com/connect-iq/articles/core-topics/Manifest_and_Permissions.html) | 2026-11-05 | 在线 SDK 文档，未标统一版本 | Monkey C、设备目标声明、按模块权限；预算和兼容设备由平台约束，不能推断 S31 有该 VM |
| S19 | [Flipper 应用](https://developer.flipper.net/flipperzero/doxygen/applications.html)、[FAP](https://developer.flipper.net/flipperzero/doxygen/apps_on_sd_card.html)、[文字输入](https://developer.flipper.net/flipperzero/doxygen/js_gui__text_input.html)、[许可](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/LICENSE) | 2026-11-05 | 在线文档 / dev 分支许可快照 | FAP 是带元数据的 ELF；亦有 JS GUI 模块。固件 GPL-3.0，仅借鉴概念，不复制；其动态原生模式不符合 R-037 |
| S20 | [Playdate 开发](https://play.date/dev/)、[C API](https://sdk.play.date/3.0.6/Inside%20Playdate%20with%20C.html) | 2026-11-05 | C API 3.0.6；开发入口查阅时快照 | Lua/C 可混用、有模拟器和性能工具；SDK 有自己的许可，不能当作可移植的开源系统 |
| S21 | [Android 组件目录](https://developer.android.com/develop/ui/compose/components)、[平台架构](https://developer.android.com/guide/platform) | 2026-11-05 | 在线 Compose / Android 平台文档，无统一发布版 | 对照组件种类；Android 依赖 Linux、ART、框架与进程服务，不是换一门语言就能安装 APK |
| S22 | [Android 软键盘调整](https://developer.android.com/guide/topics/manifest/activity-element#windowSoftInputMode)、[输入法显示](https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/visibility)、[全屏提取输入说明](https://android-developers.googleblog.com/2009/04/updating-applications-for-on-screen.html) | 2026-11-05 | 当前 View 文档；全屏提取原理说明为 2009-04 官方文章 | `adjustResize` 缩可用区域、`adjustPan` 移窗口；extract UI 是输入法的文本编辑界面，不是任意应用截屏放大 |
| S23 | [Apple 选择与输入](https://developer.apple.com/design/human-interface-guidelines/selection-and-input)、[UITextInputTraits](https://developer.apple.com/documentation/uikit/uitextinputtraits?changes=_7)、[keyboardLayoutGuide](https://developer.apple.com/documentation/uikit/uiview/keyboardlayoutguide?changes=_2&language=objc)、[SwiftUI 列表和网格](https://developer.apple.com/documentation/swiftui/picking-container-views-for-your-content?changes=l_1) | 2026-11-05 | 在线 UIKit / HIG 文档快照 | 字段声明键盘类型、安全输入；布局 guide 表示键盘占据空间。直接网页部分仅返回需 JS 提示，事实限官方搜索可见的 API 摘要，不声称读完示例或测试过 UIKit |
| S24 | [BOOX 输入设置](https://booxsupport.zendesk.com/hc/en-us/articles/8569189574932-Language-and-Server) | 2026-11-05 | 帮助中心文章，未标固件版本 | 系统可选默认键盘。没查到可用于本项目的开闭键盘脏区、光学耗时或应用避让实现；不能用“也是墨水屏”代替性能证据 |

## 2. 能比较和不能比较的资源数字

【文档】以下是上游给的**特定例子**，不包含本项目的字库、控件、输入法、TLS、草稿、权限桥和显示缓冲。所有 S31 的 Flash 增量、内部 RAM、PSRAM、冷启动与建页时间：**没查到同配置数字，本任务未测**。因此设计比较表不把这些数当成绩。

| 对象 | 原来源数字 | 条件和缺口 | 来源、日期 |
| --- | --- | --- | --- |
| Berry | 内核代码小于 40 KiB，堆小于 4 KiB 的示例 | ARM Cortex-M4、Thumb、ARMCC；没有控件应用、S31 或启动耗时 | S06，2026-11-05 |
| QuickJS | hello 示例 x86 代码 210 KiB；运行时完整生命周期小于 300 微秒 | 官方桌面例子，不能预测 RISC-V、中文页面或屏幕稳定可读时间 | S09，2026-11-05 |
| JerryScript | 小于 64 KB RAM、小于 200 KB Flash 的设备目标 | 配置、实际程序和测试板条件不足以作横比，不是完整 SDK 大小 | S10，2026-11-05 |
| MicroQuickJS | 可从 10 kB RAM 运行；引擎约 100 kB ROM | ROM 数字注明 ARM Thumb-2；不是 S31 链接结果 | S11，2026-11-05 |
| Lua、MicroPython、XS/Piu、LVGL XML、Brookesia | 没查到本任务所需的同配置数字 | 不填推测的几百 KB，也不引用未经复核的二手数字 | S01～S08，2026-11-05 |
| WAMR | 本文不摘一个“最小体积”作为代表 | 解释器、AOT、libc 和扩展差别大；原 README 的 Cortex-M4F 数据不能替代 S31 | S12，2026-11-05 |

【估算】C 原生少一层 VM，声明式少一份任意代码执行器；这说明应优先量什么，不能证明某个候选必然比另一个快。Flash 大小比较必须使用相同编译器、裁剪、字库和链接选项，对基线取差；RAM 比较必须计入桥接、VM 栈、GC 峰值和碎片。

## 3. 设计稿和本地代码取样

【实现】主分支代码依据基线 `2163603`。界面副本只读依据提交 `d136a3a`（开工读取时工作树干净），下文以“UI 副本”指代；定位是任务书给出的同级 `ui-shell/repo`，不是本分支已有文件。收尾又核到 `a429a48`，与开工提交之间 `system/ui/`、`system/include/` 无差异；副本仍在被其他会话使用。对它只读，不合并、不写入，也不声称其测试本次通过。

【实测】在 `$INK_DESIGN/png/` 枚举到 **177 个 PNG**。逐张纳入按文件名排序的九张联系表浏览，并放大查看 `Components`、`Foundation`、`StateKit`、`PartsNew`、`ImeVoiceStates`；输入页又按页面角色对照设计第 4 节。没有运行带窗口的模拟器。下表列出全部文件名，分类只用于核查，不更改设计分类。

| 分组 | 已浏览 PNG（不带扩展名） |
| --- | --- |
| 基础和公共零件 | `Components`、`Foundation`、`PartsNew`、`StateKit`、`IconSheet`、`Interaction`、`StatusBar`、`TopBar`、`PageBar`、`Seg`、`Toggle`、`Slider`、`Stepper`、`Group`、`Keyboard`、`Icon`、`Notice`、`SaveMark`、`Stages`、`ActivityCard`、`Receipt`、`AppList` |
| 主页和锁屏 | `Today`、`Main`、`Apps`、`LockPage`、`Lock`、`LockImage`、`LockPlaying`、`MainPlaying`、`Control`、`ControlPlaying`、`MainInCall`、`Notifications`、`IconMenu`、`TodayEdit`、`Search`、`SearchEmpty` |
| 开机 | `FirstBoot`、`BootWifi`、`BootWifiPass`、`BootJoining`、`BootJoinFail`、`BootClaim`、`BootNoHub`、`BootConfirm`、`BootRestore`、`BootHello`、`BootRevoked`、`BootRecovery` |
| 家人和孩子 | `Members`、`MembersBack`、`MemberSwitch`、`Pin`、`PinWrong`、`PinWait`、`PinForgot`、`PinSet`、`FactoryReset`、`FactoryDone`、`KidHome`、`KidToday`、`KidApps`、`KidControl`、`KidLimit`、`KidTimeUp` |
| AI | `AiListen`、`AiListenTap`、`AiThink`、`Ai`、`AiLong`、`AiSpeak`、`AiOffline`、`AiTimeout`、`AiBudget`、`AiHome`、`AiAsk`、`AiLog`、`AiReceipts` |
| 确认 | `ConfirmWeb`、`ConfirmSecret`、`ConfirmDelete`、`ConfirmParent`、`ConfirmSetting`、`Confirm`、`ConfirmCall`、`ConfirmExpired`、`StressConfirm` |
| 输入法 | `ImePinyin`、`ImeCandidates`、`ImeEdit`、`ImeLong`、`ImeSymbols`、`ImeAi`、`ImePassword`、`ImeVoice`、`ImeVoiceStates` |
| 备忘和时间 | `Upcoming`、`Memo`、`MemoDetail`、`MemoEdit`、`MemoRecord`、`MemoPending`、`MemoChecklist`、`MemoUndo`、`MemoSaveFail`、`Calendar`、`Clock`、`ClockRing` |
| 文件和导入 | `Tasks`、`Import`、`ImportFind`、`FilePicker`、`ImportJob`、`ImportFail`、`Files`、`FileList`、`FileActions`、`FileMove`、`Transfer`、`TransferLive`、`TransferUsbBusy`、`TransferUsbOn`、`TransferUsbFail`、`StressFiles` |
| 网页 | `Remote`、`RemoteLoading`、`RemoteOffline`、`RemoteAddress`、`RemoteType`、`RemoteBlocked` |
| 设置 | `Settings`、`SetNetwork`、`SetBluetooth`、`SetDisplay`、`SetSound`、`SetVoice`、`SetMembers`、`SetStorage`、`SetAbout`、`SetPrivacy`、`SetKid`、`Wallpaper`、`Records`、`AiMemory`、`StressLarge` |
| 通信 | `Phone`、`PhoneCall`、`PhoneIncoming`、`PhoneRecents`、`PhoneContacts`、`Sms`、`SmsThread`、`SmsCompose`、`StressThread` |
| 电源升级备份 | `BatteryLow`、`BatterySaving`、`Charging`、`PowerMenu`、`PowerOff`、`PowerOffEmpty`、`Update`、`UpdateReady`、`UpdateBlocked`、`UpdateFail`、`UpdateInstall`、`UpdateDone`、`Backup`、`BackupFail`、`Restore`、`RestoreRun`、`RestoreDone`、`RestoreFail` |
| 图片和压力 | `Camera`、`Gallery`、`Photo`、`StressNotes` |

【实测】按文件名排序，以“文件名、一个空格、单文件 SHA-256、换行”组成清单再取 SHA-256：`9cc976825afb7c5f09f57ef433af509ea21604827c1c899cee9e884f6c80ecf1`。只记录摘要，不提交设计图或联系表。规范称 176 画板，与当前 PNG 数量不同；未擅自删图或修改规范。

## 4. 取证缺口

- 【文档】交互式决策板链接本次不可访问，无法读取其 `decisions` 集合；只使用仓库已记录的 D-08、D-10 等决定，不将任何推荐升级为已选。
- 【文档】BOOX 只找到默认键盘设置，其他墨水屏设备的输入避让和刷新成本没查到可复核实现；不写成竞品已验证的推荐。
- 【实现】A2UI-lite 是底座文档提到的上游方向；本基线 `system/`、`apps/` 中未检出 A2UI 渲染器，不能把设计当成本地功能。
- 【实测】本任务测试只验证现有工程没有因文档变更发生回归；不证明运行时候选已适配 S31。具体命令、结果和限制见[报告](../tasks/appfw-research/REPORT.md)。
