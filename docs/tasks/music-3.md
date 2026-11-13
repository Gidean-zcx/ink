# music-3：音乐换成 kiikoread 改版的播放器（曲库、收藏、四种循环、标签、歌词、续播），经嫁接接口整块搬进来；新音乐接过旧音乐的名字、能力和数据，旧音乐退役

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 1、2、6、9、11 条；第 8 条不碰设备）。

**D-140（总规则第 11 节）**：本任务是“搬代码”（kiikoread 改版的音乐播放器）。原文整文件或整函数拷进来，`SOURCE.md` 写来源、固定提交、改了哪些行和为什么；为这台设备改的地方（墨水屏刷新、灰阶、108 px、随字号一起放大）也逐条记进 `SOURCE.md`。原文搬不动就写证据、停下，写进“要用户定”，不悄悄换成自己写。

## 为什么

用户 2026-11-12（D-129）：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“我更接受你提出的移植完整应用嫁接和封装…到我们的系统中”。D-131：音乐属于第一优先的“现有应用”，要做到成熟可用。

声音调研（`docs/tasks/replan-media/REPORT.md` 第三块“完整应用”，来源 S06）把 **kiikoread 改版**排在本地音乐第一：目录浏览、收藏、列表/单曲/随机/收藏四种循环、标签、外置和内嵌歌词、按家人记住的进度和状态，“闭环最可核查”。交付计划 3b 节音乐（播放器）一行：搬代码（经嫁接接口），胶水 600 行，依赖嫁接接口和 audio-out-1。（本任务把胶水上限提到 850 行，理由见“范围和预算”。）

替换方式照 D-139（阅读只留一个）：新音乐就叫“音乐”，是唯一的音乐应用；旧音乐（`system/apps/music`）退役，它的应用名、能力名（`music.*`）、各家人的收藏和上次听的歌由新音乐接过来，按 D-138 自动迁移；用它的测试改成走新音乐；旧代码留在 git 历史里。注意：D-139 的用户原话只说了阅读（“只保留一个新阅读就可以了”），D-135 和交付计划写的是音乐“并存、用户说换再删”。所以**删旧音乐放在最后一个单独提交**，主会话合并前可以单独留住它。

交付计划第 8 节并进来的已知问题：“音乐每次刷新都重复绑定按钮”。

### 现状（基线 `c4ad7dea`）

- **旧音乐** `system/apps/music/music.c`（258 行，LVGL）：照 kiikoread 布局简化重做的播放页、播放列表、全部歌曲。9 个能力 `music.open/play/pause/next/prev/seek/list/now/favorite`（`:246` 起）；按家人的两个集合 `music.favorites`（`:29`）、`music.last`。问题：
  - `show_values()` 每次刷新都对播放键调 `ink_shell_bind`（`:74`），而 `ink_shell_bind`（`system/ui/shell/navigation.c:53-73`）每次都新分配一份绑定、再挂三个事件回调。刷新越多，回调和内存越多。这就是计划里的已知问题。
  - 没有循环模式、随机和收藏循环，曲末只按顺序放下一首（`finished()`，`:253`）；没有行内收藏。
  - 页面开着时每 5 秒调一次 `music.now`（`:173`、`:226`），不是变了才刷。
  - 默认目录直接把 `/me/音乐` 这串路径显示给人看（`:42`、`:213`）。
- **已经搬进来的 kiikoread 部分**：`system/services/audio/vendor/kiikoread/`，有 `music_tags.c`、`music_lyrics.c` 原文，`metadata.c` 补了专辑和 Vorbis 注释（`ink_music_tags`），`SOURCE.md` 固定在 `746ce316`。标签和歌词用 `fopen` 读解析后的路径。
- **播放**：`system/services/audio/media_service.c` 提供 `media.play/pause/resume/seek/stop/status` 和 `media.finished` 事件，按家人、按文件记听到哪（`media.positions`，`:41`）。audio-out-1/2 已合入：解码后用 speexdsp 降到 16 kHz 直接送出，不再走 Opus。扬声器芯片固定 16 kHz，见 `docs/tasks/audio-out-1.md:11-17`。媒体执行器的栈是 8192 B，music-2 估算已用 6,960 B。
- **嫁接接口** `system/graft/graft.h`、`graft.c`：一块 L8 画面，交脏区和刷新意图（`INK_GRAFT_PAGE/WHOLE/PART`）；点按、长按、按键、滑动、晃动；成员文件（只许 `/me/`，加上读 `/family/fonts/`；**打不开 `/family/audio`**）；偏好集合（名字要以应用 id 加点开头）；打开、暂停、恢复、关闭和快照。同一时间只挂一个客户端。Mogo 的接法是现成例子：`apps/mogo/mogo.c`（171 行；BG 执行器上排版绘制，UI 执行器提交，点按换成能力调用，见 `ink_mogo_input`），字体先找 `/me/fonts/InkSC.ttf`、再找 `/family/fonts/InkSC.ttf`（`apps/mogo/port/runtime.c:51-59`）。
- **符号冲突（必须避开）**：`apps/mogo/port/drawing.c` 在 `ink_apps` 里定义了全局符号 `ui_clear_page`、`ui_fill_round_rect`、`ui_draw_round_rect`、`ui_text_box`、`ui_draw_button`、`ui_nav_back`、`epd_fill_circle`、`epd_draw_circle` 等，和 kiikoread 的 `app_music.c` 要用的名字相同，而 `apps/mogo` 只读。所以音乐有自己的端口层（`apps/music/port/`），画图函数一律加前缀（用 `-include` 的头文件把 `ui_*`、`epd_*` 改名成 `ink_music_*`），**不链接也不复用 Mogo 的 `drawing.c`**，只共用字体引擎 `ttf_font`（经 `ink_mogo_native` 或同一批源文件，`music.cmake` 里说明）。`app_music.c` 引用的 `app.h`、`app_registry.h`、`bt_host.h`、`settings.h`、`display.h`、`e0470_epaper_waveform.h`、`ui_gesture.h`、`ui_kit.h`、`ui_menu.h`、`ui_nav.h`、`freertos`、`esp_log` 都用端口层的替身头文件顶上，替身算胶水。
- **歌放在哪**：传书把音乐传到 `/me/音乐`（`ports/esp32s31/vendor/read_pico_transfer/read_pico_transfer.c:1774`，提交 80cabda2）；全家共享的放在 `/family/audio`（`tests/music/run.py` 用的就是它）。`system/services/file/transfer.c:331` 还在建 `/me/music`，留下一个空文件夹。这个文件归传书，不归本任务，写进报告。
- **外面用到音乐的地方**：主屏“正在进行”卡 `system/ui/shell/activity_card.c:85-97` 显示文件名，直接调 `media.pause/resume`；锁屏 `system/ui/lock_mark.c:392` 看 `media.state`；文件应用用 `media.play` 直接放音频（`system/apps/files/files.c:276`）；`tests/integration/apps.c:146` 断言应用数是 23；`tests/integration/ai_surface.c:64-65` 有 `music.*` 能力表。

## 来源（搬代码，固定副本只读）

kiikoread 改版，`oouu00/Metalio-S31-kiikoread-xiugai` 提交 `746ce316faf61ce5228eceee618eacb50a02a395`。本机副本在 `$INK_DEPS/../ink-hw/upstream/oss/kiikoread-xiugai`，用到的文件头都是 Apache-2.0。整文件或整函数拷，不挑几行重写。

- `main/apps/app_music.c`（1049 行）是页面：
  - 播放页：模式、输出、重连三个胶囊（`:51-62`），曲名，单行歌词，点进度条跳转，上一首/播放/下一首，音量，4 行播放列表，行内收藏，翻页；
  - 全屏“全部歌曲”（12 行）：有子文件夹就按目录层级浏览（`:142-279`），没有就收藏在前平铺（`:126-131`）；
  - 命中表 `target_at`（`:293-316`）；
  - 局部刷新：歌词行只刷 `LYRIC_DIRTY`，进度条只刷自己那条窄条（`s_bar_drawn/s_bar_shown`，`:78-80`），`on_tick` 按变化计数节流（`:759`）；手势 `:818`，按键翻页 `:1003`。
- `main/music/music_player.c`、`music_player.h` 只拿它的曲库和规则部分：
  - 扫描：两层、最多 200 首、后缀表（`ext_wanted` 和 `scan_dir`，`:297-332`；`music_scan`，`:1277`）；
  - 收藏：按完整路径记（`fav_key`，`:333`；`music_favorite_toggle`，`:1514`）；
  - 曲末规则 `finish_track`（`:465-522`）：单曲循环重放同一首；随机在多于一首时不选当前这首；收藏循环只在收藏里转，收藏清空就退回列表循环；手动上一首/下一首始终按完整曲库；
  - 切模式时没有收藏就跳过收藏循环（`music_cycle_mode`，`:1549`），模式名 `music_mode_label`（`:1557`）；
  - 重扫时按路径把正在放的曲目找回来（`:1306-1340`）；
  - 记住上次的曲目、位置、是不是在放（`:224-238`、`:1016-1041`）；
  - 从文件管理直接放的单个文件放完就停，不接曲库的下一首（`s_single`，`:465-474`）。
- 已在树里的 `music_tags.c`、`music_lyrics.c` 照旧用。

**不搬**（vendor-common 第 2 条，写进 `SOURCE.md`）：解码任务、PCM 环、重采样、A2DP 拉流、蓝牙模组上电和重连、NVS 读写、锁屏时暂停播放的策略（我们的媒体服务规定锁屏后照样放，见 `system/include/ink/sys/media.h` 开头），以及 ESP 日志。播放一律交给媒体服务，不建第二套播放任务或队列。

## 要做的

1. **新音乐挂在嫁接接口上**：放在 `apps/music/`，页面原文放 `third_party/kiikoread_music/`。照 `apps/mogo/mogo.c` 的分工：
   - BG 执行器画，UI 执行器用 `ink_graft_present` 交脏区；
   - 实现 `open/close/pause/resume/input/snapshot`；
   - 换家人时先关会话、清掉 kiikoread 的全局状态（`s_tracks`、`s_fav`、`s_order`、目录和页码等），再给新家人打开；
   - 端口层按上面“符号冲突”一条做：自己的前缀画图函数和替身头文件，不碰 Mogo 的 `drawing.c`。

   页面结构照 kiikoread：播放页加 4 行列表，全屏全部歌曲，平铺和按目录两种。和我们设计语言冲突的地方改掉（vendor-common 第 11 条）：
   - 所有点按范围不小于 108 px（`INK_CONTROL`）。原表里 40～64 px 的行和 72 px 的圆键都按令牌重新排；一屏放得下几行就放几行，只有一页时不画“1/1”。
   - 尺寸和字跟系统“文字大小”一起缩放：kiikoread 的 `sy()` 换成 `ink_ui_px`，孩子按 110%。文字、图标、间距一起放大。
   - 不画重框和圆圈：去掉 `draw_frame` 的三层框（`:328`）、`circle_frame`（`:331`）和播放键外的圆。行间用细分隔线，一屏只有播放这一个主按钮。
   - 按下不反色（D-31），结果就是反馈。
   - 去掉它自己的底部标签栏（`ui_nav_draw`）和返回箭头：状态栏和返回由外壳负责。
2. **播放器核心**：保留 `music_player.h` 作为页面调用的接口，下面换成整函数拷来的曲库、收藏、模式、续播逻辑，加上接到媒体服务的胶水：
   - 播放用 `media.play`，播放/暂停切换用 `media.pause`、`media.resume`，跳转用 `media.seek`，状态和进度读 `ink_media_status`。
   - **曲末接着放**：在开机初始化时订阅 `media.finished`，不放在嫁接客户端里。这样离开音乐页、锁屏、打开阅读以后，照样按当前模式接下一首。从文件应用直接放的单个文件，放完就停。订阅回调跑在媒体执行器上（栈 8192 B）：它只许选出下一首的路径并调 `media.play`，不在里面扫描、读标签或读文件。
   - 随机用系统的随机数。
   - 扫描和读标签放在 BG 执行器，不放媒体执行器。用文件索引（`ink_fs_index(..., INK_FS_AUDIO, ...)`，旧音乐 `:154` 已经在用）。标签和歌词照旧走 `ink_music_tags`、`music_lyrics_load` 直接读路径，**不经嫁接的成员文件接口**（它打不开 `/family/audio`）。要在模拟器和真机文件层各确认一次 `/me/音乐` 和 `/family/audio` 的标签都读得到，读不到写进报告。
   - 曲库来源：当前家人的 `/me/音乐`，加上全家的 `/family/audio`；两层，最多 200 首。超过 200 首时，页面和快照要写清“只列出了前 200 首”。
   - 格式：媒体服务能放的照放（mp3、flac、wav、ogg、opus）。kiikoread 还列 m4a、aac，这次不加 AAC 解码（解码器来源和 S31 体积要另查，见 replan-media 第三块）。这类文件照样列出来，标“这种格式还不能放”，点了说明原因，不当成坏文件；写进报告。
3. **按家人的数据**：收藏、上次听的歌、循环模式按家人存；音量照旧是系统设置 `audio.volume`；听到哪由 `media.positions` 记。
   - 优先沿用 `music.favorites`、`music.last` 两个集合，格式不变就不用迁移。
   - 模式用一个以 `music.` 开头的新集合（可以经 `ink_graft_preferences_open`）。
   - 要换格式的话，第一次打开或开机时自动迁移（D-138），带测试。
4. **能力：人和 AI 用同一个入口**：
   - 9 个旧能力的名字和参数保持兼容，`music.list` 的 `view` 照旧收 `player/list/all`。
   - 新增 `music.mode`（设成某种模式，或 `cycle` 顺着切）。`music.favorite` 加一个可选的 `path`，给行内收藏用。新增 `music.favorites`（列出收藏）。`music.now` 补上模式、是否收藏、当前歌词行。
   - 每个点按区都对应能力，对照表写进 `SOURCE.md`：

     | 点按区 | 对应能力 |
     | --- | --- |
     | 模式胶囊 | `music.mode` |
     | 上一首、播放/暂停、下一首 | `music.prev`、`music.play`/`music.pause`、`music.next` |
     | 进度条 | `music.seek` |
     | 音量加减 | `settings.set` 改 `audio.volume`（只是调用它，不改设置的文件；孩子的音量上限照旧） |
     | 列表行 | `music.play`（带 `path`） |
     | 行内收藏 | `music.favorite`（带 `path`） |
     | 翻页、全部歌曲、进文件夹、上一级 | `music.list` |
     | 输出胶囊 | 现有的 `bt.*` 能力和 `audio.route` 状态；没有合适的单步能力就写进报告，不在音乐里造第二套蓝牙管理 |
     | 重连胶囊 | 原厂蓝牙模组的断电重启兜底。我们的蓝牙服务没有对应能力就不自己造，列进报告“要用户定的”，不悄悄删掉 |

   - 点按的处理照 `ink_mogo_input`：算出命中区，用触摸来源调 `ink_cap_call`。
5. **页面快照**（页面 id `Music`）写清：当前曲名、歌手、专辑、进度、播放状态、循环模式、当前歌词行、是否收藏、声音从哪儿出来、这一页能看到的行（名字、歌手、是否收藏、是否正在放、是不是文件夹）、第几页共几页，以及曲库的说明（超过上限、有不能放的格式、缺文件）。不出现其他家人的路径。
6. **少刷新**（D-127）：
   - 进度走动只刷进度条那一条和时间；
   - 歌词换句只刷歌词行；
   - 改模式或收藏只刷那一个控件；
   - 换页、进出全部歌曲才刷整页。

   只在页面看得见、没被盖住时更新进度（用接口的暂停和恢复）。用 `media.state` 的变化加一个页面开着时才有的轻量计时（照 kiikoread 的 `on_tick` 和变化计数），取代旧音乐每 5 秒一次的能力调用。
7. **缺东西要说清楚**（D-138）：
   - **字体**：画字要 TTF。复用树里已有的字体引擎（Mogo 的 `ttf_font`）和全家那一份 `/family/fonts/InkSC.ttf`，不往应用分区再塞一份字库。读不到字体时，用 LVGL 和共用空状态出一页说明：缺什么、会怎么补上。这时 AI 和能力照样能放歌，绝不白屏。
   - **卡上必需文件清单**：auto-provision-1 合入了，就把这份字体登记进去；还没合入就写进报告。
   - **空曲库**：用共用空状态（`ink_ui_empty`）告诉人歌放哪（“用传书传上来，或者放进 文件 › 音乐”），不显示路径字符串。
8. **替换旧音乐**（照 D-139）：
   - 应用 id 还是 `music`，名字“音乐”，图标 `headphones`，孩子可用。这样主屏的 6 个常用、全部应用、搜索和 AI 都照旧能找到它，应用数还是 23。
   - `system/boot/main.c` 里 `ink_music_init` 那一行指向新音乐。
   - 删掉 `system/apps/music/music.c` 和 `CMakeLists.txt:262` 那一行。
   - 旧测试改成走新音乐（见验收 9），`CMakeLists.txt` 里 `sim-music`、`music-product` 等（`:2115-2128`）改指新源文件。
   - 删除放在最后一个单独提交。
9. **测试一起带上**：kiikoread 没有音乐的主机测试，就新写主机测试覆盖搬来的规则：四种模式的曲末选择、收藏循环在收藏清空时退回列表、随机不重复当前这首、手动上下首按全曲库、重扫后找回正在放的曲目、200 首上限、单曲直放放完就停。

## 共同要求

- 来源：功能、页面、状态、出错处理照上面的来源一个不落，不自己发挥。`third_party/kiikoread_music/SOURCE.md` 写清：每个文件的来源路径、提交、SHA-256；改了哪几行、为什么；没搬什么、为什么。原样拷的文件和函数单独记录。
- 设计语言：嫁接画面里的尺寸、线宽、灰度都从 `system/ui/ink/ui_theme.h` 的令牌来，不写死像素。外壳的浮层（来电卡、确认卡、AI 面板、键盘）和缺字体的说明页用 `ink_ui_*` 共用控件。`tools/ui_scale_check.py` 要过。
- 不变的规矩：家人隔离（孩子看不到也放不了大人的 `/me/音乐`）；确认卡只给 D-105 的高风险动作，音乐里没有；按下反馈照 D-31；少刷新照 D-127。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交。新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由）。新文案的英文加进 `assets/i18n/en.tsv`，或者照 Mogo 放进应用自己的 `en.tsv`。共享文件只在末尾追加本任务的行，合并冲突由主会话处理。
- 截图放 `docs/tasks/music-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- **只改**：
  - 新的 `apps/music/`（接线、端口层和替身头文件、`music.cmake`、`en.tsv`）；
  - 新的 `third_party/kiikoread_music/`；
  - `system/services/audio/vendor/kiikoread/`（复用，要改就同步改它的 `SOURCE.md`）；
  - `system/services/audio/media_service.c`：缺小接口时可以增量加，列进报告；
  - 删除 `system/apps/music/music.c`；
  - `system/boot/main.c` 里音乐初始化那一行；
  - 根 `CMakeLists.txt`（删旧音乐那一行，加 `include(apps/music/music.cmake)` 和翻译那一行，改音乐测试目标）；`ports/esp32s31/main/CMakeLists.txt` 只加照 Mogo（`:60-61`、`:77`）写的音乐 include 行，那里本来没有旧音乐的行；
  - `tests/music/`、新增的音乐测试；
  - `tests/integration/ai_surface.c`（末尾追加）、`docs/system/capabilities.md`（重新生成）、`docs/tasks/music-3/`。
- **不碰**：
  - `system/graft/*`：嫁接接口只许改嫁接接口的那个任务（reader-2 或同期改它的任务）改，缺什么写进报告“嫁接接口缺口”一节，例如没有计时、只挂一个客户端、成员文件只许 `/me/`；
  - `apps/mogo/`、`third_party/mogo/`：只读，只许链接它的字体引擎，不链接它的 `port/drawing.c`，不改；
  - `system/ui/shell/activity_card.c`（shell-sweep-1）：“正在进行”卡想显示标签里的曲名，写进报告；
  - `shell_home.c`、`lock_mark.c`、设置的文件（`audio.volume` 只经 `settings.set` 调用）、蓝牙服务的文件；
  - `system/services/file/transfer.c`、`read_pico_transfer`（transfer-2）；`system/apps/files/`（files-3）。
- **能力登记**：在本任务自己的文件里用 `ink_cap_register`。
- **代码量**：我们的胶水（接线、端口层、替身头文件、能力、迁移）不超过 **850 行**（比交付计划的 600 行多，因为要自己做一套不和 Mogo 冲突的画图端口层；超了先停下说明）。删旧音乐的行不算。对 kiikoread 原文的改动（不算整文件、整函数原样拷贝）不超过 **600 行**（`app_music.c` 有 48 处写死的坐标矩形要按 108 px 和 `ink_ui_px` 重排），超了先停下，在报告里说明。测试另计。
- **固件和内存**：固件净增（已减去删掉的旧音乐）不超过 **60 KB**；内部静态 RAM 增量不超过 2 KB；画面缓冲只在页面开着时占 PSRAM，关页面就还。
- **起步**：主会话给的分支（已含 design-lang、home-1、control-1、graft-mogo-4、audio-out-2 的主线）。改嫁接接口的任务要是先合入，本任务排在它后面合并。
- 不改分区，不碰设备。

## 验收

1. **照 kiikoread 走一遍**（模拟器脚本，每步截图并断言）：`/me/音乐` 和 `/family/audio` 里有带标签的歌和一个子文件夹。打开音乐 → 曲名显示标签里的名字 → 播放 → 歌词行出现，换句时跟着变 → 点进度条跳转 → 行内收藏一首 → 切到收藏循环 → 让这首放完，下一首是收藏里的 → 单曲循环重放同一首 → 列表循环按顺序走，走到头回到第一首 → 随机在多于一首时不放当前这首（固定种子，断言 20 次都不重复当前这首）→ 清空收藏后收藏循环退回列表循环 → 手动上一首/下一首始终按全曲库 → 全部歌曲按文件夹进出、翻页。同一条路径再用能力调用（来源是 AI）走一遍，结果和快照都一样。
2. **在后台照样接着放**：离开音乐页、锁屏、打开阅读以后让歌放完，按当前模式接下一首；回到音乐，显示的是正在放的那首。来电卡和 AI 面板盖上再关掉，页面恢复原样。
3. **续播和家人**：大人放 A 到 1 分钟 → 换成孩子，放 B → 换回大人，按播放从 A 的约 1 分钟接着放，模式和收藏都还在。孩子看不到大人的 `/me/音乐`，也放不了 `/members/owner/...`。模拟器用现有的“带同一份闪存镜像重启”的测试方式重启后（沿用 `tests/music/run.py` 或同类测试已用的做法，不新造），上次的曲目、位置、模式都还在。
4. **迁移**：先写入基线旧音乐格式的 `music.favorites` 和 `music.last`，升级后新音乐看到同样的收藏和上次的歌。反向对照：去掉迁移或兼容读取，这条必须失败。
5. **不再重复绑定**：播放页开着刷新 200 次进度，再点一次播放，只发出一次能力调用（测试在载体里数 `ink_cap_call` 和 `ink_graft_present`），事件回调和内存的数量不变。嫁接页面本身没有 LVGL 绑定，所以只要断言页面开着时没有新增的 `ink_shell_bind`/事件回调登记。反向对照：每次刷新都重新登记点按处理，这条必须失败。
6. **刷新**（统计来源：测试载体记录每次 `ink_graft_present` 的脏区和刷新意图，再加模拟器显示统计）：进度走动只刷进度条和时间那一条；歌词换句只刷歌词行；改模式只刷模式控件；在播放页放歌的过程中没有整屏刷新。
7. **缺东西时**：拿掉字体（`/me/fonts/InkSC.ttf` 和 `/family/fonts/InkSC.ttf` 两处都藏起来）→ 出说明页（快照写着缺字体），不是白屏，同时 `music.play` 照样能放。空曲库 → 共用空状态写清歌放哪。m4a 文件 → 列出来，标“这种格式还不能放”，点了有说明。
8. **截图，改前改后并排**：播放页、全部歌曲（平铺和按文件夹两种）、空曲库、缺字体说明页、孩子的播放页，系统默认文字大小和最大一档各一套。测试断言所有点按区不小于 108 px，并且都在屏幕内。
9. **全量**：主工程普通全量 ctest 过（含 `music-formats`、`music-product`、`audio-out-*`、`ui-scale`、`integration-apps` 的应用数 23）；ASan 跑音乐、嫁接接口、媒体相关测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，并且链接没有重复符号（尤其不和 Mogo 的 `ui_*`、`epd_*` 冲突）。媒体执行器的栈估算照旧低于 8192 B，那条已有断言不动。

   报告写：固件净增；产品代码行数；kiikoread 原文改动行数；`gcccheck` 结果；嫁接接口缺口；要用户定的（重连胶囊、开机是否自动接着放、AAC 解码、输出胶囊怎么对到蓝牙能力）；标签读取在两个目录的确认结果；以及**改过的每一个已有测试**。预计会改的：
   - `tests/music/product.c`：触摸从 LVGL 绑定查找改成嫁接画面的命中坐标，所有 AI 断言和数值不变；
   - `tests/music/regressions.py`：`timeout` 和 `skip` 两个反向对照改指新文件，失败条件一样严格；
   - `tests/integration/ai_surface.c`：只追加。

   测试不放宽、不删、不加等待来凑。
