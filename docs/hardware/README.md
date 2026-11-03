# 实机记录（CloudZao Metalio E-Ink4 Plus）

本目录记录实物到货后的每一步操作与结果，是实测数据的唯一维护位置。数字写回模拟器档案或其他文档时，注明出自这里的哪一次记录。

**公开仓库，只放脱敏内容。** 整片备份、原始日志、NVS 内容、TF 卡数据只留在用户的电脑上；本目录只收脱敏摘要。可能含 Wi-Fi 密码、设备令牌、IMEI/ICCID、MAC 等。

## 现状（2026-11-03）

- **到货**：2026-11-03 到货，比预计的 12 月初早。已经用 USB 连到用户的 Mac（Apple Silicon）。
- **能操作设备的只有用户本机的程序。** 助手所在的云端会话碰不到 USB 设备：
  - 它在用户 Mac 上的命令行是隔离的虚拟机，看不到 USB 串口；
  - 屏幕控制对终端类应用只允许点击，不能输入。
- **因此的分工**：本机的 Codex CLI 负责接触设备；助手负责定步骤、核对结果、把实测值写回档案与文档。
- **第 0 步**（只读识别 + 整片备份 + 出厂固件日志）已交给本机 Codex，提示词见 [phase0-prompt.md](phase0-prompt.md)。结果放在用户电脑的 `~/work/ink-hw/bringup-<日期>/`，脱敏摘要在其中的 `share/SUMMARY.md`。

## 规则

1. **写入前提**：整片备份已核对（两次读取一致，并与设备比对通过），且用户明确同意。写入包括烧录、擦除、写 NVS、OTA。
2. **eFuse 不烧**：任何 eFuse 烧写都不可逆，不做。
3. **每次写入都要能复原**：事先写好恢复步骤，并确认备份能覆盖。
4. **不拆机、不改硬件**，除非用户另行决定。

## 上游配置中与上机有关的事实

出处：上游 [sdkconfig](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/sdkconfig) 与 [partitions.csv](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/partitions.csv)，提交 690a023，2026-11-03 查阅。这是上游源码的配置，**不等于这台机器里的出厂固件**，以第 0 步的实际读出为准。

| 项 | 上游配置 |
| --- | --- |
| 芯片 | `esp32s31`，ESP-IDF 6.1.0，CPU 320 MHz，最低芯片版本 0 |
| Flash | 16 MB，DIO，80 MHz |
| 偏移 | bootloader 在 0x2000，分区表在 0x8000 |
| PSRAM | 八线（OCT），200 MHz，启动时初始化 |
| 控制台 | UART0，115200；USB 串口/JTAG 作第二控制台 |
| USB | 启用 TinyUSB MSC，可把 TF 卡挂到电脑上 |

分区表：

| 分区 | 类型 | 偏移 | 大小 |
| --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 | 16K |
| otadata | data/ota | 0xd000 | 8K |
| phy_init | data/phy | 0xf000 | 4K |
| model | data/spiffs | 0x10000 | 400K |
| ota_0 | app | 0x80000 | 5M |
| ota_1 | app | 0x580000 | 5M |
| resources | data/spiffs | 0xa80000 | 400K |
| font_data | data/0x40 | 0xae4000 | 5M |
| coredump | data/coredump | 0xfe4000 | 64K |

**工具版本**：esptool 需 5.3 或更新。5.3 起才有 ESP32-S31 的 stub 刷写器，并修了它的 Flash 读写命令。出处：[esptool v5.3.0 发布说明](https://newreleases.io/project/github/espressif/esptool/release/v5.3.0)，2026-11-03 查阅。

## 接下来

验证项见 [风险与验证](../system/risks-and-spikes.md)。

| 步骤 | 内容 | 对应验证项 | 写设备 |
| --- | --- | --- | --- |
| 第 0 步 | 识别芯片、Flash、PSRAM、安全状态；整片备份；出厂固件的启动日志与刷新日志 | SP-00 前半 | 否 |
| 第 1 步 | 测量固件：只报告硬件信息（PSRAM、内部 RAM 各段、Flash 型号），并在真机上跑指令级模拟器的同一批校准内核（拷贝、字形、送行等），直接对比模拟值；不碰屏幕、电源和无线 | SP-00、校准 D-35 的送行成本 | 是，只写一个应用槽，可按备份复原 |
| 第 2 步起 | 屏幕点亮与刷新时序、Flash 擦写停顿、音频并发、电流等 | SP-01、SP-02、SP-12、SP-13、SP-14 | 是 |

第 1 步写设备的方式与复原步骤，要等第 0 步确认三件事后再定：USB 连到哪个控制器、安全功能是否关闭、当前从哪个槽启动。

## 记录

| 日期 | 步骤 | 结果 | 详见 |
| --- | --- | --- | --- |
| 2026-11-03 | 到货，USB 连接用户的 Mac；第 0 步交给本机 Codex | 进行中 | [phase0-prompt.md](phase0-prompt.md) |
