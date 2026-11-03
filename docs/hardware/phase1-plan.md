# 第 1 步写入方案：测量固件（待用户同意）

起草：2026-11-03。**写设备前必须得到用户明确同意。** 本文是给用户审阅的方案，执行结果另记。

## 做什么

把测量固件 [tools/hw-probe](../../tools/hw-probe/README.md) 写进空着的 ota_1，让引导程序启动它，跑约 1 分钟，记下串口输出，然后复原：

- 测出各成本类在真机上的周期数，与指令级模拟器逐项对比，重点是墨水屏送行，它决定 D-35；
- 报告硬件信息：内部堆各区段、PSRAM、Flash、复位原因。

测量固件不初始化 Wi-Fi、蓝牙和 PHY，不碰 NVS 和 I2C；屏幕电源、4G、按键保持出厂固件留下的状态；运行时不写 Flash（核心转储已关）。

## 前提（已满足）

- 整片备份两次读取一致，并与设备比对通过；第二份在本机另一个目录，已核对（[第 0 步 §7](2026-11-03-phase0.md)）。
- 安全启动、Flash 加密都没开，下载模式可用；引导程序没有开启回滚。
- 当前从 ota_0 启动出厂固件，ota_1 为空。

## 只写两处

| 地址 | 内容 | 大小 | SHA-256 |
| --- | --- | --- | --- |
| 0x580000（ota_1） | 测量固件 `ink-hw-probe.bin`（可复现构建，版本 1） | 252 720 B | `6817b0105aff3e8c26b3c07c299a788d32c636a051236b8b7504be10bd0e0040` |
| 0xd000（otadata） | `otadata_ota1.bin`：保留出厂记录（seq 1，指向 ota_0），另一扇区新增 seq 2，引导程序改选 ota_1 | 8 KiB | `b7e293bb607d3bddb99b7f38a7a45afd5823c0c61e3216e67858bbc759535282` |

**不写**：引导程序、分区表、ota_0（出厂固件）、NVS、phy_init、model、resources、字库、coredump、eFuse。

写入工具 `flash_probe.py` 只允许写上面两个地址范围，其他地址在接触设备前就会拒绝；没有 `--approved` 参数时什么都不写。

## 步骤

1. 设备保持 USB 连接、处于醒着的状态。待机时 USB 会断开，需要你短按电源键唤醒。
2. 写入：

   ```bash
   tools/hw-probe/flash_probe.py install --port '/dev/cu.usbmodem*' --dir ../ink-hw/phase1 --approved
   ```

   工具依次做这些：
   - 先监听串口，等屏幕空闲，避开刷新中间的复位，原因见第 0 步 §2；
   - 进下载模式，写上面两处，与设备逐区比对；
   - 复位进入测量固件，记录串口 60 秒。
3. 对比：`tools/hw-probe/compare.py ../ink-hw/phase1/probe_console.txt`，结果写进 `docs/hardware/` 和指令级校准报告。
4. 复原：

   ```bash
   tools/hw-probe/flash_probe.py restore --port '/dev/cu.usbmodem*' --dir ../ink-hw/phase1 --backup ../ink-hw/bringup-20261103/private/flash_full_1.bin --approved
   ```

   - 写回出厂 otadata，引导程序回到 ota_0；
   - ota_1 被用到的前 256 KiB 恢复成备份里的内容，也就是擦除状态；
   - 与设备比对，复位回出厂固件。
5. 复原后读一次两处的内容，确认与备份一致。

预计用时：写入约 10 秒，测量约 1 分钟，复原约 10 秒。

## 风险与对策

| 风险 | 对策 |
| --- | --- |
| 测量固件启动失败 | ota_1 起不来时，引导程序会从选中的槽往回试到 ota_0，出厂固件照常启动；otadata 已有有效记录，回退时引导程序不会改写它（代码核查：ESP-IDF v6.1 `bootloader_utility.c` 的 `bootloader_utility_load_boot_image` 与 `set_actual_ota_seq`）。之后按第 4 步复原即可 |
| 测量固件运行中卡死 | 下载模式由芯片 ROM 提供，不依赖应用，esptool 照样能连上复原；必要时按住 BOOT 再复位 |
| 复位落在屏幕刷新中间，高压一直开着 | 复位前等屏幕空闲（与第 0 步相同）；测量固件本身不碰屏幕电源 |
| 4G、充电器等保持出厂固件的状态 | 测量期间不收发数据；复原后出厂固件照常重新初始化 |
| 写入中途断电或断线 | ota_0 和引导程序没有被写，最坏情况是 ota_1 或 otadata 半写；重新执行复原，或用整片备份恢复这两处 |
| 意外写错位置 | 工具只允许两个地址范围；仍有整片备份可恢复 |

整片恢复是最后手段，不在计划内，需要时另行确认：用 `write-flash 0 flash_full_1.bin` 写回整个 16 MiB。

## 执行后要记录

- 硬件报告：内部堆各区段、PSRAM、Flash、复位原因；
- 各成本类的实测周期与指令级模拟器的比值；
- 是否需要按 §5.4 调整 D-35 的预填或行时间；
- 复原结果。
