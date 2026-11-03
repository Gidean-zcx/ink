# tools/hw-probe：第 1 步测量固件

在 Metalio E-Ink4 Plus 实机上跑指令级模拟器（`tools/iss`）用过的同一批校准内核，测出每个成本类的周期数，与 `tools/iss/out/costs.json` 逐项对比。重点是墨水屏送行（`k_feed.c`），它决定 D-35。顺带报告硬件信息：芯片、主频、PSRAM、Flash、复位原因、当前槽、内部/DMA/PSRAM 各堆区段。

固件只做这些：不初始化 Wi-Fi、蓝牙和 PHY，不碰 NVS 和 I2C，屏幕电源、4G、按键都保持出厂固件留下的状态；核心转储关闭，所以运行时不写 Flash。写入方案与复原步骤见 [docs/hardware/phase1-plan.md](../../docs/hardware/phase1-plan.md)，**写设备前必须得到用户同意**。

## 构建

```bash
tools/hw-probe/fetch_deps.sh            # tinf、miniz，版本与 calibrate.sh 相同
export IDF_TOOLS_PATH=~/esp/.espressif && . ~/esp/esp-idf/export.sh
cd tools/hw-probe && idf.py --preview set-target esp32s31 && idf.py build
```

- 配置照搬出厂固件（上游 sdkconfig @690a023），只改三处：`-O2`（指令级校准的主要变体，上游是 `-Os`）、关核心转储、出错即停机。
- 可复现构建：同样的源码得到同样的镜像，便于在写入方案里核对 SHA-256。
- `partitions.csv` 与出厂分区表逐字节一致（已比对）。

**不要用 `idf.py flash`**：它会写引导程序、分区表，并把应用写进 ota_0，覆盖出厂固件。

## 文件

| 文件 | 作用 |
| --- | --- |
| `main/hw_probe.c` | 报告硬件，在 1 核上依次跑送行内核和其余校准内核 |
| `main/iss_rt.h`、`main/iss_rt_idf.c` | `iss_rt` 接口的 ESP-IDF 实现：周期计数器计时，`esp_cache_msync` 写回/作废 PSRAM 缓冲，`heap_caps_aligned_alloc` 分配 |
| `make_otadata.py` | 由设备当前的 otadata 生成选中某个槽的 otadata（保留原记录，另一扇区写更高的序号） |
| `flash_probe.py` | `install` / `restore`：只允许写 otadata 与 ota_1，写前等屏幕空闲，写后与设备比对，需要 `--approved` |
| `compare.py` | 从串口日志取出各区段周期数，与指令级模拟器的拟合值、慢角、理想值并列 |

与模拟器的差异：`ISS_CACHE_INV_I` 在实机上不做（代码留在 I-cache，带 `#cold` 的区段实际是热的）；只对 `iss_psram_alloc` 分配的 PSRAM 块做写回与作废（内部 SRAM 不经缓存）。
