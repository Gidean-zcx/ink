# audio-out-2 执行报告

基线：`a39e9693bb4dea2edd0b5549486d583122dc0a13`。状态：核心实现、普通全量、ASan 回归和 S31 编译完成（S31 由主会话补跑）。未连接设备，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】四场景均 0 次/0 ms 欠载；1 kHz SNR 91.96 dB、10 kHz 衰减 88.49 dB。原基线 44 次/660 ms；保留 540 ms 预缓冲再把滤波移回输出任务，仍 43 次/645 ms，欠载断言失败 | C1、C2 |
| 2 | 通过 | 【实测】[改前频谱](before-spectrum.png)、[改后频谱](after-spectrum.png)，分别 71757 / 25697 B；[改前 WAV](before-music.wav)、[改后 WAV](after-music.wav)，均为同一原创双音样例的实际 speaker.wav | C2、C3 |
| 3 | 部分通过，继续验证 | 【实测】普通全量 436 项执行通过、3 项按原条件跳过；ASan/UBSan 相关 12/12、S31 宿主 77/77 通过；固件待补 | C4、C5、C6 |

## 没做到的

- S31 编译验证仍在进行。未做真机试听、烧录或串口操作；宿主模拟与计算量估算不能冒充 S31 实机结果。未运行 Linux 全套测试。

## 和设计不一致的

- 暂无；保留原厂 HAL 及 DMA 4×240 帧。复用原 PSRAM 队列保存处理后的 16 kHz PCM，未另建队列。

## 改过的已有测试

- `bench.c` 的独立 FIR 参考改用共享质量常量，仍为 Q4；所有逐样本断言保留。
- `control.py` 适配生产侧滤波的位置，并增加把滤波放回输出任务的反向对照；使用随测试提交的补丁，不依赖 CI Git 历史。欠载和音质断言未放宽。
- `measure.py` 增加手动指定基线程序；继续保留失败并返回非零。`plots.py` 增加基线标题及 WAV 导出。既有断言、预算、超时、合成素材均未改。

## 要用户定的

- 暂无。

## 顺手发现

- 高档初版试运行：Q8 的 1 kHz 与合成音乐各 1 次/255 ms 欠载；Q7 的完整流程 2 次/285 ms 欠载。因此保守选用 Q4。高档试跑在锁改为首次 PCM 分配之前；最终版本仅完整验收 Q4，不把早期结果当作最终版本更高档一定不可用的证明。
- 全量首轮发现新互斥锁启动时分配导致 ui-scene-ImeVoice 的内部内存余量为 32672 B，低于原有 32768 B 门槛；中止并改为首次 PCM 时分配。定向复跑余量 32776 B、无警告；Opus 的 finish/abort 不取得 PCM 锁。最终普通全量原话：`100% tests passed out of 439`，814.95 s；hub-contract、hub-json-crosscheck、hub-simulator-e2e 按原条件跳过。
- 【实测】ASan/UBSan 原话：`100% tests passed out of 12`，44.60 s；覆盖 determinism、determinism-reader、integration-media、两项 handsfree/voice policy 场景、memo-record、voice-name-assist、privacy-records、music-formats、music-product、audio-out 和双反向对照。无 sanitizer 诊断；未运行 ASan 全量 439 项。
- 【实测】GCC 警告检查原话：`gcccheck: 3 compiled, 0 with findings`，文件为 audio_service.c、bench.c、device.c；质量扫档工具另以 clang -O2 手动编译运行。
- 【实测】S31 宿主测试原话：`100% tests passed out of 77`，108.63 s，退出 0。

## 新增的机制

- 任务书第 1 项要求的生产侧滤波：在现有媒体执行器调用 PCM 入口时计算；生产端独占 FIR，首次 PCM 时分配一把互斥锁，串行化推送、排尾和销毁；输出任务和 Opus 流不取得这把锁。
- 任务书第 1 项要求的缓冲：复用 PSRAM 播放队列，媒体上限 34×60 ms，PCM 启播阈值 9×60 ms。无新增任务、线程、队列、状态机、持久格式或用户配置。
- 任务书第 2 项要求的固定 Q4 编译常量，由生产端与独立参考测试共用，无运行时档位配置。

## 资源与档位

【实现】媒体执行器仍为优先级 4，声音任务仍为 7，DMA 仍为 4×240 帧。滤波、混为单声道、FIR 排尾全部在调用 PCM 入口的生产端；AU.lock 只管入队和状态，滤波时不持有它。处理后的 PSRAM 队列由原媒体循环限制在约 2.04 秒，启动至少准备 540 ms；短文件沿用 EOF 直接排完的规矩。

【实现】44.1 kHz 输入时，队列分配上界约 35×1920=67200 B（含一块在途输出），加单声道暂存 5292 B、FIR 请求 4328 B，共 76820 B；沿用原队列元数据与 staging，不另分配整块第二队列。所有这些大缓冲和 FIR 均显式请求 PSRAM。第一轮原速率队列上界为 185220 B，单比较队列减少 118020 B，未把它当作实机测量。唯一运行时内部增量是一把首次 PCM 才创建的 FreeRTOS 互斥锁，尺寸待 S31 编译核对；DMA 新增为 0。

【实测】按 Q10→Q0 逐档运行同源 FIR，44.1 kHz 单声道合成信号降到 16 kHz（与双声道同相混音等价）；完整播放另测真实双声道解码和混音。宿主耗时不含初始化和调度，是加载状态下的一次 CPU 计时；完整数字见 [qualities.json](qualities.json)，频响见 [档位图](quality-sweep.png)。

| 档位 | tap | 宿主 ms/音频秒（1 kHz） | 10 kHz 衰减 dB | S31 ms/音频秒【估算】 | 取舍 |
| --- | ---: | ---: | ---: | ---: | --- |
| Q10 | 712 | 23.77 | 100.16 | ≥647 | 软件双精度，排除 |
| Q9 | 536 | 21.20 | 100.16 | ≥487 | 软件双精度，排除 |
| Q8 | 448 | 13.18 | 100.16 | 407 | 初版模拟存在欠载 |
| Q7 | 352 | 9.06 | 100.16 | 320 | 初版模拟存在欠载 |
| Q6 | 264 | 10.06 | 100.16 | 240 | 只做单独测量与估算，保留更多供流余量 |
| Q5 | 224 | 7.85 | 98.35 | 204 | 同上，不认定为完整流程通过 |
| **Q4** | **176** | **2.89** | **88.49** | **160** | **采用，完整流程零欠载** |
| Q3 | 136 | 3.36 | 96.17 | 124 | 不必为既定指标再降档 |
| Q2 | 88 | 2.20 | 94.67 | 80 | 同上 |
| Q1 | 48 | 1.52 | 66.53 | 44 | 同上 |
| Q0 | 24 | 0.63 | 52.03 | 22 | 同上 |

【估算】沿用第一轮 320 MHz、Q4 3200 cycles/输出采样的保守模型，按实际 tap 比例计算；没有调低成本让测试通过。Q9/Q10 的数仅为单精度成本下界，实际软件双精度更慢。Q4 每 60 ms 音频约 9.6 ms CPU；宿主 WAV/FLAC/MP3/Vorbis 解码分别为 0.055/0.288/0.784/0.677 ms/音频秒，若按宿主的 20～50 倍估计，总计约 161～199 ms/音频秒，留出调度余量。Q4 是保守选择，不声称已证明它是所有工况下可用的最高档。

**为什么不能更小**：【实现】相对基线产品代码新增 79 行、删除 67 行，按增删相加为 **146 行**，低于 300 行。主要是搬出旧的输入分段与输出侧 FIR、增加生产侧入队/排尾/互斥生命周期；未增加第二任务或第二队列。看过的更小方案：滤波继续放在原输出任务（第一轮已经实测失败）；直接在 AU.lock 内滤波虽能少一把锁，但会阻塞输出取数，违背这次任务的目的，因此未采用。

## 来源与架构审视

- 查阅日期：2026-11-12。SpeexDSP 原文 `resample.c:226-238` 为质量表，`:677-705` 为单/双精度选择；六份上游原文和许可证未改，版本与哈希见 [SOURCE.md](../../../third_party/speexdsp/SOURCE.md)。
- 原厂 `874538e:main/boards/common/audio/bt_audio_codec.cc:66-98,138-163` 为 I2S 从机、16 kHz、双槽复制；现有原文适配 `vendor/metalio_bh1098/codec.inc:3-70`。未改协议或硬件时序。
- MyPaperS31 `325b538:main/audio/audio_radio_s31.cpp:370-430` 也是转换后输出；本任务要求拆到生产/输出两侧，保留我们的 HAL。其 `:112-113` DMA 6 描述符没有采用，原因仍是双向 DMA 内部增量会超过 4 KB。
- 【实现】职责仍归声音服务，设备只走 HAL，模拟器与 S31 仍用同份 FIR。用户与 AI 继续调用既有 media.play/pause/resume/seek/stop，没有新增用户动作、页面或绕过软总线的接口。

## 要改的文档

- 暂无；不修改共享文档。

## 复核命令

主工程配置沿用总规则第 5 节，构建 `taskpolicy -c utility cmake --build build -j2`。

```bash
# C1
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^audio-out' --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/measure.py --build build
# C2：手动历史基线，后一个命令预期退出 1，保留欠载及音质失败
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/control.py --build build --baseline a39e9693bb4dea2edd0b5549486d583122dc0a13 --build-only
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/measure.py --build build --binary build/audio-out-baseline
# C3：work/measure-env 内固定 numpy 2.0.2、matplotlib 3.9.4，仅用于手动报告
MPLCONFIGDIR=work/mpl-cache taskpolicy -c utility work/measure-env/bin/python -B tests/audio_out/plots.py --build build --out docs/tasks/audio-out-2 --font "$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" --before audio-out-baseline --before-title '改前：输出任务内 SpeexDSP Q4，含欠载'
# C4：最终普通全量，退出 0
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# C5：配置与普通构建相同，只换 build-asan 并增加 -DINK_SANITIZE=ON
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '(audio|media|music|determinism|voice|opus|record)' -j2
# C6：S31 宿主机，全 77 项
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests
taskpolicy -c utility cmake --build build-s31-tests -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
# C8：GCC 警告检查
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/services/audio/audio_service.c tests/audio_out/bench.c tests/audio_out/device.c
# C9：逐档宿主测量，不是 S31 实机计时
taskpolicy -c utility clang -O2 -DNDEBUG -UNDEBUG -DFLOATING_POINT -DOS_SUPPORT_CUSTOM -DEXPORT= -Iplatform/include -Isystem/include -Ithird_party/speexdsp/include -Ithird_party/speexdsp tests/audio_out/quality.c system/services/audio/codec_formats.c system/services/audio/vendor/kiikoread/metadata.c system/services/audio/vendor/kiikoread/music_lyrics.c third_party/speexdsp/resample.c -o work/audio-quality -lm
taskpolicy -c utility work/audio-quality
taskpolicy -c utility build/test-audio-out-bench tests/music/fixtures
```

## 主会话补充（2026-11-12 晚）

- 【实测】Codex 运行在 S31 编译前结束，主会话在本分支提交 `4f36aac6` 上补跑 `ports/esp32s31/build.sh`（独立构建目录）：退出 0，编译警告 0；镜像 4,277,152 B；DIRAM 已用 165,350 B。重采样器的分配经 `os_support_custom.h` 全部在 PSRAM，内部内存只多首次 PCM 时建的一把互斥锁。
- 未做：真机试听（设备不在），合并后的试用版上机时请用户听。
