# B. 墨水屏产品与开源系统架构调研：给 MCU 墨水屏 AI 设备 OS 的经验

> 调研日期 2026-11-01。约定：带来源编号 [xx] 的陈述是**有据事实**（来自代码、官方文档或媒体）；标 **【推断】** 的是本文自己的分析。代码类结论来自实际 clone 的仓库，commit 见文末。
> 目标设备：ESP32-S31（512KB SRAM + Octal PSRAM，16MB flash），4.7" 1216×684 16 灰并口屏（EPDiy），LVGL 9，FreeRTOS/ESP-IDF，Wi-Fi + 4G，音频，触摸 + 按键，前光。

## 0. 一页结论

1. **最大的风险是范围，不是技术。** MuditaOS（FreeRTOS 手机系统）约 35 万行 C/C++（含厂商 BSP）。changelog 里修复 252 条、新增只有 28 条。Pure 仅售约 3000 台，最后一版固件是 2024-03，下一代 Kompakt 直接换成 AOSP [M1][M3][M4]。ESP32-S31 没有 AOSP 这条退路，所以只能"薄设备、厚 Hub"。
2. **刷新必须由一个调度器统一裁决。** 应用只提交"区域 + 语义刷新类型"，合并、升降级、按计数全刷、丢弃过期帧都交给调度器。范本是 KOReader UIManager [K1]，MuditaOS 和 CrossPoint 是 MCU 上的简化版 [M1][C1]。
3. **单前台 Activity + 少量常驻 Service + 类型化消息 + 唯一渲染任务。** CrossPoint 把"每个 Activity 一个 8KB 渲染任务"改成全局只有一个 [C1]。
4. **页面化，不滚动。** 内容预先排版成页缓存，翻页就是读一块再贴图。这和"有终点"的日报天然契合。
5. **位置 = 内容哈希文档 ID + (章节, 码点偏移) + xpointer/CFI 字符串 + 百分比兜底。** 页码只是派生值 [C1][K1]。
6. **多成员从第一天起作为所有用户数据的命名空间。** 参照 Kindle Household [D5]。KOReader、CrossPoint、MuditaOS 都是单用户设计。

## 1. MCU 级系统

### 1.1 Mudita Pure / MuditaOS（NXP i.MX RT1051 + FreeRTOS，GPLv3）[M1]

| 方面 | 设计要点 |
|---|---|
| 内存 | 64KB OCRAM + 448KB DTCM + 16MB SDRAM。系统代码和系统堆在片内，应用代码和大缓冲在 SDRAM。屏幕 480×600。 |
| 分层 | BSP → Worker（连接硬件的线程）→ Service（常驻）→ Application（有 UI、生命周期有限的 Service 子类）。文档原话："除了总线消息，没有其他正确方式执行动作"。 |
| Bus | 同步/异步单播、订阅式多播、广播。`connect(typeid(Msg), handler)` 信号槽。旧的万能 `DataReceivedHandler` 已弃用。 |
| 启动 | Service manifest 声明名称、依赖和超时，SystemManager 按拓扑排序启动。**没有失败兜底**，启动失败直接抛 `SystemInitialisationError`。 |
| 应用 | ApplicationManager 负责启动、切换、窗口和显示访问权。manifest 声明可处理的 action 和 AutoLockPolicy。文档自承 popup "是逐步加进系统的，不是设计出来的"。 |
| GUI→E Ink | service-gui 在 context pool 中渲染整帧。同一区域来了新请求，旧请求就过期丢弃；屏忙时只缓存最后一帧。service-eink 与上一帧逐行求差，间隔小于 1/4 屏高的脏块合并，按 8 行对齐。刷新模式只有 NONE/FAST/DEEP，由应用调用 `refreshWindow()` 时指定（apps 中有 42 处显式 DEEP）。刷新失败后下一次强制 DEEP；空闲 200ms 关屏幕电源；刷新期间由 eInkSentinel 锁住最低 CPU 频率。 |
| 数据 | 自带 sqlite3.c + 自定义 VFS，按领域分库（联系人、短信、通话、笔记等），配异步 query 接口。 |
| 文件系统 | FAT（固件，只读）+ ext4（备份）+ ext4（用户）。选 ext4 是为了"意外断电时尽量不丢数据"。 |
| 蜂窝 | 完整状态机；AT→CMUX 三通道；URC 处理；CSQ 信号在"持续上报 / 混合 / 15 分钟轮询"间切换以省电；模组睡眠条件是"无通话 + UART 空闲 1s"；四种手机模式里有"仅短信"（定时联网收信）。 |
| 电源 | 按 Idle 调频，sentinel 可请求最低频率。放弃了 Suspend，因为应用代码在 SDRAM 中执行，"几乎不可能"优雅恢复；改用定制 LowPowerIdle 做到 2.08mA。文档自评 PowerManager 只是"第一次迭代"，把低功耗逻辑放进 ApplicationManager 是"糟糕的设计选择"。 |
| 测试 | Linux 模拟器，文档明说"看不到 E-Ink 刷新"。pytest harness 经 USB-CDC 驱动整机；UI 树可导出为 JSON（DOM）供断言。 |
| 结局 | module-* 与 products 约 34.9 万行 C/C++（我的统计：不含 sqlite3 和测试，含厂商 BSP）。Pure 最后版本 1.12.0（2024-03），仓库最后提交 2025-09。 |

**问题（有据）**：从 1.0.1 到 1.12.0 的 changelog 里，至少 31 条修复是崩溃、卡死或内存泄漏。另有多条与刷新有关，如"空列表不必要的 deep refresh"和"信号强度更新引起不必要刷新"。VoLTE 到 2023 年才按国家逐个以 experimental 加入 [M1]。2021 年现场测试期，官方称"开年以来已解决 100+ 问题" [M2]。用户报告过随机卡死、闹钟不响、短信和通话不可靠 [M6]。创始人说，更好的信号、音质和日历"无法在最终产品中交付" [M3]。

**为何 Kompakt 改用 AOSP**：有据的部分只有这些：Kompakt 跑 AOSP 12 分叉，去 Google，可侧载，带地图、相机、天气等应用；Offline+ 模式在硬件上切断 GSM 和麦克风；因驱动授权不会完全开源 [M4][M5][M7]。**官方的明确理由没有找到。**【推断】可能的原因：这些功能在 RTOS 上等于要自造网络栈、渲染和编解码；LTE/VoLTE 认证依赖 SoC 厂商的 BSP；工程力量长期被稳定性修复占满。

**教训【推断】**：
- 凡需要 Android 级软件栈的（网页、富媒体、AI、日报生成），全部放到 Hub。
- 电话和短信最后做，并且收敛到唯一的 modem service + 状态机。
- 服务启动允许降级：某个服务挂了，书仍然能读。
- 服务之间禁止同步阻塞调用。
- 应用间用 action 路由解耦。Mudita 自己的"新 AppManager 提案"就是要让应用不再知道彼此的名字 [M1]。
- 状态栏刷新要节流。
- 低功耗下的恢复路径必须在架构期验证。

### 1.2 CrossPoint Reader（Xteink X4/X3 用 ESP32-C3，现已扩展到多款 S3 设备，含 Metalio E-Ink 4，即 3.97" SPI 兄弟款，不是 Plus）[C1]

- **约束**：约 380KB 可用 RAM，无 PSRAM，单帧缓冲 48KB。项目规则：栈上局部变量小于 256B；内部 RAM 与 PSRAM 分开预算，并且要看最大连续块；进度保存要去抖。
- **Activity**：PR #1016 引入 ActivityManager，只保留一个渲染任务，配全局 RenderLock 和 Activity 栈；用 `startActivityForResult/finish` 取代存回调，消除了 `delete this` 一类生命周期问题。
- **页缓存**：章节首次打开时排版，写到 `.crosspoint/epub_<hash>/sections/N.bin`。文件头含全部影响排版的设置，作为 cache key。格式已到 v73：排版规则一变就 bump 版本、强制重建。页内存行、图、链接矩形和可见文本偏移表。
- **位置**：权威位置是 (spineIndex, 码点偏移)，页码从当前排版推导。它与 KOReader XPath 双向映射，百分比只做兜底。
- **持久化**：progress.bin 采用 tmp+rename 写法。原因见 #2275：原地截断写遇到断电，FAT 簇链损坏，文件再也清不掉。摘录库带稳定同步 ID、待上传标志、`.deleted` 墓碑和 `.bak`。
- **刷新**：FAST/HALF/FULL 三级；默认每 15 页做一次 HALF（可选 1/5/10/30/从不）；睡眠画面用 FULL 或 HALF 清屏。
- **CJK**：SD 卡上存按字号预光栅化的 `.cpfont`，每页只 prewarm 本页要用的字形（上限 512 个）。有 PSRAM 才允许直接加载 TTF。支持原生 ruby。
- **Wi-Fi**：会话结束后"静默重启"（`ESP.restart()` 加 RTC 标志）来清除堆碎片，再回到原来的界面。
- **范围**：SCOPE.md 拒绝交互应用、写作、RSS/浏览器类"主动联网"（理由是"后台 Wi-Fi 耗电"）和 PDF。服务集成放进 SD 卡插件（`device.json` 复用固件已有界面）。测试只有 71 组 host 端 gtest，没有设备模拟器。

### 1.3 其他 MCU 阅读器

| 项目 | 文本 / CJK | 分页与缓存 | 刷新 / 睡眠 |
|---|---|---|---|
| Open Book / libros [O1–O3] | 纯文本。Babel：Unifont 全 BMP 字形放 flash，每个码点 O(1) 查表，带双向类和断行属性 | `.pag` 侧车存每页 (偏移, 长度)，分页一次后翻页 = seek+read。书库文件有备份和工作副本 | 早期 SAMD51 只有 192KB RAM |
| EPDiy epub reader（LilyGo T5 4.7）[E1] | miniz + tinyxml2，无 CSS，仅拉丁字形，必须 PSRAM | — | 30s 后 deep sleep；**显示状态存 SD，唤醒后才能正确局刷** |
| M5Paper EPUB reader [E2] | 增量解析 ZIP/XHTML | 有界页缓存 + 检查点重建；封面缩略图放 PSRAM LRU，并持久化到 SD | 前后页缓冲；脏区局刷；周期清残影；light sleep |
| Inkplate [E3] | — | — | 默认每 10 次局刷自动全刷一次（建议 5–10） |
| PaperBoyS3 [E4] | — | — | 绕开标准波形直接驱动；只刷游戏区，可达 30–60fps |
| Metalio 原厂固件（同硬件）[E5] | 主机端转换成 `.ebook`：分块 zlib + 章节索引，运行峰值 RAM 小于 64KB | 列表和阅读器规定"不滚动" | LVGL 9.5；Clear/Balanced/Fast/Extreme 四档；滚动时用 DU，停下后用 GL16 收尾 |

【推断】按 4bpp 算，一帧 1216×684 = 415,872B。EPDiy highlevel 在 PSRAM 中分配 front、back 和 2 倍大小的 difference 缓冲 [E5]，合计约 1.66MB。512KB SRAM 连一帧都放不下，所以帧缓冲、页缓存和字形缓存全部放 PSRAM，SRAM 留给 DMA、Wi-Fi/TLS 和任务栈。deep sleep 之后 back buffer 不可信，要么把最后一帧落盘，要么唤醒后的第一帧全刷。

## 2. Linux / Android 产品

### 2.1 KOReader（重点）[K1][K2]

- **运行时**：Lua/LuaJIT 前端 + C/FFI 底座（crengine、MuPDF、各平台 framebuffer 驱动）。同一套前端运行在 Kindle、Kobo、PocketBook、reMarkable、Android 和 SDL 桌面上。
- **UIManager**：单例窗口栈。`sendEvent` 投给最上层窗口，`broadcastEvent` 投给全部。**绘制和刷新分成两个队列**：`setDirty(widget, refreshtype, region, dither)` 只负责入队，到下一个 UI tick 统一绘制，再执行刷新。
- **刷新类型**（优先级由低到高）：`a2 < fast < ui < partial < [ui] < [partial] < flashui < flashpartial < full`。
  - partial：白底文字（阅读器用），计入全刷计数。
  - ui：混合 UI（文档说"拿不准就用它"）。
  - fast：反色高亮。
  - a2：键盘这类纯黑白内容。
  - flashui：弹窗首次出现或关闭时防残影。
  - 带方括号的：要求驱动不合并。
- **调度规则**：
  - 新请求与队列里相交的请求合并成外包矩形，取较高优先级。
  - 每 6 次 partial（默认值）升级一次：没有区域时升为 full，有区域时升为 flashui。
  - 用户设置可以把闪烁类请求降级。
  - 滚动期间一律降为 fast。
  - 刷新区域可以由 lambda 在绘制之后再计算。
- **设备映射**：同一个语义类型在不同设备上用不同的 EPDC 波形。例如 Kindle 的 fast=DU、ui=GC16_FAST、full=GC16，Kobo 的 ui=AUTO。
- **插件**：每个插件是 `xxx.koplugin/{_meta.lua, main.lua}`。`init` 里注册菜单项和 Dispatcher 动作；按方法名接收事件（`onPageUpdate/onSuspend/onNetworkConnected` 等）；`is_doc_only` 表示只在阅读器里加载。Dispatcher 是全局动作注册表，手势和热键都绑定在它上面。
- **数据**：
  - 每本书一个侧车 `book.sdr/metadata.<ext>.lua`，全局设置只有一份。
  - 生词本插件用 SQLite，字段有 word、due_time、review_count、streak 和上下文。
  - 高亮存 pos0/pos1（xpointer）、epubcfi 和时间。
- **同步（kosync）**：文档 ID 是部分 MD5。上报 {document, progress(xpointer), percentage, device_id}。拉取时区分"较新/较旧"，默认较新的提示、较旧的不同步；代码注释强调"新"指时间上更新，不是更靠后。失败的请求进离线队列，最多保留 4 周、200 条。
- **多用户**：无。Profiles 插件只是"设置预设"，不是用户。
- **开发**：`kodev run` 启动 SDL 模拟器，可设屏幕尺寸和 DPI。

### 2.2 其他产品

| 产品 | 运行时 / 刷新 / 扩展 | 数据与同步 | 多用户 | 启示 |
|---|---|---|---|---|
| Kobo Nickel | Linux。Nickel 以 Qt 的 `-platform kobo` 参数启动，环境里有 QT_* 变量 [K1]。社区靠 NickelMenu 在运行时 hook 符号来扩展 [B2] | `KoboReader.sqlite`：content 表（ReadStatus、___PercentRead）、Bookmark 表（ChapterProgress、容器下标、偏移）；ShelfContent 表带 `_IsDeleted/_IsSynced` [B1] | 未见（未核实） | 软删除 + 同步脏位 |
| Kindle | Linux。`com.lab126.*` 服务（powerd、wifid、cvm…）经 LIPC 通信 [D1] | 内容目录是 `/var/local/cc.db`，恢复它时要先停 ccat 服务 [D2]；`.sdr` 侧车目录 [B1]。Whispersync 记"最远位置"，重读时它仍停在书尾，要手动重置 [D3][D4] | **每户最多 4 个儿童档案**。家长选择共享哪些书，自己的笔记和高亮对儿童不可见。Dashboard 可设阅读目标、就寝时间、书库；购书要 PIN；生词自动收集 [D5] | 多成员照此设计；位置同时记"最近"和"最远" |
| reMarkable | Linux。专有主程序 xochitl。rM2 由 xochitl 内的软件 TCON 驱动屏幕，第三方程序要靠 rm2fb shim 才能绘图 [R1–R3] | 同步是独立服务 rm-sync；文档按 UUID 组织，每页一个文件 [R1] | 单账号 | 同步独立成服务；波形/时序由一个模块独占 |
| PocketBook | Linux。InkView 库：事件回调，加上 `FullUpdate/PartialUpdate/DynamicUpdate` [K2][P1] | — | 2010 年固件加入 User Profiles（不同人用不同配置）[P2] | 刷新 API 只需少数几个语义函数 |
| BOOX | Android。系统刷新档位优先于应用设置。每个应用可单独设刷新模式、按点击次数全刷、DPI，并能从云端拉优化配置 [X1]。BSR 四档 [X2] | — | 未核实 | 系统档位 × 工具/成员配置，由 Hub 下发 |
| Light Phone III | AOSP 分叉，用 React Native 写；**OLED 屏，不是墨水屏** [L1]。功能以"tools"形式由在线 dashboard 管理；SDK 审核制，带模拟器 [L1][L2] | — | — | 工具模型 + 网页配置台 |
| Mudita Kompakt | AOSP 12 分叉，自研应用，侧载需 USB [M4] | Offline+：硬件切断 GSM 和麦克风 | — | 离线要有"硬"语义（可让 4G 模组断电） |
| Bigme / 海信 | Android 墨水屏手机，厂商自定义刷新增强 [H1][H2] | — | — | 【推断】通用应用的体验不可控，反过来说明"封闭工具集"更合适 |

## 3. 综合：MCU 墨水屏 AI 设备的地基（按重要性排序；均为【推断】，括号内是依据）

**L1 范围与分层**（依据：Mudita、CrossPoint SCOPE、Light 审核制）
- 设备只做固定的 6–8 个"工具"：阅读、听书、日报、问答、备忘、单词、成员/设置、备份。
- 生成、聚合、AI、格式转换都在 Hub 上完成。
- 写一份 SCOPE.md，列出"永不做"的事：浏览器、无限流、设备端第三方代码、完整的 EPUB/PDF 兼容。
- modem 先只做数据通道。

**L2 中央刷新调度器**（依据：KOReader、Mudita、Metalio、Inkplate）
- 接口：`refresh_submit(area, kind, owner)`。kind 分为 TEXT（GL16，计数）、UI、FAST（DU，按下反馈）、A2（键盘/电平/进度）、FLASH_UI、FULL。
- 规则：
  - 相交的请求合并，取较高优先级。
  - TEXT 每 N 次升级一次，N 可按成员和工具配置。
  - 同一区域来了新帧，旧帧作废；屏忙时只保留最新目标帧。
  - 连续交互期间降为 FAST，停下后用 GL16 收尾。
  - 刷新失败后，下一次全刷。
  - 空闲时关 TPS65185。
  - 每次刷新都带温度参数。
  - 状态栏更新要节流。
- 由唯一的 EPD 任务独占 EPDiy。
- LVGL 9 采用 DIRECT 模式，缓冲放 PSRAM。`flush_cb` 只登记脏区，到 `lv_display_flush_is_last()` 时把脏区和控件声明的 kind 一起交给调度器。重绘和刷新分成两个队列。

**L3 任务模型**（依据：Mudita Bus 的经验和坑、CrossPoint）
- 单前台 Activity 栈 + 固定的几个服务：display、input、audio、net、store/sync、power。
- 服务之间只用类型化消息队列，不做同步阻塞调用。
- manifest 声明依赖和资源需求（联网/音频/保持唤醒）；启动时允许降级。
- 只有一个渲染任务；工作线程数量有上限。
- 跨工具跳转走 action，例如"打开文档@位置"、"就本页提问"、"朗读本章"。

**L4 页面化与预排版**（依据：CrossPoint、Open Book、Metalio）
- 每个列表都有页码，也有终点。
- Hub 输出设备格式（分块压缩 + 章节索引）。
- 页缓存的 key = 内容哈希 + 字体/字号/边距 + 格式版本。
- CJK：flash 里存 1–2 套预光栅化的 4bpp 字形，按页 prewarm。
- 中文禁则断行和拼音 ruby（给孩子用）要尽早做。

**L5 定位器**（依据：KOReader、CrossPoint、Kobo、Kindle）
- 文本：`{doc, spine, cp_offset, xpointer?, percent}`。
- 音频：`{doc, track, ms}`。
- 注释：`{uuid, member, doc, start, end, quote, note, ts, deleted}`。
- AI 的页面上下文 = 当前 Locator 区间 + 可见文本 + 选区。回答里的引用可以反向跳转回原位置。

**L6 存储分层**（依据：Mudita ext4、CrossPoint #2275、Open Book 备份文件）
- 缓存放 SD，可以重建，并带版本号。
- 用户状态放内部 LittleFS：每个成员一份追加日志，定期做快照，用 tmp+rename 写入。
- 内容放 SD。
- 设备端不一定需要 SQLite，用追加日志 + 内存索引就够了。SRS 排程可以放在 Hub 上算。
- 备份就是把日志推到 Hub，恢复就是重放日志。

**L7 同步冲突**（依据：KOSync、Whispersync、Kobo、CrossPoint）
- 位置：LWW，同时保留 last 和 furthest；远端明显较新时提示用户，绝不静默后退。
- 注释：UUID + 墓碑。
- 学习日志：只追加，合并时去重。
- 离线队列要有上限；UI 永远不等网络。

**L8 多成员**（依据：Kindle Household）
- 所有用户记录的键都带 `member_id`。内容和排版缓存全家共享；位置、注释、生词、设置和 AI 历史按成员隔离。
- 儿童档案：家长在 Hub 网页上选白名单、设阅读目标和就寝时间；AI 用儿童策略；退出需要家长 PIN。
- 切换成员的顺序：先落盘当前状态，再切换命名空间，然后全刷一次。

**L9 电源与联网**（依据：Mudita、CrossPoint、EPDiy reader、KOReader）
- 联网按会话进行，由前台工具声明需求；同步批量完成。
- 阅读时用 light sleep，保留 PSRAM 内容；deep sleep 前标记"唤醒后首帧全刷"。
- 内部堆单独预算；保留"静默重启"作为兜底。

**L10 开发流程**（依据：KOReader、Mudita、CrossPoint、rM2-stuff、Light、Metalio WASM 模拟器）
- 整个系统要能在 SDL 下跑起来，模拟器要显示每次刷新的类型、区域、闪烁和残影累计。
- 用 UI 树 JSON + 串口注入输入做端到端测试；用 host 单测覆盖刷新合并和日志合并。
- 所有磁盘格式都带版本号，并配格式文档。

**L11 扩展与 AI 接口**（依据：KOReader Dispatcher、CrossPoint device.json、Metalio 设备端 MCP/A2UI）
- 设备上不运行第三方代码。扩展 = Hub 下发的声明式页面 + 设备上固定的控件集。
- "动作注册表"同时绑定按键，并作为 AI 可调用的工具清单。
- 每个前台工具都实现 `getPageContext()`。

## 4. 未能核实 / 存疑

1. Mudita 官方关于改用 AOSP 的理由：论坛正文和 JSON 接口被代理拒绝或只返回元数据，hackaday.io 页面也被拒。文中的原因是推断，Kompakt 的 SoC 也未核实。
2. "EinkWise"（BOOX）这个名称没找到出处。本文改用有据的 App Optimization 和 BSR；BOOX 固件 4.x 页面抓取失败。
3. Kobo 没有多用户：只是没找到相关功能，没有官方声明。Nickel 的刷新管线也未核实。
4. Kindle 的框架细节（booklet/Pillow/Mesquite）、cc.db 表结构、Whispersync 协议：只核实到名称和路径。
5. reMarkable 基于 Qt 只有间接证据；同步协议和波形未核实。PocketBook 现行固件是否还有 Profiles 不清楚（只找到 2010 年的记录）。
6. Light Phone III 用 React Native 只有 Wikipedia 一处来源；PaperBoyS3 的波形和 DC 平衡细节（Hackster 文章抓取失败）。
7. M5PaperS3 的社区固件未深入（只看了原版 M5Paper 的阅读器）；Bigme/海信每个应用的刷新策略未核实。
8. ESP32-S31 相关：PSRAM 容量、deep sleep 时 PSRAM 是否保持、SQLite 的性能，都未验证。性能数据没有实测；Mudita 的代码行数含厂商 BSP。

## 5. 来源

- [M1] MuditaOS https://github.com/mudita/MuditaOS （commit 2cd0e47，2025-09-25）：module-sys/README.md、SystemManager/doc/*、module-apps/ModuleApps.md、service-appmgr/service-gui/service-cellular 文档、service-eink/ServiceEink.cpp、module-vfs/README.md、doc/howto_simulator.md、test/README.md、pure_changelog.md
- [M2] https://www.mudita.com/community/blog/mudita-pure-production-update-february/ （2021-02-11）
- [M3] https://goodereader.com/blog/smartphones/mudita-is-going-to-be-releasing-the-kompakt-and-pure-2 （2024-01-30，转述 Forbes Poland）
- [M4] https://www.kickstarter.com/projects/mudita/mudita-kompakt/faqs
- [M5] https://liliputing.com/mudita-kompakt-is-a-minimalist-phone-with-an-e-ink-display-and-modern-features-like-wireless-charging-and-a-fingerprint-reader-crowdfunding/
- [M6] https://forum.mudita.com/t/instead-of-the-review-as-mudita-pure-is-just-alpha-release/4504 （2022-06-18）
- [M7] https://www.androidauthority.com/mudita-kompakt-hands-on-ces-2026-3631800/ （2026-01-14）
- [C1] CrossPoint Reader https://github.com/daveallie/crosspoint-reader （= crosspoint-reader/crosspoint-reader，develop 42d58da，2026-11-01）：README、SCOPE.md、.agents/rules/*.md、docs/activity-manager.md、file-formats.md、sd-plugins.md、src/SilentRestart.h、ProgressFile.h、lib/KOReaderSync
- [O1] https://github.com/joeycastillo/libros （3be5d84）；[O2] https://blog.tindie.com/2020/07/diy-multilingual-ebook-reader-pcb/ ；[O3] https://thenewstack.io/the-open-book-project-an-ebook-reader-you-can-build-yourself/
- [E1] https://github.com/martinberlin/epdiy-esp32-epub-reader ；[E2] https://github.com/raphaelfukuda/M5PaperEpubReader ；[E3] https://docs.soldered.com/inkplate/6/basics/partial-update/ ；[E4] https://www.cnx-software.com/2026/07/02/paperboy-game-boy-emulator-works-at-60-fps-on-esp32-s3-e-ink-devkit/
- [E5] Metalio E-Ink4-Plus https://github.com/CloudZao/Metalio-E-INK4-Plus （690a023）：README §9/§11、tools/ebook/SPEC.md、main/display/epd_refresh.h、display/highlevel.c（EPDiy 副本）
- [K1] KOReader https://github.com/koreader/koreader （035e00b，2026-10-31）：frontend/ui/uimanager.lua、pluginloader.lua、dispatcher.lua、docsettings.lua、plugins/kosync|vocabbuilder|profiles、platform/kobo/nickel.sh
- [K2] https://github.com/koreader/koreader-base （a5ad895）：ffi/framebuffer_mxcfb.lua、ffi/inkview_h.lua
- [B1] calibre https://github.com/kovidgoyal/calibre ：src/calibre/devices/kobo/driver.py、kobo/bookmark.py、kindle/driver.py；[B2] https://deepwiki.com/pgaskin/NickelMenu/3-architecture
- [D1] https://www.sixfoisneuf.fr/posts/kindle-hacking-deeper-dive-internals/ ；[D2] https://kindle-series-manager.readthedocs.io/en/latest/backup.html ；[D3] https://www.40tech.com/2011/01/04/how-to-reset-the-furthest-read-location-on-your-kindle/ ；[D4] https://www.guidingtech.com/fix-kindle-books-not-syncing-between-devices/ ；[D5] https://www.aboutamazon.com/news/devices/amazon-kindle-kids-setup-questions-answered
- [R1] https://developer.remarkable.com/documentation/xochitl ；[R2] https://github.com/ddvk/remarkable2-framebuffer ；[R3] https://github.com/timower/rM2-stuff/
- [P1] https://github.com/simmsb/inkview-rs ；[P2] https://www.mobileread.com/forums/showthread.php?p=932933
- [X1] https://onyxboox.com/files/manuals/BOOX_Poke4series_User_Manual_(20220617).pdf ；[X2] https://shop.boox.com/blogs/news/boox-super-refresh-bsr-technology
- [L1] https://en.wikipedia.org/wiki/Light_Phone ；[L2] https://engadget.com/2161468/the-minimalist-light-phone-iii-will-soon-support-a-curated-set-of-third-party-apps
- [H1] https://liliputing.com/bigme-hibreak-pro-smartphone-has-a-6-13-inch-e-ink-display-and-dimensity-1080-processor/ ；[H2] https://liliputing.com/hisense-a9-android-smartphone-has-a-6-1-inch-e-ink-display-with-snapdragon-662-and-up-to-6gb-ram/
