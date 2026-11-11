# port-wifi-2：Wi-Fi 底层照原厂 wifi_station 原文

先读 `docs/tasks/vendor-common.md`（公共规矩），再读 `docs/tasks/driver-audit-1a/REPORT.md` 的“Wi-Fi”一节和前 10 个坑里的 W02、W03、W04。

## 要做的

1. 照原厂 `wifi_station.cc`（及它的头和组件来源声明）的原文：STA 初始化、`ApplyWifiProfile`（Plus 固定的省电和发射功率参数，W02）、停止和恢复、重连、扫描流程，接到 `ports/esp32s31/hal_wifi_s31.c` 下面。
2. W04：关 Wi-Fi、飞行模式、切到 4G 时照原厂真正停射频（`esp_wifi_stop`），不是只断开；再开时照原厂恢复。
3. W03：AP 一度消失、切到 4G 以后，回到 AP 附近要照原厂的扫描/重连流程重新发现，让系统的自动选网能切回 Wi-Fi。
4. 保留：已知网络和密码来自系统服务、只在内存里（不写 NVS，原厂直接读写 NVS 和 SsidManager 的部分不搬）；HAL 回调、在线双网路由（`ink_s31_net_changed`、LTE 网卡的默认网卡切换）、SNTP→HAL 时间回调。

## 范围和预算

- 只改：`ports/esp32s31/hal_wifi_s31.c`、`ports/esp32s31/tests/` 里 Wi-Fi 相关测试和桩、新增 vendor 拷贝目录。不改 `s31_modem.c`（4G 任务在改）。
- 胶水新增不超过 200 行。

## 验收

1. W02、W03、W04 各有测试和反向对照；S31 宿主机测试全过；主工程普通全量过；S31 编译通过。
2. 报告有上机核对清单：开关 Wi-Fi 后射频真的停（日志）、AP 掉线再回来能切回、连上后对时、4G 和 Wi-Fi 来回切。
