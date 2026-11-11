# BH1098 / I2S 原厂来源

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)，提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77），2026-11-11 查阅。许可证为旁边的原文 [MIT](LICENSE)。`upstream/` 是整文件原文，未改任何字节；不编译其中的页面、Settings/NVS、原厂 Application 或任务创建代码。

| 本地文件 | 原厂路径 | SHA-256 |
| --- | --- | --- |
| `upstream/metalio_audio.cc` | `main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc` | `6202914787681a22f50f9810f7235053fecccd252c9b2765c4734cdaf046463b` |
| `upstream/metalio_audio.h` | `main/boards/metalio-e-ink4-plus/periph/metalio_audio.h` | `c1621ad2a1f0a4052b23317127e23f508c734638ad7de03ca4e69882dacac34d` |
| `upstream/bt_audio_codec.cc` | `main/boards/common/audio/bt_audio_codec.cc` | `724c61bafcd4a9ebb53dd7e63ddeea0b25c6e94b799572e9475e7e8751cef39e` |
| `upstream/bt_audio_codec.h` | `main/boards/common/audio/bt_audio_codec.h` | `6fa91e5a7af2f2cc65beb1a1dc0f32209d306c64ff86c36c521454086fdad4ee` |
| `upstream/simple_uart.hpp` | `main/boards/common/audio/simple_uart.hpp` | `50289951c532eddac6f4c9ebc3c771c8030d7c9157322f2e1d29ef22446074c2` |
| `upstream/bluetooth_uart.cc` | `main/display/screen/bluetooth_screen/bluetooth_uart.cc` | `8e1ad2629b3c37ba2dfe96a676f0cdc178cd23e6de3689c47e302027d3eebc04` |
| `LICENSE` | `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |

## 实际接入与必要适配

现有构建目标是 C，保持 HAL 头文件和构建文件不变。下面是从原厂完整函数去掉产品依赖后接入的 C 版本；这些 `.inc` 由生产 HAL/驱动直接包含，不是只作参考的副本。未引入 C++ 运行时或新任务。原厂 C++ 整文件未逐字编译，不能把 C 适配说成零改动。

| 接入文件和行 | 原厂函数和行 | 必要改动 |
| --- | --- | --- |
| `mode.inc:2-4` | `metalio_audio.cc:72-74` PowerUnchanged | 原厂原子 power/epoch 改用现有板级电源查询，UART 锁仍由调用方持有。 |
| `mode.inc:6-33` | `metalio_audio.cc:76-126` ApplyMode | `SimpleUart::sendString` 换现有 HAL 完整写入检查；同步读行代替 observer/ReplyScope/互斥应答缓存，发送 MODE 前读完 RX 阶段的 ERROR/FAIL；700 ms、1 s、10 ms 和 mode1 应答边界不变，持续核对电源周期。 |
| `mode.inc:35-58` | `metalio_audio.cc:135-184` ModeWorker | 保留一轮至多 4 次、间隔 1 s 和上电稳定等待；删通知循环、任务创建、PowerNeedHold 和原厂 UI 日志；在现有调用执行器同步执行。共享轨复位由 port-board-2 统一负责；这里不切 P14。HAL 的 completed/epoch 对应原厂 s_completed_epoch/s_ready_epoch。 |
| `commands.inc:2-19` | `bluetooth_uart.cc:365-409` Bluetooth_apply_mode_commands | 整函数换 C 签名和现有有界 send；删页面状态、日志、模式 5 分支（本 HAL 合同仅模式 1/2/3），三个命令序列和 700 ms 不变，返回 UART 写入结果。 |
| `commands.inc:21-31` | `bluetooth_uart.cc:439-466` Bluetooth_call_mode_task / Bluetooth_music_mode_task | 去任务退出和页面/PowerPolicy 依赖，沿用已有音频持有与 capture/call 仲裁；保留两向 200 ms 顺序；按原厂发送结果结束，不再额外要求 SCO 应答才算成功。 |
| `response.inc:3-32` | `bluetooth_uart.cc:198-341` Bluetooth_handle_response_line | 去页面/日志/HID/改名/版本/模式 3、5 产品分支，模式 1/2、CONNECT SUCCESS/TIMEOUT、SCO 转现有 HAL 状态。RECONNECT 只有名称且不证明连接成功，保留原厂“不置为已连接”的语义，不猜地址。扫描列表投影沿用 HAL 的有界 UTF-8/地址格式、去重逻辑，对应原厂 Bluetooth_parse_bt_device_line 和 INQ COMPLETE，应答仍由同一个读行入口消费。 |
| `codec.inc:3-52` | `bt_audio_codec.cc:57-136` BTAudioCodecDuplex::Start | 去 Settings/NVS、C++ 锁和类字段，接现有 A.tx/rx；I2S 配置逐项使用原厂值，固定原厂 config.h 的 GPIO42/43/44/45、16 kHz。通道 enable 留给现有 HAL 生命周期，保留零填充 DMA 预载和 RX 溢出回调。 |
| `codec.inc:54-71` | `bt_audio_codec.cc:138-164` BTAudioCodec::Write | std::vector 换已有 PSRAM 缓冲；音量已由系统服务处理，此处固定 unity；C++ cast 换 C cast；锁、PA 和调用方超时留在 HAL；保留 64 位乘法、32 位限幅、两槽复制及按实际写入字节数返回。 |
| `codec.inc:73-91` | `bt_audio_codec.cc:166-187` BTAudioCodec::Read | std::vector 换已有 PSRAM 缓冲；保留 250 ms、失败返回 0、右移 12、限幅 ±32767；交错输入按 HAL 合同拆成 mic/ref，时间戳和丢样计数由原 HAL 提供。 |

## UART 与资源

`upstream/simple_uart.hpp` 仅保留原文作接口来源，**不编译它的 begin/uartReceiveTask**，否则会重复安装 UART1 并增加 4096 B 接收任务。沿用 `hal_input_s31.c:87-121` 的 input-hal（4096 B、核 0、现有内部栈）和 try-lock 非阻塞 poll；同步命令持同一 BH 锁读取 UART1。原厂 4096 B PSRAM 模式初始化任务也不创建；现有媒体/蓝牙执行器调用有界初始化，穷尽后同一电源周期不重新启动。

保留已有 audio-capture 的 4096 B PSRAM 栈及 4800 B PSRAM PCM 缓冲；DMA 仍 4×240 帧/方向。没有新常驻缓冲、任务、文件格式或配置。所有原厂原始 UART 日志均未接入，避免设备名/地址泄露。

## 接入文件 SHA-256

- `codec.inc`：`63f86c833df0107da9173c06869a589f571f97b910ffc1d9dbf6df18eefc8ce6`
- `commands.inc`：`3843848561053c26ba0b170fe43a12b16d545c66f9f916bedab72ba2cac11ba4`
- `mode.inc`：`e7efbfda50b70517f1507879f9c6eef6b3b3c834392c9bf0db2959518fde2741`
- `response.inc`：`03ec0f738814019d3ef4ef9000a94ae41e4c307bd0a3d3ce7ed8949fc3cec354`
