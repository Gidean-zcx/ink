# 复审实验与复核说明

日期：2026-11-03。产品检出固定为 `4309087408aa57e0d4668e5cf13551cb3945438c`。
这些程序只链接或读取现有实现，不改产品源码、原测试、profile 或 Git 状态；不打开真实摄像头、麦克风、串口或网络连接。大文件、构建文件与完整日志均在 `build-audit/`，本目录仅保存源程序、方法与精简结果。

## 环境与现有回归

- macOS 15.7.9 / arm64；Apple clang 17.0.0。
- 普通与 ASan 构建使用 CMake 3.28.3；测试执行器 CTest 4.4.4。为核实上轮同版本配置错误，又以 CMake 4.4.4 在独立目录完成一次配置。
- 普通构建 `INK_WERROR=ON`、GUI 开启，源码 SDL2 路径（`SDL2_DIR-NOTFOUND`）；38/38 通过，4.47 秒。
- ASan + UBSan 构建 `INK_WERROR=ON, INK_SANITIZE=ON, INK_GUI=OFF`；37/37 通过，10.62 秒。**这组未包含 GUI；不是“ASan 验证过真实媒体后端”。**
- 配置、构建及 CTest 命令退出码均为 0。补充定时器探针另复现 ASan 错误，不能把已有套件全绿视为本次通过验收。

从仓库根目录运行：

```bash
AUDIT_DEPS_ROOT=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$AUDIT_DEPS_ROOT/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$AUDIT_DEPS_ROOT/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$AUDIT_DEPS_ROOT/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$AUDIT_DEPS_ROOT/build/_deps/sdl2-src"
cmake --build build-audit -j8
"$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/ctest" --test-dir build-audit -j8 --timeout 300 --output-on-failure

cmake -S . -B build-audit/asan -DINK_WERROR=ON -DINK_SANITIZE=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$AUDIT_DEPS_ROOT/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$AUDIT_DEPS_ROOT/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$AUDIT_DEPS_ROOT/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$AUDIT_DEPS_ROOT/build/_deps/sdl2-src"
cmake --build build-audit/asan -j8
"$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/ctest" --test-dir build-audit/asan -j8 --timeout 300 --output-on-failure

"$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/cmake" -S . -B build-audit/cmake444 -DINK_WERROR=ON \
  -DINK_CJK_FONT="$AUDIT_DEPS_ROOT/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$AUDIT_DEPS_ROOT/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$AUDIT_DEPS_ROOT/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$AUDIT_DEPS_ROOT/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$AUDIT_DEPS_ROOT/build/_deps/sdl2-src"
```

日志：`build-audit/r2-configure.log`、`r2-build.log`、`r2-ctest.log`；ASan 对应 `r2-asan-configure.log`、`r2-asan-build.log`、`r2-asan-ctest.log`；同版本配置 `r2-cmake444-configure.log`。CMake 4.4.4 配置耗时约 35 秒，退出 0，未再次重复整套编译。

## 补充探针

`run.py` 把未修改的 `ports/sim/sim_main.c` 与本目录的 `extra_probe.c` 链接现有静态库。`extra_probe.c` 包含 `probe.c`，以自定义 `ink_main` 驱动 HAL/系统服务。编译参数完整保留在脚本中；可执行文件为 `build-audit/r2-probe` 或 `r2-probe-asan`。主机编译目标为 macOS 15.7；这些命令不适用于原样复制到 Linux。

每项独立启动 `--profile profiles/metalio-plus.json --end 20s --no-random-outages`，最长实际等待 40 秒。模拟器 stdout/stderr 在 `build-audit/r2-probes/<模式>[-asan]/run.log`；截图和 metrics 同目录。只有 boot-fault 项额外加 `--i2c-faults 1:io-expander`。所有输入及故障都是模拟注入。

```bash
python3 docs/audits/2026-11-03-codex-r2/experiments/run.py \
  storage_cache tca_shadow input_fault touch_blocks_power camera_cut camera_timeout \
  audio_unpowered sleep_rails display_fault nt26_unicode \
  r2_display_cut r2_camera_deadline r2_audio_sleep_race r2_wake_read_fault \
  r2_timer_rearm_delete r2_wake_control r2_audio_sleep_late r2_audio_sleep_entry r2_input_boot_fault
python3 docs/audits/2026-11-03-codex-r2/experiments/run.py --asan r2_timer_rearm_delete
```

结果见 [results.json](results.json)，按模式/是否启用 sanitizer 合并，重复运行覆盖同名观测。**普通探针退出 0 仅表示进程正常退出，是否存在错误行为要看观测值；它们不是全部自带断言的产品回归测试。** 最后一次运行把显示用的 4096 B 探针缓冲改为静态，避免把审计函数自身的大栈缓冲告警误当成产品缺陷；未改任何产品行为。

| 模式 | 核实点与本轮观测 |
| --- | --- |
| `storage_cache` | 先写 OLD，再成功覆盖 1200 B 的 N；读取新数据，pending=failed=0（AUD2-002）。 |
| `tca_shadow` | 失败的电机置位不被后续放大器写入重放（AUD2-006）。 |
| `input_fault` | 初始化完成后总线持续失败，实体键无事件（AUD2-012 的正常初始化分支）。 |
| `r2_input_boot_fault` | 从开机起总线失败，present=1、ready=0，仍送 POWER 按下；这是 AUD2-012 残余旁路。 |
| `touch_blocks_power` | 手指保持触摸时 POWER 按下/释放均约 50.736 ms 送达（AUD2-013）。 |
| `camera_cut` | 200 ms 时切 P05，此版发生在较长的初始化内，返回 -3；**不能用它声称覆盖帧中断电**。既有 board 自测在 500 ms 时切电并断言 -5。 |
| `camera_timeout` / `r2_camera_deadline` | 比较实际调用耗时；0/1/100 ms 预算仍消耗约 390–400 ms（AUD2-016），现有测试只断言错误码。 |
| `audio_unpowered` | P14 关闭时 500 ms 内零样本（AUD2-015 的时钟门控）。 |
| `sleep_rails` | 电机开启时睡眠返回 0，保持供电。睡眠中引脚的正向覆盖来自既有 board 自测。 |
| `display_fault` | PMIC 持续故障，回调 ok=0（AUD2-017 的启动失败）。 |
| `r2_display_cut` | 等 `sim_display_playing()` 后经板驱动拉低 PMIC 控制脚，ready=0 却 ok=1；图像改变 4096 px（AUD3-003）。 |
| `nt26_unicode` | 拒绝不成对代理、残缺十六进制及过长 UTF-8（AUD2-009）。 |
| `r2_wake_read_fault` / `r2_wake_control` | 一次短 POWER 按键；故障组 401–650 ms 总线失败。恢复后方向 00,80/keys=1，后续音量键仍无事件；对照方向 c0,e3/keys=2，再按音量键 keys=4（AUD3-002）。 |
| `r2_audio_sleep_entry` | sleep 调用时 busy=0，进入序列中另一个任务才开始音频；复电后 start=0、mode=clock=0，观察结束仍零样本（AUD3-004）。 |
| `r2_audio_sleep_late` / `r2_audio_sleep_race` | 启动后 600 ms 睡眠同样失败；10 ms 的较早交错能恢复并取得 18000 样本。不是“任意交错都会失败”。 |
| `r2_timer_rearm_delete` | 回调给信号并做有界 CPU 工作，另一任务 delete 等待；回调合法重启自身后产生悬空事件。普通进程退出 0，ASan 进程 -6 / `heap-use-after-free in timer_fire`（AUD3-001）。回调不做阻塞等待。 |

ASan 探针设 `ASAN_OPTIONS=detect_leaks=0`：本次核实 UAF，未作泄漏验收；VTK 结束时会保留停驻任务。宿主符号化出现 `atos` 警告，但仍报告 `timer_fire`、释放与分配栈；文件行号来自对同一检出代码的独立核查。

## 内存、上游表与截图对照

```bash
/Users/bytedance/Documents/Workspace/ink/build-deps/pyenv/bin/python \
  docs/audits/2026-11-03-codex-r2/experiments/evidence.py
```

脚本先在 `build-audit/` 编译 `memory_probe.c`。此探针直接包含当前 `sim/mem/pools.c`，以其 TLSF 遍历求出的真实空闲块之和对照公开统计，编译命令也记录在 [driver-evidence.json](driver-evidence.json)。连续分配与零池修复通过，但隔个释放 50/100 个块后仍多报 400 B；10 万次分配/释放后归属表容量保持 4096。

另逐项比较本项目与官方 `690a023c66862af103a3187464373ee7c5efcd14` 的 GC2145 表，去掉注释后 586 个寄存器/值对及顺序全部相同；散列见 JSON。只读官方允许目录中的文件。PNG 分析只计算断电探针前后不同像素数，原图留在构建目录；没有用图像差异推断实机画质。

## 桌面重启

```bash
python3 docs/audits/2026-11-03-codex-r2/experiments/gui_restart_probe.py
```

使用 `SDL_VIDEODRIVER=dummy`、独立 `--data build-audit/r2-gui-restart`，虚拟 4 秒点击重启，最多观察 25 秒。对自己启动的 PID 运行 `lsof`，4 次启动间管道 fd 始终为 4。完成后仅向该子进程发送 SIGTERM，故记录的 -15 是有意结束，不是程序崩溃。结果见 [gui-restart-results.json](gui-restart-results.json)，完整日志留 `build-audit/r2-gui-restart/gui-restart.log`。

## 覆盖限制与产物校验

Linux V4L2 错误/短帧路径及 macOS 授权/状态同步只作代码核查，没有声称完成真摄像头或授权弹窗测试。没有运行 Linux 二进制、TSan、真实双核 ESP-IDF、实机功耗/波形、远程中枢或在线 CI；现有 VTK 的线程调度不能证明任意 SMP 交错安全。

`record.py` 是审计记录维护工具：合并 JSON 输入并刷新报告的核实表、新问题区。`validate.py` 检查全部 23 个旧编号、4 个新编号、必填字段、证据文件与源位置、大小及只读边界；输出 `validation.json`。所有复现摘要均源于本轮重新执行，未沿用第一次审计的运行结果。
