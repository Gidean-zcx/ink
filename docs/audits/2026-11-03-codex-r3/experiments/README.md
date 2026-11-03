# 第三轮复审：复现说明

审计日期：2026-11-03。仓库提交：`95587e5a5f641fa4fd44c313f366e69f244faeb1`。
环境：macOS 15.7.9 / arm64，Apple Clang 17.0.0；本机 `cmake --version` 为 3.28.3，指定虚拟环境中的 CTest 为 4.4.4。无需联网或设备。

## 文件与判读

- `probe.c`、`extra_probe.c`：复用第二轮独立探针，更新搬迁后的板级包含路径；定时器探针改为打印实际重启返回值。
- `deep_probe.c`：本轮显示、唤醒、音频补充场景，通过包含旧探针复用回调和故障注入。只替换实验可执行文件的应用入口，不改产品实现。
- `run.py`：链接当前构建的原始静态库；每个场景独立进程、20 s 虚拟时间、40 s 墙钟超时。日志、图像、音频、可执行文件均在 `build-audit/`；摘要写入 `results.json`。
- `layout_check.py`：纯目录重组的源文件对照，以及从当前真实编译命令提取包含目录的正负编译检查。
- `history_check.py`：用 `git archive` 导出重组前后源码至 `build-audit/history/`，各自构建并比较全部测试指标及 PNG，不切换或修改工作区。
- `record.py`：把核实与新问题 JSON 渲染为报告中的对应章节，用于增量记录。
- `validate.py`：检查审计包格式、范围、证据完整性和关键观测，结果在 `artifact-validation.json`。

**探针的 exit=0 只表示进程正常结束，不表示产品通过。** 例如显示错误地回调成功、录音返回成功却零样本，正是本轮复现的缺陷。按下面的观测表判读。ASan 探针启用 AddressSanitizer 与 UndefinedBehaviorSanitizer，`ASAN_OPTIONS=detect_leaks=0`；本轮没有声称完成 LeakSanitizer 或 ThreadSanitizer 检查。虚拟时间不是墙钟性能测量。

`results.json` 按场景、sanitizer、pqbn 三元组保存最后一次结果；早期条目没有 `pqbn` 字段时表示 false。共 27 组已执行配置（含 5 组 sanitizer、3 组 PQ-BN），不是 27 个全部通过的断言测试。

## 离线构建

从仓库根目录执行。依赖是 TASK 指定的本机缓存；`CMAKE_EXPORT_COMPILE_COMMANDS` 供目录边界检查使用。

```bash
INK_AUDIT_DEPS=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_AUDIT_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" \
  -DINK_CJK_FACE=2 -DPython3_EXECUTABLE="$INK_AUDIT_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_AUDIT_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_AUDIT_DEPS/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_AUDIT_DEPS/build/_deps/sdl2-src" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-audit -j 8
"$INK_AUDIT_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

内存检查：同一配置命令将构建目录换为 `build-audit-asan`，再加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，随后对该目录 build、ctest。普通构建 40/40，sanitizer 构建 39/39，配置/构建/CTest exit 均为 0。完整日志保留在相应构建目录的 `build.log`、`ctest.log`，不纳入审计交付。GUI 冒烟由现有 CTest 使用 dummy 视频驱动，不调用真实摄像头。

## 已执行的补充实验

```bash
python3 docs/audits/2026-11-03-codex-r3/experiments/run.py \
  r2_timer_rearm_delete r2_wake_read_fault r2_wake_control \
  r2_audio_sleep_entry r2_audio_sleep_late r2_audio_sleep_race \
  r2_input_boot_fault r2_camera_deadline r2_display_cut
python3 docs/audits/2026-11-03-codex-r3/experiments/run.py \
  r3_display_transient r3_display_vcom r3_display_multi \
  r3_wake_touch r3_wake_audio r3_boot_audio r3_wake_permanent \
  r3_audio_oom r3_play_sleep_entry r3_display_p05
python3 docs/audits/2026-11-03-codex-r3/experiments/run.py --pqbn \
  r3_display_transient r3_display_vcom r3_display_multi
python3 docs/audits/2026-11-03-codex-r3/experiments/run.py --asan \
  r2_timer_rearm_delete r3_wake_touch r3_wake_audio r3_boot_audio r3_display_transient
build-audit/pools-test
python3 docs/audits/2026-11-03-codex-r3/experiments/layout_check.py
python3 docs/audits/2026-11-03-codex-r3/experiments/history_check.py
```

本轮以若干小批次执行上述命令；摘要中的每条 `command` 是实际子进程命令。不要并行运行两份 `run.py`，它们共用探针可执行文件及结果索引。

| 场景 | 本轮观测与结论 |
| --- | --- |
| r2_timer_rearm_delete | 删除期间回调重启 rc=-3，删除后及 500 ms 后 calls=1；常规/ASan 均无 UAF。 |
| r2_wake_read_fault / control | 恢复 cfg=c0,e3，POWER 2 个事件，后续音量后共 4 个。 |
| r2_audio_sleep_entry / late / race | mode=clock=1，有样本；睡眠返回或拒绝符合当前忙碌状态。 |
| r2_input_boot_fault | present=1 ready=0 时 keys=0，不再绕过失败的板初始化读键。 |
| r2_camera_deadline | 0/1/100 ms 预算约 1.443/1.443/100.352 ms 返回 -4，长预算失败可能仍处于 11 帧预热，不据此认定正常摄像头坏了。 |
| r2_display_cut | 持续失电回调 ok=0。 |
| r3_display_transient | 波形开始后 20 ms 切 PWRUP，再过 20 ms 复电并重使能 VCOM；EPDiy id=1 ok=1，PQ-BN id=1 ok=0。前者为 AUD4-001。 |
| r3_display_vcom | 波形中 VCOM_CTRL 低 20 ms 再恢复，两档均 id=1 ok=1，为 AUD4-001。 |
| r3_display_multi | 4 个 128×128 不相交请求。PQ-BN 同时在途，各失败恰一次，pending=0；EPDiy 首个失败，后续排队请求重新上电后成功。两档 id=5 重试成功。 |
| r3_wake_touch | POWER 唤醒时总线短故障，触摸持续按住；总线恢复数秒仍 cfg=00,80、keys=1，抬手后才恢复，见 AUD4-002。 |
| r3_wake_audio | 故障长于同步恢复窗口，睡眠已返回而 P14 未恢复；录音 rc=0，后来 power=1 仍 samples=0、mode=clock=0，见 AUD4-003。 |
| r3_boot_audio | 开机 io-expander 故障；应用入口解除后板与按键恢复，但录音 rc=0 持续无时钟、零样本，见 AUD4-004。 |
| r3_wake_permanent | 总线保持故障，应用仍能继续，键配置未恢复；后台以有间隔的重试持续到仿真结束。 |
| r3_audio_oom | 占满内部池后录音和播放均返回 -1；释放实验占用后 busy=0 且可睡眠，未见失败分支泄漏租约。 |
| r3_play_sleep_entry | 播放在睡眠进入期间启动，返回时 mode=clock=1；写 2400 样本，停止后可睡眠。 |
| r3_display_p05 | P05 拉低后 panel_ready 仍为 1，刷新仍成功。官方源码称 P05 为“摄像头/屏幕电源”；其电气影响尚未由原理图/实测确认，作为覆盖边界记录，不另列已确认严重/高问题。 |

显示截图只用于核实模拟器是否提交了目标像素，不能证明真实面板的光学行为。`display-image-evidence.json` 记录 `before` 与 `after-cut` 的不同像素数；PQ-BN 失败时已经有部分像素变化可以是已运行相位的结果，不能单凭该数字判定假成功。

## 目录重组证据

`layout-evidence.json` 的 85 对源/头文件对照和 12 个编译检查均符合预期。源对照只针对 `4309087 → dc085cb`：去注释及空白，并规范化 board 包含名前缀与 GUI 默认档案路径；不是 C 语义等价证明。结合人工 diff 核查和历史测试，两份快照各 37/37，39 份完整 metrics.json 对象与 410 张 PNG 哈希相同。

`history-evidence.json` 含两份快照各 configure/build/ctest 的完整参数、退出码和末尾摘要。它不验证后来各修复提交的行为与旧版本相同；那些修复由本轮当前版本测试和探针单独核查。

## 硬件依据与未验证项

2026-11-03 读取任务限定的官方源码检出 `690a023c66862af103a3187464373ee7c5efcd14`，目录为 `/Users/bytedance/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus/`。主要对照 `main/boards/common/power_policy/` 和 `main/boards/metalio-e-ink4-plus/` 的供电、按键、板级显示代码。未读取该 upstream 目录之外的私有实机资料。

Linux 摄像头只做代码核查；macOS 只编译和 GUI 冒烟，未请求媒体权限；未核验远端 CI，未运行实际 ESP-IDF 端口、SMP 并发或硬件功耗/光学校准。现有测试全部通过不覆盖本轮找到的故障交错。
