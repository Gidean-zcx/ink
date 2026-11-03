# 第八轮补充实验

对象 `38511ec9b490b20210087dc17a9df31eec0ded6d`；历史对照 `06a882fb7c39ddaac41a111c161e2f26c9798070`；执行日期 2026-11-04。所有命令从仓库根目录执行。只读产品源文件、既有测试和档案；源码、结果摘要写在本目录，二进制、WAV、PNG、trace 和完整日志留在任务允许的 `build-audit/`、`build-audit-asan/`。

环境：macOS 15.7.9 / arm64，Apple clang 17.0.0，CMake 3.28.3，本地依赖环境 CTest 4.4.4，系统 Python 3.9.6。依赖根目录按 TASK 固定为 `/Users/bytedance/Documents/Workspace/ink`，脚本启用 `FETCHCONTENT_FULLY_DISCONNECTED=ON`，不联网。音频、显示和 AI 都是模拟器运行；没有实机、真实声学、真实 Opus 或真实 AI 服务结论。

## 构建与既有检查

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/suites.py current
python3 docs/audits/2026-11-03-codex-r8/experiments/suites.py asan
python3 docs/audits/2026-11-03-codex-r8/experiments/suites.py before
```

`current` 为 `INK_WERROR=ON`，43/43 通过；`asan` 加 `INK_SANITIZE=ON, INK_GUI=OFF`，42/42 通过；`before` 为无 GUI 的历史正常场景对照，5/5 通过。脚本为 CTest 设置 `SDL_VIDEODRIVER=dummy`；完整 configure/build/CTest 参数、退出码和日志位置见 [suite-steps.json](suite-steps.json)，逐测试结果见 [suite-evidence.json](suite-evidence.json)。

`before` 用 `git archive` 只导出构建所需目录到 `build-audit/before-src/`，不切换分支或替换当前源文件。该导出目录存在时脚本拒绝覆盖；重复构建可使用 `suite-steps.json` 内已记录的 before 三条命令，无需再次导出。

## 业务、故障与长静止探针

[probe.c](probe.c) 包含本轮变频变幅音频、连续提示音、故障取消/换流和长静止刷新实验。[replay.c](replay.c) 复用第七轮及更早审计的只读探针，因此本目录需要保留在完整仓库中使用。`run.py` 链接实际产品静态库；`--before` 链接历史库及头文件，`--asan` 链接内存检查库，`--pqbn` 仅选择既有 PQ-BN 档案。

以下为本轮 45 组实际运行的等价分组命令；`AUDIT_END` 是虚拟终止时间，每个子进程另有 55 s 主机超时。重复执行会替换同组合的结果摘要，并在构建目录重生成运行文件。不要并行启动多个 `run.py` 写同一结果文件。

```bash
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py r7_audio_tone_normal r6_audio_tone_stall r6_audio_tone_wait r6_audio_cancel_reopen r7_audio_replace_reopen r6_audio_permanent r4_normal r4_normal_activity r4_service_normal r6_audio_display r6_audio_network r6_audio_newstream r6_audio_refault r8_audio_control r8_audio_tones r8_audio_fault_cancel r8_audio_fault_replace r5_ai_normal r5_ai_speaker_recover r6_ai_new_turn
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --pqbn r4_normal r4_normal_activity r6_audio_display r7_display_multi r7_display_recut r7_rails_cancel_fault r7_rails_wake_fault
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --pqbn r8_idle
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --asan r8_audio_control r8_audio_tones r8_audio_fault_cancel r8_audio_fault_replace r6_audio_permanent r6_ai_new_turn
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --before r7_audio_tone_normal r4_normal r4_normal_activity r4_service_normal r6_audio_display r8_audio_fault_cancel r8_audio_fault_replace
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --before --pqbn r4_normal r4_normal_activity r6_audio_display
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r8/experiments/run.py --before --pqbn r8_idle
```

这 45 组进程均 exit=0；探针中的业务结果仍需对照输出和指标，不能把 exit=0 当作没有缺陷。原始命令、摘录见 [results.json](results.json)。故障注入中的失败回调、一次 AI 声音失败及 DMA 损失是预期证据，不混入“正常路径无新增失败”的判断。

`r8_audio_control` 与 `r8_audio_tones` 各用 50 帧、每帧 60 ms，逐帧改变频率/振幅，使缺样、重样有可辨识对照；后者加五次 60 ms 提示音。以下直接比较模拟器输出样本，不只是累计时长：

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/audio_integrity.py
```

[audio-integrity.json](audio-integrity.json)：去掉五段提示音后，48000 个语音样本逐个相同；故障取消后的尾音是全局提示音，换流尾部等于完整新流。全局提示音未因 `ink_audio_stop_all` 清除，在修复前后都存在，详见报告边界。补充 sanitizer 探针使用 `ASAN_OPTIONS=detect_leaks=0`；本轮不声称做了 LeakSanitizer 或真实多核竞态检查。

## PQ-BN 状态等价性

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/pq_checks.py
```

[pq_skip.c](pq_skip.c) 直接编译当前 `pqbn_v4.c`，比较跳步与逐相静止运行后的 hot/cold、tile_plan、band，核对实际扫行数，并比较下一次白转灰规划。普通、ASan/UBSan 各 23 组（跨 4096、65536、uint32 回绕，0/1/255/512/513/768/1024/2048/4096/4500/9000/65536 等跳步，512/684/4 行高度）。**编译均 exit=0，探针均 exit=1，11/23 组差异**；这是确认产品反例，不是 sanitizer 报错或构建失败。`pq_checks.py` 保留探针失败码，自身正常汇总退出。详见 [pq-results.json](pq-results.json)。

旧 F-8 正反对照：[clock_cycles.c](clock_cycles.c) 从第七轮 `pqbn_clock` 适配，两个版本使用相同当前内核；唯一差异是静止时是否调用新 `pqbn_v4_skip`。运行 20 相 + 静止 255 相，重复三次，不跨 4096；旧算法错误像素 92.2%，新算法 0。

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/auxiliary.py clock
```

上面的辅助脚本封装 [clock-cycle-results.json](clock-cycle-results.json) 中本轮已执行的四条精确命令；预期旧算法运行 exit=1，新算法 exit=0。原第七轮独立程序硬编码旧推进逻辑，不能原样重跑后就据此断言新产品仍未修复。

## PQ-BN CPU 指令级复核

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/pq_cost.py
```

[pq_cost.c](pq_cost.c) 以 1216×684、fb4/hot/cold 在 PSRAM、tile/band 在 SRAM、executor 在 IRAM、其余内核在 flash 的布局测量。与 `tools/iss/bench/run_pqbn.sh:33–48` 使用相同 S31 GCC 参数及 fitted/slow 模型；编译器为本机 `riscv32-esp-elf-gcc` 15.2.0。每个测量区段之前初始化并清空缓存；计时区段只含维护调用，不含初始化和打印。缓存冷启动、存储延迟、CPU 单发射等仍是仓库 ISS 假设，不是芯片实测。

白色静止屏的 255 相维护：7,881,837 条目标指令，fitted/slow 为 11,881,482 / 18,590,900 cycles；当前模拟器仅记 340×43=14,620 cycles。executor 空行对照只需 31/38 cycles，说明用错的是工作类别。脚本还测灰色、跨 4096 边界、单相 2 行返回 1 行的情况。完整编译/ISS 命令、指令数与退出码见 [pq-cost-results.json](pq-cost-results.json)。

该 JSON 保留首次辅助程序因宏 `W/H` 与结构字段冲突产生的编译失败；改为枚举后编译和两次 ISS 均退出 0。这不是产品构建失败。重跑会追加记录；取末次成功的 fitted/slow 结果。

## 已知 F-6、摘要及产出检查

```bash
python3 docs/audits/2026-11-03-codex-r8/experiments/auxiliary.py known
python3 docs/audits/2026-11-03-codex-r8/experiments/summarize.py
python3 docs/audits/2026-11-03-codex-r8/experiments/image_compare.py
python3 docs/audits/2026-11-03-codex-r8/experiments/validate.py
```

`auxiliary.py known` 封装 [known-results.json](known-results.json) 的历史 `gen_race` 重放命令，当前结果 page_turns=2、tools.calls.done=1，已知旧快照仍执行。本轮不修 F-6、不新增编号；重复 request id 仅核查代码，没有做重复网络包实验。

`summarize.py` 只读取运行文件，输出 [metrics-evidence.json](metrics-evidence.json)、[comparison.json](comparison.json) 和 [suite-evidence.json](suite-evidence.json)。`image_compare.py` 使用本机已有 Pillow 读取最终 PNG，输出 [image-comparison.json](image-comparison.json)，不改图像。11 组前后对照中 9 组 PNG 一致，两组 PQ-BN 有少量灰阶差异。

`validate.py` 检查 JSON 格式、问题编号、复核结论、证据计数与退出码、文件大小、相对链接，以及 HEAD 和产品工作区未变化；它只验证审计产物一致性，不代替软件测试。结果见 [artifact-validation.json](artifact-validation.json)。本目录没有构建日志，所有文件均小于 1 MB。
