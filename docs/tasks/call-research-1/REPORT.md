# 4G 通话与音频路径调研

日期：2026-11-11。基线：`343ce09e9cc78002138d15b75a1982d114b8a1a3`。

## 验收表

| 编号 | 结论 | 证据 | 命令／定位 |
| --- | --- | --- | --- |
| 1：路径、试验步骤、批准项 | 源码和资料已核查；试验清单整理中 | 原厂固定提交、同芯片厂商资料和 AT 手册；板上音频出口仍未实测 | 见下文 |
| 2：范围与隐私 | 进行中 | 开工工作区干净，HEAD 与任务基线一致；只新增本任务目录 | `git status --short --branch`；`git rev-parse HEAD` |

## 当前结论与证据边界

- **呼叫控制有条件可行；这块板的双向声音仍是信息不足。**【实测·任务书转述】模组为 `EC718PM`，固件 `XJ_EC718PM_Modem_AI801_V0101.001.b05`；外置卡已有 EPS、IMS 注册，值得先做一次受控通话。注册和拨号返回 `OK` 均不等于已接通或双向有声。
- **已经找到的音频链只有 ESP ↔ BH1098 ↔ 本机麦克风／功放。**【实现】原厂和当前端口均没有把蜂窝通话 PCM 接入这条链；切 BH1098 的耳机 SCO 模式只建立蓝牙音频，补不上模组到 ESP 的一段（V1～V5、P1～P4）。
- **不能据公开资料断言 AI801 能经现有串口或 USB 给主控送语音。**【资料】同芯片 NT26 有 PCM/I2S 和可选 VoLTE，但它是模组及固件能力，不能直接当作本机连接或 AI801 帧协议（S1、S2）。
- 本次只有本机源码／文档检查和公开资料核对，没有新增设备实测。先做只读识别，再分开验收“呼叫建立、下行声音、上行声音”；没有原理图也能缩小范围，但一次无声不能证明没有隐藏连线。

## 通话音频的可能路径

可信度分别评价“资料中的能力”和“本板实际可用”，不把两者合并。

| 路径 | 出处与证据类型 | 可信度与成立条件 | 本次结论 |
| --- | --- | --- | --- |
| 模组 PCM/I2S ↔ 外部 Codec／BH1098 的另一路数字接口 ↔ 麦克风、喇叭 | 【资料】S1 数字音频、S2 I2S/PCM；【实现】V1 只列 ESP 的一组 I2S，V2 没定义第二路 | 芯片能力高；本板低。须确认 BH 固件有第二路接口、实际连线及主从时钟 | 有条件可行，未找到接通证据；不能把两路主机的输出并接 |
| 模组模拟音频 ↔ BH1098 模拟输入／音频切换器 ↔ 功放与麦克风 | 【资料】S1 的模拟音频随子型号不同；S4 的 Air780EVT 内置 ES8311 后才有 MIC/SPK 脚 | 其他模组高；本板低。PWM 播放不等于双向模拟通话，BH 模拟输入也未确认 | 信息不足；不能给 EC718PM 芯片名称直接添一对模拟通话脚 |
| 模组语音 PCM ↔ UART2 私有帧 ↔ ESP ↔ 既有 BH I2S | 【实现】V3、P1 帧 type 仅实现 0/1；【资料】未找到 AI801 的音频帧定义 | UART 联网和 AT 高；音频低。须取得语音启停、上下行帧、编码和时钟定义 | 当前没有软件接通；type 2/3 未使用不代表已预留给语音 |
| 模组语音 ↔ USB PCM／UAC ↔ ESP USB Host ↔ BH I2S | 【资料】S2 的 USB 是 Device；【实现】V1、V3 的主控接入是 UART，未见模组 USB 音频接线或驱动 | USB 外设能力高；通话 PCM、USB 连线和本板可用性低 | 信息不足；USB 串口／网卡不等于 USB 声卡 |
| 模组语音 ↔ 私有 IP 数据流或 type 1 二进制流 ↔ ESP ↔ BH I2S | 【估算】现有 type 0 承载以太网，type 1 用作 AT（V3） | 低；只有 AI801 专用协议或可辨认的流能确认 | 保留为观察分支；不能因没看到 type 2/3 就排除全部主机语音 |
| ESP ↔ BH1098 I2S ↔ NS4150／麦克风；或 BH ↔ 蓝牙耳机 SCO | 【实现】V1、V2、V4、P2、P3 | 对已有实现高；蜂窝连接仍未知 | 这是已实现的本机音频末段，不是完整蜂窝通话链 |

【估算】最保守的板上工作假设是：蜂窝模块目前只通过 UART 给 ESP 提供网络和 AT，本机声音由 BH1098 独立负责；模组的通话声音尚未进入该链。源码没有第二路 PCM 定义或音频切换器控制，支持先采用此假设；源码看不到的模组—BH 硬连线仍可能推翻它。

【实现】BH 是 I2S 主、ESP 是从，GPIO42/43/44/45 分别为 BCLK/WS/DOUT/DIN，16 kHz、32 位双槽；模组到 ESP 已声明的只有 GPIO58/59/60/3 的 UART 与握手。`PowerNeed::PhoneCall` 在原厂固定提交仅出现三处：枚举及两处策略判断，没有业务持有者。它表达电源意图，不能证明音频接线（V1、V5）。

## 公开资料与源码出处

全部查阅于 2026-11-11。以下说明均为转述，不摘录手册正文；AT 命令按原语法列出。

| 编号 | 原厂资料、版本和定位 | 能证明什么／不能证明什么 |
| --- | --- | --- |
| S1 | [利尔达 EC71X 产品介绍](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/)，§二、§三；[底包表](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/general/index.html)，F7B_A/F7D_A | EC718PM 有带 VoLTE 的 NT26 底包；PCM/I2S、模拟输出和 VoLTE 随子型号而异；本机准确料号和 AI801 不在表内确认 |
| S2 | [系统资源开发介绍 Rev1.0](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/general/resource-information.html)，§2；[API 入口 Rev1.0](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/software/api-overview.html)，Audio／VoLTE | 芯片支持 I2S/PCM 主从和 USB Device；API 在模组内部执行，不是能直接链接进 ESP 的语音库 |
| S3 | [合宙 Air780EP AT 手册入口](https://docs.openluat.com/air780ep/at/app/at_command/)；[产品说明](https://docs.openluat.com/air780ep/product/) §简介；[EC718P 旧系列说明](https://wiki.luatos.com/chips/air780ep/mcu.html) | 普通 EP 与语音变体的能力不同；旧系列页仅 EPV 标注 VoLTE。通用手册有音频章节不代表每个固件都支持，更不证明 AI801 支持 |
| S4 | [合宙硬件手册 V1.4](https://docs.openluat.com/atmozu/product/file/Air700ECT%26Air780EPT%26EHT%26EVT%26EVT-1.8V%26EG2%26EGT%E7%A1%AC%E4%BB%B6%E6%89%8B%E5%86%8CV1.4.pdf)，§3.3／§3.6，PDF 第 9／12 页 | EVT 的 MIC± 为脚 3/4、SPK± 为脚 5/6；证明带 Codec 模组的模拟通路，不能移植其管脚号到本板。该文件含旧 V1.3 页眉，页脚 11/14 与 PDF 页序不同，故同时给章节和 PDF 页序 |
| S5 | [合宙 AT 手册 V1.0.14](https://docs.openluat.com/atmozu/product/file/Air780EHT%26EGT%26EVT%26Air700ECT%26Air780EPT%26EG2%26EX2T_AT%E6%8C%87%E4%BB%A4%E6%89%8B%E5%86%8CV1.0.14.pdf)，2026-08-30；§6.1～6.4（PDF 第 95～98 页）、§17.1～17.5（第 261～266 页） | TTS、播放配置／Codec、音量和呼叫控制的参考语法；语音章节限定 EVT。`CTTS` 的 PCM 参数描述 TTS 输出，不是蜂窝通话 PCM 接口 |
| S6 | [合宙语音指令说明](https://docs.openluat.com/atmozu/product/command/audio/)，§6.5、§6.6、§7.7 | `CLVL` 的平台差异、持久化和查询限制；`CRSL` 是本地播放音量。它补充的 `CLVL` 在 S5 中未检出，采用时需探测，不当作 AI801 保证 |
| S7 | [3GPP TS 27.007 V11.5.0／ETSI TS 127 007](https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/11.05.00_60/ts_127007v110500p.pdf)，§7.18、§8.23/8.24、§8.37、§8.71 | 标准 `CLCC`、`CLVL`、`CMUT`、`CLAC`、`CIREG` 的语法；采用固定历史版本核对基础语义，不代表定制固件全部实现 |

### 原厂源码与当前实现

上游统一固定在 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；本机原厂克隆 HEAD 仍为 `690a023c66862af103a3187464373ee7c5efcd14`，未移动。行号用最新导出核对，全文搜索用只读 `git grep … 874538e -- main docs` 补齐。

| 编号 | 出处（文件／行号） | 核对结果【实现】 |
| --- | --- | --- |
| V1 | [config.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/metalio-e-ink4-plus/config.h#L143-L162)，143～162 | BH I2S、独立控制串口和模组 UART；PA_EN 是 TCA9555 P0.4，不是模组自己的 GPIO |
| V2 | [BH1098 协议](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/docs/bh1098-audio-protocol.md#L23-L61)，23～61、98～143 | 模式 1 是本机音频；SCO 是蓝牙耳机链；没有蜂窝 PCM 第二口的定义；模式 6 讲的是 BH 到电脑，不能充当模组 USB 音频证据 |
| V3 | [uart_eth_modem.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/common/board/uart_eth_modem.h#L195-L206)，195～206；[对应 .cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/common/board/uart_eth_modem.cc#L1384-L1397)，1384～1397 | 四字节头 type 两位，枚举及收包仅 0/1；2/3 静默忽略，不是音频实现 |
| V4 | [metalio_audio.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc#L76-L126)，76～126；[bt_audio_codec.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/common/audio/bt_audio_codec.cc#L61-L110)，61～110、140～186 | 模式 1 初始化、I2S 从机及 PCM 槽位转换；没有模组音频读写 |
| V5 | [power_policy.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/common/power_policy/power_policy.h#L26)，26；同目录 `power_policy.cc` 76、723；[README](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/boards/common/power_policy/README.md#L26-L28)，26～28、114～140 | PhoneCall 无持有者；P14 共享供电，复位 BH 会影响蜂窝模块，试验期间不能拿复位蓝牙当排查捷径 |
| P1 | [s31_modem.c](../../../ports/esp32s31/s31_modem.c)，50～74、130～160 | 有限 AT 允许列表；只处理 type 0/1，新增只读命令与原始帧计数需要另做有限诊断入口 |
| P2 | [hal_cellular_s31.c](../../../ports/esp32s31/hal_cellular_s31.c)，93～102；[hal_bt_s31.c](../../../ports/esp32s31/hal_bt_s31.c)，117～150 | 拨号／接听成功后调用 BH 通话标记；仅有已连耳机时才切 SCO，没有搬运蜂窝音频 |
| P3 | [hal_audio_s31.c](../../../ports/esp32s31/hal_audio_s31.c)，54～75；[hal_audio.h](../../../platform/include/ink/hal_audio.h) | 已有 16 kHz 单声道 PCM HAL 与 BH 的 32 位双槽转换，可作为未来主机语音桥的末段 |
| P4 | [nt26.c](../../../drivers/chips/nt26/nt26.c)，397～429 | 当前拨号保留语音分号，静音是模组 `CMUT`；这些控制命令不能替代音频传输 |

### AI801、PCM 和 USB 的资料缺口

【资料】精确检索 `XJ_EC718PM`、`AI801 EC718PM`、`芯讯 EC718`、`XJCFG`，没有取得与当前固件对应的公开原厂手册或语音帧协议；不能从 `XJ` 前缀确认厂商身份。也没有找到能证明 AI801 支持通话 PCM 经 UART/USB 传输的资料。结论是“未查到”，不是“不支持”。

【资料】[利尔达 SDK](https://github.com/lierda-iot/cat1_sdk/tree/59f1250c4b8caad4bbf4f74167b7b39c79097f8c) 的 `liot_volte.h` 180～205 提供 Codec 类型和音量，`liot_voice_call.h` 177～197 提供通话音频开关和事件回调；`demo_volte.c` 124～125 使用外置 ES8311。它们在模组内部运行，不能直接拿到 ESP 上用，更不能证明 AI801 暴露了对应 AT。SDK 根许可证 Apache-2.0；VoLTE 库是预编译库，复用前还须核对对应库的授权，不能只看根许可证。

**缺少的明确项**：AI801 的通话通道查询、PCM/I2S 配置查询、AEC 查询及 USB/UART 通话音频启停命令。本次不编造这些命令，也不把其他品牌的私有命令后面添 `?` 就发给设备。可先用标准 `AT+CLAC` 发现公开命令名；即使列出名字，也须拿到该固件的参数和恢复说明后才能写配置。

## 上机试验步骤

待核查。仅供主会话执行，按不产生通话费用到可能收费排列。

## 要用户定的

- 实际拨号、可能收费的通话和修改模组音频配置均不在本次执行范围；试验前需用户明确授权。

## 没做到的

- 按任务书不碰设备，不改产品代码、测试或构建文件，不运行全量测试、ASan、S31 构建和 gcccheck；本次产品代码改动 0 行。

## 和设计不一致的

待核查；本次不修改设计和实现。

## 改过的已有测试

无。

## 新增的机制

无。

## 要改的文档

待调研结束后列出；本次不修改共享文档。

## 顺手发现

待核查。

## 交付检查

待完成文件、链接、差异范围和隐私检查；按里程碑本地提交，不 push。
