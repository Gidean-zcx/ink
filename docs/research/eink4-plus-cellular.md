# E-Ink4 Plus：蜂窝通信、硬件条件与社区固件

查阅日期：2026-10-26。关联 R-002、R-013、I-008、I-013、I-014。用户已限定只考虑 E-Ink4 Plus，并补充可直接依据原理图和硬件信息判断可开发性，不以现成固件为前提。

## 当前结论与上一轮修正

**有条件可行**：Plus 的板级源码直接连接 NT26 网络路径，模组厂商也提供短信和 VoLTE 能力资料。可以继续在 Plus 上研究开发，无需因为未找到完整电话 app 就改换硬件。

**仍不足以确认“现有 Plus 只差开发 app”**：NT26 是系列名，具体子型号、模组固件功能和音频接线尚未锁定。短信可作为较直接的软件开发方向；语音通话还需确认模组与板载麦克风/扬声器之间实际可用的双向路径。

上一轮主要依据 README、屏幕配置和 Claw4 拨号代码，未追踪 Plus 自己的板类构造，因而把通信研究重点放在 Claw4。这一判断不充分。本轮在**同一个固定提交**找到 Plus 的直接 NT26 接入证据；修正来自核查范围扩大，不是上游突然增加功能。其他型号仅保留为历史比较或驱动参考。

## 证据分层：有代码不等于已装配，有硬件不等于已完成产品

| 判断层次 | 已取得的依据 | 当前能说到哪一步 |
| --- | --- | --- |
| 板级控制链路 | Plus 板类继承 DualNetworkBoard，并传入 NT26 专用引脚；配置定义模组与其他外设共享的供电/复位 | **有代码依据**，不只是仓库里存在一个闲置通用模组类。实物是否装配及其版本仍需对应。 |
| 蜂窝联网 | 选择蜂窝模式时创建 Nt26Board；后者构造 UART 网卡模组、处理注册事件、提供 AT 接口 | **有实现路径**，本项目未编译或联网实测。 |
| 短信收发 | 厂商有短信 API 与产品能力说明；社区有相关固件声明 | **有条件可行**，取决于模组功能固件、SIM 业务和具体发送/接收实现。 |
| 呼叫控制 | 厂商有 VoLTE、拨号、接听、挂断与事件 API；CloudZao 其他板有 AT 拨号参考 | **有条件可行**，不能把这些当作 Plus 当前固件已完成电话功能。 |
| 通话音频 | Plus 有 BH1098 与 S31 间的双向 I2S 音频代码 | **信息不足**：它证明设备录放音基础，不证明 NT26 的通话音频已经接入此链路。 |
| 待机接收 | 模组、电源、异步事件与唤醒路径需要协调；现有代码有共享电源管理 | **需单独设计与验证**。显示待机、MCU 睡眠和模组断电不是同一种状态。 |

这里的“可开发”指有成立条件的技术路线；不要求先找到完整第三方系统，也不把尚未确认的电路连接写成事实。

## Plus 固定源码核查

仓库：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)。提交：`ad8edf4150a688737c0d7c00cae4413079b262ef`。以下为只读代码核查，均未在实机运行。

1. [metalio_e_ink4_plus_board.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc#L54)的板类构造向 DualNetworkBoard 传入 NT26 TX/RX/MRDY/SRDY，默认网络参数为 0，即 Wi-Fi。
2. [config.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/metalio-e-ink4-plus/config.h)定义 TX=58、RX=59、MRDY=60、SRDY=3；注释说明扩展器 P12 为触摸/4G 等共享复位，P14 为 4G、蓝牙与功放共享电源。这是所查配置的映射，不能不分板卡版本套用。
3. [dual_network_board.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/common/board/dual_network_board.cc)只初始化当前选择的网络；切换模式先保存设置再重启。现有实现不能直接作为“Wi-Fi 上网的同时蜂窝电话常驻”的证据。
4. [nt26_board.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/common/board/nt26_board.cc)使用 UART2、2 Mbps 和握手引脚构造 UartEthModem，暴露 SendAtCommand 等接口。[uart_eth_modem.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/common/board/uart_eth_modem.cc)主要处理数据网络和注册事件。
5. [bt_audio_codec.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/common/audio/bt_audio_codec.cc)有 I2S 收发；[bluetooth_uart.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/display/screen/bluetooth_screen/bluetooth_uart.cc)的通话模式发送的是蓝牙 SCO 指令，不能凭函数名将其认定为 NT26 蜂窝音频。
6. [power_hw.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/common/power_policy/power_hw.cc)在相关共享电源操作中通知蜂窝服务准备断电/恢复。新系统若要持续接收来电短信，需要重新审视这些电源策略及功耗，不能直接继承阅读器的休眠行为。

本轮检索范围包括上述网络文件、network_screen 文件组、音频与电源文件，以及完整未截断的仓库文件清单。在所查网络文件中未找到完整的短信服务或拨号/接听状态机；文件名清单也未见独立短信/电话页面。这个结果不等于穷尽所有依赖，更不等于硬件不支持。

## 模组厂商资料：可以实现，但要对应子型号与固件

利尔达的 [NT26 产品能力表](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/index.html)将短信标为可选，且不同系列的音频接口、VoLTE 选项存在差异。其 [SDK 型号表](https://github.com/lierda-iot/CAT1.bis_OpenCPU#4-supported-models)还单列 F7B_A VoLTE 底包与 NT26FCNB70WNA。不能只凭“NT26”三个字推断每一块模组都具备相同语音能力。

[厂商 API 说明](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/software/api-overview.html)提供文本/PDU 短信接口，以及 IMS、呼叫发起、接听、挂断和事件回调接口。这些属于**模组内部 OpenCPU SDK**，不是可直接在 ESP32-S31 上调用的普通 C 库。若板上模组当前运行串口网卡固件，还需核对该固件暴露哪些 AT/数据通道；不能假定换用任意底包仍保留原有联网协议。

由此得到的工程判断：短信可能主要需要补控制协议、编码、存储和通知；通话可能还需要调整模组固件与音频路由。只有确认现有接线能承载音频，才能进一步判断可否全部用软件完成。当前未提出改 PCB 或更换硬件。

## 无名科技的固件与社区线索

沿用户提示检查了 [Nologo 论坛](https://chat.nologo.tech/)、其公开帖子给出的旧刷机入口，以及 Metalio 网站链接到的 [CloudZao Burner 固件仓库](https://burner.cloudzao.com/)。旧入口访问出现证书/抓取问题，不能据此推断没有资料。新仓库的静态页面初始计数为 0，但公开列表 API 成功返回数据；本轮采用实际 API 返回核查，未登录、未发布、未下载或烧录二进制。

访问范围说明：论坛读到的是搜索工具返回的首页和公开帖子文本，直接访问出现证书过期/超时，未完成全站论坛检索；新固件仓库与其公开列表接口可以直接读取。不能把受限的论坛检索当作不存在其他项目的证据。

| 项目 | 发布者明确标注的设备和版本 | 本轮结论 |
| --- | --- | --- |
| 布丁 Pudding，作者 paouke | 普通 E-Ink4 / ESP32-S3；v0.1.0 测试版，2026-10-14 | 简介列电话、短信和蜂窝网络，同时明确“短信目前无法发送，诊断中”。是很接近用户描述的线索；没有标为 Plus，不能直接安装或视为完整可用方案。该条目未提供 GitHub 源码链接。 |
| RickyOS，发布者 Ricky AI Studio | 仓库条目标为 Plus / S31；v1.2.5 | 确认存在 Plus 适配的发布声明，简介未说明短信/通话。不能与仅凭名称搜到的其他 GitHub 仓库自动认作同一源码。 |
| kiikoread · Metalio EINK 4 Plus | Plus V1.5、34Pin、S31；目录条目 rc100 | 侧重阅读、传书、前光、蓝牙翻页与 OTA，未从所读说明确认蜂窝电话。作者 [源码说明](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/e942aeb1745241273d737c584c88a8068ef6e933/README.md)还限定 E0470A14-AF 面板，提醒不能仅按“Plus”名称统一驱动。 |
| 固件口袋 | Plus / S31；发布者描述为集成多个适配固件 | 与动态功能组织相关的新线索，待另行研究。目录描述尚不足以证明无重启的 app 动态加载，未升级为选定方案。 |

当次 Plus 分类返回 13 个条目；已检查其简介和版本说明，未发现可直接证明 Plus 完整短信/通话的声明。这是有限范围的搜索结果，不是“社区尚无人开发”的结论。目录原始字段的必要摘录与实际查询参数保存在 [来源记录](/home/workspace/reader/docs/research/sources/2026-10-26-cloudzao-firmware-catalog.json)，用于复核型号、版本及作者原话。

其他已初查仓库包括 [mogo-e4p](https://github.com/chopper1026/mogo-e4p)与若干阅读固件；没有找到足以补上 Plus 通话音频证据的材料。无需为此扩大到替换主板或购买其他设备。

## 原理图和硬件判断还缺什么

当前固定仓库的完整文件清单只发现屏和 I/O 扩展器 PDF，没有找到整机原理图/BOM；其 [README 的原理图章节](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/README.md#8-schematics-and-docs)实际主要给出固件配置入口。不能把 config.h 称为已经审阅的原理图，也不能断言原理图不存在于其他渠道。

下一步需要对应的是少量关键事实：

| 事实 | 确认后能解决的问题 |
| --- | --- |
| Plus 具体板卡版本、4G 是否装配、NT26 屏蔽罩完整型号和固件版本 | 判断是可开启的软件功能，还是当前模组变体不提供所需功能。 |
| 模组到 BH1098/其他音频器件或 MCU 的实际音频连线与时钟关系 | 判断拨号后声音如何双向传送，是否可在既有硬件上开发。 |
| 实际 SIM 接入形式、卡的短信与语音业务 | 判断能否使用所需业务；Claw4 的内外 SIM 描述不能直接套给 Plus。 |
| 模组供电保持、异步事件及主控唤醒连接 | 判断锁屏/待机时能否接收并提醒，以及续航代价。 |

这些证据用于缩小判断范围，当前不要求用户逐项提供，也不以暂缺它们为理由停止产品原型讨论。未来验证可按“识别模组与功能 → 短信双向收发 → 呼入/呼出和双向音频 → 待机与断网恢复”推进；本轮没有执行刷机、AT 发短信或拨打电话。

## 对新产品的关键架构建议

蜂窝连接、短信存储/事件和通话状态可以作为系统服务，电话与短信界面作为 app，AI 调用同一套动作。数据网络选择与蜂窝来电服务应分别表达；切到 Wi-Fi 不应被默认为用户要求停止接电话。来电与故事、音乐、AI 对话之间由统一音频策略协调。

上述为助手建议，尚未确定接口、进程模型、驱动移植方案或首版范围。是否已存在第三方 app 只影响复用成本，不决定需求是否成立。
