# 审计实验与复现说明

这些是独立审计材料，未加入产品 CMake 或原有测试。对象为 `3a435a59812b9d3d40fbe04358c9b17ef4eb15bf`。仅使用本地依赖、模拟器和审计进程，不联网、不打开宿主摄像头/麦克风、不连接设备。

## 环境与产物

- 本次环境：macOS arm64，AppleClang 17.0.0.17000603，CMake 4.4.4。Python 依赖环境为 TASK 指定的 Python 3.11.15；审计辅助脚本只用 Python 标准库。
- 从仓库根目录执行下列命令。构建目录 `build-audit/`、`build-audit-asan/` 保留大文件、静态库、图片及模拟文件系统；不随审计附件复制这些构建产物。
- `probe.c` 提供审计专用 `ink_main`，链接未改动的产品静态库和原 runner；替代启动装配以隔离单一 HAL/服务行为，并不替代被核查的实现。
- `memory_probe.c` 在单个翻译单元中包含原 `sim/mem/pools.c`，用同一 TLSF 的遍历值对照公开计数，观察归属表。仅将 `vtk_host_call` 设为同步调用，不模拟系统并发。
- GUI 重启实验依赖本机 `/usr/sbin/lsof` 和现有窗口布局；脚本坐标是该固定提交的“重启设备”按钮。它只终止自己创建的子进程。
- 这里的探针是**观测工具**。退出 0 表示运行完成；缺陷由日志和指标之间的矛盾判定，不是“回归测试通过”。尚未将建议修复或断言加进产品测试。

## 构建与现有测试

先设置路径：

```sh
cd /Users/bytedance/Documents/Workspace/ink-audit-1
D=/Users/bytedance/Documents/Workspace/ink
AUDIT="$PWD/docs/audits/2026-11-03-codex/experiments"
```

本次首先在尚无配置缓存的 `build-audit/` 执行 TASK 原命令：

```sh
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$D/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$D/build/_deps/sdl2-src"
```

首次与同命令重试均 exit 1，见 [configure.log](configure.log)、[configure-second.log](configure-second.log)。报错为 `CMAKE_OBJC_COMPILE_OBJECT` 等 Objective-C 内部变量缺失，详见 AUD2-022。

为了继续审计，只通过审计文件提前启用 OBJC；保留上一步的依赖缓存参数：

```sh
cmake -S . -B build-audit -DCMAKE_PROJECT_ink_INCLUDE="$AUDIT/enable_objc.cmake"
cmake --build build-audit -j 8
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

这三步均 exit 0，CTest **35/35**，11.98 s。见 [configure-workaround.log](configure-workaround.log)、[build.log](build.log)、[ctest.log](ctest.log)。

**缓存说明：**完成 workaround 后，`build-audit/CMakeCache.txt` 已含该 include；直接再次执行原命令不会移除它。独立复现原始失败要用新的空构建目录，或显式清除该审计 include 的缓存项；不要把“带 workaround 的重配置成功”误当成原问题自行消失。

ASan/UBSan 使用独立构建目录，仍保留 Werror 和同一个审计 include：

```sh
cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_SANITIZE=ON \
  -DCMAKE_PROJECT_ink_INCLUDE="$AUDIT/enable_objc.cmake" \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$D/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$D/build/_deps/sdl2-src"
cmake --build build-audit-asan -j 8
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit-asan -j 8 --timeout 300 --output-on-failure
```

三步均 exit 0，CTest **35/35**，14.11 s。见 [asan-configure.log](asan-configure.log)、[asan-build.log](asan-build.log)、[asan-ctest.log](asan-ctest.log)。没有将 ASan/UBSan 通过解释成不存在数据竞争；本次未跑 TSAN。

## 审计探针

先完成普通构建及现有测试，以生成字体、测试内容与静态库，再执行：

```sh
python3 docs/audits/2026-11-03-codex/experiments/build_probes.py
python3 docs/audits/2026-11-03-codex/experiments/run_probes.py
build-audit/audit-memory-probe > docs/audits/2026-11-03-codex/experiments/memory.log
python3 docs/audits/2026-11-03-codex/experiments/gui_restart_probe.py
python3 docs/audits/2026-11-03-codex/experiments/collect_results.py
```

这些命令本次均 exit 0。编译器完整参数见 [build_probes.py](build_probes.py) 和 [probe-build.log](probe-build.log)；脚本当前面向本机 macOS，含部署版本参数和 TASK 的 LVGL 路径。独立内存探针的第二条编译命令也完整保留在脚本里。所有子场景命令、退出码和关键输出在 [probe-results.json](probe-results.json)。

`run_probes.py` 无参数执行 10 个 HAL/服务模式和 2 个 AI 场景；给名称则只重跑指定项，并更新对应的结果记录。例如：

```sh
python3 docs/audits/2026-11-03-codex/experiments/run_probes.py storage_cache
python3 docs/audits/2026-11-03-codex/experiments/run_probes.py ai_held_dismiss ai_held_control
```

重跑会覆盖本目录相应日志及构建目录里的对应输出。HAL 探针由 `AUDIT_PROBE` 环境变量选择，使用 `--no-random-outages` 和 20 s 虚拟运行上限；AI 场景为 65 s。每个子进程的宿主超时为 45 s。实验不挂预算文件，因此不能用退出码替代状态断言。

| 模式/文件 | 核查对象 | 观察结果与报告编号 |
| --- | --- | --- |
| [ai_held_dismiss.script](ai_held_dismiss.script)、[ai_held_control.script](ai_held_control.script)；[关闭组日志](ai_held_dismiss.log)、[对照日志](ai_held_control.log) | 按住录音中按 HOME，与仅去掉 HOME 的对照 | 采集 61965 / 3000 ms；休眠 0 / 36279.986 ms，AUD2-001 |
| [storage_cache.log](storage_cache.log) | 同路径小文件成功改为大文件 | durable_size=1200，read_n=3、仍为 OLD，AUD2-002 |
| [tca_shadow.log](tca_shadow.log) | I2C 写失败后更改另一位 | 马达写失败，开启功放时马达却开启，AUD2-006 |
| [input_fault.log](input_fault.log) | 初始化后 expander 100% 超时 | 仍收到 POWER 按下，AUD2-012 |
| [touch_blocks_power.log](touch_blocks_power.log) | 持续触摸期间 POWER 按/松 | 延迟 1833.269 / 1433.270 ms，AUD2-013 |
| [sleep_rails.log](sleep_rails.log) | 马达开、keep_modem=false 时进入休眠 | 电源状态仍开，电流账变 1.5 mA，AUD2-014 |
| [audio_unpowered.log](audio_unpowered.log) | P14 断电后录音 | 仍成功收到 7920 samples，AUD2-015 |
| [camera_cut.log](camera_cut.log)、[camera_timeout.log](camera_timeout.log) | 采集中断电、600 ms deadline | 仍出完整帧；总耗时超过 deadline，AUD2-016 |
| [display_fault.log](display_fault.log) | PMIC 100% 传输失败 | callback_ok=1 且 panel_ready=0，AUD2-017 |
| [nt26_unicode.log](nt26_unicode.log) | 非法 UTF-8/不配对 UTF-16 代理项/残缺十六进制 | 被接受或错误转码，AUD2-009 |
| [memory_probe.c](memory_probe.c)、[memory.log](memory.log) | 能力组合、TLSF 空闲量、归属表 | EXTERNAL+DMA 落内部；多报 800 B；零池产生 1008 B；live=0 时表已 2 MiB，AUD2-005/010/011 |
| [gui_restart_probe.py](gui_restart_probe.py)、[gui-restart.log](gui-restart.log)、[gui-restart-results.json](gui-restart-results.json) | 同一进程重复 exec 重启 | PIPE fd 4→6→8→10，AUD2-021 |

GUI 脚本使用 `SDL_VIDEODRIVER=dummy`、`--gui-screenshot`，该提交的 headless 路径跳过宿主媒体设备。每次 re-exec 会重新解析相同的自动点击参数，故可连续触发重启。脚本观察到第 4 次启动后只向自己启动的进程发送 SIGTERM；本次子进程最终退出码 **0**，不是崩溃。每次 lsof 查询退出码均 0。完整命令和每个样本在结果 JSON 中。

[collect_results.py](collect_results.py) 从 `build-audit/{audit-probes,audit-scenarios,test-out}/*/metrics.json` 提取与报告有关的字段到 [evidence-summary.json](evidence-summary.json)。这是便携证据摘录，不冒充所有原始 trace；大文件仍在构建目录。现有 GUI smoke 的 `build-audit/test-out/gui-smoke/window.png` 已目视检查，但没有把该检查扩展为所有 UI 页面的验收。

## 代码核查与边界

没有动态复现的发现（AUD2-003/004/007/008/018/019/020/024）按报告的具体文件行号复核；Linux 媒体、真实双核交错、异步权限回调只提供代码路径和后续验证建议，不伪称运行过。AUD2-023 是文档交叉核查。实体键项中 RTC 旁路、音频项中完整模式语义也明确区分了已跑实验与静态核查。

官方依据只从允许的 upstream 仓库读取，固定命令形式：

```sh
git -C /Users/bytedance/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus show 690a023c66862af103a3187464373ee7c5efcd14:main/boards/metalio-e-ink4-plus/config.h
git -C /Users/bytedance/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus show 690a023c66862af103a3187464373ee7c5efcd14:main/boards/common/camera/dvp_camera.cc
```

以上文件读取 exit 0；其他固定提交路径在报告中给出。本次没有进行设备构建/烧录、实机/真实 hub 验证、摄像头/麦克风采集、联网检索完整芯片手册、长时间磨损及穷举掉电测试，原因见报告末尾。

## 产物校验

```sh
python3 docs/audits/2026-11-03-codex/experiments/validate_artifacts.py
git diff --exit-code
git diff --cached --exit-code
git status --short
```

[validate_artifacts.py](validate_artifacts.py) 仅验证审计材料、来源定位、当前提交和仓库变更边界，将机械检查结果写入 [validation.json](validation.json)。它不代替人工核查，也不证明代码语义或安全性。校验包括 JSON 必填字段、编号/定级统计、A～I 节、附件/本地链接、来源行号、复现结果、文件大小，以及有限的敏感标识格式扫描。扫描结果只记录数量，不输出疑似内容；“无匹配”不等于通用秘密检测证明。

任务要求单个附件小于 1 MB；脚本按更严格的 1,000,000 字节上限检查。根仓库和官方仓库 HEAD 均再次核对；产品 tracked diff 应为空。未提交、推送或创建分支。

