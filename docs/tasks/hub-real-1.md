# hub-real-1：中枢的语音换成真的——本机开源模型识别、Mac 自带 `say` 朗读，都按协议转成 16 kHz 单声道 60 ms 的 Opus

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）。这是只改中枢的轻任务。

## 为什么

`hub-link-1` 报告“语音现状”：`hub/ink_hub/speech/fake.py:13` 的识别对任何声音都回固定文字，`:21` 的朗读回的是 `0xA5` 开头的假包，不是 Opus。所以设备上“按住 AI 键说话”“回答念出来”“听写”在真机上都还不能用。

用户的决定（D-130，2026-11-12）：DeepSeek 不提供语音，“识别先用这台 Mac 本地跑开源 Whisper（不花钱）”；朗读另给方案——交付计划第 5 节第 5 项推荐“朗读先用 Mac 自带的中文语音（免费）”，本任务照这个做。下载识别模型要用户点头：**本任务不下载任何模型文件**，只把要下载的文件名、来源、大小写进报告，主会话问过用户再下。

协议（`docs/hub/device-protocol.md` 第 26、84-85 行，设备 `system/services/ai/ai_service.c:582-585`）：设备上行和中枢下行的声音都是 Opus、16 kHz、单声道、每帧 60 ms（960 个采样）。网关已经按这个节奏发（`hub/ink_hub/voice/flow.py:94-111`），只是缺真的编解码、识别和合成。

## 成熟来源

- **Opus**：和设备用同一份 libopus——S31 构建已经下好的 `$INK_DEPS/build-s31/esp-opus-1.0.5.zip`（78/esp-opus 1.0.5，定点 libopus，BSD-3；SHA-256 见 `ports/esp32s31/opus.cmake:3`）。它是完整的 libopus 源码树（带 `Makefile.unix`、`CMakeLists.txt`）。参数照设备：`ports/esp32s31/hal_opus_s31.c`（16 kHz、单声道、`OPUS_APPLICATION_VOIP`、每帧 960 个采样）。
- **识别**：Whisper（D-130），用 whisper.cpp（MIT，C/C++，Mac 上有 Metal 加速）的命令行程序。本任务不编译、不下载它，测试用假程序。
- **朗读**：macOS 自带 `say`，能直接输出 16 kHz、16 位、单声道的 WAV（`--file-format=WAVE --data-format=LEI16@16000`）。

## 要做的

1. **Opus 编解码**：写一个小脚本（例如 `hub/tools/build_opus.sh`）把上面的 libopus 编成本机共享库（Mac 出 `.dylib`，Linux 出 `.so`），输出到被忽略的目录（`work/` 下）；中枢用 `ctypes` 加载（路径由环境变量给，例如 `INK_HUB_LIBOPUS`，没给就到默认输出位置找）。只包这几个函数：建编码器和解码器、编一帧、解一帧、释放。不新增 Python 依赖。
2. **识别**（`ink_hub/speech/` 里新文件）：`transcribe(frames)` 把收到的 Opus 帧逐帧解成 PCM，写成 16 kHz 单声道 WAV（放进权限 700 的临时目录，用完删掉），调用 whisper.cpp 命令行（程序路径和模型文件路径都由环境变量给，例如 `INK_HUB_WHISPER_CLI`、`INK_HUB_WHISPER_MODEL`），中文用 `-l zh` 并给一句简体普通话的提示词，读回文字。
   - 太短或基本是静音（例如不到 0.3 秒，或音量低于阈值）直接回空字符串，**不调模型**（Whisper 对静音会编出“谢谢观看”之类的话）。阈值写成一个常量，报告说明怎么定的。
   - 超时（例如 20 秒）、程序不存在、退出码不为 0：抛出现有网关能处理的错误，设备看到现有的“这次没有完成，请重试。”。
   - 识别出的文字和声音不进日志。
3. **朗读**：`synthesize(text)` 把文字从标准输入或临时文件交给 `say`（**不放在命令行参数里**：防止进程列表看到内容，也防止参数注入），生成 16 kHz 单声道 WAV，按 960 个采样一帧编成 Opus 包，最后不满一帧的补静音，返回包列表。中文用 `say` 的中文声音（环境变量可改，例如 `INK_HUB_SAY_VOICE`）；设备语言是英文时用英文声音（`lang` 现在不传到 `synthesize`，如果要改接口，只在 `speech/` 里加默认参数，网关那边的调用写进报告由 `hub-pair-1` 或主会话接）。
4. **按环境选择**：`ink_hub/speech/__init__.py` 加 `from_environment()`，返回 `(asr, tts)`：`INK_HUB_ASR` 取 `fake` 或 `whisper`，`INK_HUB_TTS` 取 `fake` 或 `say`；都没设返回 `(None, None)`（用假的）；选了真的但缺程序、模型或 libopus 时启动就报清楚的中文错误，说明缺哪个环境变量。`hub-pair-1` 会在 `Hub` 和 `__main__` 里调用它；在它合入前，你的测试直接给 `Hub(asr=..., tts=...)` 传实例（`hub/ink_hub/gateway/server.py:406` 已支持）。
5. 在 `hub/README.md` 加一节“怎么开真的语音”：编 libopus 的命令、要设的环境变量、`say` 的中文声音怎么查（`say -v '?'`）。

## 范围和预算

- 只改：`hub/ink_hub/speech/`、新的 `hub/tools/build_opus.sh`、`hub/tests/` 里新增的语音测试和假程序、`hub/README.md` 的语音一节、`docs/tasks/hub-real-1/`。
- 不碰：`hub/ink_hub/gateway/`、`__main__.py`、`voice/flow.py`、`models/`（都归 `hub-pair-1`）、`content/weather.py`。天气不在本任务（用户 2026-11-12：`hub-real-1` 只做语音）。
- 产品代码不超过 400 行，脚本不超过 40 行（不算测试）。
- 不下载模型，不编译 whisper.cpp，不调任何付费接口。可以在本机实际跑一次 `say` 和你编出的 libopus 来验证（都在本机，不联网）。
- 轻任务：不做主工程构建；中枢测试用主检出已有的能力表：`INK_HUB_CAPS=$INK_DEPS/build/test-out/hub/caps.json PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests`。

## 验收

1. **照协议走一遍语音问答**（假设备 + 假模型，见 `hub/tests/fake_device.py`）：设备发 `ai.listen`，上行是用你的 libopus 把一段 1.5 秒正弦波编成的真 Opus 帧 → 假的 whisper 程序收到的 WAV 是 16 kHz、单声道、时长等于帧数 × 60 ms（假程序检查文件头并回固定文字）→ 设备收到 `ai.stt` → 收到 `ai.delta` → 收到的朗读帧每一帧都能用同一个 libopus 解成 960 个采样 → `ai.tts.end` 的帧数等于实际发出的帧数 → 最后 `{"done":true}`。用假的 `say`（一个写出正弦波 WAV 的小脚本）跑，CI 上也能过。
2. **听写**：`asr.stream` 会话结束后收到 `rt.asr` 和 `rt.asr.done`，文字来自假识别程序。
3. **静音不调模型**：发一段全零的帧，回空文字，假程序没有被调用（假程序被调用就让测试失败）。反向对照：去掉静音判断，这条测试必须失败。
4. **失败路径**：假程序超时、退出码非 0、程序不存在，设备都收到现有的失败提示，连接不断。
5. **不留痕迹**：每轮之后临时目录是空的；日志里查不到识别出的文字和朗读的文字。
6. **真的 `say` 和 libopus（只在 Mac 上跑，其它平台明确跳过并打印原因）**：对“今天天气怎么样？”合成，包数约等于音频时长除以 60 ms；把包解回 PCM 写成 WAV 放进 `docs/tasks/hub-real-1/`（不超过 300 KB），报告写你听过、是不是中文、有没有截断。没编出 libopus 时这几条跳过并在报告写原因，不算通过。
7. 中枢 pytest 全过（命令见上），没有跳过以外的失败。
8. 报告必须有：
   - **要用户同意才下载的东西**：whisper.cpp 的版本或提交、来源网址、要编的目标；推荐的模型文件名、下载网址、大小、SHA-256（能查到的写上；中文建议比较 `ggml-large-v3-turbo` 的量化版和 `ggml-small`，写清各自大小和取舍），以及下载后的编译和启动命令。
   - 启动中枢要加的环境变量（交给 `hub-pair-1` 的启动脚本或主会话）。
   - 每句话从说完到出字、从出字到开始朗读的耗时：这次只能估算，标【估算】，写清主会话上机要量什么。
   - 这台 Mac 上 `say` 可用的中文声音名（`say -v '?'` 的中文那几行）。
