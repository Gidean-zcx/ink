# music-1 执行报告

本轮部分完成，**未通过全部验收**。基线 `69cc9868a5e6b95c8376293b385308431fef92a0`；固件源码提交 `d2be1d73`。只做本地提交，未接设备、未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 解码库、标签歌词原文和许可 | 通过 | 固定提交的 12 个文件哈希核对成功；MIT-0 / MIT / 文件头 Apache-2.0，见[解码库来源](../../../third_party/audio_formats/SOURCE.md)和[标签歌词来源](../../../system/services/audio/vendor/kiikoread/SOURCE.md) | `python3 tests/music/verify_vendor.py` |
| 1.2 四格式 PCM、seek、释放 | 通过已测样本 | 16 kHz 单声道、44.1 kHz 双声道各 3 秒；8 个样本逐样本比较、1 秒 seek、关闭后分配回到基准 | `./build/test-music-formats tests/music/fixtures` |
| 1.3 标签、歌词、坏数据 | 部分覆盖 | ID3v2.3 UTF-16 / v2.4 UTF-8、FLAC / Vorbis 的曲名、歌手、专辑及 LRC 通过；坏数据测了截短 ID3 和坏 LRC 标签，未逐格式覆盖损坏容器 | 同上 |
| 1.4 切歌、成员进度、AI 与页面 | 未全过 | 普通构建 `pass=182, fail=1, finished=1`；孩子首次播放回执超时，媒体栈另有告警；切歌、成员进度断言、AI 进度与页面、收藏点按已走通 | `python3 tests/music/run.py --build build` |
| 1.5 原有 Opus 与确定性 | 通过 | 普通构建的 integration-media、determinism、determinism-reader 均通过，原断言未改 | C1 |
| 1.6 反向对照 | 通过 | 临时对象撤掉格式分发后，原始 PCM 测试在打开文件断言处退出 -6；正常对象不变 | `python3 tests/music/reverse.py --build build` |
| 2.1 普通全量 | 未通过 | 原话：`98% tests passed, 8 tests failed out of 403`；935.60 秒；另 3 项按原有条件跳过 | C1 |
| 2.2 ASan / UBSan | 定向检查完成，功能仍失败 | 最终 5 项中 4 项通过；music-product 仍为同一回执超时，没有 ASan / UBSan 错误诊断；未跑 ASan 全量 | C2 |
| 2.3 S31 编译及体积 | 通过 | 两个版本 build.sh 均退出 0，编译 warning 为 0；4485632 → 4659344 字节，增加 173712 字节（169.640625 KiB，3.8726%） | C3 |
| 2.4 S31 宿主全量 | 首次未全过 | 原话：`99% tests passed, 1 tests failed out of 72`；cell-detect 失败，单独复跑通过；未改蜂窝代码 | C4 |
| 2.5 GCC 与 UI 尺寸 | 通过 | `gcccheck: 8 compiled, 0 with findings`；`ui-scale: 1 files, 0 violations`；没有实跑 Linux | C5 |
| 2.6 解码时间和内存 | 已测宿主 | 每秒音频解码时间、1024 帧最慢块、分配峰值见 measurements.tsv；不等于真机表现 | 原始 PCM 测试 |
| 3 截图与上机清单 | 截图完成，布局有偏差 | 三种页面 × 100% / 120%，六张均小于 300 KiB，已逐张查看；清单见末尾 | music-product 的实际模拟器截图 |

## 没做到的

- M4A/AAC 按任务书排除，没有许可清楚的解码来源。
- 音乐运行测试三次修复后仍有孩子冷播放回执超时：24.940 秒处报 `timed out, it may still finish`。普通构建还报媒体执行器配置 8192 字节、宿主估算使用 10496 字节。按总规则第 7 节停止继续修此测试，没有抬预算、延长超时或新开线程。
- 没有完成共享能力文档、AI 覆盖清单、旧应用数量断言和翻译登记的同步；这些不在允许修改范围。
- 文件夹播放、跨进程“继续”、音量和进度条手势、长曲目、歌词变化的刷新面积尚未做专项实测。代码中的“快进10秒”参数在建页时绑定，局部更新时没有同步重绑，仍需处理。

## 和设计不一致的

- 现有音频队列只收 16 kHz 单声道 Opus。新格式先解码，再降采样／混成单声道，最后经现有 Opus HAL 编码；有音质损失。逐样本一致只在原始解码接口验证，不能称为无损播放。
- 播放页和播放列表分开为页签；全部歌曲采用统一组件的四行分页。未照搬上游播放页底部四行列表和全部歌曲十二行的布局。

## 改过的已有测试

没有修改、删除或放宽任何已有测试。新测试的 PCM 容差固定为 WAV / FLAC 0、Vorbis 1、MP3 3 LSB，实测最大误差分别为 0、0、1、1 LSB。新测试切成员时移除了误用于孩子的“跳过首次设置”辅助操作。

## 要用户定的

主会话需决定是否放开音频服务改动范围，补 PCM 输出入口，或接受目前的 16 kHz 单声道二次编码；栈和冷播放超时都仍须解决。当前结果尚不满足上机验收条件。

## 顺手发现

- S31 宿主 cell-detect 在 `ports/esp32s31/tests/cell_host.cc:184` 的 MRDY 两秒等待断言中止；独立复跑 1/1 通过（14.99 秒）。这些源码相对基线没有修改，未认定根因，也未放宽等待。
- 普通全量的八项失败为 i18n-memo、i18n-shell-remaining、i18n-extras、i18n-literals、capability-table-doc、integration-ai-surface、integration-apps、music-product。前三类涉及新“音乐”名称及文案、清单和数量同步。
- 全量后仅补齐新能力 JSON Schema 的根 `type: object`；定向复查的类型断言失败已归零。AI-surface 仍有 12 条清单／参数示例失败；注册表导出和运行时交叉检查均通过。普通全量未因这一声明修正重复跑第二遍。

## 新增的机制

- 格式分发及解码器实例：任务书第 1 条要求，全部使用现有媒体执行器；没有新增播放任务或线程。
- 每实例 4 KiB 顺序输入缓冲、512 KiB Vorbis 临时空间：格式读取接缝，放外部内存，实例关闭时释放。
- music.favorites、music.last：任务书的收藏和继续播放，使用现有成员数据集合，没有新持久文件格式。
- LVGL 五秒定时器：任务书 D-127；常规进度只更新数字、歌词和条宽，没有每秒整页重画。

## 要改的文档

- `docs/system/capabilities.md` 按总规则第 7 节交主会话统一生成：新增 9 项音乐能力，静态目录 470 → 479。
- 主会话同步 `tests/integration/ai_surface.c` 的覆盖清单与参数样例、`tests/integration/apps.c` 的应用数量（增加音乐后为 20）、`assets/i18n/` 以及翻译检查的文案／固定目录规则。
- 首版支持与音频资源说明需如实记录 M4A 缺口、16 kHz 单声道二次编码和未通过项。

## 资源与证据边界

【实测】产品 C/H 新增 545 行、替换删除 12 行（不计原文、测试、文档）；CMake 另增 22 行，仍在 600 行预算内。为什么不能更小：四种解码、元数据、页面和原子能力均为任务要求；只接解码或复制上游播放器会分别缺功能，或绕开现有媒体焦点与成员进度。

【实测】[measurements.tsv](measurements.tsv) 来自普通构建。16 kHz 单声道每秒音频的解码时间：WAV 0.0963 ms、FLAC 0.1547 ms、Vorbis 0.2557 ms、MP3 0.3053 ms。表中另列 44.1 kHz 双声道与最慢块。分配峰值不包含真实 Opus 编码器（宿主测试使用 1 字节 stub），歌词原文另常驻 68736 字节，不能把表中值当整条设备音频链预算。

【实现】RISC-V `.su` 中 drflac_open_with_metadata_private 单帧 4944 字节，drwav_init__internal 为 4432 字节。stack_report.py 排除间接回调和部分库帧，其执行器“已知直接链 48 字节”不是安全上界。媒体栈 8192 字节，原音频任务栈 26624 字节（`system/services/res/exec.c:14`、`system/services/audio/audio_service.c:55`）。

【实测】基线在独立检出中以同一 SDK／配置构建；原厂只读对象源 HEAD 前后仍为 690a023。普通工程以 Werror 编译通过；保留第三方 SDL 弃用和既有重复链接提示。S31 编译 warning / error 为 0，SDK 的 CMake 组件依赖建议仍存在。

【实测】六张截图：[播放100%](player-100.png)、[播放120%](player-120.png)、[列表100%](list-100.png)、[列表120%](list-120.png)、[全部100%](all-100.png)、[全部120%](all-120.png)。

## 复核命令

配置按总规则第 5 节，构建均加 `taskpolicy -c utility`、最多 `-j2`。ASan 构建增加 `-DINK_SANITIZE=ON`，复用相同字体资源；补编了两个确定性用例各自的专用程序后取得最终 4/5 结果。

```bash
# C1：普通全量；原始结果针对 eb921cd9，之后只补了 Schema 声明
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# C2：最终源码的内存检查
 taskpolicy -c utility cmake --build build-asan --target sim-music test-music-formats sim-media sim-selftest sim-configured -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '^(music-formats|music-product|integration-media|determinism|determinism-reader)$' -j2
# C3：当前分支与指定基线的独立检出分别运行
 INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
# C4：S31 宿主测试，失败单项另以 -R '^unit-s31-cell-detect$' -j1 复跑
 taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests
 taskpolicy -c utility cmake --build build-s31-tests -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
# C5：GCC 与尺寸检查
 taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/music/music.c system/services/audio/codec_formats.c system/services/audio/vendor/kiikoread/metadata.c system/services/audio/vendor/kiikoread/music_lyrics.c system/services/audio/media_service.c system/boot/main.c tests/music/formats.c tests/music/product.c
 taskpolicy -c utility python3 tools/ui_scale_check.py system/apps/music/music.c
```

## 上机先核对清单

- 先解决冷播放超时并核对媒体解码／Opus 编码的实际栈余量；本次没有烧录或串口测试。
- 主会话播放 MP3、FLAC 长曲，记录卡顿、解码块时间、内存、耗电；验证切歌、seek、文件夹和继续播放。
- 分成员核对同一首的进度、歌词；验证来电、录音和语音打断，以及沿用现有接口的耳机路由和断开暂停。
- 100% / 120% 下核对歌词、进度的局部刷新，离开页面后停止定时刷新，确认卡顿与残影情况。

## 最终文件检查

【实测】改动仅在任务允许位置；没有修改共享需求、决策、已有研究／审计文件或设备驱动。原文哈希、链接、截图体积和 `git diff --check` 均已核对。对基线至最终提交执行任务指定隐私扫描，零匹配；构建产物与原设计稿未提交。
