# music-1：本地音乐播放器（成熟解码库 + kiikoread 改版的标签、歌词和播放页）

先读 `docs/tasks/vendor-common.md`（公共规矩，第 1～3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。

用户在功能清单里勾了“本地音乐播放器：MP3、FLAC、WAV、OGG、M4A，浏览、切歌、音量、进度”（`docs/tasks/reuse-crosspoint-1/picks.md`）。现在只能放 Ogg Opus 并记住进度，没有音乐应用。

输入：

- `docs/tasks/reuse-crosspoint-1/REPORT.md` 第 79、80、142、152、153、229、274 行（解码器、标签歌词、播放器的来源和“合并解码与元数据，保留媒体服务和路由”的结论），`port-queue.json` 的 M01、M02、M03。
- 固定副本（只读，`$INK_DEPS/../ink-hw/upstream/oss/`）：
  - `dr_libs`（提交 f12e197，`dr_mp3.h`、`dr_flac.h`、`dr_wav.h`，选 MIT-0）；
  - `stb`（提交 4e9b368，只取 `stb_vorbis.c`，选 MIT）；
  - `kiikoread-xiugai`（提交 746ce31，`main/music/music_tags.c`、`music_lyrics.c` 和 `main/apps/app_music.c` 文件头是 Apache-2.0）。它的播放核心 `music_player.c` 用 `esp_audio_simple_dec`、自己的任务和 NVS，**不搬**；它的播放页 `app_music.c` 用自己的帧缓冲绘图，**照搬布局和交互，用 LVGL 重现**。

## 要做的

1. **解码库原文入库**：`dr_mp3.h`、`dr_flac.h`、`dr_wav.h`、`stb_vorbis.c` 整文件放 `third_party/audio_formats/`，旁边 `SOURCE.md` 和许可证选择。在现有媒体服务（`system/services/audio/media_service.c`）里加一个格式分发层（`system/services/audio/codec_formats.c`），用现有的流读取和内存分配接口喂解码器；Ogg Opus 照旧。M4A/AAC 没有许可清楚的来源，**不做**，报告列缺口。
2. **标签和歌词**：`music_tags.c`、`music_lyrics.c` 原文放 `system/services/audio/vendor/kiikoread/`（保留 Apache 声明，`SOURCE.md`），胶水换掉它的 ESP 依赖；读 ID3/FLAC/Vorbis 标签里的歌名、歌手、专辑，同名 `.lrc` 歌词。
3. **音乐应用**（`system/apps/music/music.c`，注册进应用列表）：照 `app_music.c` 的排法——曲名和进度、上一首/播放/下一首、音量、单行歌词、播放列表翻页、收藏、“全部歌曲”按文件夹浏览；用我们的主题令牌和统一组件。曲库在卡上的音乐目录（沿用现有文件服务的路径和成员规则）。墨水屏只刷变化的部分，进度条不要每秒整屏刷（D-127：按秒更新数字的局部、或者降到合理频率，报告写清）。
4. **AI 接入**（人和 AI 共用）：`music.play`（某首、某个文件夹、继续）、`music.pause`、`music.next`、`music.prev`、`music.seek`、`music.list`、`music.now`（正在放什么、进度）；音量用现有音量能力。页面快照给当前曲目、进度、列表这一页。播放位置按成员记住（现有媒体服务的进度机制）。来电、录音、语音打断照现有音频焦点规则。
5. 蓝牙耳机：只用现有音频路由接口，跟着 `port-bt-audio-2` 的结果走，不另做蓝牙逻辑。

## 范围和预算

- 只改：新增 `third_party/audio_formats/`、`system/services/audio/codec_formats.c`、`system/services/audio/vendor/kiikoread/`、`system/services/audio/media_service.c`（只加格式分发的接入点）、新增 `system/apps/music/`、`CMakeLists.txt` 和固件 `ports/esp32s31/main/CMakeLists.txt` 里这些文件的几行、注册应用列表那一处、生成的 `docs/system/capabilities.md`、新增测试、`docs/tasks/music-1/`。不碰别的任务在改的文件。
- 胶水和界面新增不超过 600 行（拷来的原文不算）。不新增播放任务或线程（解码在现有媒体服务的执行上下文里做，写明每块解码的时间和内存）。

## 验收

1. 测试：自己生成的几秒 MP3、FLAC、WAV、Ogg Vorbis 测试音频（原创或合成的音调，提交小文件或测试时生成）解码出的 PCM 和参考一致（容差写明）；seek、坏文件、播放中切歌和释放；标签和歌词解析（含坏标签）；成员各自的进度；AI 调能力和页面一致；Ogg Opus 原有测试不放宽照样过；反向对照。
2. 主工程普通全量 ctest 过；S31 编译通过、无新警告，报告写固件增大多少、每种格式在宿主机上解码 1 秒音频的时间（真机由主会话量）。
3. 截图（播放页、列表、全部歌曲，常规和 120%，`docs/tasks/music-1/`，单张不超过 300 KB）；上机核对清单（主会话在真机放 MP3 和 FLAC，看卡顿、耗电和刷新）。
