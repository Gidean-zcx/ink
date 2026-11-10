# hw-selftest：真机自检固件和烧录、复原脚本（只编译、只写脚本，不碰设备）

通用规矩：`ink-codex/briefs/subagent-rules.md`（仓库外，和仓库同级的工作目录）。

用户 2026-11-10 晚上：设备已连上，要“开始进行所有可以自动化的真机相关的测试与调试”，没空配合操作和观察，“别搞挂了就行”。主会话负责真正上机（烧录、复原）；你只写代码和脚本、编译、在没有设备的情况下验证脚本逻辑。**你不能打开串口、不能运行 esptool 连设备、不能读 `ink-hw` 目录。**

## 背景

- 设备：ESP32-S31，USB-Serial/JTAG，16 MB flash，16 MB PSRAM。分区表（出厂）：nvs 0x9000 16K、otadata 0xd000 8K、phy_init 0xf000 4K、model 0x10000 400K、ota_0 0x80000 5M（出厂固件）、ota_1 0x580000 5M（空）、resources 0xa80000 400K、font_data 0xae4000 5M、coredump 0xfe4000 64K。
- 上次（phase1，`docs/hardware/2026-11-03-phase1.md`）的做法验证过：把测试固件写到 ota_1（0x580000），把 otadata 写成选中 ota_1，复位跑测试；复原时写回出厂 otadata、把 ota_1 写回备份里的原内容，两处都和设备比对，复位后出厂固件照常从 ota_0 启动。
- port-hal-1 已写好 S31 端口的 I2C、UART、GPIO 中断和板级接线（`ports/esp32s31/`，`docs/tasks/port-hal-1/REPORT.md` 有 HAL 状态表和上机核对清单）；芯片驱动在 `drivers/chips/*`，板级常量在 `boards/metalio_plus/`。同时有 Codex 在改 `ports/esp32s31/`（port-hal-2），所以你尽量不改端口目录里的现有文件。

## 要做的

1. **自检固件**：一个单独的 ESP-IDF 工程（建议 `tools/hw-selftest/`，参考 `tools/hw-bench/` 的工程写法），复用端口的 HAL 源文件和 `drivers/chips/*`，开机后自动跑一遍硬件检查，在 USB 串口打印一行一个 JSON 的结果，最后打印 `HWTEST_DONE`，然后一直空转、不睡眠（保持 USB 连接）。
2. **安全要求（硬性）**：
   - 第一件事：通过 TCA9555 把屏幕电源（TPS65185 的使能）关掉，并读回扩展芯片的输出寄存器证明已关；读回失败就打印 `SAFETY_FAIL` 并停止后面所有测试（只空转）。
   - 不驱动屏幕；不写存储卡（只检测卡在不在）；不写 NVS 和 flash；不碰 eFuse；Wi-Fi 只扫描、只报数量，不连接、不打印网络名；不配对蓝牙；前光只设为 0。
   - 充电芯片、电量计、RTC、加速度计、触摸只读（不改充电电流等配置）。
   - 4G 模组（NT26，UART2）只发只读的 AT 查询：`AT`、`ATI`、`AT+CGMR`、`AT+CPIN?`、`AT+CSQ`、`AT+CREG?`、`AT+CEREG?`；**不发** `AT+CFUN`、`AT+CMGF` 等任何设置类指令，**不查** IMEI、ICCID、IMSI（`AT+CGSN`、`AT+CCID`、`AT+CIMI` 一律不发）。注意 port-hal-1 报告里说 NT26 上游走的是 UART2 2 Mbps 的“UART 网卡”帧协议，不一定回逐行 AT 文本：收到什么就原样记十六进制前 64 字节，不要反复重试。
   - 每项测试有超时（例如 2 秒），卡住就记失败、往下走；整套测试有总超时。
3. **测试内容**：I2C 扫描（列出应答的地址，对照仓库资料里的 9 颗芯片）；每颗芯片读身份或关键寄存器（触摸芯片号应为 0x64、固件 0x03；电量计电压、电量百分比、电流；充电芯片状态和充电电流读回；RTC 时间是否有效；加速度计身份和 xyz；扩展芯片输入口；TPS65185 版本寄存器能读就读）；GPIO2、GPIO5 电平；PSRAM 大小和一段不破坏数据的读写测试；flash 芯片 ID；CPU 频率；内部 RAM 剩余；简单的计时基准；Wi-Fi 扫描数量。结果里不能有 MAC、序列号、IMEI、网络名这类标识。
4. **烧录、复原脚本**（建议 `tools/hw-selftest/run.py`，Python，用 `esptool` 的命令行）：参数是串口、整片备份文件路径、自检固件路径、输出目录。步骤：①核对备份文件大小（16 MB）并从备份里取出分区表，确认 ota_1、otadata 的位置和上面一致，不一致就停；②从备份里取出 ota_1 原内容和 otadata 原内容存成单独文件；③生成选中 ota_1 的 otadata（按 ESP-IDF 的 otadata 格式，序号在出厂 otadata 的基础上加一，和 phase1 一样），写 ota_1 和 otadata，写后 verify；④复位，抓串口输出直到 `HWTEST_DONE` 或超时（例如 120 秒），存到输出目录；⑤复原：写回原 otadata 和原 ota_1 内容，verify；⑥复位让出厂固件启动，抓启动日志确认 `Loaded app from partition at offset 0x80000`。任何一步出错都直接跳到复原。**绝不**整片擦除，**绝不**写上面两处以外的地址。脚本要有 `--dry-run`：只打印将执行的命令和地址，不连设备。
5. 在报告里写清：固件大小（要小于 5 MB）、每项测试的预期值、脚本的完整步骤和复原步骤。

## 验收

1. 自检固件用 `export IDF_PATH=$HOME/esp/esp-idf IDF_TOOLS_PATH=$HOME/esp/.espressif` 后能编过（照 `tools/hw-bench` 的构建方式），没有新警告。
2. `run.py --dry-run` 用一个你自己造的 16 MB 假备份（分区表按上面的值生成、其余填 0xFF）跑通，打印的地址只有 0xd000 和 0x580000 两处；对 otadata 生成逻辑写单元测试（对照 ESP-IDF 的格式，序号加一、CRC 正确）。
3. 不改模拟器和产品代码的行为；模拟器全量测试不受影响（如果你只加了 tools 下的文件，跑一遍相关的 Python 单元测试即可）。
4. 报告放在最后一条回复里（运行环境不让子代理写报告文件）。
