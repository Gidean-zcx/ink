# ESP32-S31 试用版编译

只构建，不提供连接设备、烧录或监视命令。系统、应用、驱动和 LVGL 分成独立构建目标，系统与应用不包含模拟器头文件。

使用已安装的 ESP-IDF v6.1 与 S31 工具链：

```bash
export IDF_PATH="$HOME/esp/esp-idf"
export IDF_TOOLS_PATH="$HOME/esp/.espressif"
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
# macOS；Linux 去掉 taskpolicy 前缀
INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh
```

`INK_DEPS` 已设置时使用其 `build/_deps/lvgl-src`（v9.5.0）；也可指定 `INK_LVGL_SOURCE_DIR`。未提供本地源码时下载同版本到 `build-s31/lvgl`。不修改依赖源码、已安装 IDF 或工具目录。临时文件、组件缓存、配置、镜像和报告都在本仓库的 `build-s31/`。脚本每次从提交的 `sdkconfig.defaults` 生成配置，覆盖该目录中的临时 `sdkconfig`，保证本地和 CI 使用相同默认值。

编译用 `partitions-trial.csv` 逐项保持原厂 `690a023` 的 `partitions.csv`，两个应用槽各 5 MiB；不再使用骨架的 1500 KiB 单应用尺寸。它用于 SDK 编译和尺寸检查，设备继续保留原厂分区表，不写生成的分区表或引导程序。

- `size.txt`、`size-components.txt`：IDF 静态尺寸，动态任务栈和堆另计。
- `stack.txt`：GCC `-fstack-usage` 与 `-fcallgraph-info=su` 的入口帧、已知直接调用链。缺失的库函数、间接回调和递归不计，不能当作安全栈上限。
- `osal_selftest.c`：开机检查内存能力、队列、互斥、信号量、PSRAM 栈、定时器和微秒等待。本任务只编译它，未上机执行。

入口先建立 RTC 启动保护并记录版本、复位原因，再自检、初始化板级芯片和屏幕，随后执行 `ink_main()`，启动系统服务、UI 和应用。没有屏幕则报告启动异常。I2C、串口、GPIO、输入和屏幕使用已接入的端口；存储、资源、网络、音频仍依赖对应并行任务合入，不能把本分支编过当作整机已可使用。

默认 `INK_S31_LIGHT_SLEEP=OFF`：锁屏、前光关闭和 CPU 降至 160 MHz 仍走原服务，HAL 拒绝浅睡，不切睡眠用的共享电源，不重置 USB。直接用 IDF/CMake 配置时对应 `-DINK_S31_LIGHT_SLEEP=OFF`；`ON` 仅保留给有人在场的后续验证，当前不提供上机步骤。模拟器不受此开关影响。

版本格式是 `g<8 位 Git 提交号>-<UTC 年月日时分秒，年份两位>`，例如 `g12345678-261110123456`；保存在 IDF 应用描述中，开机打印，设备信息页通过已有 `hal_ota_info()` 读取。板名为 CloudZao E-Ink4 Plus (ESP32-S31)。中断看门狗 800 ms，任务看门狗 20 秒、不开 panic，与原厂 `690a023` 的 `sdkconfig:2443–2458` 一致。

连续三次在启动后 60 秒内发生 panic、看门狗或 CPU lockup 复位，下一次启动选择原厂 `ota_0` 并重启；只写 `otadata`，不改应用分区。运行满 60 秒撤销本次启动标记并清零。RTC 记录带校验，但断电/上电会清零，不能覆盖断电式反复开机。原厂分区表必须保留且能找到有效 `ota_0`；本脚本仅生成镜像，不写入分区表或设备。回原厂只恢复启动槽，恢复原厂字库仍见[闪存用途](../../docs/hardware/flash-layout.md)。

正常关机/重启调用显示端口的 `trip()`；panic 经链接包装先停 LCD、拉低 XOE/MODE，再用不依赖调度的 GPIO I2C 写 TCA9555，按 VCOM_CTRL/PWRUP → 10 ms → WAKEUP 顺序关 PMIC，随后交还 IDF panic。I2C 断线或被拉死时不能保证电源已经关闭，必须上机核对。

主机换算测试由根工程的 `unit-s31-osal-ticks`、`unit-s31-hal-map`（I2C 错误码映射、串口分行）运行；可移植性修复的正反编译对照：

```bash
python3 tests/esp32s31/portability.py build-s31
```

本次证据与限制见 [任务报告](../../docs/tasks/port-skeleton/REPORT.md)。

输入、电源的独立宿主机测试（不修改模拟器构建，也不连接设备）：

```bash
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host
taskpolicy -c utility python3 ports/esp32s31/tests/boot_negative.py build-s31-host
taskpolicy -c utility python3 ports/esp32s31/tests/partitions.py "$IDF_PATH" "$INK_UPSTREAM/partitions.csv" build-s31
# 内存检查：目录换 build-s31-host-asan，配置加 -DINK_SANITIZE=ON。
```

测试运行实际的纯逻辑头文件和电源 HAL；IDF 与板级硬件在电源测试中由记录调用顺序的假实现替代。反向对照在构建目录的临时副本上撤掉修复，不依赖 Git 历史。分区测试读取两份 CSV 和已编译镜像，核对原厂全部条目，并使用 SDK 尺寸检查器验证恢复骨架布局后会出现警告。状态、出处与上机清单见 [port-hal-2 报告](../../docs/tasks/port-hal-2/REPORT.md)。

启动、安全停屏的验收、任务栈和上机边界见 [port-boot-1 报告](../../docs/tasks/port-boot-1/REPORT.md)。
