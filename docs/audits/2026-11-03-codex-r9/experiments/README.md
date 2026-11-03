# 第九轮复现实验

审计对象 `31642df9fb101e79041d03b1edb215174c6dc116`，历史对照 `38511ec9b490b20210087dc17a9df31eec0ded6d`。2026-11-04 在 macOS arm64 离线执行，只有主代理，没有设备访问。命令从仓库根运行。

源文件及摘要仅写本目录；构建、完整日志、PNG、WAV、trace 留在获准的 `build-audit/` / `build-audit-asan/`。所有产品源码、原有测试、档案和文档不变。本目录脚本会重写对应结果摘要，不要并行启动两个写同一 JSON 的脚本。

## 1. 构建和既有测试

```bash
python3 docs/audits/2026-11-03-codex-r9/experiments/suites.py current
python3 docs/audits/2026-11-03-codex-r9/experiments/suites.py asan
python3 docs/audits/2026-11-03-codex-r9/experiments/suites.py before
```

`current`：Werror，43/43。`asan`：ASan/UBSan、无 GUI，42/42。`before`：用 `git archive` 导出旧版本至构建目录，无 GUI，跑 5 个正常对照场景。配置、构建、CTest 均 exit=0；所有依赖指定 TASK 的本地路径并开启 `FETCHCONTENT_FULLY_DISCONNECTED=ON`。历史源目录存在时脚本拒绝覆盖；需复跑时使用 [suite-steps.json](suite-steps.json) 内已记录的配置/构建/CTest 命令。逐测试名称见 [suite-evidence.json](suite-evidence.json)。

## 2. 跳步状态与扫行数

```bash
python3 docs/audits/2026-11-03-codex-r9/experiments/pq_checks.py
python3 docs/audits/2026-11-03-codex-r9/experiments/pq_extended.py
```

`pq_skip.c` 复用第八轮原 23 组反例；普通和 sanitizer 都变为 23/23 通过，包含返回行数的两个取整反例。[pq-results.json](pq-results.json) 保留逐例输出。

`pq_extended.c` 使用真实 init/phase/motion API 生成混合黑白/灰阶、完成计划、手动运动以及自动运动退出后的静止状态。普通和 sanitizer 各 552/552；540 个直接逐相对照，12 个极大步长使用周期参考：先跑至少 65536 相达到饱和，并明确核对再跑 65536 相完整状态相同，再折叠极长时间。它不声称实际跑过数十亿相。比较全部持久状态、实际扫行数，以及退出运动、换灰屏后 20 相的逐行输出。结果见 [pq-extended-results.json](pq-extended-results.json)。

前提是跳过期间没有待规划/未完成的驱动工作；手动运动标记保留时也已无推送。skip 不是驱动 phase 的通用替代。ASan 探针设 `detect_leaks=0`，不作全系统泄漏结论。

## 3. 指令级成本

```bash
python3 docs/audits/2026-11-03-codex-r9/experiments/pq_cost.py
python3 docs/audits/2026-11-03-codex-r9/experiments/pq_cost_extended.py
```

本机 ESP RISC-V GCC、与 `tools/iss/bench/run_pqbn.sh` 相同的编译/存储布局，fitted 和 slow 两档；真实内核、1216×684，像素状态在 PSRAM、tile/band 在 SRAM、executor 在 IRAM，其余内核在 flash。初始化和缓存清空不计入测量区段。

原探针 4 种维护 + 1 空行对照，扩展 9 种维护 + 1 空行对照；两脚本编译和 ISS 均 exit=0。原 `pq_cost.c` 的打印仍保留旧 `rows*43` 算式，当前计费须用 `42495 + rows*54949`，不能把这个历史打印当作新实现。原始区段计数、cycles、指令数和完整命令见 [pq-cost-results.json](pq-cost-results.json)、[pq-cost-extended-results.json](pq-cost-extended-results.json)。

主要 255 相漏账已修；扩展复现 AUD10-001：白屏 `4095→4096` 只扫 1 行，slow 0.424781 ms，模型仅 0.304513 ms。附带长跳测量不代表当前模拟器调用了长跳（它仍上限 255）。这些是仓库 ISS 模型内的测量，不是 S31 实机值。

## 4. 正常、并发、长静止和供电故障

`probe.c` 新增三次长静止 + 显示/语音并发探针，通过本目录 `replay.c` 复用前轮探针，链接未修改的产品库。所有输出均在本轮目录/构建目录；不会运行前轮脚本去覆盖前轮结果。

以下 10 条命令对应最终 22 个组合，每个进程 exit=0；`AUDIT_END` 为虚拟时间，单进程主机超时 55 s。

```bash
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --pqbn r8_idle r9_concurrent r9_concurrent_control
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --before --pqbn r8_idle r9_concurrent
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py r9_concurrent
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --before r9_concurrent
AUDIT_END=780s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --asan --pqbn r9_concurrent
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py r4_normal_activity r6_audio_display r7_audio_tone_normal
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --before r4_normal_activity r6_audio_display
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --pqbn r4_normal_activity r6_audio_display r7_display_multi r7_display_recut r7_rails_wake_fault
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --before --pqbn r4_normal_activity r6_audio_display
AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r9/experiments/run.py --asan --pqbn r7_display_multi r7_rails_wake_fault
```

- `r8_idle`：4/60/600/4 s 静止后局部刷新，与前轮相同的探针。
- `r9_concurrent`：4/60/600 s 显示静止后，分别播 100 帧变频、变幅语音（每段 6 s），并发全屏 GL16 黑屏、GC16 白屏；全部 6 回调成功，音频 18000 ms、欠载 0，最终显示/音频/存储为空。`control` 保留相同语音，省略显示。为隔离显示静止和自动锁屏，使用公开 `INK_NEED_NO_LOCK` hold，不阻止空闲降频。
- `r4_normal_activity`：正常显示、录放音、睡眠唤醒与并发摄像头；`r6_audio_display`：3 s 语音中刷新；`r7_audio_tone_normal`：18 s 语音中插入提示音。
- `r7_display_multi/recut`：16 区域、断电后补清/补清再断电；`r7_rails_wake_fault`：唤醒时 I2C 故障后恢复。

首次包装探针因旧探针内的 `ink_main` 宏覆盖而编译失败，修正本目录包装后通过，不是产品构建错误。初版并发未禁止自动锁屏，600 s 后的浅睡使最后一个输出标记未在截止前出现；保留 [preliminary-results.json](preliminary-results.json)，不作为完整测试通过。最终结果为 [results.json](results.json)，追加记录的辅助编译命令见 [probe-builds.json](probe-builds.json)。

## 5. 样本、指标、截图与产物核对

```bash
python3 docs/audits/2026-11-03-codex-r9/experiments/audio_integrity.py
python3 docs/audits/2026-11-03-codex-r9/experiments/summarize.py
/Users/bytedance/Documents/Workspace/ink/build-deps/pyenv/bin/python docs/audits/2026-11-03-codex-r9/experiments/image_compare.py
python3 docs/audits/2026-11-03-codex-r9/experiments/validate.py
```

6 个 WAV 的 352000 样本逐个相同，含 288000 语音样本及两段各 2 s 的记录静音；见 [audio-integrity.json](audio-integrity.json)。12 组历史对照中默认 EPDiy 6 组所有指标一致，10 组最终 PNG 字节相同；PQ-BN 阅读/AI 分别仅 26/71 个像素不同，见 [comparison.json](comparison.json)、[image-comparison.json](image-comparison.json)。后者只读图片，不改图像。

`validate.py` 检查 JSON 格式、问题/核实编号、命令退出码、证据数量、大小和相对链接，以及固定 HEAD、产品文件未变和未跟踪文件均在本审计目录；结果见 [artifact-validation.json](artifact-validation.json)。它是审计交付检查，不代替产品测试。
