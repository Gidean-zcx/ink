# port-sleep-vendor：S31 的浅睡、调频和唤醒照原厂做法改

这是 port-hal-2 审查查出问题的修复，不是新功能。总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先，**第 9 节是这个任务的核心**。报告写到 docs/tasks/port-sleep-vendor/REPORT.md。只编译和跑宿主机测试，不碰设备。

## 背景

port-hal-2 已合入（`ports/esp32s31/hal_power_s31.c`、`s31_board.c`、`s31_input_logic.h` 等）。拿它对照原厂固件（`ink-hw/upstream/Metalio-E-INK4-Plus`，提交 690a023，MIT），查出下面几处和原厂不一致，真机上很可能出问题。11-10 的上机自检第二、三轮直接调 IDF 浅睡，醒后没有输出、USB 不回来，见 `docs/hardware/2026-11-10-selftest.md`。原厂的总说明在 `main/boards/common/power_policy/README.md`，先读完。

1. **S31 浅睡补丁**：原厂给 IDF 浅睡包了一层，在 `main/boards/common/power_policy/s31_sleep/`（`s31_sleep.cc`、`s31_sleep.h`、`s31_sleep_sdk.cmake`）。入睡前把 PSRAM 时钟交给晶振，并按 CPU retention 条件处理 MSPI 时钟；醒后先恢复 MPLL/MSPI，再切回。醒后做 MPLL 校准前，要先恢复 DREG/PVT 前置条件、补模拟初始化。说明见 README 末节。我们的 `hal_power_sleep` 是直接调 `esp_light_sleep_start()`。
2. **醒后把 USB 串口叫回来**：原厂 `power_hw.cc` 的 `ReviveUsjAfterSdRestore` 依次做：开总线时钟、断开 pad、等 150 ms、恢复 PHY 默认值、接上 pad、最多等 800 ms 看主机连上。这一步在 SD 恢复之后做，只在非定时唤醒时做。我们的控制台只走 USB（UART0 的默认引脚是 4G 模组的 UART2），醒后不恢复，就再也看不到日志。
3. **调频**：原厂用 `esp_pm_configure` 设固定频率（最低 = 最高，`power_hw.cc` 的 `ApplyCpuFreq`），不在两个频率之间动态切。我们是最低 160、最高 320，靠锁动态切换。
4. **唤醒源**：原厂只用电源键和定时。电源键的链路是 P17 → 扩展芯片 INT → GPIO2，EXT1 低电平。BOOT 键 GPIO61 叫不醒浅睡：S31 只有 GPIO0～7 是 LP 引脚，见 IDF `components/soc/esp32s31/include/soc/soc_caps.h` 的 `SOC_RTCIO_PIN_COUNT`。我们给 GPIO61 配了 GPIO 唤醒。
5. **扩展芯片的睡眠配置**：照 README 里“TCA9555：端口方向、锁存与电源控制”那一节。
   - 正常态的输入掩码：Port0 0xC0，Port1 0xE3。
   - mask 阶段：先写安全的输出锁存（P10/P11 输出 1；P06/P15/P16/P07/P13 输出 0），再把两个端口都设成输出，最后只把唤醒用的输入改回输入。
   - 屏幕 PMIC 休眠时序：P00/P01 拉低，等 50 ms，P02 拉低，再等 120 ms。
   - 共享电源断开顺序 P05 → P14，恢复顺序 P14 → P05。
   - 醒后先锁存 P17，再恢复按键方向。

## 要做的

预算：我们自己的产品代码不超过 250 行。照搬的原厂补丁不算在内，但要放在单独的目录（例如 `ports/esp32s31/vendor/s31_sleep/`），保留 MIT 版权声明，写明来源仓库和提交号。

- 移植 s31_sleep 补丁，cmake 部分也要在我们的 S31 构建里生效。IDF 版本不一致、锚点对不上时，构建要失败并说明原因，不许静默跳过。
- 加 USB 串口恢复，放在醒后恢复流程里合适的位置。我们没有原厂那套 SD/CNNT 复用，说明放在哪一步、为什么。
- 调频改成和原厂一样设固定频率：活跃和空闲时整体切换，不靠锁动态切。频率选择写清理由。
- 唤醒源照原厂对齐：
  - 删掉 GPIO61 唤醒；除非找到 IDF 的依据证明它可行，并写出处。
  - 我们有两处和原厂不同：P16 RTC 唤醒（待机时闹钟要能响）、加速度计 INT1 运动唤醒（D-121 拿起亮屏）。两处保留，但要照原厂的规矩做：睡前确认共享 INT 已经空闲为高，不是高就取消这次浅睡并回退。
- 扩展芯片的睡眠方向、锁存，以及屏幕 PMIC 休眠时序，照原厂对齐。
- 端口单测（`ports/esp32s31/tests/`）补上对应的顺序断言，反向对照照旧做。

## 验收

1. S31 能编过（`ports/esp32s31/build.sh`），0 警告；端口单测的普通版和 ASan 版都通过。
2. 反向对照，下面四种情况各自要有测试失败：
   - 撤掉 USB 恢复；
   - 撤掉补丁调用；
   - 改回动态调频；
   - 加回 GPIO61 唤醒。
3. 报告里有一张对照表：原厂的每一步（README 里进入、退出浅睡的步骤，TCA 表，唤醒链路，调频）→ 我们对应的代码行 → 一致还是不一致，不一致写理由。原厂出处写到文件和行号。
4. 模拟器全量测试照旧通过（这个任务不该影响模拟器）；gcccheck 通过；做隐私扫描。
5. 更新“上机先核对清单”，写明哪些只有上机才能确认。
