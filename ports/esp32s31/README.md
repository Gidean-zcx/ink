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

入口自检后检查屏幕；桩报告无屏幕时记录状态并返回，不启动完整系统。HAL 均标注后续驱动来源，返回不可用或失败；日志和随机数用 IDF 的实现。字体、资源、存储、通信和屏幕驱动尚未接入。

主机换算测试由根工程的 `unit-s31-osal-ticks` 运行；可移植性修复的正反编译对照：

```bash
python3 tests/esp32s31/portability.py build-s31
```

本次证据与限制见 [任务报告](../../docs/tasks/port-skeleton/REPORT.md)。
