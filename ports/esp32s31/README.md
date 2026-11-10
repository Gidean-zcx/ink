# ESP32-S31 编译骨架

只构建，不提供连接设备、烧录或监视命令。系统、应用、驱动和 LVGL 分成独立构建目标，系统与应用不包含模拟器头文件。

使用已安装的 ESP-IDF v6.1 与 S31 工具链：

```bash
export IDF_PATH="$HOME/esp/esp-idf"
export IDF_TOOLS_PATH="$HOME/esp/.espressif"
# macOS；Linux 去掉 taskpolicy 前缀
taskpolicy -c utility bash ports/esp32s31/build.sh
```

`INK_DEPS` 已设置时使用其 `build/_deps/lvgl-src`（v9.5.0）；也可指定 `INK_LVGL_SOURCE_DIR`。未提供本地源码时下载同版本到 `build-s31/lvgl`。不修改依赖源码、已安装 IDF 或工具目录。临时文件、组件缓存、配置、镜像和报告都在本仓库的 `build-s31/`。脚本每次从提交的 `sdkconfig.defaults` 生成配置，覆盖该目录中的临时 `sdkconfig`，保证本地和 CI 使用相同默认值。

- `size.txt`、`size-components.txt`：IDF 静态尺寸，动态任务栈和堆另计。
- `stack.txt`：GCC `-fstack-usage` 与 `-fcallgraph-info=su` 的入口帧、已知直接调用链。缺失的库函数、间接回调和递归不计，不能当作安全栈上限。
- `osal_selftest.c`：开机检查内存能力、队列、互斥、信号量、PSRAM 栈、定时器和微秒等待。本任务只编译它，未上机执行。

入口自检后接板级芯片（`s31_board.c`：扩展芯片、充电、电量计、时钟、加速度计、前光、触摸，顺序同模拟器），再检查屏幕；桩报告无屏幕时记录状态并返回，不启动完整系统。I2C（`hal_i2c_s31.c`）、串口（`hal_uart_s31.c`）、GPIO 中断（`s31_irq.c`）用 IDF 驱动实现，引脚在 `s31_board.h`，都还没上机。`hal_input_s31.c` 在输入任务中去抖、读 FT6336U、分发按键和触摸，并操作马达；`hal_power_s31.c` 接 CPU 频率锁、浅睡板级顺序、唤醒分类、关机和重启。屏幕总线与 TF 卡的停启仍待各自端口接入，不能据此认定整板浅睡可用。其余 HAL 在 `hal_stubs.c`；字体、资源、存储、通信和屏幕驱动尚未接入。

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
# 内存检查：目录换 build-s31-host-asan，配置加 -DINK_SANITIZE=ON。
```

测试运行实际的纯逻辑头文件和电源 HAL；IDF 与板级硬件在电源测试中由记录调用顺序的假实现替代。反向对照在构建目录的临时副本上撤掉修复，不依赖 Git 历史。状态、出处与上机清单见 [port-hal-2 报告](../../docs/tasks/port-hal-2/REPORT.md)。
