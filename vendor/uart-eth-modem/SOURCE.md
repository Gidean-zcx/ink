# uart-eth-modem 来源与改动

- 来源：[CloudZao/Metalio-E-INK4-Plus@874538e](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)（v2.0.77），查阅日期 2026-11-11。只读取对象，未移动上游 HEAD。
- `uart_eth_modem.*`、iot_eth 和 UHCI 按文件头为 Apache-2.0；板级生命周期为原仓库 MIT。完整许可证随文件保留；不是 GPL。
- 帧协议、主任务、独立 TX、AT 探测、网卡和 DMA 使用原厂主体。下表同时给原文与本地哈希，**不把适配后的文件声称为逐字节原文**。

| 本地文件 | 原厂路径 | 原文 SHA-256 | 本地 SHA-256 |
| --- | --- | --- | --- |
| `uart_eth_modem.cc` | `main/boards/common/board/uart_eth_modem.cc` | `5e9157aa89136be4c4ff3d634fd7d4066b1ea200373b9a60266f603d72c11c26` | `ad9fd56e46b1bd06d8fc8d93965d309d51ed757a367b3a717afa0fc06c328788` |
| `uart_eth_modem.h` | `main/boards/common/board/uart_eth_modem.h` | `00ffb790a900a072e6daeefd0192c361b93d7df13b08669ad4389d2cef47b78d` | `8ff6aba40eda4ac5cb3f4057f6492a7ddfe5eed6226936d1c435034f1eb354c2` |
| `LICENSE` | `main/boards/common/camera/GC2145-LICENSE` | `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` |
| `LICENSE-MIT` | `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `nt26_lifecycle.cc` | `main/boards/common/board/nt26_board.cc` | `44545151b3748c015835e384c66d7c670877f65c95014e92c21d890a92c98d06` | `d93f4f1d2ccd860ddbedccb25dc2aa32354cda205481bc39c9b3104c7b9e77c9` |
| `nt26_lifecycle.h` | `main/boards/common/board/nt26_board.h` | `6fa5271d2b35c779d9c043b4a67762ed07547763b896ef046b08b47c06bb6ad8` | `a0ba586f09641b91f9a17b19210d73c5fbe701381195a1fed4e8289b9a111627` |

## 改动原因

1. `SendAt` 与 `SendAtCollectUntil` 接同一允许列表；必需 NAT/波特率配置拒绝时返回 `ESP_ERR_NOT_SUPPORTED`，停止该网络初始化。设备标识查询撤下；原厂日志只输出固定格式串，绝不格式化私有参数。
2. AT 回复按行累积，保留跨帧内容、`>`/PDU、迟到回复隔离和超时结果；帧解析仍是原厂 `HandleRxData`。RX 溢出通知送原厂主队列，清除半帧和半行；Stop 停 DMA、归还剩余借用后才 Deinit。
3. 已开数据通道仍发原厂 SPIC；仅无 ACK 才关开射频，沿用本项目每次开机最多一次的限制。依据 `docs/hardware/2026-11-10-trial.md` 换装 13–15。
4. `+ECNETDEVCTL` 0/1 清握手/IP、发断开和重建通知；注册不等于 DHCP。选网、流量和 IP 事件接现有 C HAL；不选 LTE 就不发 LINK_UP。
5. `ECSCLKEX` 按未知持久性拒绝：初始化保留配置只读失败事件，不打印初始化成功；已有真实握手的通道接 iot_eth，睡眠配置没有成功。这个继续接网行为与任务书“被拒不能继续”的严格解释冲突，见 REPORT，待主会话复核。
6. `nt26_lifecycle.*` 从 `nt26_board.*` 整函数提取 `OnNetworkEvent`、`SignalWaitBits`、`TearDownModem`、`PreparePowerOff`、`StartOrRestartModem`、`IsNetworkReady`、`EnsureNetworkReady`、`OnPowerRestored`；保留 epoch、共享所有权、操作互斥、P14 后 1 秒、5 秒节流。删除 Board/UI/Application/NVS/状态栏接口，不复制重启整机或额外恢复任务；零等待请求不在网络事件任务里阻塞冷启动。配置拒绝不触发无限配置重试。
7. `CMakeLists.txt`、`ink_modem_log.h` 为本项目适配；`ports/esp32s31/s31_*` 是 C HAL/策略胶水。源组件的硬件实现仍来自 `vendor/uart-uhci`，不另建 UHCI 对象。
8. phone-3 复核（2026-11-13）：在已列出的本地 1502–1524 段内，`AT+CLCC` 等待回复期间收到的 `+CLCC:` 行算作它的回复（原来按 URC 收走，查询只拿到 `OK`），用于通话接通前每秒查一次；行数不变。

## 改动行清单

范围由原文与本地逐行比较生成；“删除”是原文该段没有进入本地。生命周期中整段省略的是未选择的原厂 Board 功能，只有所需整函数进入构建。

- `uart_eth_modem.cc`：原 18 → 本地 18–21（insert）；原 56 → 本地 60（replace）；原 166 → 本地 170–176（insert）；原 262 → 本地 273–282（replace）；原 264 → 本地 284–287（insert）；原 271 → 本地 295（replace）；原 276–279 → 本地 300–307（replace）；原 298 → 本地 326（insert）；原 305 → 本地 334–348（insert）；原 313 → 本地 357（insert）；原 324 → 本地 369（insert）；原 339 → 本地 385（insert）；原 344 → 本地 391（replace）；原 642 → 本地 689（replace）；原 662 → 本地 709（replace）；原 666 → 本地 713（insert）；原 699 → 本地 747（replace）；原 801 → 本地 849–850（insert）；原 948 → 本地 998–1009（insert）；原 1247 → 本地 1309–1313（insert）；原 1248 → 本地 1315（insert）；原 1417 → 本地 1485–1487（replace）；原 1424–1439 → 本地 1494–1498（replace）；原 1441 → 本地 1500（replace）；原 1443 → 本地 1502–1524（replace）；原 1445–1452 → 本地 1526（delete）；原 1464–1466 → 本地 1537（replace）；原 1511–1514 → 本地 1582–1584（replace）；原 1516–1519 → 本地 1586–1594（replace）；原 1530 → 本地 1605（insert）；原 1538 → 本地 1614（insert）；原 1567–1568 → 本地 1644–1647（replace）；原 1574 → 本地 1653–1654（replace）；原 1582 → 本地 1662–1663（replace）；原 1621 → 本地 1702（replace）；原 1636 → 本地 1717（insert）；原 1642–1645 → 本地 1724–1725（replace）；原 1672 → 本地 1752–1756（insert）；原 1681 → 本地 1766（replace）；原 1742–1743 → 本地 1827（delete）；原 1745–1747 → 本地 1828（replace）；原 1816 → 本地 1897（replace）；原 1833 → 本地 1914–1918（insert）；原 1834 → 本地 1920–1921（insert）；原 1848 → 本地 1936–1941（insert）；原 1882 → 本地 1976–1985（insert）。

- `uart_eth_modem.h`：原 66 → 本地 66（insert）；原 152 → 本地 153（replace）；原 194 → 本地 195–205（insert）；原 195 → 本地 207–211（insert）。

- `nt26_lifecycle.cc`：原 1–20 → 本地 1–4（replace）；原 22 → 本地 6（delete）；原 26–29 → 本地 9（replace）；原 31–37 → 本地 11（delete）；原 39–61 → 本地 12（delete）；原 80–82 → 本地 30（replace）；原 84 → 本地 32（insert）；原 89 → 本地 38（insert）；原 109 → 本地 59（insert）；原 111 → 本地 62（insert）；原 116–119 → 本地 68（replace）；原 121 → 本地 70（insert）；原 124 → 本地 74（replace）；原 132 → 本地 82（insert）；原 190 → 本地 141（insert）；原 193 → 本地 145（insert）；原 219 → 本地 172（replace）；原 253 → 本地 206（replace）；原 273 → 本地 226（insert）；原 277–279 → 本地 231（delete）；原 285 → 本地 236（insert）；原 292–319 → 本地 244（delete）；原 325–544 → 本地 249（delete）。

- `nt26_lifecycle.h`：原 1–13 → 本地 1–12（replace）；原 21 → 本地 20（replace）；原 34–36 → 本地 33（delete）；原 40 → 本地 36（delete）；原 42–43 → 本地 37–40（replace）；原 45–46 → 本地 42（replace）；原 48–50 → 本地 44（replace）；原 52–77 → 本地 46–47（replace）；原 79–102 → 本地 49（delete）；原 104–105 → 本地 50（delete）。

- 本项目新增 `CMakeLists.txt`：5 行；SHA-256 `1db0ad4e3d7ba6b78256effe77ff1ae21511a043d4aa168a516f5d7fdc6c3024`。

- 本项目新增 `ink_modem_log.h`：17 行；SHA-256 `f5ba2908aaac16391d6f87c233e667b24a8beb2fb4667a37114dd6514fb816e0`。
