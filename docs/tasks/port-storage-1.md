# port-storage-1：真机上的字体图标、`/user` 和存储卡

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-storage-1/REPORT.md。**只编译、跑宿主机测试，不碰设备**（上机由 Claude 做）。

## 背景

用户要明天早上能刷进去试用的版本（见 port-boot-1 的背景）。S31 端口里资源（`hal_asset_*`）和文件（`hal_fs_*`）都还是桩，界面没有字。今晚并行的还有 port-boot-1、log-1、port-audio-1、port-wifi-1，共用 `ports/esp32s31/CMakeLists.txt`、`hal_stubs.c`、`sdkconfig.defaults`、`build.sh`，你只删自己的桩，改动集中。

闪存用途已经定了，见 `docs/hardware/flash-layout.md`，照它做；硬规则：不写 nvs、ota_0、resources、phy_init、引导程序和分区表。

## 要做的

预算：产品代码净增不超过 350 行（打包工具另算）。

1. **资源**：写一个打包工具，把模拟器用的同一套资源（`build/assets` 的字体、图标等，`tools/ui_assets_check.py` 查的那些）打成两个格式相同的包：先按顺序装进 font_data 的前 4.5 MiB，装不下的进内嵌在程序里的第二个包（随程序映射）。`hal_asset_map` 按名字查两个包，用 `esp_partition_mmap` 映射；先核对 S31 的闪存映射地址空间够不够（和 16 MB PSRAM、程序本身一起），不够就按资源单独映射并计数，写明依据。`build.sh` 产出 `build-s31/ink-fonts.bin`（写到 0xae4000）。总量超了要编译失败并说清超多少。模拟器的资源预算检查改成和这个布局一致（两个区各自的上限）。
2. **`/user`**：用仓库里同一份 LittleFS（`third_party/littlefs`，模拟器在用），块设备接 `model` 分区（400 KiB，4 KiB 块）。第一次挂不上就格式化（这个分区在出厂固件里是空的）。模拟器的设备档案 `user_partition_kib` 改成真实的 400，如果因此有测试失败，照实写进报告，不要为了过测试放宽。
3. **存储卡**：`/content`、`/cache` 照模拟器的路由，挂 FAT。引脚、总线（SDMMC 还是 SPI、几线）、插卡检测照原厂代码，写出处。加编译开关让卡只读挂载（上机测试时 Claude 用，避免改用户卡上的东西），默认可写。`hal_fs_card_present`、换卡回调、`hal_fs_volume_info` 要真实。
4. **写闪存时的约束**：闪存写会停缓存，照现有存储服务的写入时机（D-36：放声音时不写），不要另开写入路径。
5. **刷机和回出厂脚本**（只生成命令和核对，不在任务里执行）：`tools/device/flash_trial.sh` 写 ota_1、font_data、选 ota_1 的 otadata，写前检查整片备份存在且核对过，写后逐段比对；`tools/device/restore_factory.sh` 从备份写回 otadata、model、font_data 并比对。备份路径从环境变量读，不写进仓库。

## 验收

1. S31 能编过，0 警告；端口单测普通版和 ASan 版都通过；打包工具和两个包的查找有宿主机单测，包括超预算失败、名字查不到；做反向对照。
2. 模拟器全量测试照旧通过（档案改 400 KiB 后的失败照实报告）；gcccheck 通过；做隐私扫描。
3. 报告里写清：资源两个包各多大、余量多少；`/user` 实际可用多大；存储卡的出处；脚本怎么用、怎么回出厂。
