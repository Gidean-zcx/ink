# 音乐测试素材

全部是本任务原创合成的 3 秒音调：左声道 440 Hz，右声道 660 Hz，振幅 9000；16 kHz 单声道与 44.1 kHz 双声道。无真实歌曲、声音、账户或设备数据。

生成工具：SoundFile 0.13.1，libsndfile 1.2.2；只在生成素材时使用，不进入产品依赖或 ctest。`generate.py` 可重建。参考 `.pcm` 是 libsndfile 解码的有符号 16 位小端交错样本，在输入文件采样率和声道数下逐样本比较；WAV/FLAC 容差 0，Vorbis 1 LSB，MP3 3 LSB（解码器整数舍入）。MP3 分别加 ID3v2.3 UTF-16 和 ID3v2.4 UTF-8 标签；FLAC/Vorbis 标签由 libsndfile 写入。

`bad.mp3` 是故意截短的 ID3；LRC 含三条时间行和一条坏时间标签。`formats.c` 的 Opus stub 只供原始 PCM 和解析器宿主测试；真实音频服务、焦点和路由在 `product.c` 及原有 integration-media 中验证。

| 文件 | SHA-256 |
| --- | --- |
| `bad.mp3` | `bf6662e42a766abb83d9a72777c7343e75a66e40c9a2145f3f06688a61df4df2` |
| `tone-16000-1.flac` | `12d4f6638f9f1654cbc8965973af8063b7692a4e26d9d8a0c53ed16c9334581c` |
| `tone-16000-1.flac.pcm` | `436ff20dbe8e6f4e0c89ff8ea60b658b9ed1aeb3eb5ead468d5a6ee1abf04e32` |
| `tone-16000-1.lrc` | `79b0c2bb6278a251704791e61efcf7b4d607b8a8871b63a45e1baae0ebe58af7` |
| `tone-16000-1.mp3` | `d5d75dac882534978d0683cdc419f3b66f5176ae83f4f440e8ea9ac88e897244` |
| `tone-16000-1.mp3.pcm` | `0f40a70169de5da6aac754aeaeff2450bca34c9bf5895d6f28dcb6044ecc4d4a` |
| `tone-16000-1.ogg` | `ccdd9a4846be30d2efda45d887df901821020eae7b2bada4ada5560b4e4891e4` |
| `tone-16000-1.ogg.pcm` | `8744c08945f582c5ed26e54ea20f0b7297ce201402e1295e6008bad268e607ca` |
| `tone-16000-1.wav` | `b69ccbfbdde8843abc6c88027cb43c1eb512c201597139bea4012e28d131a566` |
| `tone-16000-1.wav.pcm` | `436ff20dbe8e6f4e0c89ff8ea60b658b9ed1aeb3eb5ead468d5a6ee1abf04e32` |
| `tone-44100-2.flac` | `e8e5d192939d8730b6cc22566f5b7d8dd39224a801154a1a20f2c4a3c79a4305` |
| `tone-44100-2.flac.pcm` | `c2a6c091358a396dac8eefeaa5ea90bb26c835dea003b07918d1cf56a4ef8073` |
| `tone-44100-2.lrc` | `79b0c2bb6278a251704791e61efcf7b4d607b8a8871b63a45e1baae0ebe58af7` |
| `tone-44100-2.mp3` | `bcc9b144683e3f4d9d4d4749a6eca18f5a44c3398b5697bbf3734dcbd1ee373f` |
| `tone-44100-2.mp3.pcm` | `829a9c0f7a362d84ed3d54ad3b0f6fa5c2cd926be35df6a6ce9d8f05737c5e91` |
| `tone-44100-2.ogg` | `1d559edd7bb929442e54b5d67272fe528d317c61601f9036436250c9a95a633c` |
| `tone-44100-2.ogg.pcm` | `dcc5d666304e0a4c96ed16a5d483eac70a8b101b1f11e2db07f5f4755cb830d2` |
| `tone-44100-2.wav` | `226d83ce940eb0908bd70cb5d24e36a0c31fee1ff49bfb8fd00f69f9426e9cc7` |
| `tone-44100-2.wav.pcm` | `c2a6c091358a396dac8eefeaa5ea90bb26c835dea003b07918d1cf56a4ef8073` |
