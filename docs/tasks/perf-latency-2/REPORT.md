# perf-latency-2 任务报告

基线：`6fbe0c4051843810a3252769ab34d150ac4c6097`。分支：`claude/codex-perf-latency-2`。状态：测量及两处优化完成，完整验证进行中。主检出与本分支开工时均为指定基线，已包含其它补缺任务。没有远端写入或接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 分段及前后对比 | 【实测，模拟器】完成，保留两处有收益的修改 | 下表、[汇总](measurements.json)、[逐样本数据](samples.ndjson) | A |
| 1b 反向对照 | 【实测，模拟器】通过，两处分别撤回均触发新断言 | 远程更新恢复 GC16；录音期间恢复额外 GC16，均退出 1 | B |
| 1c 确定性 | 【实测】普通版通过，ASan 待验证 | 原有两项确定性测试未改 | C、D |
| 2a 普通全量 | 【实测】通过 | 391/391，无跳过，665.27 s | C |
| 2b 内存检查 | 待完整验证 | 待补 | D |
| 2c S31 编译 | 待完整验证 | 只编译 ELF | E |
| 2d gcccheck | 待完整验证 | 两个产品 C 文件 | F |
| 2e 范围、隐私 | 待最终扫描 | 产品 C 新增 9、删除 12，净减 3 行；CMake 新增 3 行，合计净增 0 | G |

## 前后对比

【实测，模拟器】单位 ms；每格为 p50 / p95。每版六组现有场景，各 20 个独立进程；相同 seed=42、原档案、原脚本、原预算、RelWithDebInfo 和默认 annotated CPU 模式。逐动作样本合并后按 nearest-rank 取分位数，不平均每轮分位数。固定种子用于受控对照，重复运行不会形成真实硬件的时延分布。

| 指标（场景／档案） | 每版样本 | 优化前 | 优化后 |
| --- | ---: | ---: | ---: |
| 录音提示（ai_ask／EPDiy） | 60 | 505.3 / 506.1 | 505.3 / 506.1 |
| 录音提示（ai_ask／PQ-BN） | 60 | 620.8 / 623.0 | 542.4 / 569.3 |
| 首句文字（ai_ask／EPDiy） | 40 | 2534.7 / 2674.2 | 2534.7 / 2690.6 |
| 首句文字（ai_ask／PQ-BN） | 40 | 2165.3 / 2293.4 | 2165.1 / 2293.1 |
| 翻页可读（reader_basic／EPDiy） | 280 | 693.8 / 702.8 | 693.8 / 702.8 |
| 翻页可读（reader_basic／PQ-BN） | 280 | 534.1 / 566.4 | 534.1 / 566.4 |
| 首次可见反馈（reader_basic／EPDiy） | 300 | 239.6 / 366.2 | 239.6 / 366.2 |
| 首次可见反馈（reader_basic／PQ-BN） | 300 | 259.2 / 425.6 | 259.2 / 425.6 |
| 本地点按反馈（remote_browse／EPDiy） | 60 | 247.7 / 254.8 | 247.7 / 254.8 |
| 远程点按更新（remote_browse／EPDiy） | 60 | 1373.2 / 1373.8 | 1252.3 / 1252.8 |
| 解锁可见（standby／EPDiy） | 60 | 1002.8 / 1002.8 | 1002.8 / 1002.8 |

【实测，模拟器】远程更新 p95 降 120.9 ms（8.8%）；PQ-BN 录音提示 p95 降 53.7 ms（8.6%）。EPDiy 首句 p95 增加 16.4 ms（0.6%，刷新调度变化），没有宣称该项改善。解锁最大值前后均为 1002.8 ms。通用可见反馈、EPDiy 翻页、录音提示和远程更新仍有建议值缺口，未改建议值或门槛。

### 分段定位

【实测，模拟器】下表为优化后的 p50 / p95。输入为动作时间戳到首次 UI 处理；处理为 UI 处理到提交，包含绘制，首句还包含服务与中枢等待；送屏为提交到该更新的波形开始，包含复制、排队、上电、差分或 PQ-BN 规划；波形为实际显示 trace。各段分位数不能直接相加，逐样本五段（另含回调尾段）之和已自动核对。

| 指标 | 输入 | 处理 | 送屏 | 波形 |
| --- | ---: | ---: | ---: | ---: |
| 录音提示／EPDiy | 20.1 / 20.1 | 142.7 / 143.5 | 96.7 / 96.7 | 245.8 / 245.9 |
| 录音提示／PQ-BN | 20.1 / 20.1 | 142.7 / 143.5 | 123.6 / 134.1 | 245.5 / 323.9 |
| 首句文字／EPDiy | 20.1 / 20.1 | 1977.8 / 1999.3 | 88.2 / 222.6 | 448.6 / 448.6 |
| 首句文字／PQ-BN | 20.1 / 20.1 | 1853.5 / 1977.8 | 61.0 / 64.6 | 230.6 / 230.6 |
| 翻页／EPDiy | 21.9 / 21.9 | 109.2 / 138.5 | 108.1 / 109.6 | 454.2 / 454.2 |
| 翻页／PQ-BN | 21.9 / 21.9 | 109.2 / 138.5 | 125.4 / 163.4 | 279.7 / 294.7 |
| 可见反馈／EPDiy | 21.9 / 72.3 | 109.2 / 170.2 | 108.1 / 123.7 | 0 / 0 |
| 可见反馈／PQ-BN | 21.9 / 72.3 | 109.2 / 170.2 | 127.6 / 183.1 | 0 / 0 |
| 远程本地反馈 | 0 / 0 | 1.4 / 1.4 | 6.3 / 13.4 | 240.0 / 240.0 |
| 远程页面更新 | 0 / 0 | 652.6 / 653.1 | 123.7 / 123.7 | 476.0 / 476.0 |
| 解锁 | 0 / 0 | 366.5 / 370.5 | 188.3 / 192.3 | 444.0 / 444.0 |

【实现】远程原探针从 `remote.pointer` 处理开始计时，不含手指按住和触摸驱动时间，因此输入列为零；解锁原探针由电源服务发起，唤醒和派发计入处理。没有改变计时起点。通用 `ui.action_to_first_change_ms` 原定义止于波形开始，故其波形列为零，不能当作文字已可读。翻页组沿用原 TEXT 指标，含打开书时产生的 TEXT 样本，反馈组也含书架和回主页操作。

【实测，模拟器】最大的开销分别是：录音提示的波形与约 135 ms 绘制；首句的中枢／服务等待；翻页的波形；可见反馈的绘制和送屏／规划；解锁的唤醒处理与波形；远程更新的服务／传输／解码等待加绘制，以及波形。远程处理段中的绘制 p50/p95 为 149.2/149.2 ms，其余约 503 ms；原波形 p95 为 617.5 ms，改后 476.0 ms。

### 保留的改动及无收益部分

- **远程脏区溢出。**【实现】原先超过 `DIRTY_MAX=32` 就置 `repaint`，`show_update` 随即采用 FULL；现将溢出区域合入最后一格并保留最强刷新意图。首次连接、重连及中枢明确要求 FULL 的行为照旧。没有扩大数组、增设缓存或放宽预算。【实测】三次更新均从全屏 GC16 改为 GL16；原有成员、重连及整张图片一致性测试在普通全量中通过。
- **AI 提示层细化。**【实现】删除打开后立即投递的整层清刷，初始 FAST 反馈与后续真实文字更新保留。【实测】PQ-BN 提示更早完成；ai_ask 的刷新请求 36→34，EPDiy 总数 32→32、GC16 9→6；remote_browse 总数 18→16、GC16 7→3。EPDiy 录音提示没有收益，这处修改保留的依据是 PQ-BN 收益和减少清刷。
- **可见反馈、翻页、解锁。**未修改。分段没有证明一个同等小、无需新机制或硬件参数调整的改动可缩短这些路径。没有保留无收益的产品改动，没有调刷新相数、频率、预算或建议值。

## 复现与验证记录

复现时选择新的输出目录。先按任务书普通构建命令配置，ASan 仅换 `build-asan` 并加 `-DINK_SANITIZE=ON`。命令均使用 `taskpolicy -c utility`，构建和 CTest 最多 `-j2`，采样串行。本任务未启动其它构建或测试来与采样并跑。

```sh
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"
py="$INK_DEPS/build-deps/pyenv/bin/python"
bench=tests/perf_latency_2/run.py
out=work/perf-latency-2/reproduce
# A：手工模式才读取基线历史；CI 的默认回归模式不依赖历史。
taskpolicy -c utility "$py" "$bench" --build build --runs 20 --reverse all --out "$out/before-final"
taskpolicy -c utility "$py" "$bench" --build build --runs 20 --regression --compare "$out/before-final/summary.json" --out "$out/after-final"
# B：两条均应退出 1，分别恢复对应产品代码，临时重链接，不改源码或正式构建。
taskpolicy -c utility "$py" "$bench" --build build --runs 1 --regression --reverse remote --case remote_browse --out "$out/reverse-remote"
taskpolicy -c utility "$py" "$bench" --build build --runs 1 --regression --reverse panel --case ai_ask --out "$out/reverse-panel"
# C、D：普通和 ASan/UBSan 全量。
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# F：使用实际普通构建编译表。
taskpolicy -c utility "$py" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/remote/remote.c system/ui/shell/ai_panel.c
# G：完成所有提交后再扫描提交差异；0 命中时 rg 退出 1。
git diff --check
git diff --stat 6fbe0c4051843810a3252769ab34d150ac4c6097..HEAD
private_paths='/''Users/|/private/''tmp'
git diff 6fbe0c4051843810a3252769ab34d150ac4c6097..HEAD | rg -n "${private_paths}|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}"
```

【实测】A 前后各 120 次进程均退出 0，原场景预算均通过。B 恢复远程代码后三次更新全部恢复 GC16，退出 1；只恢复提示层代码，三次录音各多一次 GC16，退出 1。正向全组回归通过。数值和原始日志在 `work/perf-latency-2/`，汇总和逐样本数据已纳入报告目录。

【实测】C 普通构建退出 0；全量 CTest 退出 0，原话 `100% tests passed out of 391`，665.27 s，无跳过。`determinism`、`determinism-reader` 和新回归均通过。日志：`work/perf-latency-2/{build-final,ctest}.log`。

D、E、F、G 结果待补。

## 没做到的

- 真机波形、光学可读时刻、闪存停顿未测，模拟器数字不能视为实机数字；未连接串口、模组或设备。
- 仍未把全部指标拉到 SRS 建议值；本轮只保留远程更新和 PQ-BN 录音提示两处局部收益。
- 未运行 Ubuntu 原生 gcc 全量；本机 Apple clang，gcccheck 为额外交叉编译检查。

## 和设计不一致的

【实现】提示层首次打开仍用既有 FAST，删除立即追加的整层 FLASH_UI，减少重复清刷；关闭、真实内容更新和错误卡的路径未改。界面规范 §5 的浮层 FLASH_UI 原则与这里既有的先 FAST 行为存在差异，本次按 D-127 减少即时清刷，没有改文档迁就实现。

【实现】2026-11-11 核对原厂只读副本 `690a023`：`main/boards/metalio-e-ink4-plus/display/highlevel.c:205–243` 按前后帧求差分；`display/waveforms/my_waveform.h:6–12` 描述完整 GC/GL 48 相、均衡 36/37 相。没有修改或复制原厂代码，也没有改变硬件波形参数。EPDiy 和 PQ-BN 档案属于两条模拟路径，不能将前者的收益直接套到实际 S31 输出层。

## 改过的已有测试

无。新增 `perf-latency-2` 回归，检查真实场景的三次远程更新不因区域过多变清刷、录音提示不额外清刷；沿用所有既有预算，未放宽断言、跳过失败或增加 sleep。临时探针只加入重链接的测试对象，不进入产品。

## 要用户定的

暂无新的产品决策。剩余建议值缺口及真机参数留给主会话按实机证据处理。

## 顺手发现

【实测，模拟器】初次尝试用不同种子采样时，基线 ai_ask 在 seed=51 的首句 p95 为 3622.6 ms，超过既有 3500 ms 门槛；保留 `work/perf-latency-2/before/ai_ask-metalio-plus-09/run.log`，没有改预算。本报告受控对照采用现有场景固定 seed=42，不将新种子的结果混入前后分布。

【实现】架构审视：改动在既有远程显示和 AI 提示层内部；按钮与 AI 仍共用原子能力、软总线、成员隔离及快照接口。没有新增用户行为或绕过能力服务。

## 新增的机制

- 产品任务、线程、状态机、持久格式、队列、缓存、配置项：均无。
- 测试 `perf-latency-2`：任务书要求的分段采样与反向对照，复用既有场景及探针；不进入固件。

## 要改的文档

主会话可据本报告更新 `docs/tasks/backlog.md` 的 G6 进度，保留未达建议值项；本分支未改共享文档、需求、决策或既有调研资料。
