# 项目全貌与背景（接手与审计必读）

更新：2026-11-05（系统服务、系统界面设计稿、家庭中枢设计都已合入 main）。本文把散在各处的背景集中交代一遍：项目为什么存在、用户要什么、已经定了什么、做出了什么、哪些只是估算，以及怎么构建、怎么验证。细节都链接到唯一维护位置，本文不重复维护数据；两处说法不一致时，以被链接的源文档为准，并请把不一致记为问题。

外部审计请同时阅读 [审计任务说明](audits/codex-audit-prompt.md)。

## 1. 一句话

为用户本人和孩子打造一台 **AI 原生的墨水屏个人设备**（暂称“家庭 AI 手机”）：把原来刷消遣 app 的时间换成阅读、学习和知识获取，并让系统里的一切（页面、数据、动作）都能被 AI 读取和操作。

- **硬件**：只用 CloudZao Metalio E-Ink4 Plus（R-002）。
- **软件**：重新设计。上游固件只作驱动与原理参考（R-005、I-038）。

## 2. 用户、目标与约束

**谁在用**

- 用户本人（成人）。
- 孩子：5～6 岁，识字，可独立阅读，也要听书。
- 一台设备全家共用，登录切换成员（D-03、R-030）。
- 主要在中国大陆使用（R-038）。

**两个核心目标**（[需求与边界](requirements.md)，评估一切功能的标准）

- **G-001**：把刷“奶头乐”app 的时间，转成高价值信息获取与知识、能力增长。
- **G-002**：系统级原生 AI first。系统和 app 的一切行为、数据，以及任意场景和页面，都能被 AI 操作或获取。

**用户明确提出的要求**

- R-001～R-041 共 41 条，在 [需求与边界](requirements.md)。
- 原始想法 I-001～I-039 在 [想法记录](ideas.md)。
- 协作规则（[AGENTS.md](../AGENTS.md)）要求把三类内容分开记录：用户已明确的要求、用户仍在考虑的想法、助手的建议。审计时请检查有没有把建议写成了需求。

**用户已选的首版范围**

- **方案 A：AI 阅读与听书 + 平台底座**（D-01、R-031）。
- **断网时仍可用**：已下载内容的阅读与收听、备忘、单词复习（D-05、R-032）。
- **单词最小闭环**（R-036）。
- **“不做”清单**：不做通用浏览器、不做无限内容流等（R-037）。

**协作方式**（用户 2026-11-01、11-02 的指示）

- 按 [开发流程](process.md) 的 S0～S7 阶段推进。
- 确定的事直接做；不确定的给方案和推荐，记在 [决策记录](decisions/README.md) 和交互式决策板上。
- 用户未选、而且可逆的事项，按推荐“暂定”。
- 采购、改硬件、删除历史资料、花钱、对外发布，必须由用户明确选择。
- 用户原话：“按你的节奏来，先把底层搞好”。界面视觉设计放在底座之后。

## 3. 硬件（目标设备）

完整资料与出处见 [底座设计 §2～§7](system/foundation.md)、[CloudZao 硬件与源码](research/cloudzao.md)、[Plus 蜂窝通信](research/eink4-plus-cellular.md) 和 [上游固件研究](research/foundation/upstream-firmware.md)。

| 部件 | 内容 | 核实程度 |
| --- | --- | --- |
| 主控 | ESP32-S31：双核 RV32（RV32IMAFC），320 MHz，ESP-IDF 6.1，FreeRTOS（HZ=100）；芯片版本 v0.0 | 上游文档与代码；芯片版本、主频为实测 |
| 内存 | **PSRAM 16 MB**（AP Memory 八线，200 MHz）。内部 SRAM：出厂固件的 `heap_init` 是 265 KiB + 16 KiB 两段，另有 31 KiB RTC RAM，其中 64 KiB 预留给 DMA；出厂固件静态占用 226.7 KiB。模拟器档案已按实测结构改为 80 + 16 KiB 两段 + 64 KiB 预留，总量 96 KiB 仍是估计（[平台与模拟器 §5.7](system/platform-and-simulator.md)） | PSRAM 与出厂固件的内存布局是**实测**（[第 0 步](hardware/2026-11-03-phase0.md)）；本项目固件的静态占用未测 |
| 屏幕 | 4.7 英寸 1216×684 墨水屏，16 级灰，297 dpi。TPS65185 供电；LCD 外设并行驱动，EPDiy 式管线：每行 17 µs、688 行、每相至少 12 ms | 上游代码；波形与面板响应未实测 |
| 存储 | Flash：Puya PY25Q128HA，16 MB，DIO 80 MHz（手册支持挂起，ESP-IDF v6.1 不对它开启）。内部 Flash 用户分区 1408 KiB（LittleFS），资源分区约 6 MiB；TF 卡 | Flash 型号为实测；分区按上游，出厂分区表已实测核对一致 |
| 通信 | 4G 模组（NT26 路径）+ Wi-Fi。4G、蓝牙音频、功放共用一路电源 | 上游代码；短信与通话完整链路未核实 |
| 音频 | BH1098G 编解码（I2S，ESP 为从机）+ NS4150 功放 | 上游代码 |
| 输入 | 电容触摸（2 点）。盖板上 3 个虚拟键（HOME/上一页/下一页），是触摸控制器在屏幕外 Y≈1600 处识别的触点。BOOT 键直连 GPIO（拟作 AI 键，D-19）。电源与音量键经 IO 扩展芯片 | 上游代码 |
| 电池 | 1700 mAh | 上游资料 |

**实物状态**

- 整机（含 4G）2026-11-03 到货，比预计的 12 月初早（D-00）；已用 USB 连到用户的 Mac。上机步骤与结果见 [实机记录](hardware/README.md)。
- 不另买开发板（D-27）。
- **第 0 步已完成**（2026-11-03，只读，没有写设备）：硬件识别、整片备份、出厂固件的启动与运行日志，见 [第 0 步记录](hardware/2026-11-03-phase0.md)。我们自己的代码还没有在实机上跑过；PSRAM 容量、内部堆区段和 Flash 擦写时间已写回模拟器档案（[平台与模拟器 §5.7](system/platform-and-simulator.md)）。

## 4. 时间线

| 日期 | 事件 |
| --- | --- |
| 2026-10-26 | 项目建立。硬件锁定 E-Ink4 Plus，进入 S0 探索与调研 |
| 10-26～10-31 | S0 调研：竞品交互资料库（65 组）、跨品类竞品矩阵、57 期研究简报、AI 原生平台、学习动力、个人新闻、调度与体验等，见 [简报目录](briefs/README.md)。PR #1、#2（另一代理的竞品补证）合入 main |
| 10-28 | 用户在另一会话中做了墨水屏刷新算法研究（PaperBoy 对照、Plus 波形审计、PQ-BN v4 直驱），11-02 归档为 PR #7 |
| 11-01 | 进入完整产品开发流程：流程、PRD/SRS/架构草案、决策板（PR #4）。用户完成第一轮决策 D-00～D-11 |
| 11-01 | 底座设计 v1，有 5 项专题研究支撑；提出第二轮决策 D-12～D-27，用户 11-02 选完 |
| 11-01～11-02 | 两级模拟器 + 平台抽象 + 系统纵切（PR #6）。两次独立评审，提出第三轮 D-28～D-36 |
| 11-02 | 电源服务与待机场景，D-37～D-39 |
| 11-02 | 刷新算法研究集成进模拟器，第三次独立评审，D-40 |
| 11-02 | 用户在决策板选了 D-28～D-34、D-36～D-39 |
| 11-02 | PR #4、#5、#6、#7 全部合入 main，准备外部审计 |

## 5. 当前阶段

阶段门与产出见 [开发流程](process.md)。

| 阶段 | 状态 |
| --- | --- |
| S0 探索与调研 | 已完成，资料冻结为参考库 |
| S1 产品定义 | 基本完成：[PRD](product/prd.md)、[需求深读](product/needs-analysis.md) |
| S2 系统需求 | 进行中：[SRS](system/srs.md) v0.1，指标是待实机校准的建议值 |
| S3 架构与技术验证 | 进行中：[架构](system/architecture.md)、[底座设计 v1](system/foundation.md)、[风险与验证](system/risks-and-spikes.md)，以及模拟器（R-034） |
| S4～S7 | 未开始 |

**还没有做的**

- 设备端固件：`ports/esp32s31` 尚未建立，所有系统代码目前只在模拟器上运行。系统服务（数据、文件、时间、通知、确认卡、应用、文本输入、音频、Wi-Fi、成员、升级）只有模拟器验证，见[系统服务详细设计](system/system-services-design.md)。
- 家庭中枢服务器：[设计 v0.8](hub/hub-architecture.md) 的第 1 步在 `hub/`，只用假厂商和合成数据在本机验证过；真实模型和语音、账号配对、记忆、内容都还没做。
- 系统应用：界面框架和第一批系统壳已在模拟器里实现（[规范](product/system-ui-spec.md)），其余系统应用等[应用框架](system/app-framework-design.md)定了再做。
- 下一步怎么排见 [project.md](project.md) 最前面的接续记录。

## 6. 决策总览

详见 [决策记录](decisions/README.md)。交互式决策板：https://claude.ai/artifact/RWnCX8AyAbyqArc7D9Wsft ，源文件 [decision-board.html](decision-board.html)，用户的选择存在页面数据库里，由助手同步到决策记录。

| 轮次 | 范围 | 状态 |
| --- | --- | --- |
| 第一轮 D-00～D-11 | 产品与架构方向 | 用户全部已选（2026-11-01） |
| 第二轮 D-12～D-27 | 底座层面 | 用户全部已选（2026-11-02）；D-23、D-25、D-26 与推荐不同 |
| 第三轮 D-28～D-40 | 平台与系统实现 | 用户已选 D-28～D-34、D-36～D-39（D-34 与推荐不同）；**D-35、D-40 仍是暂定** |

**对系统影响最大的几项**

| 编号 | 用户选择 |
| --- | --- |
| D-03 | 全家共用一台设备，登录切换成员 |
| D-04 | AI 经家庭中枢服务器运行 |
| D-06 | 新工程，只移植驱动 |
| D-08 | 应用模型分阶段：内置 → 声明式页面 → 脚本 |
| D-10 | 界面用 LVGL 9 + 墨水屏适配层 |
| D-12 | 设备—中枢协议：自定信封 + JSON-RPC + MCP 工具/结果结构 |
| D-13 | 本地存储用混合方案：用户状态低频写内部 Flash，高频的阅读位置先合并再写，大体量事件写 TF 卡 |
| D-16 | 成员切换：成人要 PIN，孩子直接进入，成人空间闲置后自动退回 |
| D-19 | BOOT 键作 AI 键（待核实 strapping 风险） |
| D-23 | 所有页面数据对 AI 可见，含 PIN 等私密页面（R-039）；D-33 只掩码正在输入的凭据 |
| D-25 | 儿童对外通信不限制，外部副作用仍需在设备上确认（R-040） |
| D-26 / D-34 | 数据保留更长：转写 1 年（儿童 180 天），运行记录 180 天（R-041） |
| D-37 | 锁屏时 4G 保持在线，可接收短信和来电 |

暂定的两项：

- **D-35 墨水屏送行管线**：本端口设计。
- **D-40 首版显示驱动**：两种都保留，默认 EPDiy 端口。

## 7. 系统架构（已实现部分）

分层与接口见 [平台与模拟器 §1](system/platform-and-simulator.md)，设计依据见 [底座设计](system/foundation.md)，目录与构建目标见[仓库结构](system/repo-layout.md)（2026-11-03 按系统架构重组）。

```
apps/                   主页、阅读器（只依赖系统接口）
system/                 自研系统：boot/（启动装配）、services/（显示与刷新策略、输入、存储、电源、
                        音频、中枢连接、AI）、policy/（动作注册表与策略、成员）、ui/（界面任务，LVGL）、
                        text/（中文排版与字体）、util/（JSON）、include/（系统接口）
platform/include/ink/   OSAL + HAL 接口（显示、输入、存储、网络、音频、编解码、电源、设备、资源、
                        摄像头、串口、I2C）+ 探针（对用户重要的指标）
drivers/                芯片驱动 chips/（10 颗）与 PQ-BN v4 直驱内核 display/pqbn/
boards/metalio_plus/    Plus 的接法：IO 扩展芯片、屏电源顺序、触摸坐标、浅睡步骤、关机脉冲
ports/sim/              模拟器端口：OSAL（osal/）与各 HAL（hal/）在模拟设备上的实现
simulator/              模拟器：虚拟时间内核（kernel/vtk）、按设备尺寸的堆（memory，TLSF）、芯片模型
                        （models）、中枢（environment）、运行器与指标（runtime）、桌面窗口（desktop）
simulator/profiles/     设备档案：Plus 悲观档（CI 门槛）、Plus 16 MB、Plus 上游显示管线、
                        Plus PQ-BN、RK3566 类墨水屏、桌面 LCD
```

**主要服务做什么**（实现见 `system/services/*/`）

- **显示服务 + 刷新策略**：界面提交的区域按模式（DU/GL16/GC16）派发给显示 HAL。EPDiy 端口上有全局锁和周期清屏。异步面板（PQ-BN）上最多 16 个更新同时在途，不做周期清屏。
- **界面任务**：LVGL 9.5 渲染到 4 bpp 帧缓冲，所有分配放在 PSRAM（D-30）。
- **中文排版**（D-29，自写）：禁则、标点悬挂、两端对齐、分页，直接画进页面缓冲。
- **存储服务**：内部 Flash 上的 LittleFS（`/user`）+ TF 卡，小文件读缓存与预取。**Flash 守卫**：任何 Flash 读写都不与刷新波形重叠（D-36）。栈在 PSRAM 的任务访问 `/user` 视为死机，与 ESP-IDF 的断言一致。
- **输入服务**：手势识别（阈值按毫米）、长按、按键长按；锁屏时只放行唤醒键。
- **中枢连接**：WebSocket 上的 JSON-RPC，经 4G/Wi-Fi，带 TLS 成本。
- **AI 服务**：按住 AI 键，依次是提示音、开麦、预连中枢、`ai.listen`（带当前页内容、页面代次和可用动作）、Opus 上传、流式文字与语音回答。
- **动作注册表与策略关口**：按键和 AI 共用一套动作（`tools/list`、`tools/call`）。按成员白名单裁决；有外部副作用的动作需要在设备上确认，但确认卡片尚未实现，这类动作目前只会停在“待确认”。
- **成员**：内置一位成人、一位儿童作替身。中枢同步与 PIN 界面尚未实现；从儿童切到成人会返回“需要 PIN”。
- **电源服务**（D-37～D-39）：活跃、空闲（5 s 无操作降到 160 MHz）、锁屏（每 60 s 一片浅睡，电源键与 AI 键唤醒）。入睡前要等界面、屏幕、Flash 都安静。可接听待机时保留 4G 电源。

**已实现的应用**

- **主页**。
- **书架与阅读器**：分页、预排下一页、按键、滑动、点按区域翻页，阅读位置保存。

**尚未实现**

- 听书、个人报纸、备忘、单词、设置；
- 调制解调器服务（短信、来电唤醒）；
- 离线队列；
- 设备上的确认卡片、成员 PIN；
- 输入法；
- 账户与备份（R-022、R-023）。

## 8. 模拟器（R-034）：怎么做的、可信到什么程度

完整说明见 [平台与模拟器](system/platform-and-simulator.md)，它是模拟器的唯一维护文档。

**第一级：把系统编译成电脑程序，在模拟设备上运行**

- **虚拟时间内核**：每个任务是真实线程，但同一时刻只放行一个。按 FreeRTOS 语义：2 核、固定优先级抢占、同优先级按节拍轮转、互斥量优先级继承。Flash 停顿期间冻结所有任务。
- **按设备尺寸的堆**：内部 RAM 分几个区段，另有 DMA 预留和 PSRAM。
- **外设模型**：墨水屏逐行送行管线（欠载即花屏）、PMIC、触摸与共享 I2C、按 IDF 6.1 关缓存窗口的 Flash 停顿、断电撕裂、4G/Wi-Fi 与 TLS、I2S 音频、主频与浅睡、电流统计。
- **CPU 时间**：默认按代码里的成本标注计费（`costs_cycles`），结果确定。
- **验收**：场景脚本按预算判定。同一输入下，指标和截图逐位一致。

**第二级：自写 RV32 指令级模拟器（`tools/iss`）**

- 用 ESP-IDF 6.1 的 S31 编译选项交叉编译真实代码，测每个成本类的周期数：拷贝、字形、像素转换、JSON、解压、CRC、墨水屏送行、PQ-BN 内核。
- 用 CoreMark（3.08/MHz）和 P4/S3 的 PSRAM 实测数据拟合时序参数，见 [指令级校准](research/simulator/iss-calibration.md)。

**档案约定**

- `simulator/profiles/metalio-plus.json` 是悲观档，也是 CI 的门槛：成本取“拟合值 ×1.3”与“慢角”中较大的一个。
- `metalio-plus-psram16.json` 是“很可能的情况”。

**证据边界**（审计重点）

| 类型 | 内容 |
| --- | --- |
| 指令级实测（模拟的 CPU，非实机） | 上面列出的成本类，以及 PQ-BN 内核的每行、每相成本 |
| 估算 | LVGL（每像素 25 周期 + 每对象成本）、文本排版、堆管理、帧结束开销、LCD 中断占比、两核争用 PSRAM、**全部电流**、PQ-BN 的面板响应（研究的假设值） |
| 上游文档与代码核查 | 硬件参数、分区、时序常数（带出处与查阅日期） |
| 助手设计 | 底座架构、端口设计（D-35）、刷新策略、电源策略、PQ-BN 集成方式 |
| 实机 | 第 0 步：芯片、Flash、PSRAM、安全状态、分区，以及出厂固件的内存布局、启动时间和刷新时序（[记录](hardware/2026-11-03-phase0.md)）；出厂固件整屏每相 15.1 ms，与“上游管线复现”的预测一致。第 1 步：测量固件在实机上跑了指令级校准内核，30 个区段大多与模型相差不到 10%，全部在慢角以内；送行每行 28.0 µs（优化版）/ 32.3 µs（上游）（[记录](hardware/2026-11-03-phase1.md)）。系统本身还没有在实机上跑过 |

**独立评审与外部审计**（三次评审的问题全部已处理）

| 评审 | 结果 | 详见 |
| --- | --- | --- |
| 第一次 | 3 高 8 中 7 低 | [平台与模拟器 §5.2](system/platform-and-simulator.md) |
| 第二次 | 2 高 3 中、若干低，另 1 个系统缺陷。最重要：送行任务的算力几乎刚好等于扫描速度，照搬上游代码配小队列会花屏，由此提出 D-35 | 同上 §5.3 |
| 第三次（刷新算法集成） | 1 高：规划不能与扫描重叠。模拟器 3 处缺陷，另有若干低。顺带发现盖板键同时按住时主页键丢失 | [集成报告](research/eink-refresh-integration.md) |
| 外部全面审计（Codex，2026-11） | 19 条：高 10、中 7、低 2。本机复核全部成立，12 条已修复、3 条部分修复、4 条列为后续事项；复核另有 3 条新发现 | [审计记录](audits/README.md) |

## 9. 墨水屏刷新：两条路线

**EPDiy 端口（默认，D-35）**

- 两个送行任务“谁空谁取”，只转换区域内的列，预填 64 行。
- 波形表加全局锁。
- 刷新期间不访问 Flash（D-36）。
- 分析见 [平台与模拟器 §5.4](system/platform-and-simulator.md)。

**PQ-BN v4 直驱（可选）**

- 来自用户另一会话的研究：[研究报告](research/eink-refresh-algorithm.md)，资料在 [eink-refresh/](research/eink-refresh/README.md)。
- 内核在 `drivers/display/pqbn/pqbn_v4.c`：从研究代码拆出，另加了跨静止的维护接口 `pqbn_v4_skip`（2026-11-03 起的复审，见[审计记录](audits/README.md)）。
- 集成方式与复核结论见 [集成报告](research/eink-refresh-integration.md)：
  - 小更新快很多，因为没有全局锁；
  - 本 ISS 测得的算力是研究估算的 1.15～2.6 倍，必须用两个扫描任务；
  - 内核要求规划排在每相扫描之后，按原样整页翻页 908/1002 ms，现方案是 709/722 ms（集成报告当时的数；之后计入了跨静止的维护成本，长时间静止后的第一次刷新多约 15～60 ms）；
  - “规划跟在扫描后面逐带进行”的改造可降到 654/714 ms，但**尚未实现**；
  - 面板响应决定上限。

## 10. 关键数字速查（悲观档，EPDiy 端口，全部为模拟）

| 指标 | 数值 | 来源 |
| --- | --- | --- |
| 翻页完成 p50 / p95 | 709 / 722 ms | `reader_basic` 场景 |
| 操作到首次可见变化 p50 / p95 | 249 / 336 ms | 同上 |
| 点书到首页稳定 | 1.2 s | 同上 |
| AI 键到听见提示音 / 看到“正在听” | 61 ms / 513 ms | `ai_ask` 场景 |
| 松开 AI 键到第一段回答 p50 | 2.1 s | 同上 |
| 内部 + DMA RAM 最低余量 | 55 KiB（阅读）/ 38 KiB（AI，预算 32 KiB） | 两个场景；内部堆区段已按实测结构改写（[平台与模拟器 §5.7](system/platform-and-simulator.md)） |
| 锁屏到入睡 / 唤醒到页面恢复 | 约 1.05 s / 约 0.65 s（最慢 0.71 s） | `standby` 场景 |
| 可接听待机电流 | 约 6.6 mA（估算） | [平台与模拟器 §5.5](system/platform-and-simulator.md) |

PQ-BN 各配置的对比见 [system-comparison.md](research/eink-refresh/integration/system-comparison.md)。复现方法见下一节，各场景输出 `metrics.json`。

## 11. 构建、测试与复现

**依赖**

| 依赖 | 用途 | 缺失时 |
| --- | --- | --- |
| CMake ≥ 3.16、C11 编译器、Python 3 | 构建 | — |
| 能访问 GitHub | 首次配置时拉取 LVGL v9.5.0（FetchContent） | 无网络时用 `-DFETCHCONTENT_SOURCE_DIR_LVGL=本地副本` |
| `fonts-noto-cjk`、`python3-pil` | 生成中文位图字体 | 跳过阅读、AI、待机、PQ-BN 等场景，只剩少数测试 |
| 可选：`riscv32-esp-elf-gcc`（ESP-IDF 工具链） | `tools/iss/calibrate.sh` 与 PQ-BN 基准 | `iss-selftest` 不需要它 |
| 可选：numpy、Pillow、scikit-image、Noto Sans CJK | 研究场景生成（`docs/research/eink-refresh/pqbn_v4/rv/gen_scenarios.py`） | — |

**命令**

```bash
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure   # 164 项（要先建 hub 环境，见 hub/README.md，否则中枢的测试显示跳过）
cmake -S . -B build-werror -DINK_WERROR=ON && cmake --build build-werror -j8 && ctest --test-dir build-werror
cmake -S . -B build-asan -DINK_SANITIZE=ON && cmake --build build-asan -j8 && ctest --test-dir build-asan   # ASan + UBSan

# 单独跑一个场景（换档案即可比较两种显示驱动）
./build/ink-sim --profile simulator/profiles/metalio-plus.json --script tests/scenarios/reader_basic.script \
    --budget tests/scenarios/reader_basic.budget.json --assets build/assets --content build/test-content \
    --out out/reader --trace --frames
python3 tools/simreport/simreport.py out/reader --lang zh    # HTML 报告：刷新胶片、预算、时延、CPU、内存
```

**测试与 CI**

- GitHub Actions（`.github/workflows/sim.yml`）在每次推送 main、`claude/**` 和每个 PR 时运行：带 `-Werror` 的构建与全部测试，再跑 ASan/UBSan。
- `s31-size.yml` 只能手动触发，用 ESP-IDF 编译基线与上游固件，看静态内存占用。
- 测试的组成见 [平台与模拟器 §3](system/platform-and-simulator.md)，2026-11-04 之后新增的系统服务测试见[系统服务详细设计 §15](system/system-services-design.md)：
  - 模拟器自检（4 个档案）、死亡测试、两项确定性；
  - 主页、阅读、AI 提问、AI 工具调用、待机等场景；
  - 老化文件系统 + 最坏擦写、断电、上游管线复现、可移植性冒烟；
  - PQ-BN 档案上的场景、两个盖板键同时按住；
  - 审计复核加的四个回归场景：连续阅读也要落盘、连续快速翻页、关闭 AI 面板后不放迟到的语音、打不开的书上翻页不崩溃；
  - 底层系统：`systest-services`（系统服务 144 项检查）、`integration-lifecycle`（调用、任务、采集和远程画面在各种时序下的生命周期）、`integration-net-auto`（自动选网）、`integration-hal-switch`、`integration-starts`、`integration-ai-race`、语音（免按键、打断、一轮结束）、远程画面（浏览、重连）、离线意图；
  - ISS 自检。
- 预算文件在 `tests/scenarios/*.budget.json`，每条门槛写了理由（`why`）。

## 12. 已知问题与未完成项

审计可以重新评估这些问题，但请标明“已知”。

**PQ-BN**

- “规划跟在扫描后面逐带进行”（或 tile 状态双缓冲）尚未实现。档案里的 `plan_during_scan=true` 只是假设这项改造已完成的时序。
- 跨静止推进相时钟时补做的时间戳维护（内核 `pqbn_v4_skip`；原审计 AUD-011、后续事项 F-8，2026-11-03 第七、八轮复审后修好，见[审计记录](audits/README.md)）在下一次驱动开始时一次做完，按指令级模拟器实测计入成本：长时间静止后的第一次刷新多约 15～60 ms。真机端口可以改为在静止期间分批做。

**内存余量**

- AI 场景内部 RAM 最低余量 38 KiB（PQ-BN 档案 35 KiB），预算 32 KiB，偏紧。内部堆总量 96 KiB 仍是估计，要等本项目固件的 `idf.py size`。
- 任务栈：按 2026-11-03 改正后的估计（D-42），存储任务（3384 / 3584 字节）和中枢连接任务（3392 / 4096 字节）离配置的栈最近。这是主机上的偏悲观估计，是否加栈以 S31 编译的 `-fstack-usage` 为准。

**外部审计的后续事项**

- 资源准入与降级、写入回执、内容 ID 与版本、取消贯穿各层、页面版本与请求幂等、听书时的写入等，见[审计记录](audits/README.md) F-1～F-11（F-2 连续翻页的积压已完成）。

**估算项与未核实的硬件**

- 估算项见 §8。
- PSRAM 容量已实测为 16 MB（第 0 步）。BOOT 键 strapping 风险（RK-20）、4G 寻呼周期与待机电流（SP-10）、面板响应（SP-13）仍待实机验证。
- 验证计划见 [风险与验证](system/risks-and-spikes.md)，SP-00～SP-14。

**尚未实现的功能**

- 见 §7 的“尚未实现”。
- 另有 `ports/posix` + SDL 和 `ports/esp32s31` 骨架。

**仓库体积**

- `work/` 原始素材已移到归档分支 `archive/research-raw-2026-10`，见 [原始资料归档](research/raw-archive.md)。但历史提交仍包含它们，完整克隆体积大；彻底瘦身要改写历史，需单独决策。
- `docs/` 自身约 300 MB，主要是竞品资料截图和研究视频。

**公开仓库**

- 仓库可见性为 public。文档中包含用户与家庭的部分背景（如孩子年龄）、竞品手册截图等第三方资料。是否需要调整可见性或删减内容，由用户决定。

## 13. 文档地图

| 想知道 | 看这里 |
| --- | --- |
| 当前状态与历史记录 | [project.md](project.md) |
| 流程与阶段门 | [process.md](process.md) |
| 用户要求 / 原始想法 | [requirements.md](requirements.md) / [ideas.md](ideas.md) |
| 决策 | [decisions/README.md](decisions/README.md) |
| 产品定义 | [product/prd.md](product/prd.md)、[product/needs-analysis.md](product/needs-analysis.md) |
| 系统需求、架构、底座 | [system/srs.md](system/srs.md)、[system/architecture.md](system/architecture.md)、[system/foundation.md](system/foundation.md) |
| 系统服务详细设计、每个界面还缺什么 | [system/system-services-design.md](system/system-services-design.md)、[system/ui-dependencies.md](system/ui-dependencies.md) |
| 系统界面（设计稿在私有画布上） | [product/system-ui-spec.md](product/system-ui-spec.md)、[product/system-apps.md](product/system-apps.md)、[research/system-ui-references.md](research/system-ui-references.md) |
| 家庭中枢（服务器端，设计 v0.8，没有实现） | [hub/hub-architecture.md](hub/hub-architecture.md)、[hub/device-protocol.md](hub/device-protocol.md)、[research/hub-server-survey.md](research/hub-server-survey.md) |
| 模拟器与平台抽象 | [system/platform-and-simulator.md](system/platform-and-simulator.md) |
| 风险与实机验证计划 | [system/risks-and-spikes.md](system/risks-and-spikes.md) |
| 性能建模、指令级校准 | [research/simulator/](research/simulator/performance-modeling.md) |
| 刷新算法研究与集成 | [research/eink-refresh-algorithm.md](research/eink-refresh-algorithm.md)、[research/eink-refresh-integration.md](research/eink-refresh-integration.md) |
| 底座专题研究、上游固件 | [research/foundation/README.md](research/foundation/README.md) |
| S0 调研（参考库） | [briefs/README.md](briefs/README.md)、[research/interaction-library/README.md](research/interaction-library/README.md) |
| 协作规则（含 Codex 路由约定） | [AGENTS.md](../AGENTS.md) |
| 外部审计 | [audits/codex-audit-prompt.md](audits/codex-audit-prompt.md) |
| 实机上机步骤与实测结果 | [hardware/README.md](hardware/README.md) |
| 在用户电脑上用本机 Claude Code 接手 | [handoff/local-claude-code.md](handoff/local-claude-code.md) |

## 14. PR 与分支

| PR | 内容 | 状态 |
| --- | --- | --- |
| #1、#2 | 竞品交互资料补证（另一代理） | 2026-10-28、10-29 已合入 |
| #4 | 产品开发流程、PRD/SRS/架构草案、决策板、链接修复 | 2026-11-02 合入 |
| #5 | `work/` 原始素材移到归档分支（D-11） | 2026-11-02 合入 |
| #6 | 两级模拟器 + 平台抽象 + 系统纵切 + 刷新算法集成（基于 #4，含 #7） | 2026-11-02 合入 |
| #7 | 刷新算法研究归档 | 内容随 #6 进入 main |
| #8 | 家庭中枢设计 v0.8 | 2026-11-05 合入（和系统服务、系统界面一起） |

其余分支：

- `archive/research-raw-2026-10`：原始素材归档；
- `research/eink-refresh-algorithm`：#7 的分支。
