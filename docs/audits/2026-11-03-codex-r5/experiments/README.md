# 第五轮复审：离线复现

对象为 `0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3`；修复前对照为 `89b112620730902f0fa1594e83ffb146b08749be`。所有命令在仓库根目录运行。未改产品、既有测试或往轮审计文件，没有联网或接触设备。

## 构建与正式测试

实际使用任务指定的本地依赖。以下配置、构建、CTest 都 exit=0：普通构建42/42（21.71s），ASan+UBSan构建41/41（9.10s）。GUI仅普通构建执行，dummy后端、实时速度，16.94s通过。

```bash
D=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit -j 8 > build-audit/build.log 2>&1
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure > build-audit/ctest.log 2>&1

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_SANITIZE=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit-asan -j 8 > build-audit-asan/build.log 2>&1
$D/build-deps/pyenv/bin/ctest --test-dir build-audit-asan -j 8 --timeout 300 --output-on-failure > build-audit-asan/ctest.log 2>&1
```

环境：macOS 15.7.9 arm64，Apple clang 17.0.0，CMake 3.28.3，指定依赖中的CTest 4.4.4，探针运行器Python 3.9.6。第三方库有弃用等编译警告，未将其写成产品缺陷或声称“构建零警告”。

## 修复前隔离对照

使用 `git archive` 导出本地已有对象，没有切换当前检出、替换产品文件或新建分支。仅首次建目录时执行导出：

```bash
mkdir -p build-audit/before-src
git archive 89b112620730902f0fa1594e83ffb146b08749be | tar -x -C build-audit/before-src
cmake -S build-audit/before-src -B build-audit/before -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit/before -j 8 > build-audit/before-build.log 2>&1
$D/build-deps/pyenv/bin/ctest --test-dir build-audit/before -j 4 --timeout 300 \
  -R '^scenario-(ai_ask|standby|reader_basic)-metalio-plus$|^scenario-ai_ask-metalio-plus-pqbn$' \
  --output-on-failure > build-audit/before-scenarios.log 2>&1
```

历史配置、构建、4个正式场景均exit=0，场景4/4（1.52s）。当前与历史都使用同一批本地依赖。`--before`探针使用旧静态库和本轮相同观测代码；正式历史场景来自历史归档自己的测试。旧版本未注册本轮新增的PQ-BN整板CTest，本轮不把4/4称为旧版本全套通过。

## 本轮探针

`probe.c` 是新增观测代码；`legacy_probe.c` 从第四轮 `probe.c` 复制，仅改入口名及引用路径，再引用原先的第三/二/一轮探针。它们链接未经修改的产品静态库。不要运行往轮 `run.py`，以免覆写旧结果。

运行器每次会重新编译探针；多个 `run.py` 调用须顺序执行，因为会写同一可执行文件及 `results.json`。请显式指定实验名。`--before` 只用于 `r3_`、`r4_` 等旧实验；新 `r5_` 实验依赖新接口，不适用于旧库。不要组合 `--asan --before`。

以下是本轮实际运行组的可复现写法。重复校准的同名结果按 `(name, sanitizer, pqbn, before)` 更新，`results.json` 保留53个最终组合：33个当前普通、9个当前sanitizer、11个历史对照。

```bash
P=docs/audits/2026-11-03-codex-r5/experiments/run.py
python3 $P r3_display_p05 r4_p05_preoff r4_service_failed r4_service_normal r4_ai_failed r4_tca_init_change
python3 $P --pqbn r3_display_p05 r4_p05_preoff r3_display_multi r3_display_transient r3_display_vcom
python3 $P --before r3_display_p05 r4_p05_preoff r4_service_failed r4_service_normal r4_ai_failed r4_normal r4_normal_activity
python3 $P --before --pqbn r3_display_p05 r4_p05_preoff r4_normal r4_normal_activity
python3 $P r4_normal r4_normal_activity r4_held_supply
python3 $P --pqbn r4_normal r4_normal_activity r4_held_supply
python3 $P r5_service_midcut r5_service_timeout r5_service_replace r5_redraw_twice r5_redraw_overlap
python3 $P --pqbn r5_redraw_twice r5_redraw_overlap r5_redraw_gc16_other r5_gc16_powerup_fail
AUDIT_END=40s python3 $P r5_display_service
AUDIT_END=40s python3 $P --pqbn r5_display_service
AUDIT_END=60s python3 $P --pqbn r5_system_redraw r5_system_redraw_control
AUDIT_END=40s python3 $P r5_ai_speaker_fail r5_ai_speaker_recover r5_ai_normal
AUDIT_END=40s python3 $P --asan r5_ai_speaker_fail r5_service_midcut r5_service_timeout r5_service_replace r5_redraw_twice r5_display_service
AUDIT_END=60s python3 $P --asan --pqbn r5_system_redraw r5_redraw_twice r5_redraw_overlap
```

AI恢复及正常组原运行使用默认20s截止；最终指标在这之后不需要新输入，40s复跑可进一步观察声音排空。每条实际命令及模拟截止时间保存在 `results.json`。本轮ASan+UBSan探针设置 `ASAN_OPTIONS=detect_leaks=0`，不宣称LeakSanitizer通过。当前9个sanitizer组合均exit=0，无sanitizer错误。

`AUDIT_END` 是虚拟时间；运行器的单进程墙钟超时为40s，彼此独立。**探针exit=0表示实验完整结束，不表示被观察行为正确**。新问题恰恰来自这些exit=0实验的错误状态。结果判定须看回调、像素、队列、标签及指标，不能只数进程成功率。

| 实验 | 观察目标与关键判据 |
| --- | --- |
| `r3_display_p05` / `r4_p05_preoff` | 断P05时不得成功，恢复后可成功；前后对照 |
| `r4_service_failed` / `normal` | 已收8帧恢复后播放480ms，正常路径不退化 |
| `r4_ai_failed` | 麦克风失败时不发问，`mic_unavailable=1`、采音与请求0 |
| `r5_service_timeout` / `replace` | 永久启动失败有界通知、回调可重入、资源释放；新流截止期重置 |
| `r5_service_midcut` | **AUD6-001**：播放120ms后断P14，恢复10s仍无进度；手动取消再播正常 |
| `r5_redraw_twice` / `overlap` | 清理中再掉电/新请求，每用户请求一次回调，最终图像与pending正确 |
| `r5_display_service` | P05关闭5s，无无限重试；恢复后服务与HAL归空闲 |
| `r5_system_redraw` / `control` | **AUD6-002**：故障恢复成功后，下一小更新10.119s，对照0.329s |
| `r5_redraw_gc16_other` | 同根因HAL边界：局部GC16未覆盖损坏区，先前目标推迟到下一GL16才恢复 |
| `r5_ai_speaker_fail` / `recover` / `normal` | **AUD6-003**：真实AI/UI/Hub，16.842s失败提示被18.860s完成覆盖；可恢复故障不丢声音 |

`r5_visible` 是探针宿主缓冲，未加入模拟目标内存账；不能据此判断目标设备资源。区域灰度取自可见屏幕，按 `ink_native_to_logical` 处理90°旋转。首版遗漏旋转导致均值无效，已修正并重跑所有引用均值实验；最初的AI探针缺头文件编译失败也已改正。这些是审计工具的校准，没有修改产品。系统PQ-BN全屏恢复约10s，观测等待已延长至20s/22s，未将初版9s等待不足计为产品挂死。

真实产品AI链使用模拟Wi-Fi维持Hub可达，使P14音频故障不会被共享4G断网掩盖；未访问真实无线网络。`snap-ai-final.png` 已人工查看，最终标签确为“回答完成”。摄像头并行对照只验证模型返回614400字节，不证明真实摄像头驱动可用。

## 紧凑证据与结构核验

```bash
python3 docs/audits/2026-11-03-codex-r5/experiments/summarize.py
python3 docs/audits/2026-11-03-codex-r5/experiments/validate.py
```

- `results.json`：53个组合的命令、exit、关键输出。大日志、WAV、PNG、trace及Flash镜像留在 `build-audit/r5-probes/`。
- `metrics-evidence.json`：每个组合的相关指标、告警、预算和图像hash。
- `comparison.json`：4个正式场景、5个正常探针对照的全部指标差异及图像hash；缺省计数器原样记null，解读时按0。
- `suite-evidence.json`：正式测试清单与摘要、重点场景指标、环境。
- `artifact-validation.json`：JSON结构、编号、源码引用、大小和仓库只读边界核验；这是审计产物检查，不是新增产品测试。

脚本只从本地已生成结果提取证据。指标用虚拟时间和模拟硬件模型，不能当作实机时延、真实电流或真实显示质量。未执行TSan、实机测试、无限时间压力测试或完整历史问题再审。
