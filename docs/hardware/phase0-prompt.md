# 第 0 步提示词：只读识别与整片备份（交给本机 Codex）

2026-11-03 交给用户 Mac 上的 Codex CLI 执行。后来项目转给本机 Claude Code，同日由它按本提示词执行完毕，结果和出入见 [2026-11-03-phase0.md](2026-11-03-phase0.md)。下文保留原样。

**运行方式**：在终端执行下面的命令，再把提示词贴进 Codex。

```bash
mkdir -p ~/work/ink-hw && cd ~/work/ink-hw && codex
```

保持逐条确认，不用全自动模式。

**结果**：放在 `~/work/ink-hw/bringup-<日期>/`，只留在用户电脑上。脱敏摘要整理后记入 [实机记录](README.md)。

```
你在用户的 Mac（Apple Silicon）上工作。电脑通过 USB 连着一台 CloudZao Metalio E-Ink4 Plus（ESP32-S31，4.7 英寸墨水屏，带 4G），里面是出厂固件。它是开源项目 Gidean-zcx/ink 的目标硬件（背景见该仓库 docs/background.md，可选）。

本次目标：不改动设备任何内容，完成硬件识别、整片 Flash 备份、出厂固件日志采集，整理成摘要。之后是否写入固件由用户另行决定。

【绝对规则】
1. 对设备只允许读操作：chip-id、flash-id、read-mac、get-security-info、read-flash、verify-flash、espefuse summary。禁止 write-flash、erase-flash、erase-region、write-mem、任何 espefuse burn-*、espsecure、idf.py flash、OTA、写 NVS、恢复出厂。
2. 需要写入，或结果出乎意料时，立即停下，说明现象和建议，等用户决定。出乎意料包括：找不到设备、Flash 加密或安全启动已开启、两次读出不一致、esptool 报错。
3. 需要按键、插拔、操作屏幕时，告诉用户具体做什么，等用户回复“好了”再继续。
4. 备份和原始日志可能含 Wi-Fi 密码、设备令牌、IMEI/ICCID、MAC，只存在本机：绝不上传，绝不提交到任何 git 仓库（项目仓库是公开的）。
5. 执行过的每条命令和完整输出都写进日志文件。

【产出位置】~/work/ink-hw/bringup-YYYYMMDD/（今天的本地日期）
- private/：整片备份、原始日志等敏感内容；
- share/：脱敏后的日志和 SUMMARY.md（中文摘要）。MAC、IMEI、ICCID、序列号、Wi-Fi 名称与密码、令牌替换成 <redacted>，只保留末 2 位用于区分。

【步骤】
1. 环境：在工作目录建 Python 虚拟环境，装最新 esptool（必须 5.3 或更新：5.3 起才有 ESP32-S31 的 stub 刷写器，并修了它的 Flash 命令）和 pyserial。不要用 PlatformIO 自带的旧版。记录 esptool、Python、macOS 版本。
2. 找设备：
   - 请用户开机，停在主页，不要操作；线必须是数据线。
   - 用 ls /dev/cu.* 和 system_profiler SPUSBDataType 找出设备，记录 VID:PID 和产品名，判断是 S31 自带的 USB 串口/JTAG（Espressif VID 0x303A，通常是 /dev/cu.usbmodem*）还是外接 USB 转串口芯片。设备若同时以磁盘出现（出厂固件可把 TF 卡挂成 U 盘），记下来。
   - 找不到时，先换线、换口；再不行，请用户进下载模式：按住 BOOT，短按复位或电源键（或按住 BOOT 时重新插线），松开 BOOT。
3. 只读识别（esptool v5 写法，子命令用连字符）：
   - esptool --chip esp32s31 -p <端口> chip-id；不认 esp32s31 时用 --chip auto 并记录。
   - flash-id：记录厂商/器件 ID、容量，查出具体型号及它是否支持擦写挂起（suspend）。
   - read-mac。
   - get-security-info：重点。记录 Flash 加密、安全启动、下载模式是否被禁用。
   - espefuse -p <端口> summary：只读，全文存 private/，把芯片版本、封装/PSRAM 相关字段、安全相关字段摘到摘要。
   - 每次用 --after hard-reset 让设备回到出厂固件，并确认它正常运行。
4. 整片备份：
   - read-flash 0 ALL（或按 flash-id 的容量）读到 private/flash_full_1.bin，再读一遍到 flash_full_2.bin，两份 SHA-256 必须一致。
   - 再用 verify-flash 0 private/flash_full_1.bin 与设备比对。
   - 一致后，问用户第二个存放位置（如移动硬盘；除非用户同意，不要放会同步到云端的位置），复制过去，并核对 SHA-256。
5. 离线分析备份（只处理文件，不碰设备）：
   - 解析分区表。上游源码说 bootloader 在 0x2000、分区表在 0x8000，用实际数据核对。列出名称、类型、子类型、偏移、大小，并与上游 CloudZao/Metalio-E-INK4-Plus 提交 690a023 的 partitions.csv 对照。
   - 切出每个应用分区，跑 esptool image-info，并解析应用描述（esp_app_desc_t，魔数 0xABCD5432）：项目名、版本、ESP-IDF 版本、编译时间。查清 otadata 指向哪个槽。
   - NVS 只列命名空间和键名，share/ 里不写任何值。
6. 出厂固件日志：
   - 写一个小脚本：打开串口（先用 115200），每行带时间戳写文件；芯片复位时 USB 串口会短暂消失，断开要自动重连。
   - 让芯片复位（esptool --after hard-reset read-mac，或请用户按复位），至少抓 30 秒，存 private/bootlog_1.txt。关注：芯片版本、Flash 容量与模式、PSRAM 识别与容量（esp_psram 行）、heap_init 列出的各段内部 RAM、应用版本、启动耗时。
   - 然后请用户操作约 2 分钟：翻几页书、回主页、打开设置、等它待机再唤醒。同时记录到 private/runtime_1.txt，并记下每步操作的时间点。关注刷新时序（EpdTiming、每相、波形、刷新耗时）、电池、4G 模组型号与固件、内存统计。
   - 如果没有任何输出，可以尝试别的读法（其它波特率、日志是否走别的接口），但不得为此写入设备，如实记录即可。
7. TF 卡（可选，只读）：若出现为磁盘，只读列目录，不改、不删、不新建。先报告大小，用户同意后再完整复制到 private/sdcard_backup/。metalio/e-ink/logs/ 可复制一份供分析。
8. 请用户拍下出厂固件“关于本机/系统信息”页面，以及机身、包装上的型号和硬件版本标签。不要拆机。
9. 汇总 share/SUMMARY.md（中文，脱敏），依次写：
   1) 环境与工具版本；
   2) USB 接口类型与 VID:PID；
   3) 芯片版本，Flash 厂商/型号/容量，PSRAM 容量及来源，安全状态；
   4) 分区表与固件版本，当前启动的槽；
   5) 启动日志要点：内部 RAM 各段、启动耗时；
   6) 运行日志中与刷新、电源、4G 有关的内容，附用户操作时间线；
   7) 备份文件清单、SHA-256、存放位置；
   8) 异常与未完成项；
   9) 下一步建议（只建议，不执行）。
   share/ 里同时放各日志的脱敏版。最后告诉用户工作目录路径、摘要要点，以及哪些事需要用户决定。

【不要做】写设备、升级或恢复固件、改设备设置（用户自己的日常操作除外）、git 提交、上传任何文件。
```
