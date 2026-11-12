# audio-out-1：音乐不再转成 Opus——声音服务加一个 PCM 输出入口，解码后的声音用带滤波的重采样直接送声音芯片（照 MyPaperS31 在 S31 上的做法）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11、12 条；第 8 条不碰设备）。

## 为什么

接续记录的待修项：“音乐音质上限：`music-1` 把解码后的声音再编成 16 kHz 单声道 Opus 走现有播放通路，音乐听着闷”（`docs/tasks/backlog.md` “2026-11-11 夜”一节）。用户：“修啊 这些都是问题 不是新的需求任务”；D-131：现有应用和它们在真机能用所需的平台缺口排第一。交付计划把它排在“平台缺口”，来源定为 MyPaperS31 的 S31 声音输出。

现在的通路（`music-1` 报告“和设计不一致的”）：声音队列只收 16 kHz 单声道 Opus，所以 `system/services/audio/codec_formats.c:117-129` 把解码出的 PCM **不经滤波**地按位置抽点降到 16 kHz、混成单声道，再用 `hal_opus_enc_new(24000, 0)`（`:90`，24 kbps、复杂度 0）编成 Opus，播放时再解回来。抽点会混叠，低码率 Opus 会发闷，两样都伤音质。

**要先说清楚的硬件事实**：扬声器那一路的采样率是声音芯片定死的 16 kHz，不能“按原采样率直接送出”（交付计划第 3c 节的写法不成立）：

- 我们的 HAL 头文件：`platform/include/ink/hal_audio.h:4-8`（BH1098G 以 16 kHz 驱动 I2S，ESP32 是从机）；原厂代码同样（`vendor/metalio_bh1098/codec.inc:3-20`，`I2S_ROLE_SLAVE`，`HAL_AUDIO_RATE`）。
- MyPaperS31：`docs/S31_RADIO_PLAYBACK.md:5`“BH1098G 提供 16 kHz I2S 时钟”；`README.md:58`“外放按 BH1098G 实际时钟重采样”。
- kiikoread 改版：`main/music/music_player.c:64`“模组 I2S 固定 16 kHz”。

所以能做、也该做的是：**解码后的 PCM 不再经过 Opus，用带抗混叠滤波的重采样器降到 16 kHz，直接送进播放通路**。

## 成熟来源

- **S31 上的做法（照着做）**：MyPaperS31（`$INK_DEPS/../ink-hw/upstream/oss/MyPaperS31`，提交 `325b538`）`main/audio/audio_radio_s31.cpp`：
  - 解码 → 位宽和采样率转换 → 写 I2S 的一条流水线（370-430 行 `configure()`：源采样率、声道、位宽到目标的转换器）；
  - 解码和重采样必须比界面任务优先级高，否则界面一重画，DMA 里的声音就放空（86-92 行的注释和取值；`docs/S31_RADIO_PLAYBACK.md:5`）；
  - DMA 6 个描述符、每个 240 帧，约 90 ms（112-113 行）；我们现在是 4 个、约 60 ms（`ports/esp32s31/hal_audio_s31.c:14-16`）。
- **重采样器（搬代码）**：MyPaperS31 用的是乐鑫的 `esp_audio_effects`（只有芯片上的二进制库，许可写明只能配乐鑫芯片用），模拟器里跑不了，模拟器和设备就会是两套行为。所以用 speexdsp 的重采样器：`github.com/xiph/speexdsp` 的 `libspeexdsp/resample.c`（BSD-3，单文件，PulseAudio 等大量项目在用，有质量档位），只读克隆到你的 `work/`，固定一个发布提交，整文件拷进来，模拟器和设备用同一份。kiikoread 改版用的是线性插值（`music_player.c:409`），有混叠，不用。
- **立体声**：MyPaperS31 和 kiikoread 改版都往 I2S 两个声道写立体声；我们的 HAL 只收单声道，写的时候左右复制（`codec.inc:54-70`）。扬声器只有一个，单声道够；**蓝牙耳机**那一路（`hal_audio_route_set(HAL_ROUTE_BT_HEADSET)`，BH1098 模式 2）是不是把两个声道分别送到耳机，先查原厂 `874538e` 的代码（`vendor-common.md` 第 1 条的取法）。

## 要做的

1. **PCM 输出入口**：声音服务（`system/services/audio/audio_service.c`）加一个按“源采样率、声道数”收 16 位 PCM 的播放入口，内部重采样到 `HAL_AUDIO_RATE` 后进入和 Opus 同一个输出（同一个输出所有者；铃声、朗读、闹钟抢占和恢复的规矩照旧）。现有 Opus 入口（朗读、录音回放、语音）一行不改。
2. **音乐改走 PCM**：`codec_formats.c` 的 MP3、WAV、FLAC、Vorbis 解码结果直接走新入口；删掉抽点降采样和 Opus 重编码那段（`:90` 的编码器和 `:117-129`）。Ogg Opus 文件仍走原来的 Opus 路。
3. **重采样**：speexdsp 重采样器，质量档位你选（写理由：宿主机每秒音频的耗时和对 S31 的估算，参考 MyPaperS31 日志里 44.1 kHz 降采样每块约 1.26 ms，`S31_RADIO_PLAYBACK.md:71`）。用浮点还是定点按 S31 有硬件浮点来定，写进报告。
4. **调度**：对照 MyPaperS31 的结论，查我们的媒体执行器和界面任务的优先级、DMA 余量；模拟器里墨水屏刷新期间会不会放空（模拟器的 I2S 环形缓冲模型会记欠载）。只有测出欠载才动：优先调优先级；要加 DMA 描述符就照 MyPaperS31 的 6 个，写清多占的内部内存（内部内存很紧，空闲约 114 KB）。
5. **耳机立体声**：原厂代码证明耳机模式下左右声道分开送，就让 PCM 入口保留立体声直到 HAL（HAL 加一个立体声写入，设备上左右分别填，模拟器照写）；证明不了就保持单声道，在报告写清查了哪些文件和行号。
6. 切歌、暂停、拖进度、换家人：停的时候清掉重采样器里的残留，不能在下一首开头听到上一首的尾巴。

## 范围和预算

- 只改：`system/services/audio/`（`audio_service.c`、`media_service.c`、`codec_formats.c`）、`system/include/ink/sys/audio.h`（加入口）、新增 `third_party/speexdsp/`（只放重采样器和许可证，`SOURCE.md` 写提交和哈希）、立体声那一项需要时的 `platform/include/ink/hal_audio.h`、`ports/esp32s31/hal_audio_s31.c`、`ports/sim/hal/hal_audio_sim.c`、声音相关测试、CMake 里新目录的那几行、`docs/tasks/audio-out-1/`。
- 不碰：旧音乐应用 `system/apps/music/`（要被 `music-3` 换掉，不再修）、蓝牙 HAL、朗读和听写（中枢那边 `hub-real-1` 在做）。
- 产品代码不超过 250 行（拷来的重采样器不算）；固件增量不超过 15 KB；内部内存增量写进报告，超过 4 KB 要先停下说明。
- 开工没有前提，今天就能派。
- 这是服务任务，没有新的用户操作，原则上不加能力；真要加，进 `tests/integration/ai_surface.c` 的表并重新生成 `docs/system/capabilities.md`（`python3 tools/capabilities.py`），`ai_surface.c` 只在末尾追加。

## 验收

1. **照 MyPaperS31 的播放路径走一遍**（模拟器）：放一首 44.1 kHz 立体声 FLAC → 暂停 → 拖到 1 分钟 → 下一首（MP3）→ 中途来一段朗读（或来电铃声）→ 朗读完继续放。每一步检查模拟器输出的 `speaker.wav`：没有断续（欠载计数为 0）、切歌处没有上一首的残留、朗读打断和恢复照现有优先级。
2. **音质有数**：44.1 kHz 立体声 1 kHz 正弦波 → 输出里 1 kHz 的信噪比不低于 60 dB；10 kHz 正弦波（高于 16 kHz 采样的 8 kHz 上限）输出衰减不少于 40 dB。改前的通路（抽点 + Opus）用同样的输入量一次，两组数字写进报告。反向对照：重采样换回抽点，10 kHz 那条必须失败。
3. **改前改后对照图**：同一段音乐在改前和改后 `speaker.wav` 的频谱图各一张（单张不超过 300 KB），放 `docs/tasks/audio-out-1/`。
4. 原有的 Opus 测试（`integration-media`、`determinism` 等）不改断言照过；`music-1` 的格式测试照过。
5. 主工程普通全量 ctest 过；ASan 跑声音和媒体相关测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写：宿主机每秒音频的解码加重采样耗时、对 S31 的估算【估算】、固件增量、内部内存和 PSRAM 增量、耳机立体声的结论和出处，以及上机核对清单（主会话外放和蓝牙耳机各听一首、看欠载日志）。
