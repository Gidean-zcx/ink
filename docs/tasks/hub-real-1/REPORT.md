# hub-real-1 执行报告

日期：2026-11-12。基线：`28c53632ff0f50429aa694f629581058a6d9eea5`。仅改任务允许的语音实现、脚本、新测试、README 语音节和本目录；未下载模型、编译 Whisper、碰设备或推送。

## 验收表

命令均从仓库根目录执行，先设置任务给定的 `INK_DEPS`。原始日志在忽略的 `work/`，不提交其中的本机路径。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 真 Opus 上行 25 帧；假识别检查 16 kHz、单声道、16 位、24000 采样；识别字、回答字、3 帧朗读、结束帧数、最终 `done:true` 顺序正确，每包解出 960 采样 | 下方全量命令；`test_voice_protocol` |
| 2 | 【实测】通过 | `rt.asr` 为假程序文字；`rt.asr.done` 为 1 段、1500 ms | 同上，`test_dictation` |
| 3 | 【实测】通过 | 全零、低音量、0 帧、4 帧不调用模型；5 帧可识别。撤掉静音条件后同一测试失败，正常版本通过 | 同上，`test_silence_and_short_skip_model`；反向命令见下 |
| 4 | 【实测】语音问答通过 | ASR/TTS 各测超时、非零退出、程序消失，均收到现有失败提示，后续 `caps/list` 成功；听写失败回传缺口见下 | 同上，`test_failure_keeps_connection` |
| 5 | 【实测】通过 | 每项测试检查临时目录清空；假程序确认目录权限 700；问答和失败路径检查日志及 stdout/stderr 无正文；超时、取消回收子进程 | 同上，`speech_tools`、`test_cancellation_cleans_process` |
| 6 | 【实测】编解码通过；听感未验收 | 原生 `Tingting` 源音频 25718 采样/1.6074 秒，Opus 27 帧，解回 25920 采样/1.62 秒；尾部补齐 12.625 ms。[WAV](say-roundtrip.wav) 为 51884 字节。未亲耳试听，不声称中文听感和无听感截断已通过 | 下方原生命令 |
| 7 | 【实测】通过 | 普通全量原话：`190 passed in 84.07s (0:01:24)`；ASan 全量原话：`190 passed in 66.29s (0:01:06)`。均退出 0，无跳过、无失败；ASan 没有报告内存错误 | 下方全量及内存命令 |
| 8 | 【实现＋资料核查】齐全 | 模型候选、版本、大小、SHA-256、启动变量、耗时估算和本机中文声音见下；真实 Whisper 未运行 | 官方资料与 `say -v '?'` |

普通全量与保留原生声音证据：

```sh
INK_HUB_CAPS="$INK_DEPS/build/test-out/hub/caps.json" PYTHONPATH=hub \
  taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests
INK_HUB_SAY_EVIDENCE=docs/tasks/hub-real-1/say-roundtrip.wav PYTHONPATH=hub \
  taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" -m pytest -q -s hub/tests/speech/test_real_say.py
```

Opus：`taskpolicy -c utility sh hub/tools/build_opus.sh`，退出 0；`sh -n hub/tools/build_opus.sh` 通过。使用 `ports/esp32s31/opus.cmake:3` 的同源归档及 SHA-256：`f04982787072427616f2ef86e228844d4912f0a6952a2ee597bb4dc6e5ced829`。来源 78/esp-opus 1.0.5，BSD-3-Clause；许可证保留在解压目录 `esp-opus/COPYING`。Unix Makefile 缺根目录搜索路径，仅由脚本补 `-I.` 和共享库所需 `-fPIC`，未改第三方源码。16 kHz、单声道、VOIP、960 采样的依据为 `ports/esp32s31/hal_opus_s31.c:11`、`:20`、`:35`；没有改设备驱动。

内存检查覆盖同源 libopus 的编码、解码、释放及整个中枢 pytest。首次全量在 `ctypes` 加载时退出 134；最小检查显示 ASan 加载太晚，确认普通 Python 启动器清除了注入变量。改用同一解释器的框架可执行文件后，先检查预加载和编解码往返成功，再跑全量。未关闭 ASan 检查或改产品代码。首次内存检查误与普通全量重叠，后续已串行。

```sh
CFLAGS='-fsanitize=address -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address' \
  taskpolicy -c utility sh hub/tools/build_opus.sh work/hub-opus-asan
export INK_HUB_CAPS="$INK_DEPS/build/test-out/hub/caps.json"
export INK_HUB_LIBOPUS="$PWD/work/hub-opus-asan/libopus.dylib"
export INK_ASAN_RUNTIME="$(cc -print-file-name=libclang_rt.asan_osx_dynamic.dylib)"
taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" - <<'PY'
import os, sys
from pathlib import Path
real = str(Path(sys.base_prefix) / 'Resources/Python.app/Contents/MacOS/Python')
env = {**os.environ, 'DYLD_INSERT_LIBRARIES': os.environ['INK_ASAN_RUNTIME'],
       '__PYVENV_LAUNCHER__': sys.executable, 'ASAN_OPTIONS': 'abort_on_error=1', 'PYTHONPATH': 'hub'}
os.execve(real, [real, '-m', 'pytest', '-q', 'hub/tests'], env)
PY
```

反向对照只在独立进程中撤掉静音条件，不改磁盘源码、不依赖 Git 历史：

```sh
PYTHONPATH=hub taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" - <<'PY'
import inspect, textwrap, pytest
from ink_hub.speech import local
source = textwrap.dedent(inspect.getsource(local.WhisperASR.transcribe))
condition = ' or math.sqrt(sum(s * s for s in samples) / len(samples)) < SILENCE_RMS'
assert condition in source
namespace = {}
exec(source.replace(condition, ''), local.__dict__, namespace)
local.WhisperASR.transcribe = namespace['transcribe']
raise SystemExit(pytest.main(['-q', 'hub/tests/speech/test_local.py::test_silence_and_short_skip_model[0-25]']))
PY
```

结果原话：`1 failed in 0.66s`，退出 1；假识别被错误调用并以 17 退出，引发 `RuntimeError`。正常全量包含同一测试并通过。

## 没做到的

- 未运行真实 Whisper、未验证识别准确率；按要求未下载模型或编译它。假程序只证明 WAV 和调用协议正确。
- 未亲耳试听附件。当前工具能生成、分析 WAV，没有用于确认听感的音频感知能力；需复核者试听普通话和句尾。
- 【实现】听写识别失败不能回传现有提示：`hub/ink_hub/voice/flow.py:130`～`:145` 只捕获取消和连接异常，不捕获适配器的 `RuntimeError`；任务不许改该文件。六项语音问答失败测试通过，不能据此声称听写失败提示也通过。
- CLI 环境选择和网关 `lang` 透传按任务边界交给 `hub-pair-1` / 主会话；此分支设环境变量尚不会自动切换 CLI。协议测试直接向 `Hub` 注入实例。
- 按轻任务要求未构建主工程、CTest 或 S31。已阅读 `ink-codex/tools/gcccheck.py`，结果为不适用、未执行：没有 C 产品源码改动，不记成通过。没有 Linux 主机或 Docker，未实跑 Linux；原生 `say` 测试在其它平台明确跳过，其余使用假程序和同源 libopus。

## 和设计不一致的

- 未改协议或设计文档。暂取 RMS 常量 100（16 位满量程约 -50.3 dBFS）、最短 4800 采样（0.3 秒），用于拦全零和低电平，是经验假设，不是设备麦克风校准结果。低声可能被拦、大声背景噪声仍可能调用模型，主会话需实测。
- `SayTTS.synthesize(text, lang="zh")` 已支持英文 `Samantha`，中文默认 `Tingting`；网关不传语言的现状已单列待接入，没有越界修改。

## 改过的已有测试

无；只新增 `hub/tests/speech/`，未放宽已有断言或预算。

## 要用户定的

### 要用户同意才下载的东西

本次只读官方网页和模型仓库 JSON 元数据，未下载权重或 Whisper 源码。查阅日期均为 2026-11-12。

**whisper.cpp**：建议固定 [v1.9.5（提交 `d1be6fd`）](https://github.com/ggml-org/whisper.cpp/releases/tag/v1.9.5)，[源码](https://github.com/ggml-org/whisper.cpp)，MIT；只编 `whisper-cli`，Mac 开 Metal。[上游构建与量化说明](https://github.com/ggml-org/whisper.cpp/blob/v1.9.5/README.md)。

| 模型文件 | 大小（仓库元数据） | SHA-256 | 下载地址 |
| --- | --- | --- | --- |
| `ggml-large-v3-turbo-q5_0.bin`（优先建议） | 574041195 字节，574.0 MB | `394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2` | [下载](https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin)，[元数据](https://huggingface.co/ggerganov/whisper.cpp/blob/main/ggml-large-v3-turbo-q5_0.bin) |
| `ggml-small.bin`（比较候选，多语言版） | 487601967 字节，487.6 MB | `1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b` | [下载](https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin)，[元数据](https://huggingface.co/ggerganov/whisper.cpp/blob/main/ggml-small.bin) |

取舍：【资料】turbo 是 large-v3 的裁剪版，减少解码层以提速（[官方模型卡](https://huggingface.co/openai/whisper-large-v3-turbo)）；Q5 进一步省权重空间，small 下载少约 86.4 MB 且没有这层量化。【估算】中文先试 turbo Q5 再比较 small；准确率、峰值内存和速度未在本机测，不能按文件大小断言谁更快、更准。模型仓库许可标 MIT，不要误选英文专用 `.en`。

以下仅为**用户同意后的命令，本次未执行**（选 small 时同时换模型名和哈希）：

```sh
git clone --branch v1.9.5 --depth 1 https://github.com/ggml-org/whisper.cpp.git work/whisper.cpp
taskpolicy -c utility cmake -S work/whisper.cpp -B work/whisper.cpp/build \
  -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON
taskpolicy -c utility cmake --build work/whisper.cpp/build --target whisper-cli -j2
model=work/whisper.cpp/models/ggml-large-v3-turbo-q5_0.bin
curl -fL https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin -o "$model"
printf '%s  %s\n' 394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2 "$model" | shasum -a 256 -c -
```

### 交给 hub-pair-1 / 主会话的启动变量

```sh
export INK_HUB_ASR=whisper
export INK_HUB_TTS=say
export INK_HUB_LIBOPUS="$PWD/work/hub-opus/libopus.dylib"
export INK_HUB_WHISPER_CLI="$PWD/work/whisper.cpp/build/bin/whisper-cli"
export INK_HUB_WHISPER_MODEL="$PWD/work/whisper.cpp/models/ggml-large-v3-turbo-q5_0.bin"
export INK_HUB_SAY_VOICE=Tingting
```

装配时先 `asr, tts = speech.from_environment()` 再注入 `Hub`，网关合成接上 `lang=lang`，随后沿用主会话启动脚本及监听、配对参数。不设变量返回 `(None, None)`；显式选真实实现却缺资源会中文报错。正常启动不要沿用内存检查用的 ASan 库。

### 每句话的耗时

均为【估算】，假设空闲 Apple Silicon Mac、Metal、1～5 秒普通话、局域网，每次启动 CLI 并加载模型：说完（`ai.listen.end`）到识别字（`ai.stt`）约 **1～10 秒**；回答字（`ai.delta`）到设备开始朗读约 **0.3～2 秒**，含逐句 `say`、Opus 和设备 3 帧预缓冲。`ai.stt` 到朗读还须加 DeepSeek 完整回答耗时；本次没有调用模型，不能承诺首字延迟。

主会话逐句量：结束上行、CLI 启动/退出、`ai.stt`、模型完成、首个 `ai.delta`、各句首个下行 Opus 帧、设备实际开播。分别看冷启动、第二轮、长句、安静及背景噪声，统计中位数和 P95，并核对设备首帧时限；不记录声音或正文。

### 本机可见的中文声音

【实测】`say -v '?' | rg 'zh_(CN|TW|HK)'` 的声音与语言列如下；只验证 `Tingting` 的生成和编解码，未逐个试听：

```text
Eddy (中文（中国大陆）)     zh_CN
Eddy (中文（台湾）)       zh_TW
Flo (中文（中国大陆）)      zh_CN
Flo (中文（台湾）)        zh_TW
Grandma (中文（中国大陆）)  zh_CN
Grandma (中文（台湾）)    zh_TW
Grandpa (中文（中国大陆）)  zh_CN
Grandpa (中文（台湾）)    zh_TW
Meijia              zh_TW
Reed (中文（中国大陆）)     zh_CN
Reed (中文（台湾）)       zh_TW
Rocko (中文（中国大陆）)    zh_CN
Rocko (中文（台湾）)      zh_TW
Sandy (中文（中国大陆）)    zh_CN
Sandy (中文（台湾）)      zh_TW
Shelley (中文（中国大陆）)  zh_CN
Shelley (中文（台湾）)    zh_TW
Sinji               zh_HK
Tingting            zh_CN
```

## 顺手发现

- 听写错误回传缺口见“没做到的”，未越界修改 `voice/flow.py`。
- 【实现】仍是现有语音能力的适配器，按钮和 AI 沿用 `voice.chat` / `asr.stream`，不新增旁路、页面或授权规则，符合本任务内的软总线边界。原有 `asr.transcribe` / `tts.speak` 示例处理器仍属网关，本任务未将它们改成音频传输入口。

## 新增的机制

- Opus 6 个函数绑定、Whisper/say 子进程及 20 秒超时回收、700 权限临时目录、语音环境选择：任务书第 1～4 项要求。
- 配置变量为上方 6 项；`lang` 只在语音类中加默认参数。保留 WAV 和假程序的变量只供测试。
- 没有新增常驻任务、线程、状态机、持久格式、重试、缓存、队列或 Python 依赖。
- 产品 Python **180 行**（含空行、注释），脚本 **30 行**，低于 400/40 预算。为什么不能更小：覆盖 6 项 C ABI、两类状态释放、两个程序的输入输出和失败清理。看过的更小方案为同步 `subprocess.run`，会阻塞网关；直接读 Whisper stdout 会混入程序输出。保留薄的异步调用和结果文件，未建框架。

## 要改的文档

主会话合并时更新任务清单和启动说明，接入环境选择、英文朗读，并补听写失败回执；复核者试听附件，模型获同意后再验证真实识别与延迟。不把假模型结果升级为真机可用。未改共享需求、想法、决策、生成能力表及历史研究/审计文件。

## 结束检查

- 【实测】最终变更限定于任务允许的 11 个文件；`git diff --check` 退出 0，本地文档链接均存在，WAV 仅有格式和采样数据块，无路径等元数据。
- 【实测】按总规则第 6 节扫描相对基线的差异，隐私模式无匹配（扫描退出 1）；没有提交共享库、模型、构建产物或设计稿。唯一音频是任务明确要求的 51.9 KB 合成测试 WAV。
- 已复核产品 diff；全量和 ASan 通过后仅清理旧模块说明、补文档和验收附件，未再改运行逻辑。提交仅在本地，未推送。
