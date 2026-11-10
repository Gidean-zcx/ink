# port-wifi-1：真机上的 Wi-Fi 和网络对时

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-wifi-1/REPORT.md。**只编译、跑宿主机测试，不碰设备**（上机由 Claude 做）。

## 背景

用户要明天早上能刷进去试用的版本（见 port-boot-1 的背景）。S31 端口的 Wi-Fi（`hal_wifi_*`、`hal_net_*`）都是桩，设置里连不上网，时间也没法从网上对。家庭中枢还没有真实模型，这次不接中枢的 WebSocket（`hal_ws_*` 保持桩），只做联网和对时。今晚并行的还有 port-boot-1、port-storage-1、log-1、port-audio-1，共用端口的几个文件，你只删自己的桩，改动集中。

闪存硬规则见 `docs/hardware/flash-layout.md`：**不写 nvs**。所以 `CONFIG_ESP_WIFI_NVS_ENABLED=n`、`esp_wifi_set_storage(WIFI_STORAGE_RAM)`、`CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=n`，不调 `nvs_flash_init`；如果 ESP-IDF 6.1 在 S31 上不初始化 NVS 就起不来 Wi-Fi，停下来写进报告，不要绕。已知网络由系统自己存在 `/user`（经 `hal_wifi_set_known`）。

## 要做的

预算：产品代码净增不超过 250 行。

1. 实现 `hal_wifi_scan`、`hal_wifi_connect`、`hal_wifi_set_known`（断线后按已知列表自动重连）、`hal_net_link_status`、`hal_net_set_link_callback`、`hal_net_available`、`hal_net_use`（只有 Wi-Fi，蜂窝返回不可用）。照原厂的 Wi-Fi 初始化顺序和参数（国家码、省电模式等），写出处。
2. 连上以后用 SNTP 对时，写回时钟芯片的路径照现有时间服务（看 `system/services/time`），不另开。
3. 不进浅睡的试用版里，Wi-Fi 省电模式照原厂默认。

## 验收

1. S31 能编过，0 警告；端口单测普通版和 ASan 版都通过；连接状态机、自动重连、对时回调有宿主机单测（IDF 用替身），做反向对照。
2. 模拟器全量测试照旧通过；gcccheck 通过；做隐私扫描（不得出现真实的 Wi-Fi 名称和密码）。
3. 报告里写上机要怎么验：扫到网络、连上、对时成功各看什么日志。
