# 补充实验与复现说明

对象为 `9cf64adc9ea0a7ce54d1e8f6e05603928e7f17a5`，2026-11-04 在 macOS arm64 离线执行。没有产品源码、既有测试或既有文档的改动，也没有设备操作。下面所有结果均为**模拟器实验**，不是硬件测量。

## 完整构建和测试

在仓库根目录执行。依赖均使用任务指定的已有文件；构建日志只写 `/tmp`。

```bash
audit_deps=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$audit_deps/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$audit_deps/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$audit_deps/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$audit_deps/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$audit_deps/build/_deps/sdl2-src"
cmake --build build-audit -j8
"$audit_deps/build-deps/pyenv/bin/ctest" --test-dir build-audit -j8 --timeout 300 --output-on-failure

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_SANITIZE=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$audit_deps/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$audit_deps/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$audit_deps/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$audit_deps/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$audit_deps/build/_deps/sdl2-src"
cmake --build build-audit-asan -j8
"$audit_deps/build-deps/pyenv/bin/ctest" --test-dir build-audit-asan -j8 --timeout 300 --output-on-failure
```

【复现】两套配置、构建、CTest 均 exit 0：普通 **56/56**，27.90 秒；ASan/UBSan **55/55**，14.60 秒。后者因为 `INK_GUI=OFF` 不运行 `gui-smoke`。实际编译参数含 `-fsanitize=address,undefined -fno-omit-frame-pointer`。重点集成测试 `lifecycle / net_auto / starts / ai_race` 分别通过 111 / 23 / 13 / 6 个检查，均完成。

摘要在 [baseline_results.json](baseline_results.json)。完整输出位于 `build-audit/test-out/`、`build-audit-asan/test-out/`，不作为交付文件复制。`metrics.json` 的 `status=ok` 本身不代表预算通过，须同时看进程退出码和 `budget_violations`。

## 组件实验

`build_probe.py` 复用项目生成的编译选项和链接库，将本目录的入口链接为独立模拟器。需要读内部状态的实验原样 include 产品 `.c`；没有替换其逻辑。`remote_feedback.c` 复用原有 `lifecycle.c` 的组件夹具；`audio_failed_start.c` 另有显式标明的失败与交错注入。

```bash
audit_exp=docs/audits/2026-11-04-codex-system-r3/experiments
PYTHONDONTWRITEBYTECODE=1 python3 "$audit_exp/run_probes.py" build-audit
PYTHONDONTWRITEBYTECODE=1 python3 "$audit_exp/run_probes.py" build-audit-asan
```

也可用 `--only net_recovery` 等只跑某一项。普通构建的前五项先逐项执行，之后用 `--collect` 汇总已观察到 exit 0 的结果，未因此重跑。不要并行运行两个汇总脚本，它们会写同一个结果文件。

每个子进程的完整命令、退出码、`AUDIT_*` 摘录和警告在 [results.json](results.json)。本轮六个实验都在普通与 ASan/UBSan 构建上完成，无 sanitizer 报错。除了 `hal_switch` 有显式通过预算，其余为观察型实验；**exit 0 表示实验完成，不表示产品行为正确**，应按下表比较输出。

| 实验 | 输入与正对照 | 实际结果与判断 |
| --- | --- | --- |
| `net_recovery.c` | 原样产品启动；9 s 用公开设置切到内部测试卡，10.5 s Wi-Fi 消失；40 s 再设同一个 auto | 17 s、40 s LTE 已可用却仍选失效 Wi-Fi；重设后 44 s LTE 在线。确认 AUD13-001，非主动禁用自动模式造成。 |
| `ai_lost_capture.c` | 3 s 按住 AI 键 14 s；5.5 s 断网 20 s；UI 任务读取状态 | 13 s 已失败且会话关闭，但 capturing=1；松键后 19 s、网络恢复后 28 s 均显示“正在想”。确认 AUD13-002。43 s 新一轮已进入回答，脚本结束前未等待其完整结束；下一轮恢复的完整证据使用既有 `ai_turn_lost`。 |
| `remote_feedback.c` | 4×4 合法画布；白色前画布，只解码新单元 x=1 黑像素且不发 LAST；执行真实反显到期回调，再补 LAST | 到期后 display=0，期望恢复的已提交像素=255，complete=0、updates=0、owed=0；LAST 后 updates=1、owed=1。确认 AUD13-003 的局部提前提交。不推算网络中的发生率。 |
| `ui_queue_probe.c` | UI 工作暂停时投递 80 个独立节点，使 24 格唤醒队列满，再释放 UI | count=80、fifo=1、busy_during_jobs=1、waiting=0。未发现节点遗失或该路径的忙闲覆盖缺口。只核实工作节点的 FIFO，不据此保证任意输入与工作之间的总排序。 |
| `audio_failed_start.c` | 注入一次 `hal_afe_start` 失败；在错误清理解锁边界让另一个任务开始采集，稍后再正常开始 | 交错中的 begin 返回 -1，没有“返回成功却被旧清理停掉”；稍后 begin 返回 0 且收到 16 帧。原先怀疑的静默失效未复现，不记问题。 |
| `hal_switch.c` | 不启动 hub 服务；在 Wi-Fi 建立一个原始 HAL WebSocket，切到同样可用的 LTE；等过 5 s 死连接检测期限 | 两种构建均 connected=1、closed=1、wait=0，预算通过。独立确认 AUD12-005 的 HAL 修复，不借用 hub 的主动关闭。 |

UI 队列夹具与主机资源清单脚本沿用上一轮审计方法；本轮重新编译、执行。所有脚本都使用合成数据、模拟器链路和本地中枢模型。

## 原有回归测试是否能识别退化

`mutations.py` 仅在内存中改动一处待编译文本，或读取 `git show c95f48c:system/services/res/exec.c`；不改原文件。生成的对象、程序和场景输出均位于 `build-audit/`。

```bash
PYTHONDONTWRITEBYTECODE=1 python3 "$audit_exp/mutations.py"
PYTHONDONTWRITEBYTECODE=1 python3 "$audit_exp/run_probes.py" build-audit --only hal_switch --hal-mutant
```

【复现】原有场景与预算保持不变，结果见 [mutation_results.json](mutation_results.json)：

| 恢复的缺陷 / 探索 | 原有测试结果 | 能说明什么 |
| --- | --- | --- |
| 短信缓冲退回 `text[320]` | lifecycle exit 1，3 个失败 | 实际验证模组收到的全文，能发现截断。 |
| 丢弃首次 `announce` 同步错误 | starts exit 1，4 个失败 | 能发现遗留会话、调用者未获失败。 |
| 初始化改回 c95f48c 的 yield 等待 | 墙钟 50 s 超时，子进程被终止 | 原优先级交错导致活性失败；不是普通断言失败或构建失败。 |
| 旧 AI 回调解锁后关闭当前会话 | ai_race exit 1，2 个失败 | 能发现新会话被关闭；未等到后续回答，`test.finished` 未到达。 |
| 删除远程 `holding` 门控 | lifecycle exit 1，2 个失败 | 能发现 A/B 像素混合及配额错误。 |
| 自动策略强制保持当前网络 | net_auto exit 1，6 个失败 | 能发现自动选网退化。 |
| HAL 提前写 `N.type`，恢复原切网缺陷 | **net_auto 仍 exit 0，23 个通过** | hub 的 `HE_LINK` 主动关闭掩盖了 HAL 缺陷。现有断言不足以单独验证 HAL。 |
| 同一 HAL 变异运行独立 `hal_switch` | exit 1，closed=0、wait=-1 | 独立实验能区分当前实现和旧缺陷，见 results.json。 |

另保留一次排除的短信边界探索：原测试号码已经是 20 位，实验多加一位成为 21 位，参数表正确拒绝。这不能算产品问题。最初结果名误写为 `sms_twenty_digits`，JSON 中有纠正注释；脚本中的复现名称已改为 `sms_twenty_one_digits`。其三个断言失败源于测试仍期待发送成功，不用于证明修复有效或失效。

变异脚本会保存子进程 exit 1，这正是负对照的预期。脚本自身正常结束不意味着每个变异场景通过。

## 可见画面与预算

【复现】没有改场景，直接检查普通 CTest 的 `remote_reconnect-metalio-plus` 输出。23.1276 s 的帧中，AI 回答面板覆盖下方远程提示；该时段仍在 20～26 s 的断网内，计数 `remote.offline_shown=1`。归档的 [remote-offline-covered.png](remote-offline-covered.png) 与原文件 `build-audit/test-out/remote_reconnect-metalio-plus/frames/00011_023127.6ms_GL16.png` 相同。还查看了 28.9135 s 的恢复帧及解锁后的画面。由此确认 AUD13-004，而不是把“设置了标签”当作“用户看见标签”。

【复现】`remote_browse` 的 trace 在 2.697663 s 出现 `link=wifi, up=true`，之后测试使用 Wi-Fi。三个换页样本的中位数 1363.872 ms、p95/最大值 1368.874 ms，低于回归预算 1500 ms，高于目标 800 ms；首屏 1865.186 ms，本地反馈最大 283.477 ms。`target_max` 缺口不是回归预算失败，也不应当写成需求已达成。

## 资源清单的解释范围

```bash
python3 "$audit_exp/stack_inventory.py"
python3 "$audit_exp/static_inventory.py"
```

【复现】两脚本 exit 0，摘录在 baseline_results.json。主机单函数栈较大者包括 JSON 解析 2640 B、cap 目录回复 1440 B、阅读器两函数各 1376 B；它们不是调用链总栈高水位。Mach-O BSS 清单是主机 ABI 下的符号跨度，可能含对齐，也不能直接当作设备内部 RAM。

本轮 PQ-BN AI 场景的 `mem.internal_total.min_free_bytes=33504`，比 32768 B 模型门槛多 736 B。`INK_PSRAM_BSS` 标记以及模拟器堆预算都不能替代设备链接 map；这里不将主机 BSS 再从模拟堆扣除，不作设备资源验收。
