# 系统应用和界面依赖哪些底层能力

更新日期：2026-11-05。起因：用户问文件管理、通知、剪贴板和输入法该放在哪一层，音频是否设计好，以及是否应该先补底层再做交互（I-047～I-049）。

本文的**判断和建议都是助手的**，第 1～3 节的现状是对照代码和设计文档查出来的（提交 `dcba9b6`），2026-11-04 按[设计审计第 5 节](../audits/2026-11-04-codex-ui/report.md)校对（基线 `cd486a2`），留着说明当时为什么这样排。**§4.1 和第 7 节的状态已更新到系统服务的实现**（第 1、2 阶段、语音输入、升级、文字编辑，2026-11-05 合入 main，详细设计和验证记录见[系统服务详细设计](system-services-design.md)）。“有”都只指模拟器里，实机没有验证。

## 1. 结论

1. 文件、通知、剪贴板和输入法都不应该只做成一个应用：**机制放系统服务，给应用的接口放应用框架，界面放系统壳**。只有“文件”还需要一个系统应用。
2. 音频只设计好了一半。外放和板载麦克风的播放、录音路径已经有了大部分；**蓝牙基本没有设计**，本地文件播放、录成文件、音频路由也都没做，通话的声音走哪条线还不知道。
3. 现在画的界面，大多数背后还没有数据来源。**建议先定跨服务的约定（第 5 节），再补底层，再做系统壳和系统应用。** 界面框架（主题、组件、字体、图标、手势）可以并行，但触控区、灰阶区、浮层恢复和资源预算要和这些约定同步，不能只等视觉定稿。

## 2. 文件、通知、剪贴板、输入法放在哪一层

层次沿用[系统架构](system-architecture.md)：系统服务层 → 应用框架层 → 系统壳和应用。“系统壳”是用户退不出去的界面。

| 能力 | 系统服务（机制） | 应用框架（给应用的接口） | 系统壳（界面） | 系统应用 | 现在有什么 |
| --- | --- | --- | --- | --- | --- |
| **文件** | 文件服务：统一路径和权限、按成员隔离、TF 卡读写、剩余空间、变化通知、媒体索引；管 U 盘模式和无线传输的互斥（U 盘模式要先卸载 TF 卡，底座设计 §7.4） | 读写接口、能力 `file.*`、系统文件选择器 | 选文件的浮层 | **“文件”应用**：浏览、移动、删除、传输 | 应用只能读 `/content`；存储服务只管 `/user` 的小文件；HAL 已有打开、读写、目录和查剩余空间的接口，还没有包成服务 |
| **通知** | 通知服务：发布、渠道和优先级、分组、保存、勿扰、按成员隔离；**系统打断通道**（来电、闹钟、低电量）是它的最高一级，不经过前台应用的事件队列 | 发通知和撤销的接口、能力 `notify.*`、事件 | **通知栏**（在控制中心里）、状态栏小图标、锁屏卡片、来电和闹钟的全屏界面。普通通知不弹横幅 | 不需要，设置里有通知开关 | 统一的通知中心和抢占界面没有；系统总线和通信事件已有 |
| **剪贴板** | 剪贴板服务：先做一条文本，放内存不落盘，切换成员就清空；可标记为敏感；AI 读它要过策略关口，默认不读 | 读写接口；文本框架的长按菜单（选择、复制、粘贴、问 AI） | 键盘候选条上的“粘贴”提示 | 不需要 | 没有；文本选择也没有 |
| **输入法** | 输入服务：输入会话（类型、组合串、候选分页、选区、撤销、提交和取消）、拼音引擎、词库、按成员的用户词频；切换成员时清空，敏感类型单独处理；语音输入走中枢的流式识别，没网时退回键盘（R-056、D-65）；AI 联想走软总线，默认关（底座设计 §4.8） | 文本框控件：焦点、组合串、候选、动作键、类型（普通、号码、密码） | **键盘面板**是系统浮层，不是应用 | 不需要，设置里有一页 | 没有。设计有，代码没有 |

为什么这样分：

- 它们都是跨应用的。AI 也要用，比如“把这段存到备忘”“有什么新通知”，所以必须是能力，不能藏在某个应用里。
- 安卓也是这么分的：通知有 NotificationManagerService 加 SystemUI，存储有 MediaStore 加 DocumentsUI，还有 ClipboardService 和 InputMethodManagerService。
- 和安卓不同的一点：本产品不跑第三方原生应用（R-037），所以**输入法不用做成可替换的**，内置就行，能少很多接口。
- 墨水屏的影响：通知横幅不能像手机那样几秒后自己滑走，每次消失都要刷新一次。建议普通通知只改状态栏小图标（一个固定小区域），只有来电、闹钟这类才占屏幕。

## 3. 音频

### 3.1 硬件能做什么

声音都经过外置的蓝牙音频芯片 **BH1098G**：S31 经 I2S 和它连（它提供时钟，固定 16 kHz，见 RK-12），它再接功放和喇叭，麦克风和回采声道也从它回来。模式用串口 AT 命令切换。**这颗芯片没有手册，也没有搜到规格**，下面都来自[官方代码和 README](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/README.md)。

| 路径 | 系统播放 | 系统录音 | 依据 | 我们现在的状态 |
| --- | --- | --- | --- | --- |
| **外放和板载麦克风**（模式 1） | 能 | 能，还带回采，可以做回声消除 | 官方代码 | 模拟器已实现（Opus 流、提示音、录音上传、免按键、打断）；实机未测 |
| **蓝牙耳机**（模式 2） | 官方写的是“经蓝牙耳机通话”，要带麦克风的耳机 | 用耳机的麦克风 | 官方 README，无手册 | 驱动只有切模式的命令，**系统没有接**；模拟器没有耳机。扫描和配对的协议没拿到。音质很可能只是通话级，未证实 |
| **设备当蓝牙音箱**（模式 3） | 手机的声音从设备喇叭出来 | 不适用 | 官方 README | 没有接 |
| **S31 自带蓝牙**（上游关掉了） | [乐鑫资料](https://esp32-s31.espressif.com/en/official-projects/5)说原生支持 A2DP、AVRCP、HFP，[数据手册](https://documentation.espressif.com/esp32-s31_datasheet_en.pdf)（预发布版）列了 LE Audio | 经 HFP | 乐鑫资料，IDF 文档未单独确认 S31 | 没评估：要占内存，和 Wi-Fi 共用射频，板上能否同时工作要实测 |
| **4G 通话** | 未知 | 未知 | RK-07、SP-07 | 通话声音怎么到喇叭和麦克风，资料说不清，模拟器也没建模 |

### 3.2 系统设计还缺什么

| 缺口 | 现状 | 要做的 |
| --- | --- | --- |
| 输出路由 | 架构文档里写了“输出到哪里”这个状态，代码和 HAL 都没有 | HAL 加路由接口；状态 `audio.route`；耳机连上自动切换，断开退回外放 |
| 蓝牙管理 | 没有 | 蓝牙服务：扫描、配对、连接、忘记，能力 `bt.*`，状态 `bt.connected`；模拟器加耳机模型 |
| 播放本地文件 | 只能往里推 Opus 帧 | 媒体播放服务：从文件或网络解码、暂停、跳转、记位置、后台播放。上游没有 MP3 解码，要定格式 |
| 录成文件 | `audio.record` 没做（§13 第 3 步） | 录音到文件（Opus），和 AI 共用麦克风；备忘的录音靠它 |
| 音频焦点 | 代码是三档优先级加单独的提示音；设计（底座设计 §7）还有通话这一档，以及暂停和恢复规则 | 补上通话，以及“被打断后按原因决定是否续播” |
| 采样率 | 固定 16 kHz 单声道，语音够用，听音乐不理想（RK-12） | 先实测芯片能否跑 44.1 或 48 kHz；不能的话，要好音质只能走 S31 自带蓝牙 |
| 耳机时的语音 | 回声消除、唤醒词都是按板载麦克风设计的 | 实测；耳机下可能不需要回声消除 |
| 切模式的代价 | 上电后要等约 700 ms；**这路电源和 4G 共用，复位音频芯片会连带重建 4G** | 切模式只发命令，不动电源；通话中不切换 |

### 3.3 建议

先按“外放稳定、蓝牙靠实测”走（D-63）：

1. 外放的媒体播放、录音、焦点先做完，这部分不依赖未知硬件。
2. 模式 2 先实测。如果音质能接受，就用这颗芯片，省内存。
3. 如果模式 2 只有通话音质，又想用蓝牙耳机听书听歌，再评估 S31 自带蓝牙。

## 4. 每个界面还缺什么

状态：**有** = 模拟器里已实现；**部分** = 有一些，不够用；**无** = 没有；**待测** = 取决于实机。

### 4.1 通用底座

| 能力 | 谁要用 | 状态 | 说明 |
| --- | --- | --- | --- |
| 结构化数据存储 | 联系人、短信、通话记录、备忘、日历、单词 | 部分 | 追加日志加快照，按成员隔离，分页读，每条记录有版本，断电测试过（详细设计 §3、§15）。**保存回执只到“本地已落盘”**（`ink_db_durable_ver`、`ink_db_wait`）；等待上传、中枢收到要等同步 |
| 文件服务 | 文件、相册、录音、壁纸、阅读导入 | 部分 | 统一路径和权限、拷贝移动删除作业、占用和 TF 卸载、U 盘模式已有。2026-11-09 补上：稳定的文件编号和类型、按类型分页的媒体索引、拔卡和换卡状态（[file-index-1](../tasks/file-index-1/REPORT.md)）；PNG 和 JPEG 解码、按屏幕 16 级灰抖动、有上限的缩略图（[file-index-2](../tasks/file-index-2/REPORT.md)，和文件编号的接线待做）；无线传输授权（[wireless-transfer](../tasks/wireless-transfer/REPORT.md)）。真 FAT 和 USB **待测**。还缺转换作业 |
| 时间服务 | 闹钟、日历、提醒、状态栏、证书校验 | 有 | 校时、时区、闹钟、提醒、农历、重启后恢复计划；RTC 中断唤醒**待测**：出厂日志里 P16 一直为低 |
| 通知服务和打断通道 | 几乎所有应用 | 有 | 收件箱、渠道、成员、持久化、已读和清除、勿扰；来电、闹钟、低电量不经前台应用，锁定时临时放开触摸，结束后恢复 |
| 文本输入和剪贴板 | 搜索、短信、备忘、配网、设置 | 部分（选区映射已验证） | 全拼、候选分页、文本框、占位键盘、语音输入（R-056，详细设计 §9.5）、剪贴板。编辑会话有光标、选区、撤销（§9.4.1）；长文本翻页、草稿断电不丢和选区动作 2026-11-09 已有（[text-long](../tasks/text-long/REPORT.md)）；正式词库的来源和许可等决定（D-70），仓库里只有测试用的小词表 |
| 系统确认事务 | AI 操作、发短信、拨号 | 有 | 持有、展示、取消、过期、确认后执行；绑定成员、参数、内容版本，确认时重新校验；触摸和按键不能重放 AI 请求；家长 PIN。2026-11-09 起确认和自动动作的结果落盘：断电重启后查得到、不重复执行（[confirm-durable](../tasks/confirm-durable/REPORT.md)）。外观是占位（D-64） |
| 应用注册和生命周期 | 主页、全部应用、后台播放 | 部分 | 目录、前后台、冻结、恢复、权限、服务卡片。图标等视觉定稿；**可展示的作业进度和结果**只有文件作业和升级有（§15.4），草稿和导航恢复随系统壳 |
| 音频 | 听、电话、闹钟、备忘 | 有（外放） | 五档焦点、Ogg Opus 播放和续播、录成文件、路由；通话声音**待测**（RK-07） |
| 蓝牙服务 | 控制中心、设置、耳机 | 接口和模型 | 能力、状态、模拟器的耳机；**BH1098G 的协议不知道，真机上 `hal_bt_available` 为假**。外置芯片的音频模式、S31 蓝牙、翻页器 HID 还没分开 |
| Wi-Fi 管理 | 控制中心、设置、首次开机 | 有 | 自动选网，加扫描、连接、忘记；真机 HAL 没写 |
| 成员、PIN | 成员切换、儿童模式 | 有（中枢交付、握手、发布前的身份边界及页面／输入快照、已完成朗读缓存的隔离已校验，换人清除旧 AI 浮层；[设备协议报告](../tasks/device-events/REPORT.md)） | 成员表、PBKDF2 PIN、错误限速、闲置退回（计时见 D-95，等用户定）、删成员、恢复出厂。家庭绑定、中枢重置 PIN 没有 |
| 锁屏 | 锁屏、唤醒 | 部分 | 锁标记（已写上 AI 键）、电源键和 AI 键唤醒、锁屏时关成人空间已有；时钟、通知、壁纸三种画面归系统壳 |
| 配网和家庭绑定 | 首次开机 | 有（模拟器服务；页面待接） | 设计有。配网、绑定、令牌要分开（中枢协议 §3.2、§4）。系统服务明确后移，**但审计认为首版需要**，要排期 |
| 备份和恢复 | 存储设置、首次开机 | 部分（成员服务在模拟器通过；页面、设备级资料与增量对账未接） | 原表漏了。备份清单、上传回执、恢复预览、覆盖确认、未恢复项目（R-023，D-01 列在首版）。**没有人在做，要排期** |
| 系统更新和回滚 | 系统更新 | 有（失败版本历史已验证） | OTA、卡刷、自检、回滚（详细设计 §12）。**真机回滚要先换引导程序**；中枢协议还是占位 |
| 电源 | 关机、重启、低电量 | 有（关机图回执已验证） | 电量、自动锁定、关机、重启、低电量策略（15%、5%、2%）；电源键长按是电源菜单（占位外观）。关机图归系统壳 |

### 4.2 界面框架

本节和 §4.3 于 **2026-11-11 按 `d6dbdce0` 核对**，均为【实现】结论；链接的任务报告是已有验证记录，本次执行结果见 [ui-deps-refresh 报告](../tasks/ui-deps-refresh/REPORT.md)。状态沿用本节开头的定义，“有”指模拟器实现，不代表真机验收、画板一致或全量语义迁移完成。§4.1 及其他节保留原有核查范围。

| 能力 | 谁要用 | 状态 | 说明与出处 |
| --- | --- | --- | --- |
| 主题和组件库 | 所有界面 | 部分 | 卡片、胶囊、圆形按钮、开关、滑杆、分段、分页器和列表已有共用实现；尚未见共用圆环组件。见[主题接口](../../system/ui/ink/ui_theme.h#L69)、[组件实现](../../system/ui/theme.c#L190) |
| 字体 | 所有界面 | 有 | 常规、中等、粗体与数字字体按主题字号生成，并覆盖文字大小档位；运行时仍有尺寸适配和常规字库回退，不再是“中等和粗体各一个尺寸”。见[字库生成](../../tools/fontgen/ui_fonts.py)、[字体选择与缩放](../../system/ui/theme.c#L148)；真机耗时另验 |
| 图标管线 | 所有界面 | 有 | SVG 清单、哈希及五档 INKI 位图构建已有；运行时按需解压到 PSRAM。见[管线说明](../../tools/icongen/README.md)、[运行时加载](../../system/ui/icons.c) |
| 系统手势 | 控制中心、回主屏、返回、翻屏 | 有 | 按起点的 6 mm 边缘区域仲裁；控件捕获整次触摸，浮层先处理返回、主页及实体键。见[输入路由](../../system/ui/ui_task.c#L735)、[控件捕获](../../system/ui/theme.c#L385) |
| 系统浮层 | AI、确认卡、键盘、电源菜单、选文件、来电 | 有 | 已按层级登记、恢复底页、绑定成员与页面版本，并拦截落到底页的触摸；具体浮层仍各自提供取消和按键处理。见[浮层管理](../../system/ui/ui_task.c#L613)、[公共卡片](../../system/ui/shell/card.c) |
| 两段式刷新 | 翻屏、打开页面 | 部分 | 反馈优先及显式两段提交已有；普通非异步墨水屏可先黑白再补灰阶，但 `ink_ui_stage_next` 目前仅测试调用，不能写成所有页面已接。PQ-BN 的渐清走驱动自身算法。见[显示服务](../../system/services/display/display_service.c#L100)、[两段测试](../../tests/ui_shell_test.c#L128)、[PQ-BN](../../drivers/display/pqbn/pqbn_v4.c#L280) |
| 页面对 AI 可见 | 所有页面（G-002） | 部分 | 语义合同导出字段、动作、对象版本及秘密“有／无”；组合输入快照和成员校验已有。确认卡、打断、锁标记、电源菜单、成员门、关机图已迁移；键盘、AI 面板、成员页、孩子页、电量页及主页部分快照仍手写，照片像素和网页正文也不能仅靠控件快照读取。见[快照入口](../../system/ui/ui_task.c#L360)、[语义实现](../../system/ui/ui_semantics.c#L21)、[第一批迁移报告](../tasks/semantics-shell-1/REPORT.md)、[仍手写的键盘](../../system/ui/shell/keyboard.c#L126) |
| 短输入宿主 | 搜索、设置、编辑表单 | 有 | 输入框、键盘与语义字段同源，处理焦点、提交、取消及秘密字段。见[输入控件](../../system/ui/textfield.c)、[F2 报告](../tasks/ui-input-host/REPORT.md)；长文本另走草稿与分页 |
| 列表分页 | 文件、联系人、通知、设置等 | 有 | 固定可见槽位，提供位置恢复、按键翻页和同源快照。见[列表实现](../../system/ui/ui_list.c#L52) |
| 系统文件选择器 | 壁纸、导入及选文件 | 有 | 浮层浏览、类型过滤、选择与取消，按成员检查；不是只留接口。见[选择器](../../system/ui/file_picker.c#L90)、[报告](../tasks/ui-file-picker/REPORT.md) |
| 服务卡片 | 今日屏、长按图标 | 有（已有应用提供的卡片） | 同读 `app.cards`，按成员过滤、仅可见时更新且最多每秒一次；`TodayEdit` 可选卡片，`IconMenu` 已接长按及继续动作。见[卡片报告](../tasks/ui-cards/REPORT.md)、[今日屏和长按实现](../../system/ui/shell/navigation.c#L1935)。未实现的应用不会自动获得内容 |
| 全局搜索 | 全部屏 | 有（五类本地结果） | 应用、设置、联系人、备忘、文件按成员查找，每页三项，带类型、对象及版本，可打开结果；文件无适配应用时显示资料页。见[查询与分页](../../system/ui/search.c#L58)、[搜索及文件资料页](../../system/ui/search.c#L228) |
| 语言切换 | 首次开机、设置及系统应用 | 部分 | 中文／English 设置、查表回退和 AI 请求语言已有，尚未全页翻译；不等于完整地区格式设置。见[语言实现](../../system/text/i18n.c)、[设置入口](../../system/ui/shell/settings_pages.c#L33)、[i18n-1 报告](../tasks/i18n-1/REPORT.md) |

### 4.3 系统壳和系统应用

同一组的页面、浮层或画面状态并列列出；“部分”会说明页面已有而后端或设计仍缺的内容。外观差异以各任务报告为准，不把已有页面等同于画板验收通过。

| 界面（页面或画面名） | 依赖 | 状态 | 已有与剩余边界 | 出处 |
| --- | --- | --- | --- | --- |
| 主页三屏和 Dock（`home`，成人／儿童） | 应用注册、卡片、手势、时间、儿童策略 | 有 | 今日、主屏、全部应用及 Dock 可用；全部应用可分页，按成员过滤。听、报纸、单词仍有空内容入口，不能算应用已实现 | [主页实现](../../system/ui/shell/navigation.c#L496)、[主页入口](../../apps/home/home.c#L21) |
| 今日屏编辑、长按图标（`TodayEdit`、`IconMenu`） | `app.cards`、成员数据、应用动作 | 有 | 选择／排序今日卡片、长按继续动作已接；只能展示应用实际提供的卡片 | [编辑页与菜单](../../system/ui/shell/navigation.c#L1935) |
| 全局搜索、空结果、文件资料（`Search`、`SearchEmpty`、`SearchFile`） | `search.*`、应用／设置目录、成员数据、文件索引 | 有 | 五类本地结果分页并按对象版本打开；没有适配应用的文件只显示资料，不冒充已经打开内容 | [搜索页面](../../system/ui/search.c#L228)、[查询实现](../../system/ui/search.c#L58) |
| 锁屏（`LockPage`） | 时间、通知、成员、壁纸 | 有 | 保留当前页、时钟、壁纸三种样式；时钟页显示下个闹钟和通知数量，正文须唤醒后查看；有相机和紧急呼叫入口 | [锁屏](../../system/ui/lock_mark.c#L60) |
| 控制中心（`Control`、`KidControl`） | 状态、设置、通知、儿童策略 | 部分 | 网络入口、勿扰、叫名字、飞行模式、音量、前光、暖光及通知摘要已有；孩子只显示受限控制项。蓝牙和省电没有直接控制项，相关设置在其他页面 | [控制中心](../../system/ui/shell/navigation.c#L948) |
| 通知列表（`Notifications`） | `notify.list/read/dismiss/clear` | 有 | 按成员分页，逐条已读、删除及清空；不再只是控制中心的两条摘要 | [通知列表](../../system/ui/shell/navigation.c#L1453)、[通知管理报告](../tasks/notify-manage/REPORT.md) |
| 通知渠道（`SetNotifications`） | `notify.set_channel`、持久偏好 | 有 | 渠道开关、翻页及快照已接；关闭后不收新通知，已有通知保留，来电和闹钟照常；默认孩子不能修改 | [渠道页](../../system/ui/shell/navigation.c#L1169)、[报告](../tasks/notify-manage/REPORT.md) |
| 设置首页（`Settings`） | 设置表、成员、各设置子页 | 有 | 已连接网络、蓝牙、显示、声音、语音、家人、锁屏、存储与关于页；不能再写“缺后端”概括全部 | [设置导航](../../system/ui/shell/navigation.c#L1134) |
| 显示、声音（`SetDisplay`、`SetSound`） | 显示／音频设置、提示音 | 有 | 前光、暖光、字号、音量、铃声选择和试听、耳机入口已有；真实耳机与通话声音待测 | [显示与声音页](../../system/ui/shell/navigation.c#L1188)、[提示音报告](../tasks/sound-clock/REPORT.md) |
| 网络和 SIM 卡（`SetNetwork`） | Wi-Fi、网络选择、切卡、飞行模式 | 有 | 扫描、密码连接、网络选择和在线切卡已有；密码不交给 AI；真实运营商通路待测 | [网络页](../../system/ui/shell/navigation.c#L1336)、[Wi-Fi 回归](../../tests/audit10_fix/wifi_pages.c) |
| 已存 Wi-Fi（`KnownNetworks`） | `wifi.list/connect/forget`、撤销 | 有 | 未连接的已存网络也可连接、忘掉及撤销；列表翻页，凭据不进快照 | [已存网络页](../../system/ui/shell/navigation.c#L1307)、[settings-small 报告](../tasks/settings-small/REPORT.md) |
| 蓝牙耳机（`SetBluetooth`） | `bt.*`、音频路由 | 部分 | 扫描、配对、连接、断开、忘记及不可用状态已有模拟器页面；真机协议和耳机通路未验证 | [蓝牙页](../../system/ui/shell/bluetooth_page.c#L169)、[settings-a 报告](../tasks/settings-a/REPORT.md) |
| 移动网络、接入点编辑（`SetMobile`） | `net.apn*`、`net.usage*`、漫游设置 | 部分 | 接入点名称／用户名／密码、漫游、本月／上月流量及清零已有；秘密只给有无。S31 蜂窝 HAL 仍为桩，实际设置和流量来源待测 | [移动网络页](../../system/ui/shell/mobile_network.c#L49)、[合并验证报告](../tasks/cell-features-merge/REPORT.md) |
| 日期时间、手动设置、时区（`SetTime`、`SetDateTime`、`SetTimeZone`） | `time.set`、自动对时／时区设置 | 部分 | 可改整组日期时间、选城市时区，已有夏令时计算；自动时区的真实服务器来源未接，不能把合成来源测试当上线 | [时间设置](../../system/ui/shell/time_settings.c#L17)、[time-settings 报告](../tasks/time-settings/REPORT.md) |
| 离开后重新验证（`SetReturn`） | 成员返回规则、`member.idle_return_s` | 有 | 1／2／5／15 分钟选项；儿童不能改，唤醒按现有成员验证规则返回 | [返回时长页](../../system/ui/shell/time_settings.c#L17)、[报告](../tasks/time-settings/REPORT.md) |
| 语音和 AI（`SetVoice`） | 免按键、插话、活动记录 | 部分 | 免按键和插话开关可用；叫它的名字、打字接话仍明确不可设置 | [语音设置行表](../../system/ui/shell/settings_pages.c#L19) |
| 存储和备份（`SetStorage`） | 存储统计、清理、备份 | 有 | 显示空间及分类占用、清理临时文件、打开备份；备份缺口见对应行 | [存储行表](../../system/ui/shell/settings_pages.c#L26)、[统计接线](../../system/ui/shell/settings_pages.c#L454) |
| 关于和更新、语言切换（`SetAbout`） | 版本、语言设置、更新及重置入口 | 有 | 中文／English 切换、日期时间、隐私、设备详情、许可、自动下载和清空入口已接；未单独提供完整地区设置页 | [关于页](../../system/ui/shell/settings_pages.c#L33)、[语言报告](../tasks/i18n-1/REPORT.md) |
| 设备详情（`DeviceDetails`） | `hal_device_get_info`、版本、存储、电量 | 有 | 型号、系统版本、存储总量／剩余和电量；没有导出 MAC／IMEI 等识别字段 | [详情行表](../../system/ui/shell/settings_pages.c#L51)、[数据读取](../../system/ui/shell/settings_pages.c#L107) |
| 开源许可（`Licenses`、`LicenseText`） | 构建生成的许可目录与全文 | 有 | 列表及全文分页可读，快照与当前页同源 | [许可页](../../system/ui/shell/settings_pages.c#L377)、[许可生成器](../../tools/licenses.py) |
| 隐私、记录、记忆、运行记录（`SetPrivacy`、`Records`、`AiMemory`、`RuntimeRecords`） | 成员、`activity.list/delete`、AI 数据 | 部分 | 活动记录计数、本人／孩子记录与确认后删除已接；对话次数、AI 记忆和运行记录读取／删除仍未接，页面明确提示不可用 | [隐私行表](../../system/ui/shell/settings_pages.c#L44)、[记录和占位内容](../../system/ui/shell/settings_family.c#L87) |
| 成员切换、成员门及密码（`MemberSwitch`、`member_gate`、`PinSet`、`PinForgot`、`PinWait`） | 成员、PIN、限速、家长验证 | 部分 | 切换、输入／修改密码、错误等待和家长验证已接；忘记密码只有说明与家长验证，真正重置未接。AI 不能代填或提交密码 | [成员页](../../system/ui/shell/member_pages.c#L37)、[重置缺口](../../system/ui/shell/member_pages.c#L175) |
| 管理家人（`SetMembers`） | `member.add/rename/remove`、PIN | 有 | 添加、改称呼、选择成人／孩子、删除及密码入口已有；高风险动作经现有确认规则 | [家人表单和能力](../../system/ui/shell/settings_family.c#L270) |
| 孩子设置、限制提示（`SetKid`、`KidTimeUp`、`KidLimit`） | 成员时长、音量、应用策略、家长确认 | 有 | 孩子选择、每日时长、音量限制与加时已有；时段限制仍复用“明天接着来”的提示，未细分解除时间 | [孩子设置](../../system/ui/shell/settings_family.c#L197)、[限制浮层](../../system/ui/shell/kid_pages.c#L15)、[kid-apps 报告](../tasks/kid-apps/REPORT.md) |
| 孩子的应用名单（`SetKidApps`） | 成员策略、`kid.app_set`、应用目录 | 有 | 不开放／可用／禁用时段也能用三种模式；主页、触摸和 AI 都受策略约束，按孩子保存 | [应用名单页](../../system/ui/shell/settings_family.c#L180)、[报告](../tasks/kid-apps/REPORT.md) |
| 孩子的禁用时段（`SetKidPeriods`） | 成员策略、`kid.period_set`、本地时间 | 有 | 每个孩子四段、星期选择、跨午夜；页面时刻按半小时步进，能力可指定分钟 | [时段页](../../system/ui/shell/settings_family.c#L197)、[报告](../tasks/kid-apps/REPORT.md) |
| 锁屏和壁纸（`Wallpaper`） | 文件选择、图片解码、显示／电源设置 | 有 | 选相册图片、移除壁纸、锁屏样式、自动锁屏及拿起亮屏设置已有；硬件动作另验 | [设置页](../../system/ui/shell/settings_family.c#L250)、[壁纸加载](../../system/ui/shell/wallpaper.c) |
| AI 面板、打字问、对话记录（`Ai`、`AiAsk`、`AiHome`） | AI 会话、输入宿主、回答分页 | 部分 | 语音、打字、回答分页与收起／恢复已有；`AiHome` 明确是空记录页，历史对话未接；面板快照仍手写 | [AI 面板](../../system/ui/shell/ai_panel.c#L28)、[打字及空记录页](../../system/ui/shell/ai_chat.c#L35) |
| AI 离线、超时、额度用完（`AiOffline`、`AiTimeout`、`AiBudget`） | AI 错误状态、重试 | 有 | 原因、返回、重试或关闭飞行模式动作已有，按错误种类显示；不代表真实额度服务已验证 | [错误浮层](../../system/ui/shell/ai_pages.c#L16) |
| AI 动作记录和结果（`AiLog`、`AiReceipts`） | 活动记录、确认事务结果、成员 | 有 | 记录及执行回执可分页查询，和对话历史分开；不虚构未同步的资料 | [记录页](../../system/ui/shell/ai_pages.c#L224)、[ai-pages-2 报告](../tasks/ai-pages-2/REPORT.md) |
| 确认、过期及高优先级打断（`confirm`、`ConfirmExpired`、`interrupt`） | 确认事务、通知打断、成员及版本 | 有 | 人确认／取消、过期、来电和闹钟打断已有，快照由语义导出；AI 不能代人批准 | [确认卡](../../system/ui/shell/confirm_card.c#L190)、[打断卡](../../system/ui/shell/interrupt_card.c#L67)、[confirm-pages 报告](../tasks/confirm-pages/REPORT.md) |
| 飞行模式拒绝及普通拒绝（`AirplaneOn`、`Refused`） | 设置、策略回执、返回 | 有 | 告知真实拒绝原因，成人可关闭飞行模式后重试；孩子不能绕过开关 | [拒绝浮层](../../system/ui/shell/airplane_card.c#L26) |
| 键盘、文字编辑菜单（`Keyboard`） | 文本输入、剪贴板、听写、撤销／重做 | 部分 | 拼音候选、听写、选区动作及重做按钮已有；密码只能人输入，快照仍手写，不能称所有壳页已迁移语义合同 | [键盘快照及浮层](../../system/ui/shell/keyboard.c#L126)、[重做报告](../tasks/settings-small/REPORT.md) |
| 电话、记录、通话、按键（`Phone`、`PhoneRecents`、`PhoneCall`、`PhoneKeypad`） | `call.*`、通话数据、打断、铃声 | 部分 | 拨号、来电、记录、静音／耳机路由、分钟计时和失败状态已有；通话录音仍不可用；**通话声音、VoLTE 及真实模组仍待测**（RK-07、RK-18） | [电话页面](../../system/apps/phone/phone.c#L428)、[通话控制及录音缺口](../../system/services/comm/comm_service.c#L613) |
| 联系人、编辑联系人（`PhoneContacts`、`ContactEdit`） | 成员数据、联系人能力、输入 | 有 | 持久联系人、分页、编辑、拨号／短信入口与快照已有 | [联系人页面](../../system/apps/contacts/contacts.c#L236)、[phone-2 报告](../tasks/phone-2/REPORT.md) |
| 紧急呼叫（`PhoneEmergency`） | 锁定入口许可、`system.emergency_*`、临时射频 | 部分 | 锁屏、成员门及飞行模式下可进入受限拨号；AI 只开公开页，拨号／按键／挂断限本人；无 SIM 和真实呼叫均未上机验证 | [紧急浮层](../../system/apps/phone/phone.c#L63)、[合并验证报告](../tasks/cell-features-merge/REPORT.md) |
| 短信、会话、撰写、长编辑（`Sms`、`SmsThread`、`SmsCompose`、`ImeLong`） | `sms.*`、`message.*`、成员数据、通知、草稿 | 部分 | 持久会话、发送结果、草稿、通知及长短信收发拼接已有；接口文本上限已是 765，不能再写 118 字或内存 16 条；真实模组与运营商分段待测 | [短信页面](../../system/apps/sms/sms.c#L219)、[发送合同](../../system/services/comm/comm_service.c#L658)、[长短信报告](../tasks/sms-multipart/REPORT.md) |
| 备忘与清单（`Memo`、`MemoDetail`、`MemoEdit`、`MemoChecklist`、`MemoUndo`、`MemoSaveFail`） | 成员数据、草稿、保存回执、撤销 | 有 | 列表、长文编辑、清单勾选、删除撤销、保存失败与重试已有；本地落盘不等于中枢备份 | [备忘页面](../../system/apps/memo/memo.c#L377) |
| 录音备忘和待整理（`MemoRecord`、`MemoPending`） | 录成文件、音频焦点、转写、成员 | 有 | Ogg Opus 录音、取消、播放、文件关联、断电恢复及联网后转写已有；转写依赖中枢，现有报告使用合成转写 | [录音与转写](../../system/apps/memo/memo.c#L92)、[memo-record 报告](../tasks/memo-record/REPORT.md) |
| 接下来、月历（`Upcoming`、`Calendar`） | 时间、闹钟／提醒、通知 | 有 | 按日事项、月翻页、选日、提醒入口及快照已有 | [日历页面](../../system/apps/calendar/calendar.c#L157) |
| 提醒编辑、选时间（`ReminderEdit`、`ReminderTime`） | `reminder.*`、文本输入、时间 | 有 | 新建、修改、重复、删除与成员隔离已有；日期时间选择仍是本页自己的控件 | [提醒编辑页和动作](../../system/apps/calendar/calendar.c#L233)、[报告](../tasks/reminder-edit/REPORT.md) |
| 闹钟列表与编辑（`Clock`、`ClockAlarm`） | 时间、闹钟、铃声与打断 | 有 | 新建／修改、开关、重复、删除及响铃打断已有；RTC 从真待机唤醒另验 | [闹钟页面](../../system/apps/clock/clock.c#L199)、[clock-alarm 报告](../tasks/clock-alarm/REPORT.md) |
| 倒计时（`ClockTimer`） | 时间任务、提示音、成员 | 有 | 开始、暂停、继续、重设、到期提示已有；切页仍继续 | [计时页](../../system/apps/clock/clock.c#L304)、[sound-clock 报告](../tasks/sound-clock/REPORT.md) |
| 秒表（`ClockStopwatch`） | 单调时间、成员、局部更新 | 有 | 开始／暂停／继续、计次及复位；每秒改文字，离页、锁屏继续，重启清零 | [秒表页面](../../system/apps/clock/clock.c#L310)、[clock-extras 报告](../tasks/clock-extras/REPORT.md) |
| 世界时钟（`ClockWorld`） | 时区、成员数据、分钟更新 | 有 | 选城市、增删最多六个、夏令时及跨日显示、重启保留已有 | [世界时钟页面](../../system/apps/clock/clock.c#L310)、[报告](../tasks/clock-extras/REPORT.md) |
| 相机（`Camera`） | 相机服务、文件、媒体索引、资源准入 | 有 | 取景、拍照确认及 `camera.photo` 保存彩色 PNG、入索引；`camera.capture` 仍是内存采集，不能混称保存。真机出帧另验 | [相机页面](../../system/apps/camera/camera.c#L177)、[camera-gallery 报告](../tasks/camera-gallery/REPORT.md) |
| 锁屏相机（受限 `Camera` 浮层） | 锁定白名单、当前成员、拍照保存 | 有 | 锁屏、成员门和密码页可拍；只回看本次照片，不得进旧相册／文件；解锁后正常归档，浅睡边界见原报告 | [受限相机](../../system/apps/camera/camera.c#L147)、[lock-camera 报告](../tasks/lock-camera/REPORT.md) |
| 相册、看图（`Gallery`、`Photo`） | 文件索引、解码、缩略图、成员 | 有 | 九宫格分页、原比例灰阶看图、确认删除及成员隔离已有；“问这张图”只接上下文，未提供图像上传和视觉识别 | [相册与图片页](../../system/apps/gallery/gallery.c#L170)、[报告](../tasks/camera-gallery/REPORT.md) |
| 扫码结果（`Scan`） | 相机、quirc、剪贴板、联系人、Wi-Fi、电话 | 有 | 文字／网址复制、电话／名片／Wi-Fi 经人工确认执行；密码不进快照。不等于自动打开网址或实机识别率验收 | [扫码页](../../system/apps/camera/scan.c#L108)、[qr-scan 报告](../tasks/qr-scan/REPORT.md) |
| 文件、列表、操作、目标选择（`Files`、`FileList`、`FileActions`、`FileMove`） | 文件服务、索引、作业、成员 | 有 | 浏览、预览、重命名、单选／多选拷贝移动删除及失败回执已有；多选针对文件，批量操作没有组合撤销 | [文件应用](../../system/apps/files/files.c#L266)、[files-app-1 报告](../tasks/files-app-1/REPORT.md)、[files-ops 报告](../tasks/files-ops/REPORT.md) |
| 文件命名、文本编辑（`FileName`、`FileEdit`） | `file.mkdir/write`、草稿、文本输入 | 有 | 新建文件夹／文本、保存重开、断电草稿恢复已有；编辑上限 4095 UTF-8 字节，超长拒绝编辑 | [命名和编辑页](../../system/apps/files/files.c#L310)、[files-ops 报告](../tasks/files-ops/REPORT.md) |
| 选文件（`FilePicker`） | 文件服务、成员、类型过滤 | 有 | 系统浮层提供选择、返回、取消和快照，业务页面复用 | [选择器](../../system/ui/file_picker.c#L90)、[报告](../tasks/ui-file-picker/REPORT.md) |
| 无线传输（`Transfer`、`TransferLive`） | 无线传输服务、一次性授权、成员 | 有 | 开启、数字授权、地址、状态和关闭已有；仅开放授权目录，离页断开，真网络传输另验 | [传输页](../../system/ui/shell/transfer_pages.c#L71)、[transfer-pages 报告](../tasks/transfer-pages/REPORT.md) |
| U 盘传输（`TransferUsbBusy`、`TransferUsbOn`、`TransferUsbFail`） | USB 挂载互斥、成人确认、安全退出 | 部分 | 忙碌／开启／失败页及安全退出动作已接；不能列出占用应用，真 FAT／USB 未验 | [USB 页面](../../system/ui/shell/transfer_pages.c#L99)、[报告](../tasks/transfer-pages/REPORT.md) |
| 活动卡、任务列表（`ActivityCard`、`Tasks`） | 媒体、通话、`job.list/cancel` | 有 | 正在播放／通话的活动卡、运行中与完成的作业、取消及快照已有；没有进度的业务不会凭空生成进度 | [活动卡与任务](../../system/ui/shell/activity_card.c) |
| 听（独立应用） | 媒体播放、焦点、后台、内容目录 | 无（独立页面） | `media.play/pause/resume/seek` 和续播已实现，但未见“听”的应用注册与可打开页面；主页仍有该入口和空卡片，不能据服务实现改成“应用已做” | [媒体能力](../../system/services/audio/media_service.c#L898)、[主页入口与打开失败](../../system/ui/shell/navigation.c#L1745) |
| 远程画面（`remote`：`Remote`、`RemoteLoading`、`RemoteOffline`、`RemoteAddress`、`RemoteType`、`RemoteBlocked`） | 远程会话、解码、输入、中枢浏览器 | 部分 | 地址、整段输入提交、翻页、点按／长按、重连及六种状态已有；无历史网址。设备快照是状态和输入，网页正文依赖中枢；真实 Chromium 节点未验证 | [远程页面及动作](../../system/apps/remote/remote.c#L645)、[remote-app 报告](../tasks/remote-app/REPORT.md) |
| 计算器（`Calculator`） | 本地计算、成员页面、能力总线 | 有 | 四则、括号、百分数、错误状态及 AI 快照已有，不依赖联网 | [计算器页面](../../system/apps/calculator/calculator.c#L308)、[报告](../tasks/calculator/REPORT.md) |
| 天气、城市搜索（`Weather`、`WeatherCity`） | 中枢天气能力、本地上次数据、今日卡片 | 部分 | 设城市、当前天气、预报、断网显示上次结果及今日卡片已有；真实数据源未联网跑过，不能当在线天气验收 | [天气页面](../../system/apps/weather/weather.c#L269)、[weather 报告](../tasks/weather/REPORT.md) |
| 首次开机和绑定（`FirstBoot`、`BootWifi`、`BootWifiPass`、`BootJoining`、`BootJoinFail`、`BootClaim`、`BootNoHub`、`BootConfirm`、`BootRestore`、`BootHello`、`BootRevoked`） | 配网、配对、成员、恢复、进度持久化 | 部分 | 配网、绑定、失败、吊销、恢复及继续入口已有；离线示例正文未做，真实家庭服务器流程另验 | [开机页面](../../system/ui/shell/boot_flow.c#L55)、[boot-flow 报告](../tasks/boot-flow/REPORT.md) |
| 备份与恢复（`Backup`、`BackupFail`、`Restore`、`RestoreRun`、`RestoreDone`、`RestoreFail`） | `backup.now`、`restore.preview/apply`、覆盖确认 | 部分 | 当前成员手动备份、预览、恢复进度／结果和失败重试已有；自动备份不能设置，设备级资料和增量对账未完成 | [备份／恢复页](../../system/ui/shell/backup_pages.c#L33)、[backup-pages 报告](../tasks/backup-pages/REPORT.md) |
| 更新及恢复启动（`Update`、`UpdateReady`、`UpdateBlocked`、`UpdateFail`、`UpdateInstall`、`UpdateDone`、`BootRecovery`） | 更新服务、作业、文件选择、回滚 | 部分 | 检查／下载、卡上安装、阻止／失败、完成与回滚结果已有；更新说明不可查，真实固件源和引导程序回滚另验 | [更新页面](../../system/ui/shell/update_pages.c#L16)、[update-pages 报告](../tasks/update-pages/REPORT.md) |
| 电源菜单、低电量、充电和关机（`power_menu`、`BatteryLow`、`BatterySaving`、`Charging`、`off`） | 电源状态、关机图回执、成员、保存 | 有 | 关机／重启、省电、充电和关机画面已有；成员门等基础浮层已迁移语义，电量页仍有手写快照，真实功耗另验 | [电源菜单](../../system/ui/shell/power_menu.c#L47)、[电量页面](../../system/ui/shell/power_pages.c#L36)、[关机图](../../system/ui/shell/off_picture.c#L35) |
| 清空设备、完成（`FactoryReset`、`FactoryDone`） | 成员／家长验证、确认事务、工厂重置 | 有 | 真实验证与确认后清空、进度与完成页已有；AI 不可代填密码或自行确认 | [重置页面](../../system/ui/shell/factory_pages.c#L24)、[factory-pages 报告](../tasks/factory-pages/REPORT.md) |

## 5. 建议的开发顺序

**先定接口，再补底层**（D-64）。按[设计审计 §5.2](../audits/2026-11-04-codex-ui/report.md) 改过：原来的第 1 阶段写着“依赖：无”，其实漏了跨服务的约定；原来的第 1、2 阶段（数据、文件、时间、通知、确认卡、应用注册、文本输入、音频、Wi-Fi、成员和 PIN）已经做了大部分（2026-11-05 合入 main）。

| 步 | 内容 | 说明 |
| --- | --- | --- |
| 先定约定 | 成员和验证；对象编号、版本和保存回执；分页和编辑会话；确认事务；后台作业的状态；浮层和输入归属 | 可以先用模拟数据，但不能排到各服务做完之后。已经做的部分对一遍 |
| 首版纵向打通 | 家庭绑定和成员进入 → 本地备忘保存；内容转换和下载 → 离线打开 → 读听续播；任意页 AI → 确认动作 → 真实回执；备份 → 刷机后恢复 | 尽早检验约定，不先搭齐所有空壳 |
| 并行 | 界面框架：主题、组件、字体和图标管线、系统手势、系统浮层、刷新策略 | 热区、灰阶区、浮层恢复和资源预算要和约定同步 |
| 系统壳和首版应用 | 主页、锁屏、控制中心、通知栏、键盘；设置、备忘、文件、听 | 上面的约定和服务 |
| 按版本接 | 电话、短信、日历、时钟、相机、相册、蓝牙、远程 | 先定可用、不可用和失败状态，再接真实实现；用现有的网络、作业、总线，不另建一套 |
| 单列验收 | 身份、备份恢复、系统更新 | 不能散在设置应用最后；都要有串联场景（[审计报告 §4.4](../audits/2026-11-04-codex-ui/report.md)） |
| 随时 | 实机验证，见下 | 用户同意 |

不建议等底层全部做完才碰界面，也不建议反过来：前者看不到效果，后者要返工。

## 6. 只有实机能回答的问题

| 问题 | 影响 | 怎么测 |
| --- | --- | --- |
| 模式 2 能否连上普通蓝牙耳机，播放和录音都通吗，音质如何 | D-63 | 在出厂固件的设置里切模式，读串口日志拿协议。切模式可能写 NVS，要用户同意 |
| 模式 3 下系统自己的声音和麦克风还在吗 | 能否同时当音箱 | 同上 |
| BH1098G 能否跑 44.1 或 48 kHz | 听音乐的音质 | 测量固件，要写设备 |
| 通话声音怎么走，VoLTE 是否可用 | 电话能否做（RK-07、RK-18） | SP-06、SP-07，打电话要花费用，需要用户选择 |
| RTC 闹钟能否唤醒 | 闹钟可靠性 | 测量固件 |
| S31 自带蓝牙和 Wi-Fi 同时工作、占多少内存 | D-63 的备选 | 测量固件 |
| 浅灰和白能否分清，细字观感；灰底控件能否走快刷 | D-61 | 把设计稿显示到屏幕上 |

## 7. 补首版页面时新发现的要求

2026-11-04 画首版流程时发现的，当时对照的是基线 `cd486a2` 和系统服务的设计文档，“已做”是系统服务后来补上的，见它标的章节。页面见[界面规范第 7 节](../product/system-ui-spec.md)。

| 要求 | 谁要用 | 现状或来源 |
| --- | --- | --- |
| 电源键长按 | 电源菜单 | **已做**（系统服务设计 §13） |
| 成人闲置退回改规则，验证后回到原页和原位置（D-95） | `MembersBack` | **已做（按推荐暂定，模拟器）**：阅读闲置不退出；锁屏后默认 120 秒内直接返回，达到时长经成员门和密码回原页原位置；时长单处定义并可设置。已补成员门换人后的应用生命周期及后台繁忙时语音到期撤销回归。成员门仍为占位外观，时长等用户最终确认，见系统服务设计 §11.4 和[执行报告](../tasks/member-wake/REPORT.md) |
| 放声音时，锁定状态下音量键能用（建议） | `LockPlaying` | **已做**（系统服务设计 §13）。顺带发现音量键以前根本没人处理，也补了 |
| 状态栏要知道麦克风在做什么：本地等名字、声音送出去、录音 | 所有页面 | **已做**：状态 `audio.mic`（系统服务设计 §10.3） |
| 活动卡：现在放什么、暂停、回到内容；任务列表：进度、取消、结果 | `ActivityCard`、`Tasks` | **任务列表已做**：`job.list`、`job.cancel`（系统服务设计 §13.1）。活动卡的数据是现成的：`media.status`、`media.pause` |
| 作业七个阶段合成四步显示 | `ImportJob` | 任务有阶段词（§13.1）；放书的作业要等中枢协议接上 |
| 每个对象能查保存到哪一步 | `SaveMark`、`BackupFail` | **已做本地这一段**：`data.status`（系统服务设计 §15.4）。“等联网备份”要等同步 |
| 配对状态（未配对到已吊销）、设备自己的热点和小网页、存储卡配对文件 | `FirstBoot`、`BootClaim`、`BootRevoked` | 中枢设计 §13，还没人做 |
| 一小段可离线读的示例内容 | `BootHello` | D-99，新的小资源 |
| 文字编辑：移动光标、选区、撤销 | `ImeEdit` | **已做**（系统服务设计 §9.4.1）。中文的“词”现在只是两个字，要准得等词库能反查 |
| 关机前写好关机图，没电时先存再写 | `PowerOff`、`PowerOffEmpty` | **已做**（占位外观，系统服务设计 §13） |
| 无线传输：一次性数字授权，只放一个目录，超时断开 | `Transfer` | 系统服务设计 §13 明确后移；画的是建议。2026-11-09 服务已有（[wireless-transfer](../tasks/wireless-transfer/REPORT.md)），页面在 files-app 做 |
| 额度用完是一种明确的错误 | `AiBudget`、听写 | 中枢设计 §5.2、§5.3 |
| 网页的翻页、长按、提交文字、地址编辑 | `RemoteAddress`、`RemoteType` | 中枢设计 §13，D-85 用户选了首版做 |
| 文字选区的动作：问 AI、加单词 | 阅读、报纸 | R-036，第 4.2 节的文本框架 |
| 确认规则改成默认自动（D-105，R-063）：对外动作看 `_meta.asked`，高风险能力标 `ask_by_hand`；活动记录 `activity.list` | `AiLog`、确认卡 | 中枢设计 §7.5、§13；系统服务设计 §7 是按旧规则写的 |
