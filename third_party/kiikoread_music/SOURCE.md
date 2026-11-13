# kiikoread 改版音乐播放器：来源和改动

本目录是“搬代码”（D-140）：原文拷进来再按这台设备改，改动逐条记在这里。接线层（嫁接接口、媒体服务、能力、存储、画图端口）在 `apps/music/`，不算这里的改动。

## 来源

- 项目：`oouu00/Metalio-S31-kiikoread-xiugai`
- 固定提交：`746ce316faf61ce5228eceee618eacb50a02a395`（本机只读副本 `ink-hw/upstream/oss/kiikoread-xiugai`）
- 许可：用到的文件头都是 `SPDX-License-Identifier: Apache-2.0`（版权 mindreset），全文 `Apache-2.0.txt`；根目录许可范围说明一并保留为 `kiikoread-xiugai-LICENSE`。没有拷受个人学习许可约束的文件。

| 原文 | 本目录 | 原文 SHA-256 | 现在的 SHA-256 |
| --- | --- | --- | --- |
| `main/apps/app_music.c`（1049 行） | `app_music.c` | `99d6d50b59250b00a12a84f032314a9160ea12ae7054782a1fa5f9d6c69f0889` | `3a88bc193ac5c7b9b33d930ae897bad84a135a8ed858697eeadb1b91466cc0b7` |
| `main/music/music_player.h`（183 行） | `music_player.h` | `84271ec302c9d18624d20ac89047d02c6f339f62e282a2bc3762d7e0ebe19d85` | `4e29a14bf0e3b2c479f1efe2c568d74220c976a347da6361ee3d05e6c4b70063` |
| `main/music/music_player.c`（1628 行，只拿整函数） | `music_library.c` | `a98e87294803647031c656d290e7a70241e9783e01c29c976ebcabf318d70e85` | `6ab1cbf554dc70972ad2fb17040cab7daded838a8c469fcc80eedf0342ab76d8` |
| `LICENSE` | `kiikoread-xiugai-LICENSE` | `b1a477173eada381b15f2cd522bba1a9ab546759f6534e8c9943f0cb933aeb30` | 同左 |
| `licenses/Apache-2.0.txt` | `Apache-2.0.txt` | `c2ede378097fb1fcdcd128c47880c918663f475eba53e1e2dc59d95b537c8809` | 同左 |

拷进来的第一版是原样的（提交 `79b1b417`），之后的改动都能用 `git diff 79b1b417 -- third_party/kiikoread_music` 看到。`music_library.c` 的第一版是 `music_player.c` 里这些行原样排在一起：1-18（文件头）、73-77、84-87、101、112-120（状态）、295-329（扫描）、454-518（曲末规则）、1242-1247、1277-1361（重扫）、1437-1453（上一首/下一首）、1461-1473、1484-1486、1509-1565（收藏和模式）。

已在树里的 `music_tags.c`、`music_lyrics.c`（同一提交）照旧用，见 `system/services/audio/vendor/kiikoread/SOURCE.md`；这次在那里修了两个原文的错（FLAC 歌词崩溃、FLAC 时长），也记在那份 `SOURCE.md`。

## 原样拷、一字没改的函数

`tests/music/verify_kiikoread.py`（ctest `music-kiikoread-provenance`）核对下面每个函数的全文哈希；加 `--source <固定副本>` 还会和原文件逐个比。

| 原文 | 函数 | 函数全文 SHA-256 |
| --- | --- | --- |
| `main/apps/app_music.c` | `fit` | `e5aa8defc711ccb8cdbdebdcb1ac0c456814af0f9fd927f049c40e014facb22c` |
| `main/apps/app_music.c` | `sy` | `fdb7b516ad5aa7999fb04778eef3b9c65817347a243349793d7ae6d641771cc1` |
| `main/apps/app_music.c` | `mmss` | `f9492617dee5a64730e7c8060f2dbe7f1272e4c7373e7e073d104f2861602a5d` |
| `main/apps/app_music.c` | `hit` | `17dc054dc50b9fd8b3a4e73ad0700b8564ecddb61770ace62e8b5978dab1c19c` |
| `main/apps/app_music.c` | `page_count` | `9872dc045a3c3fe7535d05d9877f9a46060f8a604746c98bb01210efed103835` |
| `main/apps/app_music.c` | `rebuild_order` | `1ee995e49eeca5518e09666e35f288897f14ff50813e444f7c57295b3c6eb69d` |
| `main/apps/app_music.c` | `full_page_count` | `1f96886fe44d94017a64cffff50c937b3b6eaf6e1ae73f227e14fa6b00859df7` |
| `main/apps/app_music.c` | `child_name` | `342389e79fb7334d4c952733c65e8437e07ef3a6ac408ee013e2aef3dec849a6` |
| `main/apps/app_music.c` | `dir_is` | `16473af0920a731ea75342631382712097f7105057410a71b21495b23509c245` |
| `main/apps/app_music.c` | `dir_holds` | `a9d202353016784ebe34a01e42922d92047f98a87bfcec6786af1c6ba30c8fbc` |
| `main/apps/app_music.c` | `dir_track_count` | `0c4928e9bfd6c1ed85861eeffe1d3699e0330d63ccfcf92369b90c94d1ce1b10` |
| `main/apps/app_music.c` | `library_has_dirs` | `4d80edda60681338ba8760476c60bece6066584cbc3b03a9efa4180afe385dfc` |
| `main/apps/app_music.c` | `level_build` | `40f4e87253ae0c150b506ab846d3c0682e630a103ba4db9d9131f288b7e0ae41` |
| `main/apps/app_music.c` | `level_total` | `42da929f6cee6e632753e997a3555633d1ae7a67c120fdd71b1a70d93e0009b9` |
| `main/apps/app_music.c` | `browse_page_count` | `4153d0ef792a59cb28ab0be8db0fe9dd19244444ce76199d7a69b0c33706936c` |
| `main/apps/app_music.c` | `dir_enter` | `2ef5224fa2867fa13d3ca4ed68b795fd4a688aaa17ce4d4343221519feccdf28` |
| `main/apps/app_music.c` | `current_dirty` | `b5b193985a69d1f33f7124aecbc9634e93111730ae8b2c05af63ea5234d91e2e` |
| `main/apps/app_music.c` | `lyric_snapshot` | `b0c21142a2a9eb7b2b63e2da3cd83734084ef4b0b28fea63202db4c1d56d643f` |
| `main/apps/app_music.c` | `sync_lyric` | `9463d41a4a4dae5229549897ee8a116c291d05428aa24533a381e87d71d8f5d3` |
| `main/apps/app_music.c` | `dirty` | `90b6c8e8aac3ac8d69149a7788b3b73ecd135961f452e3a947959c22964275c8` |
| `main/apps/app_music.c` | `play_glyph` | `8682536505a0bd2996fad92461e9054b08d7957aadcd2dc9cb0548d9b62d9735` |
| `main/music/music_player.c` | `ext_wanted` | `7d8a9d9c96d901a59449c74d8ea65678142d904449030e4cd1a1245ead21b85d` |
| `main/music/music_player.c` | `next_fav_index` | `760b31cd063d3cecff2ec85def4e12aaa64f896b786c5cc663d7953953293485` |
| `main/music/music_player.c` | `finish_track` | `c7f501ba5ce9256c468877eccca4d60e21574688b8c8543623a4f9170b6eb120` |
| `main/music/music_player.c` | `music_find_path` | `2c3fd7e1315aafd406b87d6ceb4b38a42adcca6fb37e38c9c5e4d302b579de06` |
| `main/music/music_player.c` | `music_track_count` | `e61e8c2b10aabe34f384491f9696f17bc93c7306f08e5c543e10200ed3ae47c5` |
| `main/music/music_player.c` | `music_track` | `f0caa5a534f504bebf099b0cd7f7bf2022d501a9898bf6a264925d89f0bd2b6e` |
| `main/music/music_player.c` | `music_next` | `13096ea79b0b07309e27c1eb8ff35db093136a9240021c890576471641d2ec3f` |
| `main/music/music_player.c` | `music_prev` | `6d1de7c012c5cc39948dda1a808d1daf73093b4d044223bac641fbe74c270096` |
| `main/music/music_player.c` | `music_title` | `137c7b3e4c9856d1c431ad5324d89f9cbeac90b2c6d62716b1254a6f2302dde6` |
| `main/music/music_player.c` | `music_artist` | `3f5321f0da5465aff07a024b52e1c7efeee9a5b1eb030ca8f36f5931bd86aafe` |
| `main/music/music_player.c` | `music_favorite` | `71b6c496725655ee3556d7afdabd9430ac40e27f112317a7a6a846b5e6cc45fe` |
| `main/music/music_player.c` | `music_favorite_count` | `5b83536e2b322b00c5e4871eb230cca5c1fcc814fa5bf5230f7ece1732fa28f3` |
| `main/music/music_player.c` | `music_cycle_mode` | `e332a8afd0eb6375ec5a1030de130bb33030b3f208423b7e10be932185217da1` |
| `main/music/music_player.c` | `music_mode_label` | `02cd8bc8ada27385755b203a2c7c741103d35ad4b6d265ee70b8c7e3fefaf46c` |

## 改了什么、为什么

改动行数（相对第一版，`git diff --numstat`）：`app_music.c` 增 435 删 493，`music_library.c` 增 83 删 61，`music_player.h` 增 14 删 74。

### `app_music.c`（页面）

- 布局：原文 48 处写死在 684×1216 上的坐标（`MUSIC_DIRTY`、`MODE_RECT`、行 `{44,748+i*64,596,56}`、圆键 `hit_circle(342,540,52)` 等）换成 `layout()` 一处按主题令牌算：边距 `INK_MARGIN`，点按区 `INK_CONTROL`（108 px，`touch()` 取 `sy(INK_CONTROL)` 和 108 的大者），间距 `INK_GAP_S`，行线 `INK_DIVIDER`，圆角 `INK_RADIUS_KEY`，字号 `INK_FONT_TITLE/BODY/NOTE`，进度条 `INK_SLIDER_FILL`。`MUSIC_ROWS`、`FULL_ROWS` 从常数 4、12 变成“放得下几行就几行”，只有一页时不画翻页行。为什么：108 px 点按区、随系统字号一起放大（`sy()` 原样留着，`app_settings_system_font_size` 由端口给出 90/100/110/120%，孩子 110%）。
- 去掉 `draw_frame` 三层框、`circle_frame`、`epd_fill_circle` 圆键和按下反色（`s_pressed`、`ui_draw_selected_round_rect`）：设计语言不画重框和圆圈，D-31 按下不反色。按钮改成一道细边（`button()`），行间细线（`row_line()`），播放键是唯一的主按钮（黑底，`play_glyph` 原样画白色三角/双竖杠）；上一首、下一首写字代替原文的 chevron 图标。
- 去掉 `ui_nav_status`、`ui_nav_back`、`ui_nav_draw`：状态栏和返回归外壳。全屏列表左上角加一个写字的“上一级 / 播放页”按钮（原文靠返回箭头），返回键同样有效。
- “全部歌曲”按钮从列表卡标题行移到胶囊那一行；去掉“播放列表 · N 首”标题和列表卡外框（108 px 高放不进原来的标题行；数量在快照里）。
- 去掉“重连”胶囊和它的读秒（`RESTART_RECT`、`restarting()`、`s_restart_tick`）：我们的蓝牙服务没有对应能力，不自己造（报告“要用户定”）。
- 输出胶囊：文字“蓝牙”改为“耳机”，状态取 `audio.route`；点按调现有的 `shell.bluetooth`，没有单步切换通路的能力。
- 曲名从两行改为一行（截短，快照里有全名）：屏幕要多留一行列表。当前曲目所在行原文画黑框，改成在时长位置写“正在放”；不能放的格式（m4a、aac）写“不能放”。
- 点按：`on_gesture` 保留原文的命中顺序，但每个分支不再直接改状态，而是用 `music_request()` 调能力表里的同一个能力（触摸来源）：模式 `music.mode`，上一首/播放/暂停/下一首，进度条 `music.seek`，音量 `settings.set`，行 `music.play`/`music.favorite`（带 `path`），翻页、全部歌曲、进文件夹、上一级 `music.list`。原文里的状态改动（进出全部歌曲、翻页、进文件夹）搬进 `music_page_list()`，让能力也走同一段代码。
- `on_key`：原文播放页的侧键切底栏，我们没有底栏，改成盖板的上一页/下一页键翻当前列表；返回键在全屏列表里退一层。
- `on_tick`：照原文的变化计数和歌词快照；进度条另加门槛，长出至少 `INK_GAP_S`（8 px）才刷一窄条，长歌不会每秒刷一次；有提示时歌词行不刷。播放/暂停只刷播放键（`music_page_changed("state")`），跳转只刷进度那一条。
- `present`/`area_hint`：原文直接调屏幕驱动，改成只记录“已推上屏的进度宽度”；推屏由接线层经 `ink_graft_present` 做。
- `on_enter`：原文只在曲库为空时扫描，改成每次进页重扫（传书或文件里新放的歌）。`on_media_lost`：没有拔卡回调，改为播放读不了时在歌词行写“读不了这首歌”。
- 大的静态数组（`s_order`、`s_dir`、`s_dir_first`、`s_lyric_seen`）放 PSRAM（`INK_PSRAM_BSS`）。`track_dir` 用 `music_root_of()` 认两个根（`/me/音乐` 和 `/family/audio`），一行。
- 文末加“接线层入口”一节（`music_page_*`）：进页、清状态、点按、按键、计时、列表、改动后要刷哪块、画、快照用的行、测试用的点按区清单，代替原文的 `app_desc_t`。
- 界面文字加 `TR()`，英文在 `apps/music/en.tsv`。

### `music_library.c`（曲库和规则）

- 扫描 `scan_dir`：原文 `opendir/readdir/stat` 递归换成文件索引 `ink_fs_index`（只在 BG 执行器），深度 2 用路径里的斜杠数保留；后缀表 `ext_wanted` 原样（含 m4a、aac）；超过 200 首记下 `s_truncated`，页面和快照写“只列出了前 200 首”。
- `music_scan`：两个根各扫一遍再按路径排序（文件索引的顺序随卡变，原文是目录顺序）；收藏一次读全改成读 `music.favorites` 集合（`music_port_favorites_load`，格式和旧音乐一样：按路径 crc32+1 作记录号，内容是路径，所以不用迁移）；按路径找回正在放的曲目、恢复上次曲目两段原样；去掉开机自动续播一次（要用户定）和 ESP 日志。原文 `fav_key` 没拿：它是 NVS 键，我们沿用旧音乐的集合格式。
- `music_favorite_toggle`、`music_set_mode`：NVS 读写换成 `music_port_favorite_store`、`music_port_mode_store`（`music.favorites`、`music.mode` 集合）。
- `finish_track` 原样；它要的 `close_track`、`time_freeze`、`bt_trace`、`set_state`、`esp_random` 由端口头文件给出（前三个是空操作；应用框架没有随机数接口，`esp_random` 换成用时钟搅动的 xorshift，见报告“缺口”）。我们在一首歌开始时就算好下一首（`music_library_pick()` 调原文的 `finish_track`），`media.finished` 到来时（BG 执行器上的订阅）只读这条路径并调 `media.play`。
- 文末加“接线层入口”一节：清空、恢复上次曲目和模式、同步正在放的曲目、算下一首、是否超过上限。

### `music_player.h`（页面用的接口）

- 去掉没实现的声明（启动/停止播放任务、改根目录、单曲直放入口、播放暂停切换、音量设置、输出切换、重连、跳转、蓝牙开关、拔卡、射频忙、挂起、PCM 环）；`MUSIC_ROOT` 改为 `/me/音乐`，加 `MUSIC_FAMILY_ROOT`；`music_volume()` 改为 0～100 的 `audio.volume`；文末加接线层的几个声明。

## 没搬的

- `music_player.c` 的解码任务、PCM 环、重采样、A2DP 拉流、蓝牙模组上电和重连、NVS 读写、锁屏时暂停的策略、开机自动续播、ESP 日志（vendor-common 第 2 条）：播放一律交给 `system/services/audio/media_service.c`，它规定锁屏后照样放。
- 跳转的粗跳和回退（`seek_*`）、时间基准（`time_*`）：媒体服务自己做。
- `app_music.c` 的重连胶囊、底栏、返回箭头、按下反色，理由见上。

## 点按区和能力对照

| 点按区 | 能力 |
| --- | --- |
| 模式胶囊 | `music.mode`（`cycle`） |
| 上一首、播放/暂停、下一首 | `music.prev`、`music.play`/`music.pause`、`music.next` |
| 进度条 | `music.seek` |
| 音量减、加 | `settings.set`（`audio.volume`，±6；孩子的上限由设置服务管） |
| 列表行 | `music.play`（带 `path`） |
| 行内收藏（书签列） | `music.favorite`（带 `path`） |
| 翻页、全部歌曲、进文件夹、上一级、播放页 | `music.list`（`page`、`view`、`path`） |
| 输出胶囊 | `shell.bluetooth`（现有能力，打开蓝牙页；没有单步切换通路的能力） |
| 重连胶囊 | 不做：蓝牙服务没有对应能力，见报告“要用户定” |
