# 原厂 Wi-Fi STA 来源与适配

查阅日期：2026-11-11。来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)，提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77）。组件声明来自 `78/esp-wifi-connect v2.6.2`，原厂已自行维护，许可 MIT；完整原厂 MIT 许可证保存在 `LICENSE`。

## 原文副本

以下文件通过固定提交的 `git show` 取得，逐字节保留，没有修改；不编译含 NVS/SsidManager 的原厂类。

| 本目录文件 | 原厂路径 | SHA-256 |
| --- | --- | --- |
| `upstream/wifi_station.cc` | `components/esp-wifi-connect/wifi_station.cc` | `a236bae918d3aa6d54fdd2f50699938cd246ae10d0115390fa146e9f651e928b` |
| `upstream/wifi_station.h` | `components/esp-wifi-connect/include/wifi_station.h` | `88d0186b3c308b738183632d828d0b7d917b6d1be152ee7432e7323ad9cfabcf` |
| `upstream/idf_component.yml` | `components/esp-wifi-connect/idf_component.yml` | `3f18466e86aae7729dee357ba0692ce387fabf916733f989242b904e708f29a3` |
| `LICENSE` | `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |

## 实际参与构建的 C 适配

`hal_wifi_s31.c` 直接包含 `station.inc`（SHA-256：`c175820840d4bb351effa2e5efd1089210f1e410d4e933780f9a229fd9eec54f`）。既有构建只编译 C HAL，任务又不允许修改构建文件，因此在这个包含文件中适配原厂 C++ 成员、引用、容器及回调；没有另写任务或替换 HAL。原文副本不被误称为直接运行的整个类。

| 运行位置 | 原文出处（上表 cc/h） | 必要改动及理由 |
| --- | --- | --- |
| `ApplyApSelection` | cc:27–33 | C++ 引用改为 C 指针及 `->`；保留全信道、按信号、清 BSSID 与 channel 原文操作。 |
| `ApplyWifiProfile`、`NetifInitOnce` | cc:35–44 | 完整函数原文，无改动。 |
| 功率、重试次数及间隔常量 | h:16；cc:20–22 | C 不支持 `constexpr`，改为 `static const`；值仍是 48、5、10 秒。 |
| `ink_station_pause` | `PauseForLp`，cc:151–178 | 成员换成 HAL 状态；调用方持既有锁，去掉 C++ RAII；保留 STA/AP 模式检查、停定时器、停扫描、停射频和保留 netif。HAL 只清自己的连接位，停失败保留运行标志，日志不含标识。 |
| `ink_station_resume` | `ResumeFromLp`，cc:180–210 | 成员换成 HAL 状态；复用模式检查、重连计数清零、启动前 profile 和启动失败恢复。用户开关由系统决策；已有扫描候选用于快速恢复，不保存第二份 SSID/密码。 |
| `ink_scan_retry`、`ink_scan_later` | Start 的定时器回调，cc:265–281；cc:327–330、480–485 | 保留 IDF 单次 10 秒重扫。选中 Wi-Fi 时按原厂直接请求重扫；LTE/NONE 下改发既有网络通知，只有系统查询 Wi-Fi 可用性才排入扫描，避免 LTE 固定选择及 Wi-Fi 关闭时擅自启射频。扫描完若不是选中 Wi-Fi，立即按暂停流程停射频。 |
| `ink_rejoin`、`ink_station_disconnected` | `StartConnect`，cc:336–370；断线分支 cc:464–485 | C++ vector 换为最多 8 个已知网络的索引，密码仍只在既有 RAM 表；保留每个 AP 5 次重连、候选耗尽后等 10 秒重扫。配置复制用既有 `ink_join`，不搬 NVS、SsidManager 或身份日志。 |
| HAL 的初始化、STA_START | `Start`，cc:225–262；cc:426–430 | 保留 netif → RAM 初始化 → STA/profile；首次射频默认关，按系统查询/选网恢复。Plus 功率在 `esp_wifi_start` 返回后设置，确保手动 HAL 连接也先限功率再连接（SDK `esp_wifi.h:1167–1184` 允许 start 后调用）；事件、SNTP、双网路由仍走既有 HAL。 |
| HAL 的扫描完成 | `HandleScanResult`，cc:291–334 | 用既有逐条取记录接口代替 malloc 和 C++ sort；最多 8 个已知网络索引仍按 RSSI 排序并去重；保留手动扫描的输出、错误和超时语义；不打印 SSID、BSSID、IP、密码。 |

`SetUserEnabled` 的 NVS 写入、构造器 NVS 读取、`SsidManager`、扫描租约、产品回调、整机重启不迁入：系统已有设置、已知网络服务和手动扫描互斥。`Stop` 的 deinit/destroy 不迁入，开关使用原厂 `PauseForLp`/`ResumeFromLp` 保留 netif 的路径，避免破坏在线双网及 SNTP 事件注册。

没有新增配置项、文件格式或项目任务。原厂扫描候选和 IDF 定时器由本任务明确要求；定时器复用 SDK 的 `ESP_TIMER_TASK`，不创建自己的任务栈。适配中的候选表 8 个 `int`，凭据仍在既有 PSRAM 表。
