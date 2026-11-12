# music-pcm：音乐按原采样率直接播放，不再转成 16 kHz 单声道 Opus

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`。

现状（`system/services/audio/codec_formats.c` 约 117～130 行）：MP3/FLAC/WAV/OGG 解码后，按取样点直接抽成 16 kHz 单声道（没有低通滤波，会混叠），再编成 Opus 走原有的语音播放通路。声音 HAL（`platform/include/ink/hal_audio.h`）只有固定采样率（Plus 上 16 kHz）。听起来发闷、有杂音。

## 要做的

1. 解码出的 PCM 直接送声音输出，不再先编 Opus 再解码。语音、提示音、AI 朗读的 Opus 通路不动。
2. 查原厂资料，确认 Plus 的声音输出（BH1098 模式 1 的 I2S）支持哪些采样率：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/docs/bh1098-audio-protocol.md` 和原厂 `main/boards/metalio-e-ink4-plus/` 的声音代码。如果支持 44.1/48 kHz，HAL 加一个按采样率开播放的接口（模拟器和 S31 都实现），音乐用文件的原采样率或最接近的支持值；只支持 16 kHz 就保持 16 kHz。
3. 需要变采样率时用成熟的重采样实现：先找仓库或原厂里已有的（例如 speexdsp 的重采样器、ESP-ADF 的），没有就用带低通的多相实现并写清来源。不能再按取样点抽。立体声按设备声道数合成。
4. 顺手修 `music-2` 报告里提到的问题：`show_values` 每次刷新都给播放/暂停按钮追加一次绑定。

## 范围和预算

- 只改：`system/services/audio/**`、`system/apps/music/**`、`platform/include/ink/hal_audio.h`（只加接口）、`ports/sim/hal/hal_audio_sim.c`、`ports/esp32s31/hal_audio_s31.c`、`third_party/` 下新拷入的重采样代码、`tests/music/**`、`docs/tasks/music-pcm/`。不碰别的任务在改的文件。
- 胶水不超过 250 行（拷来的原文不算）；不新增任务或线程，不加大执行器栈（媒体执行器 8192 字节，`music-2` 后余量 1232 字节，大块缓冲放 PSRAM）。

## 验收

1. 测试：同一段 44.1 kHz 的正弦和扫频，输出在 1 kHz、6 kHz 处的幅度和原文件一致，混叠低于 −60 dB；音乐不再调用 Opus 编码；暂停、跳转、切歌、成员切换照旧；反向对照。
2. 主工程普通全量 ctest 过（含 `music-*`），`music-product` 连跑 3 次；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件大小、栈余量和上机核对清单（主会话在真机放 MP3 和 FLAC）。
