# music-2 执行报告

基线：`b66a4a8d6b5e476e2768d926d4f54237fb05ba99`。实现提交：`e2971264`（之后仅更新本报告）。在任务分支本地执行，不推送，不接触设备。日期：2026-11-12。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 普通全量 | 通过，3 项既有跳过 | 原话 `100% tests passed out of 415`；1020.00s；412 项执行通过，跳过原因见下 | C1 |
| 1.2 音乐连跑三次 | 3/3 通过 | 最终源码 4.52s、4.96s、4.46s；合计 14.03s；无失败与栈告警 | C1 |
| 1.3 栈与 PSRAM | 宿主估算通过 | 四格式独立 6320～6640 字节；完整音乐场景 6960，余量 1232；分配峰值见 TSV | C2 |
| 2.1 S31 宿主机 | 75/75 通过 | 原话 `100% tests passed out of 75`；160.65s；本轮无偶发失败 | C4 |
| 2.2 S31 编译 | 通过 | 固件 5077984 字节，5 MiB 应用分区余量 164896（3.15%）；编译 warning/error 均 0 | C4 |
| 3：栈、超时根因、上机清单 | 已记录，未接设备 | 见下文、measurements.tsv；原版场景和三组反向对照已运行 | C2、C3 |
| 补充：ASan/UBSan、确定性与原 Opus | 通过定向 5 项 | 原话 `100% tests passed out of 5`，19.06s；格式、产品、media、两项确定性；没有 sanitizer 错误 | C5 |
| 补充：能力、翻译、应用数、GCC、尺寸 | 通过 | 9 个能力与 22 条英文键；AI 清单、应用数与 i18n 在全量中通过；`gcccheck: 7 compiled, 0 with findings`；`ui-scale: 1 files, 0 violations` | C1、C6 |

## 没做到的

- 真机声音、解码时长和实际栈高水位留给主会话验证；本次不接设备。
- ASan 只跑表中的定向 5 项，未跑 ASan 全量；未在 Linux 上执行测试，GCC 检查是 RISC-V 交叉编译。
- 中枢的 `hub-contract`、`hub-simulator-e2e` 因所选 Python 缺 pytest、pytest_asyncio、aiohttp、yaml 按原规则跳过；`hub-json-crosscheck` 因前项未生成消息而跳过。没有改跳过规则或测试预算。

## 和设计不一致的

- 本次没有增加设计偏差。沿用 music-1 的 16 kHz 单声道 Opus 二次编码输出，未改音频 HAL；不能称为无损输出。
- FLAC、Vorbis 的打开临时状态只在打开期间需要，成功复制或失败清理后立即释放；WAV 工作区和解码器实例、Vorbis arena 在关闭时释放。

## 改过的已有测试

- `tests/integration/apps.c`：应用数 19 → 20，原因是基线新增音乐；没有删除断言或放宽预算。
- `tests/integration/ai_surface.c`：登记 9 个 `music.*` 能力，选填参数用空串、无参数用 NULL、seek 保留必填 ms；没有放宽覆盖检查。
- `tests/music/product.c`：保留原断言，增加 30 秒合成 WAV 的两次局部更新后快进点按，以及单格式栈检查模式；`run.py` 在忽略的构建目录生成 WAV。
- 没改超时、预算、PCM 容差；原版 product.c 另行链接当前实现，也通过。

## 要用户定的

- 本任务没有新增待定项；music-1 的输出格式取舍仍由主会话处理。

## 顺手发现

- 【实现】`show_values` 原来每次刷新都会给播放/暂停按钮追加绑定；本次仅修任务书指定的快进按钮，未扩大修改范围。
- S31 留有 SDK 自身的 6 条 CMake 组件依赖建议和 Kconfig NOTE；实际 C/C++ 编译 warning/error 均为 0。

## 新增的机制

- 无新任务、线程、状态机、持久格式或配置项。播放预填复用原 `M.pump_job`，音乐状态和回调统一归已有媒体执行器。
- 解码工作区：任务书第 1 条要求，继续使用现有外部内存分配接缝。
- 测试专用 `run.py --format` / `INK_MUSIC_STACK_FORMAT`：逐格式估算栈所需；没有增加产品配置项。

## 要改的文档

- 已按任务书重新生成 `docs/system/capabilities.md`：静态能力 476 → 485；其余共享文档不改。

## 栈、PSRAM 与超时

【实测】44.1 kHz 双声道样本，逐格式独立启动、播放、暂停、seek、恢复和关闭。宿主估算如下，完整数字见 [measurements.tsv](measurements.tsv)：

| 格式 | 媒体栈估算 | 8192 字节配置的余量 | 原始解码分配峰值 |
| --- | ---: | ---: | ---: |
| MP3 | 6488 | 1704 | 115009 |
| FLAC | 6320 | 1872 | 86817 |
| WAV | 6320 | 1872 | 49473 |
| Vorbis | 6640 | 1552 | 573761 |

【实测】完整音乐场景包含切歌、成员切换、收藏、局部刷新和点按，媒体栈估算 6960 字节，余量 1232；后台执行器为 4944 字节。独立格式表不是所有输入的理论上界。

【实现】原始解码测试的分配峰值包含实例、输入缓冲和工作区，但 Opus 编码器是 1 字节 stub；不能代替真机整条音频链预算。表中另列模拟器媒体外存标签的峰值，包含应用元数据等；同样不是整机 PSRAM 峰值。没有加大媒体执行器栈。

【实现】FLAC 的初始化结构改走原分配回调；Vorbis 文件打开时的完整临时解码状态改走现有 malloc/free 接缝；WAV 的 4 KiB 打开、读取、转换和 seek 工作区移进外存实例。仅改工作区位置，未改解码或 seek 算法。补丁、原始及当前哈希见 [SOURCE.md](../../../third_party/audio_formats/SOURCE.md)。

【实测】S31 `.su` 单帧：FLAC 打开 384、WAV 初始化 368、read_raw 48、seek 80、Vorbis 文件打开 32、格式 PCM 分发 48 字节。它们不是完整调用链上界；真实 Opus 编码器和 RTOS 高水位仍须上机核对。

【实测】基线重现 `24.940s ... music.play: failed timed out, it may still finish` 和媒体栈估算 10496。临时埋点/trace：请求约 21.900s 发起，处理函数约 24.794s 才开始；文件打开约 24.930s 完成、进度恢复约 24.938s 完成，音频预填到约 25.265s 才返回。主要是换人后的存储等待占住后台执行器，再叠加同步预填，超过原 3 秒交互回执期限；没有发现权限或路径拒绝。

【实现】音乐状态、能力及完成事件改由已有媒体执行器串行处理，避免后台存储工作的队头阻塞；无曲目时不打开收藏集合；播放预填排到已有 pump_job，设置状态后返回。仅移动执行器仍会卡在空页面的收藏加载；仅推迟预填仍会等后台回调，均实测不足以解决。

【实测】三组反向对照：恢复旧播放调度会重现超时；恢复旧快进绑定会使两次点按断言失败；恢复旧解码库工作区会出现媒体栈估算 9744、超过 8192 的告警。反向对照只改临时对象和临时静态库，不动工作区源码。

【实现】快进在点按时读取 UI 当前进度，调用同一个 `music.seek` 能力；不再使用建页时保存的参数。30 秒合成 WAV 从 2 秒快进到 12 秒、再从 3 秒到 13 秒，均未重建页面。

## 复核命令

配置按总规则第 5 节，增加 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 供反向对照与 GCC 检查使用。以下均在仓库根目录执行；构建最多 -j2，测试最多 -j2。

```bash
# C1：普通全量与稳定性
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^music-product$' --repeat until-fail:3 -j1
# C2：逐格式独立启动；格式依次取 mp3、flac、wav、ogg
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/music/run.py --build build --format flac
taskpolicy -c utility ./build/test-music-formats tests/music/fixtures
# C3：原版场景对照；--case 依次取 original、timeout、stack、skip
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/music/regressions.py --build build --baseline b66a4a8d6b5e476e2768d926d4f54237fb05ba99 --case original
# C4：S31，只有编译和宿主机测试
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests
taskpolicy -c utility cmake --build build-s31-tests -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
# C5：相同配置的 build-asan，额外开启 INK_SANITIZE
taskpolicy -c utility cmake --build build-asan --target sim-music test-music-formats sim-media sim-selftest sim-configured -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '^(music-formats|music-product|integration-media|determinism|determinism-reader)$' -j2
# C6：生成文档、原文哈希、GCC 和 UI 尺寸
taskpolicy -c utility python3 tools/capabilities.py --check
taskpolicy -c utility python3 tests/music/verify_vendor.py
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/music/music.c system/services/audio/media_service.c system/services/audio/codec_formats.c tests/music/product.c tests/music/formats.c tests/integration/ai_surface.c tests/integration/apps.c
taskpolicy -c utility python3 tools/ui_scale_check.py system/apps/music/music.c
```

## 规模与检查

【实测】相对指定基线，产品 C/H 新增 126 行、删除 90 行，低于 200 行预算；测试和文档另计。为什么不能更小：86 行属于三个解码库的工作区位置补丁，40 行属于调度、快进与翻译。已经试过只改执行器、只推迟预填，两者单独都不足以修超时；没有采用任务书禁止的加栈或加超时。

【实测】相对基线的 15 个改动文件均在允许路径。已检查总差异、报告相对链接、12 个 vendor 哈希与 `git diff --check`。按总规则第 6 节对基线至 HEAD 扫隐私，零匹配（grep 退出 1）；构建产物和原设计稿没有提交。

## 上机核对清单

- 主会话分别播放 MP3、FLAC 长曲，记录媒体/音频任务真实栈高水位、PSRAM 峰值、最慢解码块和声音是否连续。
- 孩子冷启动首次播放及成人/孩子往返切换：回执在原超时内完成，各自恢复进度，孩子不能打开成人路径。
- 播放页局部进度更新后多次点快进，均按当前进度增加 10 秒；再核对切歌、暂停、继续、收藏、歌词与耳机/来电打断。
- 中英文、100%/120% 下查看布局与墨水屏局部刷新；没有本次真机或窗口版验收。

## 里程碑

- 已核对 HEAD 等于指定基线，工作区干净；已读取总规则和任务书。
- 栈、冷播放、翻译、能力表、应用数和快进回归已实现；GCC 检查 `7 compiled, 0 with findings`，UI 尺寸 `1 files, 0 violations`，12 个 vendor 哈希通过。完整构建与最终检查继续执行。
- S31 编译及 75 项宿主机测试、最终 ASan/UBSan 定向 5 项均通过。
- 普通全量无失败、3 项既有跳过；最终音乐连续 3 次通过。没有需要单独复跑的偶发失败。
