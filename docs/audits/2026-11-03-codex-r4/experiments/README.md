# 第四轮复审：复现与证据

日期：2026-11-03。当前对象 `89b112620730902f0fa1594e83ffb146b08749be`；修复前对照 `95587e5a5f641fa4fd44c313f366e69f244faeb1`。

环境：macOS 15.7.9 / arm64，Apple Clang 17.0.0，CMake 3.28.3，指定虚拟环境的 Python 3.11.15 / CTest 4.4.4。全部离线、仅模拟器，无真实设备、摄像头或麦克风访问。

## 文件

- `run.py`：链接未经修改的产品静态库，运行独立场景；每进程20s虚拟时间、40s墙钟超时，执行文件、日志、PNG、模拟文件系统只写构建目录。各参数组合最后一次结果在 `results.json`。
- `legacy_probe.c`：第三轮 `deep_probe.c` 的副本，仅改原探针包含路径及最终入口名为 `legacy_main`；第二、第一轮基础探针通过相对路径引用历史审计原文件。未改历史文件。
- `probe.c`：本轮补充探针。`r4_ai_failed` 调用真实 `system/boot/main.c` 的启动装配，由模拟按键驱动实际UI、AI、音频与中枢链路。其他场景直接调用原HAL/服务；无替换产品函数的桩实现。
- `metrics-evidence.json`：关键指标、警告及I2C事务数量摘要，由 `summarize.py` 从运行产物提取。
- `comparison.json`：5组正常场景的前后库比较、全部不同指标、最终PNG是否相同，以及8% PMIC故障场景对照。
- `image-evidence.json`：P05关闭与恢复后图片的像素统计、哈希；图像本身只在构建目录。用于检验模拟器可见内容，不能证明真实面板响应。
- `board-results.json`：原EPDiy与额外PQ-BN整板测试的检查数量和预算结果。
- `record.py`、`checkpoints.py`：分组增量记录助手；不执行产品操作。
- `validate.py`、`artifact-validation.json`：审计包、结构化条目、重要观测与写入范围核对。

**探针exit=0表示进程正常结束，不代表功能通过。** 本轮保存54组配置：34组当前普通、7组当前ASan/UBSan、13组修复前对照；关键反例也正常退出。`--before`只用于已经独立构建的历史库，原运行器 `sim_main.c` 在两提交间无修改。不要并行运行多份 `run.py`，它们共用结果索引和部分可执行文件。

## 离线构建与原测试

在仓库根目录执行，路径来自TASK。

```bash
INK_R4_DEPS=/Users/bytedance/Documents/Workspace/ink
mkdir -p build-audit
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_R4_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_R4_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_R4_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_R4_DEPS/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_R4_DEPS/build/_deps/sdl2-src" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-audit -j 8
"$INK_R4_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

配置、构建、CTest均exit=0，40/40，10.57s。日志在 `build-audit/configure.log`、`build.log`、`ctest.log`。现有 `gui-smoke` 采用dummy视频驱动。

内存检查：同一配置改用 `-B build-audit-asan`，额外加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，随后构建及同参数CTest。配置、构建、CTest均exit=0，39/39，10.73s。配置日志在 `build-audit/asan-configure.log`，其他日志在 `build-audit-asan/`。补充ASan探针使用 `-fsanitize=address,undefined` 与 `ASAN_OPTIONS=detect_leaks=0`；不宣称运行了LeakSanitizer或ThreadSanitizer。

```bash
INK_SELFTEST_BOARD=1 build-audit/sim-selftest \
  --profile simulator/profiles/metalio-plus-pqbn.json \
  --budget tests/simulator/board.budget.json --out build-audit/r4-board-pqbn \
  --quiet --end 90s --no-random-outages
```

这条原预算命令**exit=1**：65项检查成功、无CHECK失败，预算要求66项。没有删预算重跑来冒充通过，见AUD5-004。默认EPDiy整板是66项成功、预算通过。

## 已执行场景

```bash
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py \
  r3_display_transient r3_display_vcom r3_display_multi r3_display_p05 \
  r3_wake_touch r3_wake_audio r3_boot_audio r3_audio_oom r3_play_sleep_entry
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --pqbn \
  r3_display_transient r3_display_vcom r3_display_multi r3_display_p05
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py \
  r4_normal r4_normal_activity r4_p05_preoff r4_touch_restore r4_audio_restore \
  r4_gpio2_accel r4_gpio2_normal r4_tca_init_change r4_service_failed r4_service_normal r4_ai_failed
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --pqbn \
  r4_normal r4_normal_activity r4_p05_preoff
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py \
  r3_wake_permanent r2_wake_control r2_audio_sleep_entry r2_audio_sleep_late r2_audio_sleep_race
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py r4_held_supply
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --pqbn r4_held_supply
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --asan \
  r3_display_transient r3_boot_audio r4_touch_restore r4_audio_restore \
  r4_p05_preoff r4_service_failed r4_ai_failed
```

实际按若干小批次运行；单项的完整子进程命令与输出在 `results.json`。`r4_tca_init_change` 初次试跑未挂接懒初始化的TPS模型，得到on_rc=1，因此不能用于判断；补充独立模型注册后重跑，最终有效结果是on_rc=0、VCOM低→高、touched=0、kept=1。只修了审计探针，产品源码没有改动。

| 组 | 判读 |
| --- | --- |
| r3_display_transient / vcom / multi | 当前两档原故障均失败、每ID回调一次、重新上电后可成功；与历史库回放比较。 |
| r3_display_p05 / r4_p05_preoff | 当前持续P05断电可回调成功；EPDiy提交16384个无供电像素，PQ-BN复电后重交同图仍空白（AUD5-001）。 |
| r4_normal / normal_activity | 两档均9/9成功刷新；3次睡眠后刷新成功；录音两次各9600样本；并行动作成功、无告警。 |
| r4_held_supply | 保持上电时马达/功放不误触发；P05/VCOM变化各回收一次，4/4成功。 |
| r4_touch_restore / audio_restore | 故障长于同步窗口；恢复前启动失败、恢复后重试成功，持续触摸不阻止按键恢复。 |
| r4_gpio2_accel / normal | 5s加速度计故障10次重查，恢复后可用；正常转动0次重查。 |
| r4_tca_init_change | API级覆盖边界：初始化直接写未计入changed；不当作另一条可达严重/高问题。 |
| r4_service_failed / normal | 8帧都入队成功；失败组0ms声音、8次播放失败后丢完，正常组480ms（AUD5-002）。 |
| r4_ai_failed | 0ms录音却出现一次ai.ask和回答；最终UI显示固定“你问：这一页讲了什么？”（AUD5-003）。 |

## 历史库构建与比较

```bash
mkdir -p build-audit/before-src
git archive 95587e5 | tar -x -C build-audit/before-src
cmake -S build-audit/before-src -B build-audit/before -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$INK_R4_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_R4_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_R4_DEPS/build/_deps/lvgl-src"
cmake --build build-audit/before -j 8
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --before \
  r4_normal r4_normal_activity r4_service_normal r4_service_failed r4_ai_failed \
  r3_display_transient r3_display_vcom r3_wake_touch r3_wake_audio r3_boot_audio
python3 docs/audits/2026-11-03-codex-r4/experiments/run.py --before --pqbn \
  r4_normal r4_normal_activity r3_display_vcom
"$INK_R4_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-audit/before \
  -R '^scenario-reader_basic-panel-power-faults$' --output-on-failure
python3 docs/audits/2026-11-03-codex-r4/experiments/summarize.py
PYTHONDONTWRITEBYTECODE=1 python3 docs/audits/2026-11-03-codex-r4/experiments/validate.py
```

历史配置、构建、该场景CTest exit均0。EPDiy正常观测相同；PQ-BN波形增加116～161µs，最终PNG相同。8% PMIC超时的掉电误判从4次降为0。未重新跑历史完整套件或历史GUI。

## 审计限制

Linux摄像头仅代码核查；未核验远端CI、未做真实SMP/TSan或长时间稳定性、未实现/运行ESP-IDF端口。现有内核/存储/蜂窝等通过完整回归，本轮没有重做前三轮每条问题的独立审计。硬件依据只来自允许的上游固定检出；电气/功耗/光学结果仍需实机验证。产品、原测试、档案、历史审计均未改动；未提交、推送、建分支。
