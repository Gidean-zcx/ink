# port-skeleton：ports/esp32s31 骨架，只编译、不烧录

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/port-skeleton/REPORT.md。

## 要做的

docs/system/platform-and-simulator.md 第 7 节写了真机的做法，第 8 节最后一条是：“`ports/esp32s31` 骨架，在 CI 中至少编译通过”。这次做的就是这一条。预算 600 行端口代码（不含生成文件），最多两小时。

- `ports/esp32s31`：一个 ESP-IDF 工程（目标 esp32s31），把 system/、drivers/ 里能编的都编进去，链接出固件镜像。本机已装好 ESP-IDF v6.1：源码在 `~/esp/esp-idf`，工具在 `~/esp/.espressif`（`IDF_TOOLS_PATH=$HOME/esp/.espressif . ~/esp/esp-idf/export.sh`）；tools/hw-bench 是这台机器上已经编过的 ESP-IDF 工程，配置可参考。CI 那边用 tools/s31-size/run.sh 的装法。
- 用 FreeRTOS 实现 `osal.h`（第 7 节第 1 条：`xTaskCreateWithCaps` 支持 PSRAM 栈、`heap_caps_*`、`esp_timer`、互斥、信号量、延时按微秒）。这部分要真实可用，不是空函数。
- 各个 HAL 先放桩：返回“没有这个硬件”或明确的失败，系统能启动到“没有屏幕”的状态即可；每个桩标清楚以后用哪个上游驱动（第 7 节列了 EPDiy 改造版、FT6336U、TPS65185、LittleFS、SDMMC）。不写真驱动。
- 打开 `-fstack-usage`，出一份各任务入口的栈用量估计（至少存储、中枢连接、界面、显示几个任务），和模拟器的主机估算对比；尤其是存储任务的栈（2026-11-09 按主机估算从 3584 加到 4096，看 S31 上实际要多少）。
- `idf.py size` 的结果（Flash、内部 RAM、PSRAM）写进报告。
- 把编译加进 CI：沿用 .github/workflows/s31-size.yml 已有的 ESP-IDF 安装，加一步编译这个工程；编译失败就让 CI 失败。

## 从哪里看起

- docs/system/platform-and-simulator.md 第 1 节（分层与移植）、第 7 节；platform/include/ink/ 下的 osal.h 和各 hal_*.h；模拟器的实现 ports/sim/ 是写法参考。
- tools/s31-size/（已有的 ESP-IDF 安装和尺寸测量）、tools/hw-bench/（已有的 ESP-IDF 工程，可参考配置）。

## 边界

- **不烧录、不连设备、不读写 ink-hw**；只编译。
- 你的沙箱只能写克隆目录：ESP-IDF 要写的缓存（组件管理器缓存、构建临时文件）都设到克隆目录里（例如 `IDF_COMPONENT_CACHE_PATH`），不要改动已装好的 ESP-IDF 和 `~/.espressif`；实在跑不起来就把卡在哪写进报告。
- 系统和应用代码不改；编不过的地方如果是可移植性问题（例如 `uint32_t` 直接用 `%u` 打印，在这个工具链里是 `unsigned long`），在系统代码里改成可移植写法，每处写进报告；如果是别的原因，写进报告不要硬改。
- 同时在做的有 privacy-records、kid-3、file-index-3、factory-pages、fw-f13，不要改它们的文件。

## 验收

1. 本机 `idf.py build` 成功，报告写清命令和固件大小（`idf.py size`）。
2. OSAL 的 FreeRTOS 实现有单元测试或开机自检（在模拟器之外至少能编译过；能在主机上测的部分配主机测试）。
3. 栈用量表：各任务的 S31 估计、模拟器的主机估算、配置的栈，三列对比（报告）。
4. CI 加上这一步并通过（本地用同样的脚本跑一遍）。
5. 模拟器的全量测试照样通过；因可移植性改的系统代码逐处列在报告里。
