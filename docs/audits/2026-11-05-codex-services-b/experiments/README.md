# B 路审计实验与复跑说明

对象提交：`4d23fc2a62f9ae39cc1e1bb0a1f1d98ce2cbb27f`。所有命令从仓库根运行，不联网、不接触设备、不使用子代理。实验只使用模拟输入和本机已有依赖。

`run_probe.py` 沿用 s3 运行器的编译、链接方式，增加 `--sim-arg`。它读取现成 CMake 目标的编译选项和链接库，用审计入口替换测试入口；`--extra` 在静态库前提供实验翻译单元。`product_probe.h` 原样 include 产品启动及现有集成测试助手。焦点与执行器实验只在公开 API 边界安排时序，没有删掉产品中的检查；OOM 观察器仅使下一次文字快照分配失败。全部产物在 `build-audit/` 或 `build-audit-asan/`。

这些探针有两种断言方向：新问题探针断言“缺陷确实出现”，因此退出 0 **不表示产品无缺陷**；修复核查及反向对照则断言正常行为。原 s3 录音探针保留其旧断言，退出 1 的具体原因见下表。不能只看进程退出码判断结论。

## 构建与现有测试

```sh
INK_DEPS=<本机路径>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build-audit -j 4
cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_GUI=OFF -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build-audit-asan -j 4
```

两种构建均退出 0，日志为 `configure.log`、`build.log`、`configure-asan.log`、`build-asan.log`。

```sh
INK_AUDIT_TESTS='^(integration-(media|text-.*|dictation.*|listening|power-menu|races|notify.*|ai-race|lifecycle|starts)|scenario-(ai_.*|voice_.*|keys_overlap.*|standby.*)|layering|sdk-boundary)$'
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 4 --timeout 400 --output-on-failure -R "$INK_AUDIT_TESTS"
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-audit-asan -j 4 --timeout 400 --output-on-failure -R "$INK_AUDIT_TESTS"
```

各 32/32，通过，退出 0；见 [ctest.log](ctest.log)、[ctest-asan.log](ctest-asan.log)。没有运行全量 85 项；未运行任务排除的相关实验。`integration-power-menu` 包含多次启动；39 份常规构建的指标摘要见 [baseline-metrics.json](baseline-metrics.json)。`ai_mic_unavailable` 的两条预期板级初始化告警由该场景主动注入；不能把“CTest 通过”写成“全部日志没有告警”。

## 补充实验

下面先定义路径，命令中的程序都只运行模拟器：

```sh
INK_AUDIT=docs/audits/2026-11-05-codex-services-b/experiments
INK_PREV=docs/audits/2026-11-05-codex-services-s3/experiments

python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/record_call_probe.c" --template sim-media --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_PREV/voice_boundary_probe.c" --template sim-dictation --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/text_oom_probe.c" --template sim-textinput --extra "$INK_AUDIT/text_observer.c"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/headset_probe.c" --template sim-media --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/audio_focus_probe.c" --template sim-media --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/power_picture_probe.c" --template sim-power-menu --sim-arg=--i2c-faults --sim-arg=1.0:pmic
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/text_boundary_probe.c" --template sim-textinput
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/voice_lifecycle_probe.c" --template sim-dictation --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/record_card_probe.c" --template sim-media --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/power_wait_probe.c" --template sim-power-menu
```

记录时较早的几项调用的是原 s3 `run_probe.py`；上面的本目录版本在这些参数下等价。完整模拟器参数和各次指标文件路径保留在日志的 `RUN` 行。耳机实验用 `audit-headset` 作模型标识，不使用硬件地址。

| 日志 | runner 退出码 | 决定性输出或检查 | 结论 |
| --- | ---: | --- | --- |
| [record-call.log](record-call.log) | 0 | 来电前 1210 ms；最终文件 3240 ms | AUD18-201 |
| [voice-call.log](voice-call.log) | 0 | `capture_busy=1 asr_frames_after_grace=26` | AUD18-201 |
| [text-oom.log](text-oom.log) | 0 | ABD 可重做到 ABC；`e4b8ade6` | AUD18-202、209 |
| [headset.log](headset.log) | 0 | `speaker_ms=990 media_state=1` | AUD18-204 |
| [audio-focus.log](audio-focus.log) | 0 | 仍有 alarm、无焦点、继续响铃并允许录音 | AUD18-205 |
| [power-picture.log](power-picture.log) | 0 | `on_panel=1 late=0 failures=2` | AUD18-206 |
| [text-boundary.log](text-boundary.log) | 0 | 光标 4 而非 3；100 字选区显示 132 字；隐藏框提交回调 +1 | AUD18-207、208 |
| [voice-lifecycle.log](voice-lifecycle.log) | 0 | `voice=3 rt=2 capture=1 focus=2`；锁屏后 `voice=0 rt=0 value=base` | 正常交接、收尾取消通过 |
| [record-card.log](record-card.log) | 0 | 录音时 USB `-9`；拔卡 `write_failed`，录音与采集均 0 | 收尾、剩余完整页可播放、借卡后拒绝新录音通过 |
| [power-wait.log](power-wait.log) | 0 | 两次关机请求在约 6059/7089 ms 进入 HAL；均 `on_panel=0`、计 late | 等待期间低电量重复输入和锁屏，无死锁；人工和低电量两条路径会分别请求关机 |

`power-wait-initial.log` 是探针首次运行：两次 HAL 回调均正确，但把 `test.finished` 加了两次，触发运行器“必须等于 1”的断言；之后只改审计探针的完成计数。该初次退出 1 不是产品死锁证据。

关机持久化必须连续运行两次，使用同一 Flash 和卡目录。原记录使用 `build-audit/audit-shutdown.img` 和 `build-audit/audit-shutdown-sd`；复跑请选择新目录，避免把前次数据当成本次初始状态：

```sh
INK_SHUTDOWN_DIR="$(mktemp -d build-audit/shutdown-replay.XXXXXX)"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/shutdown_audio_probe.c" --template sim-media --script "$INK_AUDIT/audio.script" \
  --flash-image "$INK_SHUTDOWN_DIR/flash.img" --sd-dir "$INK_SHUTDOWN_DIR/sd"
INK_AUDIT_REOPEN=1 python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/shutdown_audio_probe.c" --template sim-media \
  --flash-image "$INK_SHUTDOWN_DIR/flash.img" --sd-dir "$INK_SHUTDOWN_DIR/sd"
```

两次退出 0；见 [shutdown-audio.log](shutdown-audio.log)、[shutdown-audio-reboot.log](shutdown-audio-reboot.log)。第一次请求断电时录音尚在、持久化尚未完成；第二次记录不存在。没有注入存储错误。

文字两组探针也在 ASan/UBSan 构建复跑，退出 0，缺陷行为一致，未报内存访问错误：

```sh
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/text_oom_probe.c" --build build-audit-asan --template sim-textinput --extra "$INK_AUDIT/text_observer.c"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/text_boundary_probe.c" --build build-audit-asan --template sim-textinput
```

## 既有修复与反向对照

```sh
python3 "$INK_AUDIT/run_probe.py" "$INK_PREV/record_end_probe.c" --template sim-media --script "$INK_AUDIT/audio.script"
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/interrupt_probe.c" --template sim-notify \
  --extra docs/audits/2026-11-04-codex-services/experiments/interrupt_boundary.c
python3 "$INK_AUDIT/make_reverse.py"
python3 "$INK_AUDIT/run_probe.py" tests/integration/media.c --template sim-media --extra build-audit/audit-reverse/media_pause.c --script tests/integration/media.script
python3 "$INK_AUDIT/run_probe.py" tests/integration/media.c --template sim-media --extra build-audit/audit-reverse/media_member.c --script tests/integration/media.script
python3 "$INK_AUDIT/run_probe.py" "$INK_AUDIT/interrupt_probe.c" --template sim-notify --extra build-audit/audit-reverse/interrupt_boundary.c
```

两个媒体变异顺序运行：运行器使用同一 `audit-probes/media/` 编译位置。`make_reverse.py` 只生成构建目录中的副本，修正副本的本地头文件引用；产品文件从未被替换，所以不存在恢复产品源文件后的 make 时间戳问题。

| 对照 | 改动或原断言 | runner 退出码 / test.fail | 证据 |
| --- | --- | --- | --- |
| AUD17-005 原探针 | 原探针断言“结束丢失、再次停也没用” | 1 / 2 | [record-end-original.log](record-end-original.log)：实际 `recording=0 capture=0 focus=0 lost=15 completed=1`，两条旧缺陷断言失败，修复成立 |
| AUD15-006 当前代码 | 在响铃调用前结束打断 | 0 / 0 | [interrupt-fixed.log](interrupt-fixed.log)：`active=0 ring=0 state=none` |
| AUD16-002 反向 | 删除 `heard_ms` 无流时返回暂停位置的分支 | 1 / 1 | [reverse-media-pause.log](reverse-media-pause.log)：`(19200, 23280)` |
| AUD16-003 反向 | 仅删除 `rec_frame` 的成员检查 | 1 / 1 | [reverse-media-member.log](reverse-media-member.log)：`what was said before, no more (2820 ms)` |
| AUD15-006 反向 | 绕过 `IT.applying` 的单执行者检查 | 1 / 2 | [reverse-interrupt.log](reverse-interrupt.log)：`active=0 ring=1 state=alarm` |

反向用例中模拟器自身退出 0，运行器因测试断言失败退出 1。具体判定不能只凭模拟器的 `ok` 行。打断变异首次因副本找不到 `notify_internal.h` 而编译失败，记录在 `reverse-interrupt-setup.log`，修正副本引用后才得到上述有效运行；编译失败未作为反向对照成功。

没有重做文档中全部历史反向对照，特别是没有撤回 AUD17-005 的实现；它采用原始探针、现有回归及代码三方核查。

`add_audio_findings.py`、`register_text_power.py` 是边做边写的登记脚本；`finalize_report.py` 汇总已固定的事实，它们不是产品验证程序。指标及日志摘要由 `collect_evidence.py` 生成，见 [probe-results.json](probe-results.json)。会话原有的 `codex-run.log` 随运行增长超过 3 MB，完整移至 `build-audit/audit-runtime-codex-run.log`，未删减，不作为审计证据。
