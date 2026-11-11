# 固定来源

查阅日期：2026-11-11。原文整文件复制，不修改。

## kiikoread-xiugai @ 746ce31

- `main/music/music_tags.c` → `music_tags.c`：SHA-256 `70e39c3be789f88567e14dcc7721cee1aca201b2c48dbef034b3464837192f0f`
- `main/music/music_tags.h` → `music_tags.h`：SHA-256 `1318da3a58203f5c9d01e3718967636157b17c49fbe721be1af6014480c3286b`
- `main/music/music_lyrics.c` → `music_lyrics.c`：SHA-256 `6f4ea6a6c25a9fb3c1f568848ca1c18bdb83bf40aa352c61c403ca3e880dc26d`
- `main/music/music_lyrics.h` → `music_lyrics.h`：SHA-256 `f3e25da5d909a12213e46c60b405850231c78a1d055c506b2f0bea0730892f37`
- `LICENSE` → `kiikoread-xiugai-LICENSE`：SHA-256 `b1a477173eada381b15f2cd522bba1a9ab546759f6534e8c9943f0cb933aeb30`

许可证选择：文件头 Apache-2.0；根 LICENSE 一并保留。


## 接缝

- `esp_heap_caps.h` / `codec_io.h`：将原文的 stdio 和 ESP 分配调用映射到文件服务、OSAL 外部内存；不打开宿主机路径，不使用 ESP 日志或 NVS。
- `metadata.c`：包含原文 `music_tags.c`，重命名其两个入口，保持对歌词库的原 API；补充 ID3/FLAC 专辑及 stb 提供的 Vorbis 注释。固件构建不再单独编译被包含的原文件。
- `music_lyrics.c` 的原始 68,736 字节歌词池保留，切成员时清空可见行；不带入播放任务。
- 播放页参考 `main/apps/app_music.c`（同提交）：曲名、进度、切歌、单行歌词、收藏、分页和目录；使用本项目 LVGL 主题重现，没有复制其帧缓冲、播放器、NVS 或蓝牙逻辑。
- Apache-2.0 全文另附 `Apache-2.0.txt`，取自同一固定副本的 `licenses/Apache-2.0.txt`。文件头许可优先于根目录的其他新增内容许可；未复制受个人学习许可约束的其他产品代码。
