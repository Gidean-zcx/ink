# ESP32-S31 移植骨架报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。只编译，不连接设备。S31 构建、模拟器全量、内存检查、依赖补测与反向对照均已完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 固件已链接；106 个 system、11 个 drivers、3 个 boards、2 个 apps 的 C 文件全部交叉编译 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2 | 【实现】完成；【实测】自检已交叉编译、主机换算测试通过 | PSRAM 任务栈、heap_caps、FreeRTOS 同步和队列、esp_timer；开机自检未上机执行 | 同上；`ctest --test-dir build -R unit-s31-osal-ticks` |
| 3 | 【估算】见下表，保留存储 4096 字节 | `.su` 入口帧、`.ci` 直接调用链；主机普通构建场景估算 | `python3 tests/esp32s31/stack_report.py build-s31 --host build/test-out` |
| 4 | 【实测】本地同脚本通过；远端未运行 | 原 workflow 复用现有 IDF 安装，新增端口编译；失败直接返回非零，上传尺寸与栈报告 | `bash ports/esp32s31/build.sh` |
| 5 | 【实测】普通及 ASan/UBSan 各覆盖 258 项：255 项全量通过，3 项补跑通过 | 全量日志及补测日志在 `work/`；没有改已有测试或预算 | 总规则 §5 参数，`ctest --test-dir build --output-on-failure -j2`；ASan 换 `build-asan`，补测命令见下 |

尺寸（`idf.py size`，字节）：

| 项目 | 数值 |
| --- | ---: |
| 固件 `ink-s31.bin` | 1,205,648 |
| Flash（IDF 统计） | 1,141,764 |
| 内部 DIRAM | 82,958 |
| 外部 PSRAM 静态 BSS | 381,560 |
| LP SRAM | 24 |

这是【实测】编译、链接的静态尺寸，不包括运行时任务栈、堆分配、真实驱动和字体资源。`ink_main` 及存储、中枢、界面、显示任务符号都保留在 ELF 中；没有用空入口规避完整系统链接。

栈对照（字节）。S31 列为“入口帧 / 已知直接调用链”，主机列为本次 `metalio-e-ink4-plus` 普通构建各场景中观察到的最大估算，均不是实机测量：

| 任务 | S31 估算 | 模拟器主机估算 | 配置的栈 |
| --- | ---: | ---: | ---: |
| storage | 128 / 736 | 3472 | 4096，内部 |
| hub | 464 / 3088 | 4240 | 6144，PSRAM |
| ui | 224 / 4640 | 9664 | 10240，PSRAM |
| display | 352 / 496 | 2096 | 3072，PSRAM |
| audio | 112 / 640 | 3776 | 26624，PSRAM |
| comm | 128 / 672 | 5840 | 8192，PSRAM |
| power | 144 / 848 | 2688 | 3072，内部 |
| exec.media | 16 / 80 | 7552 | 8192，PSRAM |
| exec.bg | 16 / 80 | 6376 | 8192，PSRAM |

直接调用链不含间接回调、未提供栈数据的库函数、递归重复和 RTOS 上下文；HAL 桩也远小于真实驱动，所以这些数**不是完整上限**。存储已知链为 `storage_task → run_job → restore_check → ink_sha256_final → ink_sha256_update → block`；任务还会运行存储回调。执行器的 80 字节只覆盖循环和等待，不含工作回调。现有存储 4096 字节保持不变，不据此缩小。

环境：ESP-IDF v6.1（`fff9895c82d744c7237be8847347bdd1b07c6643`），S31 GCC 15.2.0（`esp-15.2.0_20251204`），LVGL v9.5.0（`85aa60d18b3d5e5588d7b247abf90198f07c8a63`）。核查日期：2026-11-09；依据为本地安装源码、编译器输出、生成的 `.su/.ci` 和模拟器指标。依赖源码只读，没有引入或升级第三方库。

共享系统的可移植性修复（仅 10 个文件、16 个格式调用位置，17 行替换；不改变 JSON 值或行为）：

| 文件 | 行号与修改 |
| --- | --- |
| `system/apps/remote/remote.c` | 466：坐标转 `long`，配 `%ld` |
| `system/policy/cap.c` | 957–958、1408：事件序号、启动号、确认令牌转 `unsigned long`，配 `%lu` |
| `system/policy/confirm.c` | 73：页面代次、成员代次、连接号转 `unsigned`，配 `%u` |
| `system/policy/members.c` | 909：分钟、额外分钟、日期转 `unsigned` |
| `system/services/backup/backup_service.c` | 714、803、1102：大小、偏移、分块长度、清单大小转 `unsigned` |
| `system/services/file/file_caps.c` | 371：缩略图编号转 `unsigned` |
| `system/services/file/file_service.c` | 1724：清理版本号转 `unsigned` |
| `system/services/file/transfer.c` | 102、194：授权码转 `unsigned`，保留六位格式 |
| `system/services/hub/hub_link.c` | 226、353、459：请求号、成员代次转 `unsigned` |
| `system/services/text/text_caps.c` | 242：单词编号转 `unsigned` |


【实测】反向对照：`python3 tests/esp32s31/portability.py build-s31` 对 10 个文件逐一检查；当前源码均退出 0，临时恢复为基线版本后均退出 1，共复现 23 个格式错误。微秒换算测试正常退出 0；仅在临时头文件中把目标节拍换成相对时长取整后，第 2 个边界断言失败，退出为 SIGABRT（6）。没有改动产品文件来做反向测试。

主机配置沿用总规则 §5，另外复用 `$INK_DEPS/build/_deps/sdl2-src` 和 `imgui-src`，避免下载。中枢补测配置与命令（两种构建各执行一次；同一时间只跑一轮）：

```bash
export PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src"
cmake -S . -B build -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^hub-(contract|json-crosscheck|simulator-e2e)$' --output-on-failure -j2
```

普通测试以 `INK_KEEP_EVIDENCE=1` 保留指标；`stack_report.py --host` 会输出各行的具体来源场景。构建与测试最多并行 2，桌面测试使用现有无窗口模式。

【实测】范围与隐私检查：`git diff --check`、文档相对链接检查通过；按总规则 §6 原命令扫描基线至 HEAD 的补丁，无命中（grep 退出 1）。仅修改任务文件、端口、CI、测试注册和上表列明的格式调用；没有构建产物或设计稿入库。

## 没做到的

- 未连接设备、未执行设备自检或实机栈水位测量；真实 HAL、字体与资源接入不在本任务范围。
- 未推送、未触发远端 CI，也未在 Linux 主机上运行模拟器；本地成功使用的是 CI 新增的同一构建脚本。

## 和设计不一致的

- HAL 按任务书保持不可用，134 个声明均有实现，并分组标注后续驱动。入口先自检；屏幕尺寸为零就记录“没有屏幕”并返回，不进入要求帧缓冲的完整系统启动。这是骨架行为，不宣称完整系统已在硬件启动。
- 架构审视：只替换平台实现，系统和应用没有模拟器依赖；没有新增产品动作、能力或页面，既有软总线路径保持不变。

## 改过的已有测试

- 无；没有改变期望、预算、跳过规则或断言。新增微秒换算主机测试、设备开机自检，以及栈报告和可移植性对照脚本。

## 要用户定的

- 无。

## 顺手发现

- 本地首次配置曾保留完整 IDF 默认值；最终脚本每次从提交的默认配置生成临时 `sdkconfig`，已重新构建并使用最终尺寸。
- 本机没有 Ninja。IDF v6.1 的 Makefiles 后备会硬编码 `CPU 数 + 2` 并发；脚本只在当前 Python 进程中将其限制为 `make -j 2`，不修改已安装 IDF。Linux 的 Ninja 通过 `IDF_PY_BUILD_JOBS=2` 限制并发。
- 全量初跑各 255 项通过、3 项因依赖跳过（普通 375.00 秒，ASan/UBSan 514.34 秒）。选用只读依赖目录的锁定中枢 Python 后，3 项及其能力导出前置测试两种构建都通过（各 4/4，19.02 / 19.25 秒），没有剩余未执行项。未安装依赖、未改测试。

## 新增的机制

- FreeRTOS OSAL、HAL 不可用桩和 ESP-IDF 构建配置：任务书明确要求。OSAL 使用 IDF 原生对象及 WithCaps 生命周期，没有自写调度、重试或持久格式。
- `osal-test` 短任务及自检定时器：验收 2 要求，检查 PSRAM 栈、任务名称与定时器语义，随后释放。
- IDF 的 `esp_timer` 服务及 WithCaps 自删除清理任务：OSAL 映射所需，直接使用 SDK 实现。
- 端口产品文件共 549 行（含 CMake、配置和构建脚本，不含 76 行自检及文档），在 600 行预算内。超过 100 行的原因是 OSAL 全接口和 134 个 HAL 声明均需落地；考虑过用统一桩宏缩短，保留逐项函数以便核对失败语义。

`sdkconfig.defaults` 的配置项：

| 配置 | 来源与用途 |
| --- | --- |
| `IDF_TARGET=esp32s31` | 任务书：指定目标 |
| `COMPILER_OPTIMIZATION_PERF=y` | 栈估算：采用优化后的固件代码 |
| `APP_REPRODUCIBLE_BUILD=y` | 本地与 CI：固定构建信息的生成方式 |
| `ESPTOOLPY_FLASHSIZE_16MB=y` | Plus 已知 Flash 容量；只生成镜像 |
| `PARTITION_TABLE_SINGLE_APP_LARGE=y` | 骨架镜像超过默认 1 MiB，使用 SDK 现成的大应用分区 |
| `SPIRAM=y` | 任务书：PSRAM 内存与栈 |
| `SPIRAM_SPEED_200M=y` | Plus 已知 PSRAM 时钟 |
| `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` | 既有 `INK_PSRAM_BSS` 表放外部 RAM |
| `SPIRAM_MALLOC_ALWAYSINTERNAL=16` | OSAL 契约：小分配留内部 RAM |
| `FREERTOS_TASK_CREATE_ALLOW_EXT_MEM=y` | 任务书：WithCaps 的 PSRAM 栈 |
| `FREERTOS_THREAD_LOCAL_STORAGE_POINTERS=2` | OSAL 任务身份使用独立 TLS 槽 |
| `FREERTOS_HZ=100` | OSAL 契约：10 ms 节拍 |
| `ESP_MAIN_TASK_STACK_SIZE=16384` | 骨架自检及启动入口；入口返回后由 IDF 回收 |
| `ESP_DEFAULT_CPU_FREQ_MHZ_320=y` | Plus 的目标主频 |

## 要改的文档

- 合并时更新 `README.md`、项目入口、`docs/system/platform-and-simulator.md` 中“端口待写”的状态及本次尺寸、栈证据；本任务未修改共享文档。
- `docs/system/capabilities.md` 无新增能力，不需重生成。
