# 第六轮离线复现与证据

当前对象 `8c38f17a5c2525b1859dd0c8e2723844734f6a81`；修复前对照 `0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3`。全部从仓库根目录执行。本轮没有修改产品、既有测试、档案或往轮记录；没有联网、接触设备、读取私有实机数据、调用子代理或外部模型。

## 构建与正式测试

实际环境：macOS 15.7.9 (24G830)、arm64、Apple clang 17.0.0、CMake 3.28.3、CTest 4.4.4、探针Python 3.9.6。字体生成使用任务指定的Python 3.11.15。依赖均用已有本地副本。

```bash
D=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit -j 8 > build-audit/build.log 2>&1
SDL_VIDEODRIVER=dummy $D/build-deps/pyenv/bin/ctest --test-dir build-audit \
  -j 8 --timeout 300 --output-on-failure > build-audit/ctest.log 2>&1

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_SANITIZE=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit-asan -j 8 > build-audit-asan/build.log 2>&1
$D/build-deps/pyenv/bin/ctest --test-dir build-audit-asan \
  -j 8 --timeout 300 --output-on-failure > build-audit-asan/ctest.log 2>&1
```

上述配置、构建、CTest均exit=0。普通42/42（31.45s），ASan+UBSan 41/41（11.47s）；GUI仅普通版，dummy后端17.15s。整板自检EPDiy为71项、PQ-BN为70项，均无失败、预算违规。第三方SDL2有编译警告；关掉GUI的配置提示IMGUI/SDL2变量未使用，这不是产品错误。

## 修复前隔离构建

只导出构建所需的本地已有对象，不切换检出或替换当前源文件。导出只做一次；不能覆盖已有实验目录中的工作。

```bash
mkdir -p build-audit/before-src
git archive 0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3 \
  CMakeLists.txt apps system platform drivers boards ports simulator tests tools third_party \
  | tar -x -C build-audit/before-src
cmake -S build-audit/before-src -B build-audit/before -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src > build-audit/before-configure.log 2>&1
cmake --build build-audit/before -j 8 > build-audit/before-build.log 2>&1
$D/build-deps/pyenv/bin/ctest --test-dir build-audit/before -j 4 --timeout 300 \
  -R '^scenario-(ai_ask|standby|reader_basic)-metalio-plus$|^scenario-ai_ask-metalio-plus-pqbn$' \
  --output-on-failure > build-audit/before-scenarios.log 2>&1
```

旧配置/构建/4项场景均exit=0；只声明旧版4项通过，没有重跑旧版全套。探针用相同观测代码链接旧库；运行器`sim_main.c`及相关接口在两提交间未改动。旧版正式场景来自归档中的原测试。

## 探针与运行方式

`legacy_probe.c`复制第五轮探针，只调整引用路径和入口名；继续只读引用往轮辅助程序。`probe.c`是本轮补充实验。`run.py`由第五轮运行器复制并将所有写入改为本轮目录及`build-audit/r6-probes/`，增加AUD7输出提取与55s墙钟超时。没有运行会覆盖旧记录的往轮`run.py`。

运行器每次编译探针并链接未经改动的产品静态库；**调用必须顺序执行**，共享可执行文件和`results.json`。`AUDIT_END`是模拟虚拟截止时间，与55s墙钟超时不同。`--before`使用旧库，`--asan`使用当前sanitizer库，不能两者组合。请给出明确实验名。

```bash
P=docs/audits/2026-11-03-codex-r6/experiments/run.py
# 第五轮原反例（每条可加 --before 比较旧版）
AUDIT_END=40s python3 $P r5_service_midcut r5_ai_speaker_fail
AUDIT_END=60s python3 $P --pqbn r5_system_redraw r5_system_redraw_control

# 本轮四个问题的最小重放
AUDIT_END=60s python3 $P --pqbn r6_gc16_large_other r6_gc16_partial
AUDIT_END=40s python3 $P r5_service_midcut
AUDIT_END=60s python3 $P r6_audio_tone_stall r6_audio_cancel_reopen r6_p05_off

# 正常与状态边界（其余实际组合见commands.txt）
AUDIT_END=60s python3 $P r6_audio_network r6_audio_flash r6_audio_display
AUDIT_END=60s python3 $P r6_audio_permanent r6_audio_newstream r6_audio_refault
AUDIT_END=60s python3 $P r6_ai_late_failure r6_ai_new_turn
AUDIT_END=60s python3 $P --pqbn r6_gc16_recuts
```

完整68个组合的重放命令见 [commands.txt](commands.txt)，它由已完成记录生成，包含40个当前普通、16个旧版、12个当前sanitizer组合。每条实际二进制命令、虚拟截止时间、exit与输出摘录见 [results.json](results.json)。**68个进程exit=0只说明实验运行结束，不代表68项正确性检查通过**；四个问题均在正常退出的实验中观察到。

| 探针 | 判据 / 覆盖 |
| --- | --- |
| `r5_service_midcut` | 原AUD6-001：是否续播、实际播放样本量、失败回调、写入/睡眠释放 |
| `r5_service_timeout/replace` | 原启动故障放弃、重入回调、新流重置等待期 |
| `r6_audio_permanent/newstream/refault` | 已启动流永久掉电、切流、恢复后再掉电的截止与资源释放 |
| `r6_audio_tone_stall/tone_wait/cancel_reopen` | 停滞期提示音、重开期提示音、重开阻塞期间取消 |
| `r6_audio_network/flash/display` | 4s网络间隔、1500ms任务冻结、两档全屏GC16并行播放，不误报时钟停滞 |
| `r5_ai_speaker_fail/recover/normal` | 原AUD6-003及真实UI/AI/Hub对照，故障不切断模拟Wi-Fi |
| `r6_ai_late_failure/new_turn` | 文本先完成后播放失败；下一轮清除旧失败 |
| `r5_system_redraw/control` | 恢复全屏之后的局刷是否再多清整屏 |
| `r5_redraw_twice/overlap`、`r6_gc16_recuts` | 连续失电、清理进行中更新/再丢电、回调次数、最终可见像素 |
| `r6_gc16_overlap_lost/large_other/partial` | 清理不覆盖或仅部分覆盖受损区时，受损区新请求的回调时像素 |
| `r6_p05_off`、`r5_display_service` | 持续断电每请求耗时、是否无穷重试、恢复后可用 |
| `r5_gc16_powerup_fail`、`r6_gc16_start_fail` | 用户GC16启动即失败后的残留清理状态；不据短暂playing=1判断挂死，补等驱动归零 |
| `r4_normal/normal_activity/service_normal` | 正常刷新、三次睡眠唤醒、录放音及摄像头模型并行；9组前后比较 |

可见像素按原生到逻辑坐标旋转后取均值，缓冲仅为宿主观测内存，不作为目标设备预算。最终图像正确与回调时图像正确分开判断。`r6_audio_display`默认档探针中的即时日志可能在音频结束前输出；报告的3000ms来自模拟截止后的最终指标，不把早期2055ms当成丢帧。

## 紧凑证据、检查与限制

```bash
python3 docs/audits/2026-11-03-codex-r6/experiments/summarize.py
python3 docs/audits/2026-11-03-codex-r6/experiments/validate.py
```

- [metrics-evidence.json](metrics-evidence.json)：68个组合的相关指标、告警、预算与图像hash。
- [comparison.json](comparison.json)：4个正式场景与5个正常探针的全部指标差异；9组最终PNG相同，5组探针输出逐行相同。缺省计数保留null，解读时按0。
- [suite-evidence.json](suite-evidence.json)：正式测试清单、重点场景、所有普通场景蓝牙计数与环境。
- [artifact-validation.json](artifact-validation.json)：结构、编号、源码位置、链接、大小、sanitizer日志和只读边界核验。

日志、WAV、PNG、Flash镜像与二进制留在`build-audit/`或`build-audit-asan/`。已目视检查语音失败完成态和新轮正常完成态的PNG，提示文字可见。12个sanitizer组合均无ASan/UBSan诊断，探针设置`ASAN_OPTIONS=detect_leaks=0`；不宣称LeakSanitizer、TSan通过。

所有时间、像素和资源结果来自模拟器；不是实机时延或硬件画质。未执行硬件测试、Linux复跑、真实摄像头/网络测试、无限时间压力测试或完整历史问题再审。本轮围绕六项指定处理、相邻状态组合与现有正式回归，已知历史待办不因本次未重测而视为解决。
