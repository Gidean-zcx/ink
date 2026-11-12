# hub-voice-1 执行报告

基线 `d11a96690feefebf3f4c1579b91819da116cc8fd`；分支 `claude/codex-hub-voice-1`。仅本地提交，未碰设备、未用真实厂商、未下载模型。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】最终 pytest 241 项、hub-* 6/6、内存全量 439 项通过；普通全量曾超时，集中复测通过 | `hub-contract`、`hub-json-crosscheck` 通过；全量与内存检查原话见下 | 下方构建和验证命令 |
| 2 | 【实测】流式专项通过，取消负对照失败符合预期 | `20 passed in 4.87s`；两家假厂商、节流跨间隔、0 禁中间结果、VAD 端点、首句逗号、模型未结束先发首包、三阶段取消 <100 ms、已听历史、隔离与迟到上行 | `python -m pytest -q hub/tests/speech/test_streaming.py`；`python hub/tests/speech/reverse_cancel.py` |
| 3 | 【实测】完成测量，本地未达标 | 各 9/9 流程完成（不是字词准确率统计），原始样本 [latency.json](latency.json)；下表对比目标 | `python hub/tests/speech/benchmark.py`，完整参数见 [语音运行说明](../../../hub/README.md#怎么开真的语音) |
| 4 | 【实现】完成代码调研；未做真机验证 | [AEC.md](AEC.md)，原厂提交、文件和行号齐全：BH 声明麦＋回采；S31 未引入 ESP-SR | `git show 874538e:<原厂文件>`；仅只读核查 |

| 组别 | 第一句文字 p50/p95 | 第一帧朗读 p50/p95 | 和目标比较 |
| --- | --- | --- | --- |
| 常驻 Whisper＋say（2 CPU 线程） | 17737.40 / 18472.83 ms | 18909.91 / 19957.54 ms | 未达到 ≤3 s / ≤4 s；识别定稿 17736.99 / 18469.65 ms 是主瓶颈 |
| 本机假云端 | 2.51 / 3.63 ms | 7.52 / 19.80 ms | 只证明本机处理链；没有真实云网络/厂商延迟 |

三句中文由原生 say 生成，每句每组 3 次，按 60 ms 实时上传；假 LLM。起点是最后一帧上行到达/结束通知，不是人工标注的声学末字；终点是中枢发出，**未包含设备上屏、播放、网络、独立儿童内容审核**。p95 取最近秩；保留全部慢样本，未调高 20 秒识别超时。Metal 初始化缓冲分配失败，最终数据为 CPU 模式。它们不能作为设计要求的设备末端延迟验收。

## 没做到的

- 本机极低延迟没有达到；不能把“模型常驻”写成“识别足够快”。
- Silero、Paraformer 的真实模型推理未跑：缺模型和 numpy/onnxruntime/sherpa-onnx，不自行下载或安装。Silero 状态/窗口逻辑和 Paraformer 适配已实现；默认语音检测为原有 RMS 阈值加 600 ms 静音，**不是已经跑过 Silero**。
- Whisper 服务端只在端点后推理一次，没有原生中间结果；流式中间结果由 Paraformer 或云端提供。say 仍完成一个短句 WAV 后发音频，云端适配可在整句合成完成前发首包。
- 未验证真实厂商互操作、S31 上 AEC、BH 第二槽的实际信号、真实网络上的取消耗时；100 ms 是本机假厂商逻辑取消与停止输出的测试结果。Whisper 上游有断开 HTTP 的原生 abort 回调，但没有实测它释放计算资源的 100 ms 上限；Paraformer 已开始的 native 解码片段也不能由 Python 强制抢占。

## 和设计不一致的

- 【实现】只在本连接内存保存最近一轮历史；【估算】按已发帧减 5 帧缓冲、句内每秒 5 字估算，支持 `heard_frames`。没有持久化、多轮记忆或精确文字对时，保持可逆。正常完成的回复保留全文；活跃任务的中止截断已听部分。上游完成而设备仍在播放时，带 reason 或 heard_frames 的中止也会修正历史；旧设备不带这两个字段时，不能区分这个阶段的中止和正常关闭。
- 厂商连接本次按流/短句创建，未实现设计中的跨轮连接复用；保留单条可背压链，没有复制小智的多线程队列。跨境时延未验证。
- 儿童提示、工具过滤、成员和授权沿用基线；基线没有独立额度结算及逐句儿童审核服务，本次没有补造，也不声称具备这些保护。听写规范里的多种限时/额度边界未新增，非本次验收用例。

## 改过的已有测试

- `speech/conftest.py`：假 Whisper CLI 改为常驻 HTTP 假服务器；仍验证 WAV 格式、样本数、中文参数和失败/取消；say 可执行文件检查保留。
- `speech/test_local.py`：识别样本断言移到 HTTP 事件；朗读事件按种类查找；听写容许先收到中间结果后再检查同一条定稿；不再对常驻假 HTTP 服务检查子进程 PID，say PID 仍检查；取消改检查 HTTP 服务收到断开。启动错误从旧 CLI/模型路径改为服务 URL；模型路径由独立服务端管理。
- `isolation/test_delivery.py`：旧测试直接启动听写时显式设置录音结束；保留撤权后不能投递结果的所有断言。
- `surface/test_deploy.py`：假主机预检用假程序，隔离真实 8443 占用；安装、命令顺序和不覆盖已有目录的断言不变。没有关闭真实服务或修改部署代码。

## 要用户定的

- 是否让主会话准备 Silero（2.22 MB）与 Paraformer int8（编码器 158M、解码器 68M，加词表）和可选运行库，以量真正本地流式性能；文件名、来源、大小及未核实项见 [SOURCES.md](SOURCES.md)。建议先测这一组。没有代替用户下载。
- 若走真实云端，账号、音色、模型版本和费用由主会话接入并实测；本次仅本机假接口。Whisper 启动方式已改为独立常驻服务，原来的 CLI 环境配置需要按 README 更新。

## 顺手发现

- 普通全量 `backup-restore-recovery` 触发原有 240 秒上限；多个内部阶段已通过；同一测试在内存版 95.79 秒通过。普通版集中复测 59.38 秒通过。未改相关 C 代码、测试或超时上限；首次超时原因未单独证明，保留该次失败。
- 只读依赖里的 Whisper CMake 会生成 `bindings/javascript/package.json`；首次配置被沙箱拒绝。后续用固定源码的本任务副本编译，源文件 SHA-256 与依赖副本一致；没有修改依赖目录。

## 新增的机制

- 每个流式厂商识别会话一个上行协程，与下行结果并行；任务书第 1、2 项要求，随会话取消，没有常驻后台重试。
- Whisper 常驻服务进程；任务书第 2 项要求，由运行者启动，基准自动启动并回收本次进程。
- VAD 每路独立上下文、首句断句、流式 PCM 帧拼接；任务书第 1～3 项要求。没有新持久格式或额外朗读队列。
- 最近一轮已听历史和阶段时间戳；任务书第 4、5 项要求，换成员/断线清除历史，日志不含正文。
- `INK_HUB_WHISPER_URL`：常驻识别入口，第 2 项要求。
- `INK_HUB_SHERPA_MODEL`：本地流式模型目录，第 2 项要求。
- `INK_HUB_VAD_MODEL`：可选 Silero 本地模型，第 1 项要求。
- `INK_HUB_DOUBAO_APPID`：豆包应用编号，第 2、3 项要求。
- `INK_HUB_DOUBAO_TOKEN`：豆包凭据，第 2、3 项要求。
- `INK_HUB_DOUBAO_VOICE`：豆包音色，第 3 项要求。
- `INK_HUB_ALIYUN_APPKEY`：阿里应用编号，第 2、3 项要求。
- `INK_HUB_ALIYUN_TOKEN`：阿里凭据，第 2、3 项要求。
- `INK_HUB_ALIYUN_VOICE`：阿里音色，第 3 项要求。凭据只从显式配置读取，没有令牌刷新或自动切换。

产品 Python：**新增/改写 565 行、删除 94 行，两向变动 659 行**（相对基线，含改写和注释，不含测试、文档与 21 行 MIT 许可证），小于 900 行预算。为什么不能更小：四个厂商流适配、VAD、真正本地流适配以及协议接线本身需独立处理收尾；只改原 CLI/整句合成达不到中间结果、首包和取消验收；导入小智整个框架又会引入连接、线程和大量依赖。因此使用既有 aiohttp 和网关，不加依赖、通用框架或重试。

架构审视：声学处理是 `voice.chat` / `asr.stream` 能力内的机制，沿同一网关、Scope 与工具授权，无新页面；AI 和原设备入口共用原能力，没有把内部 VAD 参数做成额外用户操作。

## 要改的文档

已改 `hub/README.md` 的语音一节。合并时主会话可同步共享接续文档中“CLI 识别/整句语音”的旧描述，以及真实语音性能和待准备的模型；本任务没有改需求、想法、决策、backlog、生成能力清单或审计资料。

## 构建和验证命令

`INK_DEPS` 由调用者设置。普通构建使用任务总规则 §5 的完整字体/Python/LVGL 参数，另设 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`；构建 `taskpolicy -c utility cmake --build build -j2`。内存构建使用同样参数，目录换成 `build-asan`，加 `-DINK_SANITIZE=ON`。

```sh
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# 内存检查使用同一命令，test-dir 换为 build-asan。
# 最终普通版集中复测：同一命令另加以下筛选。
# -R '^(hub-.*|backup-restore-recovery)$'
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=hub taskpolicy -c utility \
  "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/speech/test_streaming.py
# 反向对照：默认 cancel 应失败 3 项；其他模式各失败 1 项，均不改文件。
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=hub taskpolicy -c utility \
  "$INK_DEPS/hub/.venv/bin/python" hub/tests/speech/reverse_cancel.py
# 同一命令另加 legacy / late / vendor / model_close / heard_complete。
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility \
  "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build
```

【实测】普通与内存版配置、构建均退出 0。验证原话：

| 运行 | 退出码 | 原话 / 证据 |
| --- | --- | --- |
| 普通全量 | 8 | `99% tests passed, 1 tests failed out of 439`；`Total Test time (real) = 1903.38 sec`；`backup-restore-recovery (Timeout)`；原有 `memo-save-busy (Disabled)` |
| 最终普通版 hub-*＋备份恢复复测 | 0 | `100% tests passed out of 7`；`Total Test time (real) = 93.46 sec`；备份恢复 59.38 秒通过；其中完整 pytest：`241 passed in 87.35s (0:01:27)` |
| ASan/UBSan 全量 | 0 | `100% tests passed out of 439`；`Total Test time (real) = 1494.25 sec`；原有 `memo-save-busy (Disabled)` 未运行；其中完整 pytest：`241 passed in 95.14s (0:01:35)` |
| 流式专项 | 0 | `20 passed in 4.87s` |
| 手动反向对照 | 每个模式均为 1，符合预期 | cancel：`3 failed, 17 deselected in 0.30s`；legacy/late/vendor/model_close 各 1 failed；heard_complete：`1 failed, 1 passed, 18 deselected in 1.51s` |
| gcccheck | 0 | `gcccheck: 0 compiled, 0 with findings`，没有 C 改动，不当作 Linux GCC 编译证明 |

内存版 `INK_SANITIZE=ON`、`INK_WERROR=ON`，跑完全部 439 个启用测试。S31 完整固件未编，设备代码未改；全量中的 S31 宿主负向编译检查已跑，不等于完整固件构建。ASan/UBSan 检查宿主 C 工程，不等于检查了 Python/Whisper 第三方库本体。Linux 未在本机实跑。

【实测】基线到提交及工作区的禁止绝对路径/硬件地址扫描无匹配（grep 退出 1）；收尾提交后再次核对。`git diff --check`、24 个变更文件的范围检查、文档本地链接和 5 MB 文件上限检查均通过；没有设备代码、共享文档或构建产物夹带。两组基准各 9 条样本保留。

## 里程碑与验证记录

- `c1c4cf60`：开工报告；当时 HEAD 等于基线，工作区干净。
- `4d7dc063`：语音核心和假厂商。初次 hub 测试 `2 failed, 231 passed in 98.97s`；修正旧听写测试未标结束、安装假环境依赖真实 8443 的前提，所有原断言保留。
- `06498592`：完成原生 Whisper＋say 与假云端两组基准、原厂 AEC 调研。尝试反复识别已收前缀造成额外 CPU 负担和超时，删掉该行为；最终只在端点推理一次，不伪称 Whisper 有原生中间结果。
- `8fc56ba4`：回归修正重复识别文字、迟到上行、厂商异常收尾；显式关闭暂停在 yield 处的模型流，避免靠垃圾回收取消上游。
- `3059a8ba`：上游完成后中止也在同一关闭入口修正当前那一轮历史；正常关闭保留全文，专项和反向对照验证。
- 最终全量、复测、内存检查与六种反向对照结果见上表；没有删除失败记录或提高预算。最终普通中枢 241 项通过，先前超时的普通备份恢复集中复测通过，内存全量 439 项通过。
