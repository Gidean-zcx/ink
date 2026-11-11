# fw-size-1：S31 固件瘦身（不改功能，至少减 300 KB）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7 条；第 8 条不碰设备）。

现状（主会话 2026-11-12 02:00 量的，`build-s31/ink-s31.map`）：`ink-s31.bin` 4,838,336 字节，5 MiB 应用分区只剩 404,544 字节（8%），后面还要合入音乐（约 +170 KB）、锁屏、书签等。最大的几块：

- `libink_platform.a:ink-embedded.bin.S.obj` 约 1538 KB（编进固件的资源包：图标等）；
- `libesp_stdio.a:stdio_vfs.c.obj` 约 333 KB（数字可疑，先核实是不是别的只读数据被算到这里）；
- `libmesh.a` 约 230 KB（Wi-Fi mesh，我们不用）；
- 解压库重复：阅读器带了 miniz（`third_party/reader_miniz`），传书带了一份 zlib（`ports/esp32s31/vendor/ap_transfer/zlib/`），芯片 ROM 里还有 miniz/tinfl；
- 编译优化是 `CONFIG_COMPILER_OPTIMIZATION_PERF`（-O2）。

## 要做的

按“收益大、风险小”的顺序逐项做，每一项单独提交、单独量一次固件大小：

1. **核实大块**：`stdio_vfs.c.obj` 那 333 KB 到底是什么；`ink-embedded.bin` 里有哪些资源、各多大，有没有重复或用不到的（例如同一图标多种尺寸里有从没被调用的尺寸——用源码里实际用到的名字和尺寸核对，`system/ui/icons.c` 按名字和尺寸取）。
2. **去掉用不到的**：`libmesh` 能否通过 sdkconfig 或链接选项去掉（查 ESP-IDF v6.1 的选项；不能就说明原因）；资源包里确认没人用的图标尺寸不打包；LVGL 里我们没用到的控件和功能在 `system/ui/lv_conf.h` 关掉（逐个用编译和测试确认没人用）。
3. **合并解压库**：传书那份 zlib 是做什么用的（网页本身是预先压缩好的）；能用阅读器已有的 miniz 或 ROM 的 tinfl 代替就代替，只留一份。
4. **按组件改优化级别**：LVGL、应用、系统界面这些不卡时序的代码改用 `-Os`（在各组件的 CMake 里加，不改全局），屏幕传输、4G、声音、I2C 等驱动和中断相关代码保持原样。改前后在宿主机跑相关测试，报告写清每个组件省了多少。
5. 资源包挪到字库分区（`font_data` 分区里日志环之前还有多少空间）只做评估，写方案和收益，**不在本任务实施**（要改安装工具和分区用法，主会话来定）。

## 范围和预算

- 只改：`ports/esp32s31/main/CMakeLists.txt` 和 `ports/esp32s31/` 下的组件 CMake、`ports/esp32s31/sdkconfig.defaults`（只关不用的功能，不改 Flash、分区、安全、Wi-Fi 和 4G 的参数）、`system/ui/lv_conf.h`（只关没用的）、资源打包脚本里选择图标尺寸的那一处、传书 vendor 目录里 zlib 的接法、`docs/tasks/fw-size-1/`。不改产品功能代码；不碰别的任务在改的文件。
- 不许为了变小而删功能、关日志脱敏、关断言或改刷新、电源、4G 的行为。

## 验收

1. 每一项的前后固件大小表（字节），合计至少减 300 KB；做不到就交实测数据和下一步方案。
2. 主工程普通全量 ctest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。
3. 报告写“上机核对清单”：主会话在真机上看开机、刷新耗时、4G、声音、传书网页和阅读器翻页是否和瘦身前一样。
