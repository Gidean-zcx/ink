# 第七轮离线复审实验

当前对象 `06a882fb7c39ddaac41a111c161e2f26c9798070`；修复前 `8c38f17a5c2525b1859dd0c8e2723844734f6a81`。所有命令从仓库根目录执行。只写本轮目录与 TASK 允许的 `build-audit/`、`build-audit-asan/`，无联网、设备访问、产品改动或子代理。

## 构建与正式测试

```bash
python3 docs/audits/2026-11-03-codex-r7/experiments/suites.py current
python3 docs/audits/2026-11-03-codex-r7/experiments/suites.py before
python3 docs/audits/2026-11-03-codex-r7/experiments/suites.py asan
```

`suites.py` 使用 TASK 指定的本地字体、Python、LVGL、ImGui、SDL2 路径，开启 Werror、`FETCHCONTENT_FULLY_DISCONNECTED=ON`；8 路构建和 CTest，测试 timeout=300。当前 42/42、ASan+UBSan（关闭 GUI）41/41；历史仅重跑 4 个正常场景，未声称历史全套通过。配置/构建/测试退出码在 [suite-steps.json](suite-steps.json)，汇总在 [suite-evidence.json](suite-evidence.json)。

历史构建用本地 `git archive` 隔离导出需要的源码到 `build-audit/before-src`，不会 checkout 或替换当前文件。为避免覆盖，`before` 模式只用于该目录尚不存在时的首次构建；已有构建直接重放探针即可。所有日志、库、可执行文件、PNG、WAV 与 Flash/SD 输出均留在构建目录。

## 探针重放

`replay.c` 复制第六轮 `probe.c`，只修改 include 路径和入口名；旧的 `legacy_probe.c` 及往轮实验均通过 include 只读复用。`probe.c` 是本轮新增边界实验。`run.py` 每次重新编译探针并链接对应的原始产品库；历史模式也使用历史头文件与运行器。运行器所有持久化写入指向本轮目录，逐实验立即更新结果，未运行会改写往轮报告的旧运行器。

```bash
P=docs/audits/2026-11-03-codex-r7/experiments/run.py
AUDIT_END=60s python3 $P --pqbn r6_gc16_large_other r6_gc16_partial
AUDIT_END=60s python3 $P --before --pqbn r6_gc16_large_other r6_gc16_partial
AUDIT_END=60s python3 $P r5_service_midcut r6_audio_tone_stall r6_audio_cancel_reopen
AUDIT_END=60s python3 $P --before r5_service_midcut r6_audio_tone_stall r6_audio_cancel_reopen
AUDIT_END=60s python3 $P r7_audio_tone_normal r7_audio_replace_reopen
AUDIT_END=60s python3 $P --before r7_audio_tone_normal r7_audio_replace_reopen
AUDIT_END=60s python3 $P --pqbn r7_display_multi r7_display_control r7_display_recut
AUDIT_END=60s python3 $P r7_rails_deadline r7_rails_cancel r7_rails_cancel_fault r7_rails_wake_fault
```

`--pqbn` 选择 PQ-BN 档案，缺省为 EPDiy；`--before` 是修复前库；`--asan` 是当前 sanitizer 库（不与 `--before` 组合）。`r7_rails_*` 依赖新增板级 API，只用于当前。`AUDIT_END=60s` 是虚拟时间截止；每进程还有 55 s 墙钟超时。各次运行必须顺序执行，共享探针可执行文件与 `results.json`。显式给出实验名，不依赖缺省列表。

完整 84 个独立组合的命令在 [commands.txt](commands.txt)：当前普通 44、历史 24、sanitizer 16。为确保历史头文件/库同源，历史 24 组重跑一次，结果表保留最后一次。全部进程 exit=0 只代表运行结束，AUD8-001 仍有实测错误行为。

| 组 | 覆盖与判据 |
| --- | --- |
| `r6_gc16_large_other/partial/overlap_lost` | 原 AUD7-001；必须检查受损区自身的回调时间与像素，不能拿最终画面代替 |
| `r7_display_multi/control/recut` | 16 个分离区域并行、三轮连续更新、补清中再次丢电；各请求一次回调，成功时符合最新目标 |
| `r5_system_redraw/control` | 全屏恢复后局刷的耗时，不能再多做全屏清理 |
| `r5_service_midcut`、`r6_audio_tone_stall/tone_wait` | 实际播放样本量、丢失计数、提示音与暂存帧互不覆盖 |
| `r6_audio_cancel_reopen`、`r7_audio_replace_reopen` | 重开阻塞期间停止/换流，不补播旧帧 |
| `r7_audio_tone_normal` | 正常 18 s 流中插入提示音；用 `audit.tone_ms` 看首声响应，不能只看最终完整样本量 |
| `r6_audio_permanent/newstream/refault` | 故障期限、换流后重置、重复故障及资源释放 |
| `r6_audio_network/flash/display` | 网络稀疏供帧、极端任务冻结、刷新并行；区分预期欠载与误报停滞 |
| `r6_p05_off` | 无恢复状态时三次持续断电失败耗时，第四次复电成功 |
| `r7_rails_deadline` | 有恢复标志时绝对截止；清标志后不继承等待 |
| `r7_rails_cancel/cancel_fault/wake_fault` | 取消入睡、取消时恢复失败、唤醒恢复失败转后台，成功后 flag=0 |
| `r4_normal/normal_activity/service_normal` | 两档正常刷新、录音播放、睡眠唤醒、摄像头模型并行对照 |
| `r5_ai_*`、`r6_ai_late_failure/new_turn` | 正常问答、扬声器故障/恢复、失败提示与文字完成事件顺序、新轮清除 |

使用宿主缓冲观测可见像素，按面板旋转转换坐标；不是设备预算内的应用缓冲。早期 `r6_audio_display` 输出可能尚未播完，最终样本量以 `metrics.json` 为准。`audio.stall_lost_ms` 来自播放延迟估计，含 DMA 边界等待，不能解释成精确丢失样本数。P05 的 300 ms 是轮询截止值，不含额外调度/锁/失败关断。

## 已知高级待办复核

```bash
python3 docs/audits/2026-11-03-codex-r7/experiments/known.py
```

复用历史 `gen_race` 与 `pqbn_clock.c`，修正命令里的旧目录名后针对当前代码运行。F-6 仍有迟到 AI 翻页（最终第 3 页）；F-8 内核状态检查仍有 92.2% 静止像素时间戳等级错误，探针按预期 **exit=1**，不是编译或环境失败。保留旧编号，不放入仅允许新问题的 `findings.json`。详细命令与输出在 [known-results.json](known-results.json)。

## 证据整理与边界检查

```bash
python3 docs/audits/2026-11-03-codex-r7/experiments/summarize.py
python3 docs/audits/2026-11-03-codex-r7/experiments/validate.py
```

- [results.json](results.json)：逐组合二进制命令、退出码和关键日志。
- [metrics-evidence.json](metrics-evidence.json)：相关指标、告警、预算、最终 PNG hash。
- [comparison.json](comparison.json)：4 项正式场景和 5 组正常探针的完整指标差异，9 组最终 PNG 均相同。
- [artifact-validation.json](artifact-validation.json)：编号、JSON、源码位置、相对链接、文件大小、sanitizer、Git 只读边界及关键行为断言。

sanitizer 探针设置 `ASAN_OPTIONS=detect_leaks=0`。本轮未跑 TSan、Linux、真实硬件/摄像头/网络或长期老化；这些模拟结果不构成实机验收。
