# CloudZao 参考项目初查

查阅日期：2026-10-26。

本页保留项目起点的文档初查和固定提交记录。**当前型号已限定 E-Ink4 Plus**，其他型号仅作历史比较；没有编译、刷机或实机测试。

本轮修正：Plus 自己的板类直接接入 NT26，已有控制引脚、网络初始化和共享供电代码。此前因未追踪板类构造而只把 Claw4 作为通信重点，判断不充分。现按 Plus 继续研究短信和电话；完整模组型号、固件与音频接线仍需核对。最新证据见 [Plus 通信调研](/home/workspace/reader/docs/research/eink4-plus-cellular.md)。

初查范围：[CloudZao 的 GitHub 主页](https://github.com/CloudZao)及下列四个公开仓库的 README 和仓库页面。初查页面对应当时默认分支；随后补充的固定提交见下表。

本次是文档初查。下表规格与功能属于上游描述，尚未经过本项目编译、刷机或实机验证；最后一列是据此作出的初步判断。

| 参考项目与来源 | 上游描述的主控 | 上游描述的显示配置 | 对个人阅读器的初步参考价值 |
| --- | --- | --- | --- |
| [Metalio-E-INK4](https://github.com/CloudZao/Metalio-E-INK4#readme) | ESP32-S3 | 3.97 英寸、800×480、SPI 墨水屏 | 上游定位包含阅读，列为优先研究候选。 |
| [Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus#readme) | ESP32-S31（按上游当前文档记录） | 4.7 英寸、1216×684、并行墨水屏，冷暖双色前光 | 可研究更大显示面积与前光相关需求。 |
| [MetalioClaw4](https://github.com/CloudZao/MetalioClaw4#readme) | ESP32-P4 + ESP32-C5 | 3.95 英寸、720×720、MIPI-DSI LCD | 可参考彩屏交互及其他功能；是否适合作为主阅读设备取决于实际需求。 |
| [Metalio-Claw-1.54](https://github.com/CloudZao/Metalio-Claw-1.54#readme) | ESP32-S3 | 1.54 英寸、240×240、LCD | 若讨论极小体积或短文本可参考；长篇阅读体验需要验证。 |

## 起点阶段的发现

1. **方向已有参考基础。** 两款墨水屏项目的 README 都描述了本地书库，以及 .ebook、.epub、.txt 文件支持。因此，“用单片机做个人阅读器”有现成项目可研究；复杂排版、不同编码和大文件兼容性仍需另行核查。[E-INK4 来源](https://github.com/CloudZao/Metalio-E-INK4#readme)、[Plus 来源](https://github.com/CloudZao/Metalio-E-INK4-Plus#readme)。
2. **格式支持要区分运行位置。** 两款文档还描述了电脑端工具将 PDF、EPUB、MOBI、TXT 转为 .ebook。这不能作为设备能够直接打开任意 PDF、MOBI 的证据。后续若提出格式需求，需要明确是否接受电脑或服务端预处理。[E-INK4 来源](https://github.com/CloudZao/Metalio-E-INK4#readme)、[Plus 来源](https://github.com/CloudZao/Metalio-E-INK4-Plus#readme)。
3. **两款墨水屏方案有实质差异。** Plus 文档明确列出不同主控、并行屏幕、前光与引脚/USB 方案。若以后涉及硬件或驱动复用，需要按实际板卡版本核对。[Plus 来源](https://github.com/CloudZao/Metalio-E-INK4-Plus#readme)。
4. **阅读界面应结合刷新方式判断。** 两款上游文档描述了全刷、局刷及分页交互。若后续提出连续滚动、动画等要求，需要进一步验证目标屏幕与驱动的表现。[E-INK4 来源](https://github.com/CloudZao/Metalio-E-INK4#readme)、[Plus 来源](https://github.com/CloudZao/Metalio-E-INK4-Plus#readme)。

## 起点阶段提出的核查问题

- 原理图、PCB、BOM、结构文件是否齐全，以及与目标硬件版本的对应关系；当前只研究既有 Plus，不开展自制硬件。
- 若希望直接修改固件：相关源码和依赖是否足以重建所需功能，工具链是否可用。
- 若要求完全离线：阅读路径是否依赖账号、联网初始化或其他在线服务。
- 若关心续航与速度：电池规格、各模式电流、翻页延迟、残影和前光功耗，需要明确条件后测量。
- 若需要语音或 AI：区分设备执行与服务端执行，核查实际接口和依赖。

以上问题按后续需求逐步核查，不据此承诺功能、价格或续航。初查时未选型，后续用户已限定 Plus。

## 第一轮补充与后续修正：固定源码比较

首次补充时用户要求沿用某款 CloudZao，现已收紧为 Plus。以下保留四款的比较依据，不代表其他板卡仍是候选，也不据此更换屏幕、加 FPGA 或修改 PCB。

| 板卡 | 本轮相关证据 | 对产品方向的影响（助手推断） |
| --- | --- | --- |
| E-Ink4 | README 描述 S3、16 MB Flash、Octal PSRAM；config.h 确认 800×480、SSD1677 SPI 10 MHz；存在显示驱动 C 文件。 | 历史比较：此 SPI 控制器链路不能直接等同于 PaperBoyS3 的原始并行驱动。 |
| E-Ink4 Plus | README 描述 S31、16 MB Flash、Octal PSRAM 200 MHz；config.h 与 EPDiy 文件确认 1216×684 并行显示路径；后续确认板类接入 NT26。 | 当前唯一目标硬件。可研究并行高刷与蜂窝能力；不是 60fps 或完整电话能力承诺。 |
| Claw4 | README 描述 P4+C5、32 MB Flash/32 MB PSRAM、720×720 LCD、NT26 4G。外置 SIM 拨号有文档及代码依据。 | 保留拨号驱动原理参考；它是 LCD，已不属于候选硬件。 |
| Claw 1.54 | README 描述 S3、240×240 LCD 和音频交互；本轮未抽查显示实现。 | 仅保留历史比较，不属于候选硬件。 |

规格属于上游描述或上述代码配置；实际装机容量、版本和行为仍需对应实物。**修正此前“墨水屏已读配置未确认蜂窝”的不足**：Plus 并非仅在公共目录存在模组类，其板级构造实际传入 NT26 引脚，并有供电管理。代码仍不能单独证明每一台销售版本都装配同一模组。

### 固定提交与可复核入口

本轮通过 GitHub commits/main API 固定提交，再读取递归目录树、README、LICENSE 和列出的源文件。四个目录树返回均为未截断；本地临时副本位于 work/research/，不作为正式资料依赖。

| 仓库 | 本轮提交 | 复核入口 |
| --- | --- | --- |
| E-Ink4 | d12f481f4a42420634bb56a928f3959a977367d4 | [README](https://github.com/CloudZao/Metalio-E-INK4/blob/d12f481f4a42420634bb56a928f3959a977367d4/README.md)、[屏配置](https://github.com/CloudZao/Metalio-E-INK4/blob/d12f481f4a42420634bb56a928f3959a977367d4/main/boards/metalio-e-ink-4/config.h)、[SSD1677 驱动](https://github.com/CloudZao/Metalio-E-INK4/blob/d12f481f4a42420634bb56a928f3959a977367d4/main/boards/metalio-e-ink-4/display/esp_lcd_panel_ssd1677.c) |
| E-Ink4 Plus | ad8edf4150a688737c0d7c00cae4413079b262ef | [README](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/README.md)、[屏配置](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/metalio-e-ink4-plus/config.h)、[EPDiy](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/metalio-e-ink4-plus/display/epdiy.c) |
| Claw4 | ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950 | [README](https://github.com/CloudZao/MetalioClaw4/blob/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950/README.md)、[NT26 接口](https://github.com/CloudZao/MetalioClaw4/blob/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950/main/boards/common/nt26_board.cc)、[拨号界面代码](https://github.com/CloudZao/MetalioClaw4/blob/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950/main/display/screen/call_screen/call_screen.cc) |
| Claw 1.54 | ba2fcbde5cd7c42e6f684aea64bed63951309684 | [README](https://github.com/CloudZao/Metalio-Claw-1.54/blob/ba2fcbde5cd7c42e6f684aea64bed63951309684/README.md) |

两款墨水屏确有源代码，并非仅有刷机二进制。仓库标注 MIT；复用时仍应逐项核对第三方组件的许可与依赖。当前没有取得对应实物板卡版本的完整原理图/BOM，不推断它们一定不存在。两款 README 的原理图章节主要指向固件说明和引脚定义。

### 通信：Plus 为目标，Claw4 仅作参考

- **Plus 新增代码依据**：板类继承 DualNetworkBoard，配置 NT26 TX/RX=58/59、MRDY/SRDY=60/3，并有 P12/P14 共享复位/供电说明。蜂窝模式下创建 Nt26Board 并使用 UART 网卡与 AT 通道。[板类入口](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc#L54)
- **Plus 当前判断**：短信与通话有条件可开发。厂商子型号/底包支持、板上音频连接及 SIM 业务尚需对应；没有完整 app 不是硬件不可行的依据。[厂商与社区证据](/home/workspace/reader/docs/research/eink4-plus-cellular.md)

以下三项为上一轮 Claw4 的参考证据，不能把该板的 SIM 与音频接线直接套给 Plus：

- **文档描述**：Claw4 使用 NT26；内置贴片 SIM 主要用于数据，外置 SIM 才可通过 Phone 拨号。不能把附带数据卡当成语音卡。[固定版本说明](https://github.com/CloudZao/MetalioClaw4/blob/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950/README.md)
- **代码抽查**：call_screen.cc 中存在 SIM 检查、ATD 拨号和 ATH 挂断；Nt26Board 暴露 AT 命令发送接口。该结果只证明实现路径存在。
- **仍未确认**：模组准确子型号/固件与运营商支持、VoLTE/语音业务、来电通知及接听、双向音频、待机呼入、中文及长短信收发、断网恢复。已读 README 和拨号文件未证明完整短信服务；未遍历所有业务代码，不能断言模组不支持短信。
- **共同原则**：具备相应 4G 硬件时，短信和通话仍是用户要求，不因上游 app 缺项而删除。通话 App 能发出拨号指令不等于电话功能已经验收。

### 驱动复用：不能只复制目录

**代码证据**：Plus 的 config.h 保留 V1.2 与 V1.3/V1.4 引脚差异注释，实际启用定义不同。SSD1677 驱动有 BUSY 等待和刷新完成处理；EPDiy 路径涉及板级供电和显示时序。因此需按实际版本梳理最小依赖，包括 GPIO、I2C 扩展器、屏电源、触摸、DMA/内存及音频供电。这是移植准备工作，不要求继承原系统 UI。

**社区版本线索**：kiikoread 的 [Plus 适配说明](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/e942aeb1745241273d737c584c88a8068ef6e933/README.md)明确 V1.5、34Pin、E0470A14-AF，与 CloudZao README 的面板标记不同，暂不推断等价。硬件型号已选不等于板卡版本与波形已锁定。

**语音支持的一个具体缺口**：Plus 的 [main/idf_component.yml](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/main/idf_component.yml) 将 ESP-SR ~2.2.0 限定在 esp32s3/esp32p4 条件下。乐鑫当前 [ESP-SR README](https://github.com/espressif/esp-sr/blob/master/README.md) 已提及 S31 支持，两者不能混为一谈。是否已有其他唤醒路径、采用何版本、在 Plus 上表现如何均未验证。

### 后续需要的最小证据

先确认所选板卡实物版本、屏型号、可用内存、SDK/驱动对应版本，再提出有限验证。墨水屏验证参考 [高刷调研](/home/workspace/reader/docs/research/eink-high-refresh.md)；语音和 app 参考 [平台调研](/home/workspace/reader/docs/research/ai-native-platform.md)。本轮只整理材料，没有搭建完整固件工具链。
