# A. 上游固件源码架构调研：CloudZao Metalio E-Ink4 Plus

> 审阅对象：`CloudZao/Metalio-E-INK4-Plus` 的 **HEAD `690a023c66862af103a3187464373ee7c5efcd14`**（2026-10-30，PROJECT_VER 2.0.71）。下文链接全部指向这个 commit。先前固定的 `ad8edf4`（2026-10-23，v2.0.66）到 HEAD 改了 294 个文件（+16627/−4914）。与本报告相关的变化有四类。一是新增 `application_tasks.cc`，把单一 main loop 加 Schedule 队列换成 5 个任务域。二是新增 WorkBuddy（原始 PCM 走 WSS）和运行期 TTF 字体。三是 epd_refresh、LVGL 适配、前光快捷面板、NT26 shutdown handler 有修改。四是 MCP 的 Schedule 改为返回值。`idf_component.yml` 和分区表没变，sdkconfig 只多了 `LV_USE_QRCODE`。
> 标注说明：**【事实】** 表示直接读到的代码或配置；**【推断】** 表示我的判断；**未核实** 表示没有验证。我没有编译，也没有上机。

## 1. 血统与许可

- 【事实】README 写明“固件基于 xiaozhi-esp32”（[README_zn.md#L325](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/README_zn.md#L325)）。LICENSE 与 xiaozhi 完全相同，都是 MIT，署名“Shenzhen Xinzhi Future Technology”（[LICENSE#L1-L4](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/LICENSE#L1-L4)）。我逐文件 diff 了 xiaozhi 的各个 tag：`protocols/protocol.cc/.h` 与 v2.0.x–v2.1.0 **完全一致**。`mcp_server.h` 与 v2.0.1–v2.1.0 一致。`mcp_server.cc` 与 v2.0.2–v2.0.5 只差 61 行，差异全是 S31 的 `#if 0`。另外内嵌的 esp-ml307 是 v3.5.3，esp-wifi-connect 是 v2.6.2（[components/esp-ml307/idf_component.yml#L1](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/components/esp-ml307/idf_component.yml#L1) [components/esp-wifi-connect/idf_component.yml#L1](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/components/esp-wifi-connect/idf_component.yml#L1)）。
- 【推断】基线大约是 **xiaozhi v2.0.5（2025-11）**。NT26 和 `uart_eth_modem` 是后来从 xiaozhi v2.2.x 引入的（v2.2.4 起才有 `nt26_board` 和 `78/uart-eth-modem`）。代码注释里反复出现的“397”指同系列的 Metalio E-Ink 4（ESP32-S3）固件。
- 【事实】CloudZao 新增的部分如下：
  - S31 版 EPDiy 移植、TPS65185 和 TCA9555 板级；
  - `epd_refresh` 刷新任务和 LVGL 适配；
  - 全部 UI 页面（约 8 万行）；
  - 阅读引擎 `main/reader` 与 `.ebook` 工具链；
  - A2UI-lite 渲染器、fontpack、.ef、TTF 字体；
  - PowerPolicy、S31 浅睡、SD 访问层、USB MSC；
  - BH1098 蓝牙音频；
  - 三家云服务切换（百问、小智、网易云信，见 [main/chat_cloud.h#L8-L81](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/chat_cloud.h#L8-L81)）。
- 许可风险：
  - 【事实】`uart_eth_modem.*` 的文件头是 Apache-2.0，署名“Terrence”（[common/board/uart_eth_modem.h#L1-L2](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/uart_eth_modem.h#L1-L2)）。
  - 【事实】8 灰阶波形表是 Apache-2.0，来自 read-pico（[board/display/waveforms/LICENSE.gray8](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/waveforms/LICENSE.gray8)）。
  - 【事实】esp-ml307 目录里放的是 Apache LICENSE，但它的 yml 写的是 MIT，两者矛盾。
  - **【事实】EPDiy 上游是 LGPL-3.0-or-later，而仓库里内嵌改写的副本没有附任何 LGPL 声明。** 它对应上游 `403ba3b`（2026-04-16，该 commit 的 library.json 版本号为 2.0.0）：epdiy.c 只差 4 行，`lcd_driver.c` 从 765 行扩到 1237 行，highlevel.c 和 render.c 改动很大。社区 fork 也把 epdiy 标为 LGPL。
  - MiSans TTF 在仓库里没有附许可文件，未核实。

## 2. 设备与服务器协议

- **传输层（WebSocket 为主）**
  - 【事实】连接是进入百问页面时才建立的，`Start()` 本身什么都不做。握手头包括 `Authorization: Bearer <token>`、`Protocol-Version`、`Device-Id`（MAC）、`Client-Id`（UUID）（[main/protocols/websocket_protocol.cc#L118-L127](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/protocols/websocket_protocol.cc#L118-L127)）。
  - 【事实】客户端 hello 是 `{"type":"hello","version","features":{"mcp":true},"transport":"websocket","audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60}}`，发出后等服务器 hello 最多 10 s（[main/protocols/websocket_protocol.cc#L205-L248](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/protocols/websocket_protocol.cc#L205-L248)）。
  - 【事实】二进制帧有三种：v1 是裸 Opus，v2 带 16 B 头和时间戳，v3 带 4 B 头（[main/protocols/protocol.h#L17-L31](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/protocols/protocol.h#L17-L31)）。
- **消息类型**
  - 【事实】设备上行：`listen`（start/stop/detect，mode 可为 auto/manual/realtime）、`abort`、`mcp`（[main/protocols/protocol.cc#L42-L79](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/protocols/protocol.cc#L42-L79)）。
  - 【事实】服务器下行：`hello`、`tts`（start/stop/sentence_start）、`stt`、`llm`（emotion）、`mcp`、`system`（reboot）、`alert`、`custom`（[main/application.cc#L459-L557](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L459-L557)）。120 s 没有收到数据就判超时。
  - 【事实】百问的 A2UI 富内容走 `tts.sentence_start` 的 text 字段，内容是 A2UI JSON（[main/display/screen/assistant_screen/assistant_ingest.cc#L348-L350](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/assistant_screen/assistant_ingest.cc#L348-L350)）。
- **MQTT+UDP**
  - 【事实】代码保留了（AES-CTR UDP），但 S31 上 `EspNetwork::CreateMqtt()` 直接返回 nullptr（[components/esp-ml307/src/esp/esp_network.cc#L29-L32](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/components/esp-ml307/src/esp/esp_network.cc#L29-L32)），esp_mqtt.cc 也没有参与编译。**这条路径实际上是死的。**
- **音频格式**
  - 【事实】上行 Opus 16 kHz 单声道、60 ms 一帧（[main/audio/audio_service.h#L34-L39](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_service.h#L34-L39)）。下行先按服务器 hello 给的采样率解码，默认 24 k，再重采样到 16 k（[main/audio/audio_service.cc#L590-L594](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_service.cc#L590-L594)）。
- **唤醒词和 AFE**
  - 【事实】都关掉了：`CONFIG_WAKE_WORD_DISABLED=y`，AFE 未启用（[sdkconfig#L1268-L1276](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L1268-L1276)）。esp-sr 只在 s3/p4 上拉取（[main/idf_component.yml#L10-L13](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/idf_component.yml#L10-L13)），`model_path.h` 是空桩。交互只有 PTT：BOOT 键或屏幕长按 500 ms。
- **MCP**
  - 【事实】`mcp_server.cc` 是 JSON-RPC 2.0 实现，protocolVersion 为 `2024-11-05`，支持 initialize、tools/list（cursor 分页，单包上限 8000 B）、tools/call（[main/mcp_server.cc#L330-L486](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/mcp_server.cc#L330-L486)）。它装在聊天 WS 的 `{"type":"mcp","payload":…}` 里，**只有百问页面打开、协议在线时才可达**（[main/application.cc#L416-L419](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L416-L419)）。
  - 【事实】实际注册的工具：`self.get_device_status`、`self.audio_speaker.set_volume`；只给用户的有 `self.get_system_info`、`self.reboot`、`self.upgrade_firmware(url)`（[main/mcp_server.cc#L44-L145](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/mcp_server.cc#L44-L145)）。
  - 【事实】以下工具在本机都不会出现：屏幕亮度工具要求 `GetBacklight()` 非空，而本板没有覆写、默认返回 null（[common/board/board.h#L66](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/board.h#L66)）；主题、截图、相机工具被 `#if 0` 掉了；assets 工具要求存在 assets 分区，v1 分区表里没有。没有任何板级自定义工具。
  - 【事实】工具调用串行跑在 app_voice 工作任务上，队列满时回复“Device is busy”（[main/mcp_server.cc#L530-L542](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/mcp_server.cc#L530-L542)）。
  - **【推断】`user_only` 只影响 list 里是否列出，`tools/call` 并不检查它。所以只要服务器能发 MCP，就能让设备从任意 URL 刷固件。**
- **服务器地址、OTA 与激活**
  - 【事实】开源版把全部端点留空（[main/cloudzao_endpoints.c#L1-L43](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/cloudzao_endpoints.c#L1-L43)）。WS 的 url、token、version 来自 OTA 响应，写进 NVS 的 `websocket`、`xz_websocket`、`yx_websocket`（[main/ota.cc#L201-L275](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L201-L275)）。
  - 【事实】OTA 检查是带头部的 POST，头部包括 Activation-Version、Client-Id、Serial-Number 等（[main/ota.cc#L83-L98](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L83-L98)）。响应字段有 `firmware`、`activation{code,challenge}`、`websocket`、`mqtt`、`server_time`。
  - 【事实】激活用 eFuse `HMAC_KEY0` 对 challenge 做 HMAC-SHA256（[main/ota.cc#L705-L760](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L705-L760)）。
  - 【事实】固件下载用 8 KB 内部缓冲，写 flash 时持有 `EpdFlashGuard` 和 I2C 锁（[main/ota.cc#L41-L56](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L41-L56) [main/ota.cc#L393](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L393)）。还支持从 SD 根目录的 bin 升级（[main/application.cc#L1121-L1200](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L1121-L1200)）。
  - 【事实】**没有开启 rollback、secure boot 和 flash 加密**（[sdkconfig#L1105-L1107](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L1105-L1107) [sdkconfig#L5204](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L5204)）。
- **WorkBuddy**
  - 【事实】这是新增模块，用独立的 HTTP 加 WSS 通道，上传 16 kHz 原始 PCM，结束帧带 CRC32，不经过 xiaozhi 协议（[main/workbuddy/workbuddy_voice_stream.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/workbuddy/workbuddy_voice_stream.h)）。它和“设备接 Hub”的思路最接近，值得参考。

## 3. 运行时架构

**常驻和关键任务**（【事实】，“内部”指栈在内部 RAM）

| 任务 | 栈 / 优先级 / 核 / 栈位置 | 出处 |
|---|---|---|
| main（app_main，跑完 `Start()` 后返回） | 16 KB / – / – / 内部 | [main/app_main.cc#L15-L32](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/app_main.cc#L15-L32) |
| main_event_loop | 3 KB / 3 / 不绑核 / 内部 | [main/application.cc#L1598-L1601](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L1598-L1601) |
| app_voice / app_network / app_storage / app_audio / app_cleanup（按需创建，空闲 2 s 退出） | 8/8/4/8/4 KB / 3 / 不绑核 / 内部；队列 32/4/16/16/24 | [main/application_tasks.cc#L21-L99](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application_tasks.cc#L21-L99) |
| lv_epdiy_tick（LVGL timer 加输入轮询，10 ms 一轮） | 16 KB / 2 / core1 / 内部 | [main/display/lv_adapter_epdiy.cc#L109-L131](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L109-L131) |
| epd_refresh | 6 KB / 2 / core1 / 内部 | [main/display/epd_refresh.cc#L566](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L566) |
| epd_prep ×2（行数据供给） | 4 KB / **configMAX−1** / core0、core1 / **PSRAM** | [board/display/render.c#L415-L432](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/render.c#L415-L432) |
| touch_feed、metalio_keys、metalio_accel | 4 KB/3 KB/3 KB / 5 / 不绑核 | [board/display/metalio_touch.c#L33-L36](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/metalio_touch.c#L33-L36) [board/input/metalio_keys.cc#L211](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/input/metalio_keys.cc#L211) |
| sys_mon、cx25601n_vreg、sd_log | 4 KB/1；3 KB/5；4 KB/6/core0/内部 | [board/periph/metalio_sys_mon.cc#L14-L108](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/periph/metalio_sys_mon.cc#L14-L108) [common/storage/sd_log_writer.cc#L202](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/storage/sd_log_writer.cc#L202) |
| audio_input / audio_output / opus_codec（进入音频时创建） | 4 KB/8；2 KB/4；**26 KB**/2 / 栈在 PSRAM，TCB 在内部 | [main/audio/audio_service.cc#L148-L206](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_service.cc#L148-L206) |
| uart_eth_main / tx / init（仅 4G 模式） | 4 KB/10；3 KB/9；4 KB/4 | [common/board/uart_eth_modem.cc#L170-L185](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/uart_eth_modem.cc#L170-L185) |
| ssl_receive / tcp_receive（每条连接一个） | 4 KB / 1 | [components/esp-ml307/src/esp/esp_ssl.cc#L70-L75](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/components/esp-ml307/src/esp/esp_ssl.cc#L70-L75) |

- 【事实】另外还有几十种按需创建的任务（UI、阅读、壁纸、网络、设置页测试等），例如 reader_page 24 KB 栈在 PSRAM，book_open 和 layout 16 KB 绑 core0；全仓库 grep 到约 130 行任务创建相关代码（含少量日志字符串）。Wi-Fi 任务绑在 core0（[sdkconfig#L2560](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L2560)）。
- **调度模型**
  - 【事实】`MainEventLoop` 只等 event group，把事件分发到 5 个任务域（[main/application.cc#L1839-L1851](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L1839-L1851)）。
  - 【事实】UI 消息通过 `ScreenLvAsync` 进入两条静态队列（普通 80 条、紧急 16 条），在 Tick 里持 LVGL 锁处理，每轮最多 8 条或 5 ms（[main/display/screen/screen_common_nav.cc#L27-L35](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/screen_common_nav.cc#L27-L35) [main/display/screen/screen_common_nav.cc#L137-L184](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/screen_common_nav.cc#L137-L184)）。
  - 【事实】文档规定 UI 回调里不能等待、不能同步访问 SD（[docs/ui-scheduling.md#L7-L11](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/docs/ui-scheduling.md#L7-L11)）。
- **页面组织**
  - 【事实】页面是函数指针工厂 `lv_obj_t*(*)()`；首页用 `kApps[]` 注册（[main/display/screen/home_screen/home_screen.cc#L64-L74](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/home_screen/home_screen.cc#L64-L74)），返回栈存的是工厂函数（[main/display/screen/screen_common.h#L31-L46](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/screen_common.h#L31-L46)）。每个页面的状态放在文件作用域的 static 里，通过 `IsActive()` 判断。待机页是盖在 layer_top 上的 overlay。
- **状态共享**
  - 【事实】大量使用单例，如 `Application`、`Board::GetInstance()`、`PowerPolicy`、`Frontlight`、`SdCardManager`、`McpServer`，再配合 atomic 和代次计数。
  - 【事实】没有通用的消息总线。只有两样类似机制：`DeviceStateEventManager`，基于 esp_event 的设备状态事件（[main/device_state_event.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/device_state_event.h)）；以及 `PowerNeed` 引用计数（[common/power_policy/power_policy.h#L25-L37](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/power_policy.h#L25-L37)）。

## 4. 显示流水线

- **面板与驱动**
  - 【事实】面板是 E0470A14-AF，1216×684，8 位总线，像素时钟 20 MHz（[board/display/displays.c#L76-L85](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/displays.c#L76-L85)）。初始化用 `epd_init(…, EPD_LUT_1K)`，方向 LANDSCAPE，VCOM 1200 mV（[board/display/metalio_epd.c#L37-L43](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/metalio_epd.c#L37-L43)）。“黑色深度”设置对应 1.2/1.6/2.0 V（[main/display/epd_refresh.cc#L57](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L57)）。
  - 【事实】EPDiy 不是作为组件引入，而是改写后放在板级目录里。LCD 驱动为 S31 的 AXI-GDMA 加 bounce buffer 重写过；原来的 Xtensa SIMD 用 C 实现替代（[board/display/epdiy_riscv_ve_stub.c](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/epdiy_riscv_ve_stub.c)）。
- **LVGL 与适配层**
  - 【事实】LVGL **9.5.0**，`esp_lvgl_adapter` 0.6.3，后者只用了文件系统和解码器（[main/idf_component.yml#L5-L6](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/idf_component.yml#L5-L6)）。配置为 `LV_OS_NONE`、1 bit 色深。
  - 【事实】默认用 I1 渲染；当可见内容里有灰阶角色（封面、插图、壁纸、开了抗锯齿的正文）时，在 `REFR_START` 切到 L8（[main/display/lv_adapter_epdiy.cc#L350-L365](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L350-L365)）。
  - 【事实】逻辑分辨率 680×1212（[board/config.h#L6-L18](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L6-L18)）。PARTIAL 模式，只有**一个**绘制缓冲，大小取 I1 整屏约 104 KB 和 L8 64 行中的较大者，放在 PSRAM（[main/display/lv_adapter_epdiy.cc#L330-L349](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L330-L349)）。`lv_malloc` 被包装成走 PSRAM（[main/CMakeLists.txt#L310-L312](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/CMakeLists.txt#L310-L312)）。
  - 【事实】`flush_cb` 把脏区按旋转写进 4bpp 的 render_fb，最后一块写完时调用 `EpdSubmit`（[main/display/lv_adapter_epdiy.cc#L261-L293](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L261-L293)）。
- **epd_refresh 刷新任务**
  - 【事实】`EpdSubmit` 在 mailbox 锁里把**整帧 406 KB memcpy** 到 pending（[main/display/epd_refresh.cc#L569-L591](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L569-L591)）。
  - 【事实】Worker 会合并多次提交；如果是切屏请求，先去抖 60 ms（[main/display/epd_refresh.cc#L394-L409](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L394-L409)）。
  - 【事实】模式选择规则（[main/display/epd_refresh.cc#L468-L527](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L468-L527)）：满足以下任一条件时升级为 **GC16**：
    - 刷新档位刚切换；
    - 显式请求全刷；
    - 上次刷新失败；
    - **局刷累计 ≥24 次且距上次全刷 ≥60 s**。
  - 【事实】有灰阶变化、滚动收尾或翻页时用 **GL16**，其余情况用 **DU**。
  - 【事实】`Draw()` 做的事依次是（[main/display/epd_refresh.cc#L177-L322](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L177-L322)）：
    - 持有 `EpdFlashGuard`，与 OTA 写 flash、sys_mon 的堆遍历、P14 断电互斥；
    - 持 SD 日志锁；
    - 设置 `Display::SetPanelBusy`，Wi-Fi 起射频时会等它；
    - 锁定 CPU 频率；
    - 上下电时暂停触摸采样；
    - 每帧给 TPS 上电，完成后断电；
    - 行数据欠载时清屏、做完整 GC16 恢复，必要时重新上电再试一次。
- **波形**
  - 【事实】`MY_WAVEFORM` 由“EPD Waveform Engineering Studio V2”生成，GC16 48 相、GL16 48 相、DU 20 相（[board/display/waveforms/my_waveform_gc.h#L5](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/waveforms/my_waveform_gc.h#L5)）。
  - 【事实】共四档：清晰、均衡（默认，GC/GL 裁剪为 36/37 相）、快速、极速（8 灰阶 30 相的 GL，取自 read-pico）。都是 16 灰阶，4bpp（[main/display/README.md#L13-L24](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/README.md#L13-L24)）。
  - 【事实】开机、进出待机、关机时先做完整 48 相 GC 刷白，再用推白效果显示；普通切页是 10 条带的推白动画（[main/display/README.md#L5-L11](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/README.md#L5-L11)）。
- **温度**
  - 【事实】**没有温度补偿**：波形只有 1 个温度区间，`get_temperature` 固定返回 25 °C（[board/display/epd_board_metalio_eink4_plus.c#L949-L951](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c#L949-L951)）。
- **前光**
  - 【事实】GPIO6 冷光、GPIO7 暖光，接 YX6016N，用 LEDC 20 kHz、10 bit，亮度和冷暖可渐变调节（[board/periph/frontlight.cc#L17-L21](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/periph/frontlight.cc#L17-L21)）。每帧刷完后才恢复前光。
- **显示 RAM 估算**（【推断】，依据代码里的分配算出，未实测；一帧 = 1216/2×684 = 415,872 B）

| 位置 | 内容 | 合计 |
|---|---|---|
| PSRAM | EPDiy front/back 两帧加 difference_fb 两倍帧（[board/display/highlevel.c#L47-L51](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/highlevel.c#L47-L51)）= 4×415,872 ≈ 1.66 MB；epd_refresh 的 render/pending/effect 三帧加 effect_source 与 ripple 表（[main/display/epd_refresh.cc#L553-L561](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L553-L561)）≈ 1.68 MB；LVGL 绘制缓冲 ≈ 104 KB；波形表约 20 KB | **≈ 3.45 MB**，另加 LVGL 对象和字体缓存 |
| 内部 RAM | 行队列 2 线程×128 行×304 B ≈ 76 KB（[board/display/render.c#L388-L397](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/render.c#L388-L397)）；DMA bounce buffer 2×8 行 ≈ 4.9 KB（[board/display/output_lcd/lcd_driver.c#L716-L728](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.c#L716-L728)）；LUT 1 KB；静态波形表约 9 KB；任务栈（tick、refresh、touch）约 26 KB | **≈ 120 KB** |

## 5. 输入

- 【事实】触摸是 FT6336U，地址 0x38，挂在共享的 I2C1 上（GPIO0/1，**100 kHz**，[board/bus/metalio_i2c1.c#L151](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c#L151)）。INT 接 GPIO5，下降沿 ISR 唤醒 touch_feed 任务，手指按住期间每 10 ms 采一次（[board/display/metalio_touch.c#L109-L120](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/metalio_touch.c#L109-L120)）。复位脚是 TCA9555 的 P1.2，**和 4G 模组、摄像头共用**（[board/config.h#L94-L99](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L94-L99)）。开机可以自动升级 FT 固件到 V03（[sdkconfig#L1278](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L1278)）。
- 【事实】盖板上有 3 个虚拟键（HOME、PREV、NEXT），实现方式是识别屏幕外 Y≈1600 处的触点，长按阈值 500 ms（[board/config.h#L103-L109](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L103-L109)）。
- 【事实】实体键有 4 个：BOOT 是 GPIO61；POWER、VOL+、VOL− 分别是 TCA 的 P1.7、P1.0、P1.1，TCA 的中断接 GPIO2，与加速度计共用。POWER 长按 3 s 会在 P1.3 输出关机脉冲（[board/config.h#L111-L119](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L111-L119)）。音量键用来翻页。
- 【事实】手势只有一个：下滑呼出快捷面板。它在 indev 回调里自行实现，遇到可滚动容器时会让路（[main/display/lv_adapter_epdiy_touch.cc#L150-L210](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy_touch.cc#L150-L210)）。

## 6. 音频

- 【事实】外置蓝牙音频芯片 **BH1098G**。ESP 作为 **I2S slave**（BCLK42、WS43、DOUT44、DIN45），立体声 32 bit 槽位，16 kHz，DMA 4×240（[common/audio/bt_audio_codec.cc#L57-L136](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/audio/bt_audio_codec.cc#L57-L136)）。
- 【事实】麦克风是双声道（mic 加回采参考）。音量完全是软件做的：乘以 (v/100)² 后写回 NVS（[common/audio/bt_audio_codec.cc#L138-L164](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/audio/bt_audio_codec.cc#L138-L164)）。
- 【事实】模式控制走 UART1，115200，TX39、RX38，开机发 AT 命令 `AT+RX=2` 再 `AT+MODE=1`（[board/periph/metalio_audio.cc#L94-L110](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc#L94-L110)）。
- 【事实】功放 NS4150 由 TCA P0.4 控制；**P1.4 这一路电源同时给 4G、BT 芯片和功放供电**。最后一个使用方释放后，I2S 通道会被拆掉（[main/application.h#L92-L111](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.h#L92-L111)）。
- 【事实】本地播放只有内嵌的 OGG/Opus 文件（`PlaySound`，[main/audio/audio_service.cc#L745](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_service.cc#L745)）。**没有找到 MP3 解码。**

## 7. 网络

- 【事实】Wi-Fi 用内嵌并大幅改过的 esp-wifi-connect 2.6.2。**SoftAP 和 SmartConfig 配网都已删除**，只能在设备的设置页里配网（[common/board/wifi_board.cc#L101-L103](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/wifi_board.cc#L101-L103)）。配置会同步到 SD 卡上的 `wificonfig.json`，**密码是明文**（[docs/wifi-config.md](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/docs/wifi-config.md)）。
- 【事实】**NT26 走“UART 网卡”方式，不是 PPP。** `UartEthModem` 用 UART2，2 Mbps（会在 2M/3M 之间自动探测），引脚 TX58、RX59、MRDY60、SRDY3，用 UHCI DMA。帧头是自定义的 4 字节，同一条链路复用以太网帧和 AT 命令，再通过 iot_eth 接到 esp_netif。模组自己做 NAT（`AT+ECNETCFG="nat"`）。出处：[common/board/nt26_board.cc#L180-L187](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/nt26_board.cc#L180-L187) [common/board/uart_eth_modem.h#L196-L215](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/uart_eth_modem.h#L196-L215) [common/board/uart_eth_modem.cc#L1526-L1568](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/uart_eth_modem.cc#L1526-L1568)。
- 【事实】因此 Wi-Fi 和 4G 共用 lwIP 和 esp-tls，上层代码不需要区分。
- 【事实】`DualNetworkBoard` 开机时读 NVS 决定用 Wi-Fi 还是 Nt26Board，**切换网络要重启**（[common/board/dual_network_board.cc#L31-L80](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/board/dual_network_board.cc#L31-L80)）。
- 【事实】SIM 卡槽可用 `AT+ECSIMCFG` 切换（[main/display/screen/settings_screen/settings_network_tab.cc#L321-L490](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/settings_screen/settings_network_tab.cc#L321-L490)）。**没有任何短信或通话代码**；`PowerNeed::PhoneCall` 定义了但没人用。
- 【事实】TLS 用 esp-tls 加完整的证书包（[components/esp-ml307/src/esp/esp_ssl.cc#L38-L51](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/components/esp-ml307/src/esp/esp_ssl.cc#L38-L51)），mbedTLS 的内存走 PSRAM，接收缓冲 16 KB（[sdkconfig#L3589-L3597](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L3589-L3597)）。

## 8. 存储

- 【事实】分区表（16 MB，[partitions.csv](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/partitions.csv)，与 v1/16m 相同；bootloader 在 0x2000，分区表在 0x8000）：

| 分区 | 大小 | 用途 |
|---|---|---|
| nvs | 16K | NVS |
| otadata | 8K | OTA 元数据 |
| phy_init | 4K | PHY 校准数据 |
| model | 400K | SPIFFS，S31 上没有使用 |
| ota_0、ota_1 | 各 5M | 双应用槽 |
| resources | 400K | 图标等资源，由 esp_mmap_assets 映射 |
| font_data | 5M | subtype 0x40，UI 字库 fontpack，mmap 访问 |
| coredump | 64K | 崩溃转储 |

- 【事实】TF 卡走 **SDMMC slot0 4 线**（CLK24、CMD25、D0–D3 = 20–23，[board/config.h#L125-L139](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L125-L139)），文件系统 FATFS，挂在 `/sdcard`，最多同时 12 个文件，不会自动格式化（[common/storage/SdCardManager.cc#L83-L137](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/storage/SdCardManager.cc#L83-L137)）。长文件名用 CP936 加 UTF-8。
- 【事实】所有 FatFS 和 VFS 调用都用 `--wrap` 包了一层，用于进入待机前排空访问（[main/CMakeLists.txt#L304-L308](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/CMakeLists.txt#L304-L308)）。还附带一个 IDF 的 SDMMC 补丁（[patches/esp-idf/sdmmc-preserve-write-error.patch](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/patches/esp-idf/sdmmc-preserve-write-error.patch)），需要手动打。
- 【事实】没有使用 LittleFS。TF 卡还能通过 TinyUSB HS 以 USB MSC 方式给电脑访问。
- 【事实】SD 上的目录都在 `/sdcard/metalio/e-ink/` 下，包括 books、fonts、fonts_ttf、wallpaper、chat_log、logs、settings 等（[common/storage/sd_paths.h#L69-L90](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/storage/sd_paths.h#L69-L90)）。
- 【事实】**.ebook 格式**：魔数 `EBOK`，64 B 文件头，章节索引每章 32 B，正文按 4 KB 分块、每块独立 zlib，块类型有 TEXT 和 IMAGE，图片是 A2I1 1bpp（[tools/ebook/SPEC.md#L43-L100](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/tools/ebook/SPEC.md#L43-L100)）。PC 端转换器在 `tools/ebook`，用 Python 写成，带 Web 界面和 WASM 版 LVGL 模拟器，可从 PDF、EPUB、MOBI、TXT 转换。
- 【事实】字体有三种：
  - UI 字体是 `EFNT` 格式的 fontpack，内含 MiSans 25/30@2bpp、Emoji 和数学符号，由 CMake 直接烧进 font_data 分区（[CMakeLists.txt#L12-L19](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/CMakeLists.txt#L12-L19) [tools/fontpack/README.md#L27-L37](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/tools/fontpack/README.md#L27-L37)）；
  - 阅读字体是放在 SD 上的 `.ef` 位图字体，1 或 2 bpp（[main/display/font/epdfont.h#L14-L35](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/font/epdfont.h#L14-L35)）；
  - 还可以用 FreeType 2.14.3 在运行时直接渲染 TTF。

## 9. 阅读器

- 【事实】`BookSession` 统一处理 TXT、EPUB、EBOOK，全部流式读取。TXT 第一次打开时生成行索引和页索引（`.txt.idx`），阅读进度存为 `.pos`（POS3 格式），全书页数缓存为 `.cpg`。大章节只在内存里保留固定窗口，GB18030 和 UTF-8 都是流式解码（[main/reader/book_session.h#L22-L48](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/reader/book_session.h#L22-L48)）。
- 【事实】排版和分页按实际字形宽度计算，在 book_open、book_layout、reader_page 这些工作任务里完成并生成位图，UI 端用代次校验后才发布（[docs/ui-scheduling.md#L11-L19](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/docs/ui-scheduling.md#L11-L19)）。
- 【事实】支持竖排。漫画页面的 L8 缓存最多占可用 PSRAM 的一半，并至少留 2 MiB。
- 【事实】UI 用 fontpack 字体，正文用 .ef 或 TTF 字体。断行规则的细节**未核实**。

## 10. 电源

- 【事实】PowerPolicy 分四档：Full、NetActive、AppIdle、StandbyUi（[common/power_policy/README.md#L183-L195](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/README.md#L183-L195)）。AppIdle 档只是用 `esp_pm_configure` 把 CPU 固定到较低频率，没有开自动浅睡（[common/power_policy/power_hw.cc#L292-L303](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/power_hw.cc#L292-L303)）。
- 【事实】待机的做法是显示一个 overlay，然后反复调用 `esp_light_sleep_start`，每次最多 60 s（[common/power_policy/power_policy_priv.h#L31](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/power_policy_priv.h#L31)）。入睡前依次做这些事（[common/power_policy/README.md#L73-L86](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/README.md#L73-L86)）：
  - 卸载 SD，SD 引脚拉低并保持；
  - 让 TPS 进入 SLEEP，EPD 总线拉低并保持；
  - 释放加速度计中断，屏蔽按键；
  - 配置 GPIO2 的 EXT1 唤醒（电源键经 TCA 中断进来）；
  - 先断 P0.5（摄像头/屏），再断 P1.4（4G/BT/功放）。
- 【事实】定时醒来只用来累计时间；待机累计到设定时长后，向电源芯片发关机脉冲硬关机。**没有用深睡。** FT6336 没有进入休眠（[board/metalio_e_ink4_plus_board.cc#L129-L135](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc#L129-L135)），触摸不能唤醒。
- 【事实】S31 浅睡的 SDK 兼容是这样做的：用链接器 wrap，并在配置阶段复制一份 IDF 的 pmu_sleep.c 打补丁后编译（[common/power_policy/s31_sleep/s31_sleep_sdk.cmake#L1-L54](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/s31_sleep/s31_sleep_sdk.cmake#L1-L54)）。
- 【事实】电量计 BQ27220（0x55）虽然在，但界面显示的电量是**按电压 3.3–4.3 V 线性估算的**（[docs/battery-soc-and-charge.md#L39-L47](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/docs/battery-soc-and-charge.md#L39-L47)）。充电芯片 CX25601N（0x6B），充电电流可选 0.5/1/1.5 A，默认 1 A。
- 【事实】屏幕 PMIC TPS65185 每刷一帧上电一次。如果连续 3 次上电失败，会直接调用 `esp_restart()`（[board/display/epd_board_metalio_eink4_plus.c#L904-L921](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c#L904-L921)）。
- 【推断】待机时保持供电的有：RAM 保持的 MCU、RTC、TCA9555、电量计、触摸芯片。实际待机电流**未核实**。

## 11. 构建

- 【事实】要求 **ESP-IDF v6.1**（sdkconfig 由 6.1.0 生成，[sdkconfig#L3](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L3)），目标芯片 `esp32s31`。
- 【事实】依赖清单（[main/idf_component.yml#L1-L25](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/idf_component.yml#L1-L25)）：
  - idf ≥5.4
  - lvgl ==9.5.0，esp_lvgl_adapter ==0.6.3，esp_mmap_assets ≥1.2
  - 78/esp-opus-encoder ~2.4.1，esp_codec_dev ~1.5，esp-sr ~2.2.0（仅 s3/p4）
  - button ~4.1.3，zlib *，cjson *
  - freetype 2.14.3~1，esp_new_jpeg ^1.0.2，esp_tinyusb ^2.0.0（仅 s31）
- **【事实】有构建隐患：** CMake 里 `REQUIRES 78__uart-uhci espressif__iot_eth`（[main/CMakeLists.txt#L246](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/CMakeLists.txt#L246)），但这两个组件没有写进 yml，而 `dependencies.lock` 又被 gitignore 了（[.gitignore#L16](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/.gitignore#L16)）。从干净环境构建能否成功**未核实**。
- 【事实】sdkconfig 要点：
  - CPU 320 MHz（[sdkconfig#L2384](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L2384)）；
  - **Octal PSRAM 200 MHz**（[sdkconfig#L2289-L2295](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L2289-L2295)），`SPIRAM_USE_MALLOC`，ALWAYSINTERNAL=16，**RESERVE_INTERNAL=64 KB**（[sdkconfig#L2309-L2314](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L2309-L2314)）；
  - 允许任务栈放在外部内存（[sdkconfig#L5378](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L5378)）；
  - L1 指令缓存 16 KB、数据缓存 64 KB（[sdkconfig#L2390-L2392](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig#L2390-L2392)）；
  - Flash DIO 80 MHz，`-Os`，任务看门狗 20 s，开启 PM，关闭 ESP 自带蓝牙协议栈。
- **PSRAM 容量未核实**：sdkconfig 里没有写死容量，开机时自动探测；README 没有给出容量。
- 【推断】除去生成的字体和图片，手写代码约 13.3 万行。固件 bin 的大小未核实；README 提醒要通过 5 MiB 的 app_check_size 检查。

## 12. “新工程加移植驱动”的建议

| 子系统 | 最小移植文件集 | 对上游全局对象和单例的依赖 | 难度 |
|---|---|---|---|
| (a) 显示、触摸、按键、前光、电源 | `boards/metalio-e-ink4-plus/{config.h, bus/*, display/*（EPDiy fork、波形、epd_board、metalio_epd、metalio_touch）, input/metalio_keys·vibe, periph/frontlight·metalio_power·metalio_rtc}`；`common/power/*`；`common/sensor/pcf8563·sc7a20h`；`common/power_policy/s31_sleep/*`；`main/display/{epd_refresh.*, epd_prepare.h, epd_page_transition.h, epd_flash_guard.h, display_orient.*}` | EPDiy 和板级部分基本是独立的 C 代码，但 `epd_board_*.c` 引用了 `MetalioSd_ReleaseHold` 和 NT26 的 SRDY 脚（[board/display/epd_board_metalio_eink4_plus.c#L438-L470](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c#L438-L470)）。`epd_refresh.cc` 依赖 OtaUpgradeScreen、Frontlight、power_hw、sd_log、Settings（[main/display/epd_refresh.cc#L1-L23](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/epd_refresh.cc#L1-L23)），需要解耦。`lv_adapter_epdiy.cc` 牵连所有页面（[main/display/lv_adapter_epdiy.cc#L6-L53](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L6-L53)），**应当重写**。`power_hw.cc` 依赖 Application、NT26、待机页，**应当按 README 的上下电顺序重写** | EPDiy 和板级：中；refresh：中高；电源时序：高 |
| (b) 音频 | `common/audio/bt_audio_codec.*·simple_uart.hpp`、`periph/metalio_audio.*`、`audio/audio_codec.*·audio_service.*·processors/no_audio_processor.*` | `power_hw_bt_power_reset`、PowerPolicy、Board、Settings、Opus 组件 | 低到中 |
| (c) 4G 与 Wi-Fi | `common/board/uart_eth_modem.*`（Apache，几乎独立）、`components/esp-wifi-connect`、`components/esp-ml307` 中的 esp/、http_client、web_socket；`nt26_board`、`dual_network_board`、`wifi_board` 只作参考后重写 | 需要 uart-uhci 和 iot_eth；`Nt26Board` 和 `WifiBoard` 依赖 Application、Display、StandbyScreen、语言字符串；P1.2 共用复位、P1.4 共用电源 | 中 |
| (d) SD | `common/storage/SdCardManager.*·sd_access.*·sd_paths.h`、`periph/metalio_sd.*`（含 S31 CNNT pad 释放），可选 usb_virtual_disk | sd_log_writer、链接器 wrap、IDF 补丁 | 低到中 |

- **基本可以直接复用的：**
  - `protocols/protocol.*`、`websocket_protocol.*`（xiaozhi 协议，Hub 端可以照着实现）；
  - `mcp_server.*`，只需把 `Application::SendMcpMessage` 和 `Schedule` 换成自己的钩子，并**补上 user_only 校验**；
  - `ota.*`，保留 `WithOtaFlashAccess` 的加锁思路，去掉 CloudZao 和激活逻辑；
  - `settings.*`；
  - A2UI-lite 渲染器（与传输无关的 C/LVGL 代码，[main/display/a2ui/README.md](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/a2ui/README.md)）；
  - `tools/ebook`、`tools/fontpack`、`tools/epdfont` 这些工具链和格式规范；
  - 以后可考虑复用阅读器核心 `main/reader`。
- **建议不要移植的：**
  - `application.cc`（2210 行的 xiaozhi 状态机，加上三家云服务和激活轮询）；
  - `display/screen/*` 全部 UI（约 8 万行，耦合很重）；
  - chat_cloud 和 cloudzao_endpoints；
  - MQTT（在 S31 上本来就不可用）；
  - 摄像头相关代码；
  - WorkBuddy 的实现（可以参考思路）。

## 13. 风险和痛点

- **UI 会被阻塞。** 以下地方会在持有 LVGL 锁时轮询等待刷新完成，每 10 ms 查一次，最长 10 s：`EpdWait`（[main/display/lv_adapter_epdiy.cc#L718](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L718) [main/display/lv_adapter_epdiy.cc#L757-L758](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L757-L758) [main/display/lv_adapter_epdiy.cc#L984](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/lv_adapter_epdiy.cc#L984)）；开机流程里的 `OtaConfirmDialog::ShowBlocking`（[main/application.cc#L1139](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/application.cc#L1139)）；以及阅读设置面板持有 EpdFlashGuard（[main/display/screen/book_screen/settings/book_settings_sheet.cc#L1502](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/display/screen/book_screen/settings/book_settings_sheet.cc#L1502)）。因为 GC16 一刷就是好几秒，任务看门狗被调到了 20 s（[sdkconfig.defaults#L35-L36](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig.defaults#L35-L36)）。
- **全局锁很多。** `EpdFlashMutex` 被显示、OTA、系统监控、P14 断电、摄像头共用。I2C1 只有一条，100 kHz，上面挂着触摸、TCA（按键和 TPS 控制脚）、电量计、充电芯片、RTC、加速度计，而且有固定的加锁顺序要求（[main/ota.cc#L41-L56](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/ota.cc#L41-L56)）。另外，栈在 PSRAM 上的任务不能访问 NVS 或 flash（[main/chat_cloud.h#L17-L18](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/chat_cloud.h#L17-L18)）。
- **内存压力大。** 显示占用约 3.45 MB PSRAM，内部 RAM 也要占约 120 KB；每次提交和每帧刷新各复制一次 406 KB。
- **引脚按硬件版本写死。** config.h 里注释掉的是 V1.2 的引脚（XSTL=19、MODE=36），实际启用的是 V1.3/1.4 的（XSTL=4、MODE=19、ANA_SW=36、RTC 中断走 TCA P1.6），代码里**没有运行时版本检测**（[board/config.h#L24-L62](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L24-L62) [board/config.h#L86-L101](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/metalio-e-ink4-plus/config.h#L86-L101)）。板级文件头又写着“原理图 V1.5”。**必须先确认手上这块板的版本。**
- **复位和电源是共用的。** 复位一次 BT 音频芯片，4G 模组也会跟着重建（[common/power_policy/power_hw.cc#L199-L223](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/power_hw.cc#L199-L223)）。
- **其他问题：**
  - 没有温度补偿；
  - s31_sleep 会在配置阶段直接改 IDF 源码，IDF 一旦变化就 FATAL_ERROR，等于锁死在 IDF 6.1（[common/power_policy/s31_sleep/s31_sleep_sdk.cmake#L41-L45](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/boards/common/power_policy/s31_sleep/s31_sleep_sdk.cmake#L41-L45)）；
  - MCP 可以触发任意 URL 的 OTA，又没有签名或 secure boot；
  - 上游改动非常频繁（7 天改了 294 个文件），移植时要固定一个 commit 并记录下来。

## 附：社区 fork 与未核实项

- 【事实】`wegooo-cell/Metalio-S31-kiikoread`，HEAD `83942eb`（2026-10-30）。这是一个**纯阅读固件**：文件树里没有 NT26、Opus、MCP 相关文件，但有 BLE 翻页器。它**只支持 V1.5 的 E0470A14-AF 34Pin**。
- 【事实】它有自己的 EPDiy fork（声明 LGPL）和 PIE/SIMD 优化，使用 S31 的 JPEG 硬件解码；从 Metalio `ad8edf4` 借用了 S31 的 LCD/IOMUX 输出、TPS 和 9555 板级参考以及 s31_sleep（MIT）。
- 【事实】它新写的代码采用“个人学习许可”，**禁止商用**，复制代码时要注意。
- **未核实：**
  - 能否编译，以及固件大小；
  - PSRAM 容量；
  - 运行时实际内存占用和待机电流；
  - IDF 补丁和未声明的组件是否必需；
  - MiSans 字体许可；
  - 阅读器的断行规则；
  - BH1098 的数据手册；
  - A2UI 服务端的完整协议；
  - GitHub Releases（API 访问被拒，没能查看）。

## 2026-11-11：原厂 v2.0.77（`874538e`）相对 `690a023` 的变化

用户 2026-11-11：“官方仓库最近一直有更新，或许有一些 bug 修复和新的信息”。四个新提交（v2.0.72、编译修复、v2.0.74、v2.0.77，到 2026-11-07），275 个文件。只读副本仍固定在 `690a023`（S31 构建的屏幕输出层按这个提交校验，见 `ports/esp32s31/display_upstream.cmake`，副本不是这个提交就会悄悄换成屏幕桩）；新版本相关目录另外导出到 `ink-hw/upstream/Metalio-E-INK4-Plus-874538e`。

| 变化 | 和我们的关系 | 怎么办 |
| --- | --- | --- |
| `output_lcd/lcd_driver.c` 的 S31 修复：中止一帧改到中断所在的核上做（`esp_ipc_call_blocking`），并复位 GDMA、清挂起中断，不再调完成回调；补 AXI DMA 的帧结束中断；CKV 周期按时钟拍精确计算；8 位总线加占位时钟的配置改走新的扫描时序 | 我们直接用这个文件。刷新失败后恢复（`2d550dea`）正要先中止再开新帧，原厂的中止修复让它更可靠；新的扫描时序会改驱动屏幕的时间，可能影响真屏上标定的灰阶 | 换版本（改 `display_upstream.cmake` 的固定提交）要先上真机：刷新日志无漏行、整屏和局部刷新、灰阶由用户看过 |
| `config.h` 新常量：共享电源断开后马达脚继续拉高 500 ms 放电；外设上电后等 80 ms 再恢复下一路电源或显示总线 | 4G、蓝牙、功放共用 P14；`port-cell-1` 会管这路电源 | 合并 `port-cell-1` 时照这两个常量核对 |
| `components/78__uart-uhci`（78/uart-uhci 0.2.2，Apache-2.0）收进了仓库：串口加 UHCI DMA，4G 帧协议用它 | 只读副本原来没有 `components/`；`port-cell-1` 要移植帧协议 | 合并 `port-cell-1` 时核对它的串口层和这个组件的差别 |
| `power_hw.cc`、`power_policy`：待机浅睡的断轨和恢复顺序、断开共享电源前通知蜂窝 `PreparePowerOff`、恢复后 `OnPowerRestored` | 试用版没开浅睡 | 以后做浅睡时照着对齐 |
| 摄像头：`dvp_camera`、`dvp_camera_adapter`，开机检测 | S31 的相机还是桩 | 以后移植相机参照 |
| `docs/bh1098-audio-protocol.md`：蓝牙音频芯片的模式（本机喇叭和麦克风、蓝牙发送、接收、低功耗、蓝牙语音和 HID、USB）、I2S 格式、模式命令 | 没有提到 4G 模组；模组的通话声音仍找不到通路 | 通话能不能做仍待原理图或实测；以后接蓝牙耳机参照 |
| `docs/epd-spv-investigation.md`：试过 85 Hz 相位频率没成，已回退；保留一处原厂送线线程等待误报超时的修复 | 我们用自己的送线任务，按时间等，不受这个问题影响 | 无 |
| `ble_page_turner`（蓝牙翻页器） | D-120 不做 | 无 |
