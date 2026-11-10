# port-display-1：S31 的屏幕输出层接原厂现成代码（只编译）

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-display-1/REPORT.md。只编译和跑宿主机测试，不碰设备。

## 背景

D-124 定了先用现成的：屏幕输出层用原厂固件里的 EPDiy 代码，构建时从上游取，不放进仓库，外面包我们的 `hal_display`。许可证是 LGPL，以后发布固件时有额外义务，记进报告。

11-06 的测量固件就是这样做的，照它来：
- `tools/hw-bench/main/CMakeLists.txt`：从本机副本 `$INK_UPSTREAM` 编译原厂的 `display/`（output_lcd、output_common、waveforms、epdiy、highlevel、metalio_epd 等）和 `bus/`（metalio_i2c1、pca9555、tps65185）；
- `build.sh lab`：在这一层上面跑我们自己的刷新算法 `drivers/display/pqbn/pqbn_v4.c`。真屏上翻一页约 0.52 秒（`docs/hardware/2026-11-06-phase2.md`、`2026-11-06-bench*.md`）。

我们端口里的 `hal_display_*` 现在全是空实现（`ports/esp32s31/hal_stubs.c` 第 91～94 行）。HAL 合同在 `platform/include/ink/hal_display.h`，模拟器的实现 `ports/sim/hal/` 可以当参照。

## 要做的

预算：我们自己的产品代码不超过 350 行，原厂代码不算（它不进仓库）。

1. **取原厂代码**：S31 构建时取 CloudZao/Metalio-E-INK4-Plus 固定的提交 690a023：
   - 设了 `INK_UPSTREAM` 就用本机副本；
   - 没设就在构建目录里取公开仓库，核对提交号；
   - 取不到时构建照样能过：屏幕回到现在的空实现，构建输出里明确说“没有屏幕输出层”。
   CI 的 S31 构建要能编进真实的输出层（CI 能联网），写清 CI 改了什么。
2. **`hal_display` 实现**：放在 `ports/esp32s31/hal_display_s31.c`，用原厂的输出层送行、管屏幕电源；波形和灰度换算用我们的 pqbn 刷新算法，照 hw-bench 的 lab 构建。合同里的每个函数都要实现：
   - `get_info`、`level_luminance`、`update` 加完成回调、`pending`；
   - 如果还有休眠和唤醒的接口，屏幕电源芯片的时序照原厂 `epd_board_metalio_eink4_plus.c`，出处写到行号。
3. **安全**：项目规则第 5 条要求，驱动屏幕之前，“卡住就关屏幕电源”这类保护要先在真机上证明管用。这次只编译，但代码里要有这套保护：刷新超时就关掉屏幕电源芯片。并写清上机时怎么先单独证明它。
4. 和 port-hal-2、port-sleep-vendor 用的扩展芯片和 I2C 是同一套。不能出现两份 I2C 或扩展芯片驱动：原厂 `bus/` 和我们端口里已有的驱动（port-hal-1）重复的地方，写清用哪一份、为什么，不要两份同时在跑。

## 验收

1. 设了 `INK_UPSTREAM` 时 S31 能编过，0 警告；镜像大小的增量写进报告。不设、并且离线时，S31 也能编过（回到空实现）。
2. 端口单测能测的部分（区域拼接、灰度换算、完成回调的线程、超时保护）有宿主机测试，并做反向对照。
3. 模拟器全量测试照旧通过；gcccheck 通过；隐私扫描。
4. 报告写清：LGPL 的义务、哪些只有上机才能确认（第一次上机的步骤和复原方法），还有和原厂做法不一致的地方。
