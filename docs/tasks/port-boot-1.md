# port-boot-1：真机开机进完整系统，并带上安全网

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-boot-1/REPORT.md。**只编译、跑宿主机测试，不碰设备**（上机由 Claude 做）。

## 背景

用户 2026-11-10 晚上：“我希望明天早上你能给我一个可以初步刷入使用的版本 我会在实际使用体验中看看有哪些问题”。现在 `ports/esp32s31/main/main.c` 只做自检和板级初始化，屏幕刚接上（port-display-1），还没真正跑过完整系统。今晚另有几个任务同时做：port-storage-1（资源、`/user`、存储卡）、log-1（日志）、port-audio-1、port-wifi-1。它们也会改 `ports/esp32s31/CMakeLists.txt`、`hal_stubs.c`、`sdkconfig.defaults`、`build.sh`，你只做自己的部分，改动集中、少碰别处。

闪存用途见 `docs/hardware/flash-layout.md`，硬规则：不写 nvs、ota_0、resources、phy_init、引导程序和分区表。

## 要做的

预算：产品代码净增不超过 250 行。

1. **开机进完整系统**：`app_main` 自检、板级初始化后进入 `ink_main()`，系统、界面、应用都起来。核对所有任务的栈、优先级、核、内存位置（内部内存还是 PSRAM）在 S31 上合理：栈放 PSRAM 的任务不能碰闪存写（ESP-IDF 规则），找出来改掉或写明。任务看门狗、中断看门狗的设置照原厂 sdkconfig，写出处。
2. **试用版不进浅睡**：浅睡还没在真机上验证，用户说过不在他不在场时测睡眠。加一个编译开关（例如 `INK_S31_LIGHT_SLEEP`，默认关）：关时电源服务照常灭屏、降频，但不调浅睡，USB 一直在。模拟器行为不变。
3. **开机反复崩溃自动回出厂**：用 RTC 不掉电内存（`RTC_NOINIT_ATTR`，带校验）计数，连续 3 次在开机 60 秒内因异常或看门狗复位，就把启动分区改回 ota_0（`esp_ota_set_boot_partition`）再重启。运行满 60 秒清零。上电复位时这块内存不可信，从 0 开始，这个缺口写进报告。这是给用户的安全网：我们的固件坏了，设备自己回到出厂固件。改启动分区前打一行日志说明原因。
4. **出事时先关屏电**：异常处理和关机路径（`esp_register_shutdown_handler`、panic 钩子）里先停扫描、关面板电源，照 `hal_display_s31.c` 的 `trip()` 顺序。
5. **设备信息**：`hal_device_reset_reason` 返回真实的复位原因（`esp_reset_reason`），`hal_device_get_info` 填真实的板名、版本；版本号带 git 短提交号和编译时间，开机打一行。
6. 端口 README 写清怎么编出试用版。

## 验收

1. S31 能编过，0 警告；端口单测普通版和 ASan 版都通过；回出厂计数和“不进浅睡”开关有宿主机单测，并做反向对照。
2. 模拟器全量测试照旧通过；gcccheck 通过；做隐私扫描。
3. 报告里列出每个任务的栈、核、内存位置，和原厂的对照；写清哪些只能上机才知道。
