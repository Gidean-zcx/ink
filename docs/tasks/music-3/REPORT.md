# music-3 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

# music-3 报告

## 结果一句话
新音乐挂在嫁接接口上：页面和曲库规则是 kiikoread 改版的原文（拷进 third_party/kiikoread_music，35 个函数一字没改，其余逐条记在 SOURCE.md），按 108 px、主题令牌、随字号缩放改过；播放全交给媒体服务；人和 AI 走同一套 music.* 能力；旧音乐的名字、能力、收藏和上次的歌都接过来了；旧音乐在最后一个代码提交里删掉。

## 验收表

| # | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 照 kiikoread 走一遍，触摸一遍、AI 一遍，结果和快照一样 | 通过【实测】 | music-player main：标签曲名、歌词随位置换句、点进度条跳转、行内收藏、收藏循环接收藏、单曲循环重放、列表循环到头回第一首、随机 20 次不重复当前、清空收藏退回列表、手动上下首按全曲库、全部歌曲进出文件夹和翻页；13 个检查点触摸和 AI 逐字段相同 | ctest -R music-player |
| 2 后台照样接着放；盖住再关掉页面原样 | 通过【实测】 | 离开页面+锁屏后接下一首；打开新阅读（嫁接客户端切走）后接下一首；回音乐显示正在放的那首；来电卡和 AI 面板盖上再关，画面 CRC 和快照不变 | 同上 |
| 3 续播和家人；重启后还在 | 通过【实测】 | 大人放到 60 s→孩子放自己的→换回大人按播放从约 60 s 接着放，模式和收藏还在；孩子看不到大人的歌、放不了 /members/owner/…；带同一份闪存重启（restart 阶段）曲目、45 s 位置、模式都在 | music-player（main restart 两段） |
| 4 迁移（旧格式收藏和上次的歌） | 通过【实测】，格式没变所以不需要迁移 | 先按旧音乐格式写 music.favorites（crc32+1、内容是路径）和 music.last，新音乐显示同样的上次的歌、同一首行标“已收藏”；反向对照 compat（改读键）失败 | regressions.py --case compat |
| 5 不再重复绑定 | 通过【实测】 | 200 次进度刷新后界面事件回调数不变，再点一次播放只发 1 次能力调用、媒体 resumed +1；反向对照 rebind（每次推屏登记一个回调）失败 | regressions.py --case rebind |
| 6 刷新 | 通过【实测】 | 放歌时只有 PART 推屏，脏区只在进度条带和其上的歌词行，没有整页/整块；改模式只刷模式胶囊；收藏只刷那一行 | music-player |
| 7 缺东西 | 通过【实测】 | 两处字体都没有：共用空状态说明页+快照写缺字体，music.play 照样放；空曲库：共用空状态写“还没有歌 / 用传书传上来，或者放进 文件 › 音乐”，快照无路径；m4a 列出、标“这种格式还不能放”，点了歌词行说明，AI 收到同样的话 | music-player-missing、music-player-empty、music-player |
| 8 截图和点按区 | 通过【实测】 | docs/tasks/music-3/：compare-player/all-default/largest（左旧右新，旧图取自 docs/tasks/music-1）、all-flat-default/largest、all-folder-open-default、empty-default/largest、no-font-default/largest、child-player，单张 8～45 KB；所有点按区 ≥108 px 且在屏内（每次取点按表都检查） | music-player、music-product |
| 9 全量 | 见“全量测试”：音乐相关全过；两项相机相关失败与本任务无关 | | |

## 全量测试、内存检查、固件
- 全量 ctest（清 test-out 后分三段）：160/160；149/150（capability-table-doc，已重新生成目录提交，复跑 6/6）；144/146（ai-history-photo、autotest-cap，相机相关，去掉音乐初始化照样失败）。
- ASan：音乐、嫁接、媒体相关 14 项，13 项过；mogo-flow 并行时和 mogo-page 抢着生成同一个字体文件失败，单独跑通过。
- S31：build.sh 通过，有“factory screen output layer @690a023”，无“没有屏幕输出层”；镜像 5,160,208 B，分区余量 82,672 B；净增【估算】flash +23,150 B、内部静态 +111 B、PSRAM 静态 +12,117 B。
- gcccheck：11 个改动的 C 文件，0 个问题（修过一次 format-truncation 和 %u）。
- ui_scale_check：默认范围 0 违规；apps/music 和 app_music.c 0 违规。
- sdk-boundary：通过（中途发现音乐用了 HAL 随机数、OSAL、服务锁和 audio.h，已改掉）。
- 每个主要操作刷几次屏（AI 调用后计数）：播放新歌 2、下一首 2、暂停 1、继续 1、跳转 1、改模式 1、收藏 1、取消收藏 1、翻页 1、进全部歌曲 1、进文件夹 1、上一级 1、回播放页 1。放歌时进度每长 8 px 刷一窄条，换句刷歌词行。

## 反向对照（都按要求失败）
timeout（去掉 30 s 期限，打开后第一次调用超时）、skip（进度条用旧时长换算）、rebind、compat（tests/music/regressions.py）；original 对照通过。厂商两处修复各自撤掉：标题空指针→测试崩溃（-11），FLAC 时长→断言失败。随机规则撤掉“不选当前”那行→music-library 3 项失败。

## 没做到的
- 真机文件层确认 /me/音乐 和 /family/audio 的标签读取：不碰设备，没做；模拟器两处都确认读到（/family/audio 的“合成音调”、/me/音乐/带标签.mp3 的“合成音调 · 测试歌手”）。
- 卡上必需文件清单登记字体：auto-provision-1 没合入，没登记；缺字体时说明页写“装机时会把 fonts/InkSC.ttf 放到卡上”。
- 重连胶囊、AAC 解码、开机自动续播：没做，见“要用户定”。
- OGG Vorbis 的标签在列表里不显示（扫描用的是 kiikoread 原文 music_tags_read，不读 Vorbis 注释），显示文件名。
- 盖板键翻页、播放读不了时的提示只有代码，没有专门测试。
- 改前截图用的是 music-1 存档，不是这次现场重建的旧音乐。
- 删旧音乐的提交之后还有两个纯文档提交（重新生成 capabilities.md、对照表填验收），见“和设计不一致的”。

## 和设计不一致的
- media.finished 订阅放在 BG 执行器，不在媒体执行器：应用框架不许用服务锁（sdk-boundary），跨执行器共享“下一首”就得另造锁；放在 BG 上和用户的播放命令同一条队列，顺带消掉了“曲末自动接歌盖掉用户刚点的歌”的竞态。回调只读开歌时算好的路径并调 media.play，不扫描不读文件。代价是 BG 忙时接歌会晚一点。
- music.play 带路径时，只有“上次的歌”从记住的位置接着放，别的歌从头放：这是 kiikoread open_track 的规则；媒体服务按文件记位置，曾让刚点的歌从快结束处开始。旧测试的“大人从自己的位置接着放”照样过。
- 随机数：应用框架没有随机接口，用时钟搅动的 xorshift（模拟器里固定可重复）。
- 曲名一行、上一首/下一首写字、重连胶囊不画、输出胶囊点了打开蓝牙页：见对照表。
- 删除放最后：为了让删除是最后一个代码提交，中间“放回旧文件”两次（a6e71de0、36ac749c），最终删除是 7736b270；之后只有两个文档提交。主会话要留住旧音乐，revert 7736b270 即可（capabilities.md 届时要重新生成）。

## 改过的已有测试
- tests/music/product.c：触摸从 LVGL 绑定查找改成嫁接画面的点按表（music_page_targets），点按区 ≥108 px、在屏内、互不重叠；“skip” 两条原来点“快进10秒”期望 12000/13000，新页面没有快进键，改为点进度条上对应 12 s/13 s 的点，期望值按同一条换算算出来，仍是精确相等；收藏改点当前行的书签列，并新加“收藏后为 true、再点为 false”两条断言。所有 AI 断言和数值不变。
- tests/music/run.py：放全家字体（tools/mogo_font.py 生成，缓存在 build/music-font）、新播放器测试的内容变体（full/flat/empty、--no-font）、多阶段同一闪存镜像、读输出容忍截断的 UTF-8；原来的 product 跑法不变。
- tests/music/regressions.py：timeout、skip 改指新文件（timeout=去掉新音乐的 30 s 期限，skip=进度条用旧时长），失败条件同样严格；original 改为当前测试对当前代码；新增 rebind、compat。
- tests/music/formats.c：新增两条（FLAC 时长、无标题缓冲读内嵌歌词），只加不减。
- tests/integration/ai_surface.c：末尾加 music.mode（必填 mode）、music.favorites；music.favorite 的参数由 NULL 改为 ""（它现在多了可选的 path）。

## 新增的机制
- BG 上画、UI 上推屏的流水：一帧在路上时新改动合并到下一帧（presenting 标志、各自的脏区），是这次查出“后一帧缩小了前一帧脏区”后加的。谁要求：任务书“BG 画，UI 交脏区”。
- 页面开着且没被盖住时的 1 s lv_timer（进度、歌词），盖住就暂停。任务书要求“页面开着时才有的轻量计时”。
- 新集合 music.mode（按家人，1 条）。任务书要求。
- media.state 监听（BG）、media.finished 订阅（BG）。任务书要求。
- 探针 music.present.page/whole/part、music.present_px/y1/y2、music.touch_call、music.refreshes.*（测试计数用）。任务书验收 5、6 要求“载体里数”。
- 构建：ink_music_native 静态库（页面、曲库、画图端口，-include 替身头）。任务书要求避开 Mogo 的同名符号。

## 嫁接接口缺口
- 没有计时：音乐自己在活动里建 lv_timer。
- 同一时间只挂一个客户端：打开新阅读会关掉音乐页（播放照常，音乐在后台靠订阅接歌）。
- 成员文件只许 /me/ 加 /family/fonts/：标签、歌词、/family/audio 的扫描都直接走文件服务，不经嫁接接口。
- 没有“这帧已推上屏”的回调，进度“已上屏宽度”只能在画的时候记。
- S31 的 apps 组件不定义 INK_PSRAM_BSS、按 apps/*.c 通配：音乐在 music.cmake 里自己补；Mogo 的大静态表现在也在内部 RAM（顺手发现）。
- 应用框架没有随机数接口。

## 要用户定的
1. 重连胶囊：kiikoread 有“蓝牙模组断电重启”兜底，我们的蓝牙服务没有对应能力，现在不画。要不要在蓝牙服务加一个？
2. 开机是否自动接着放（kiikoread 开机续播一次）：现在不放。
3. AAC（m4a/aac）解码：现在列出、标“还不能放”。解码器来源和 S31 体积要另查（应用分区只剩约 81 KB）。
4. 输出胶囊：没有“切到扬声器/耳机”的单步能力，现在点了打开蓝牙页。要不要加单步能力？
5. 旧音乐删除（7736b270）合不合，D-135 写的是并存。
6. “只有上次的歌接着放，别的歌从头放”这条 kiikoread 规则是否保留（媒体服务按文件记位置）。

## 顺手发现
- 媒体服务的位置记忆：听到结尾后闪存里记的是 0，再放时却从快结尾处开始，像是卡上缓存（/cache/sys/media-pos）的旧位置胜出【实测现象，原因推断】。音乐这边用 kiikoread 规则绕开了，媒体服务没改。
- mogo-page 和 mogo-flow 并行跑会抢着生成同一个字体文件（ASan 构建里复现）。
- ai-history-photo、autotest-cap 两项相机相关测试在这个基线上失败，与音乐无关。
- kiikoread 原文两个 bug（FLAC 歌词崩溃、FLAC 时长错）已在厂商副本里修掉并记录；原项目也有这两个问题。
- system/services/file/transfer.c 还会建空的 /me/music（归传书，没动）。
- “正在进行”卡想显示标签里的曲名要改 activity_card.c（归 shell-sweep-1，没动）。
- 隐私检查：git diff 16e7be5f..HEAD 里没有 /Users/、/private/tmp、MAC 样式串。

## 要改的文档
docs/tasks/backlog.md 记：应用分区余量约 81 KB；Mogo 大静态表在 S31 上进了内部 RAM；媒体服务位置记忆的现象。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

## 结论

六条意见都核对过。五条成立，已经修好，每条都配了测试，测试在修复撤掉时都会失败。第 5 条（胶水超预算）一半成立：报告确实没写行数，这一点补上了；但胶水没有超，原文改动也没有超，证据在下面。

- 最后提交：`0cc4a819`。分支 worktree-wf_fa602b56-3c7-6，工作区 .claude/worktrees/wf_fa602b56-3c7-6，没有推送。
- 全量 ctest（先清掉 test-out 再重新配置，分三段跑，-j2）原样输出：
  - `99% tests passed, 2 tests failed out of 160`（失败的是 unit-s31-cell 和 unit-s31-cell-started，原因是 Subprocess aborted）
  - `100% tests passed out of 150`
  - `98% tests passed, 3 tests failed out of 146`（失败的是 ai-history-photo、unit-s31-cell-negative、autotest-cap；memo-save-busy 本来就是 Disabled）
  - 三个 S31 蜂窝测试单独重跑都通过。当时这台 12 核机器的负载在 11～12，它们和音乐没关系，不链接音乐代码。ai-history-photo 和 autotest-cap 是相机相关的，上一轮已经核实基线上就失败，这次失败的检查点也一样。
- gcccheck：5 个改动的 C 文件，0 个问题。
- ui_scale_check：默认范围 64 个文件 0 违规；apps/music 加 app_music.c 共 20 个文件 0 违规。

## 每条意见的处理

1. **进度条断成一段段（成立，已修）**
   - 原因：`music_page_presented()` 不管这一帧推了哪块，都把进度记成“已上屏”。嫁接接口又只拷脏区的像素，所以只推播放键或歌词的帧会留下灰缝。kiikoread 原文也有这个问题。
   - 改法：`music_page_presented(EpdRect a)` 只在本帧脏区盖住“屏上进度”到“画出的进度”之间那一段时，才记成已上屏。接线层把合并后的脏区传进去。
   - 测试：music-player 让歌一边放一边暂停、继续 5 次，再读屏上那一行像素，必须从左到右先黑后灰，中间没有灰缝。把修复撤掉，这条失败，报“608 dark”。

2. **每次进页都重扫、逐个读标签（成立，已修；说的数量级不全对）**
   - 实测（模拟器，约 15 首）：
     - 这位家人开机后第一次进页：建文件索引并扫曲库约 2.1 s，打开字体约 1.4 s。
     - 改前再次进页：重扫并读全部标签约 0.24 s，打开字体约 1.8 s。
     - 所以开页超过 3 s 主要是第一次建索引加打开字体，不全是读标签。
   - 改法：
     - `on_enter` 改回原文那一行，只在曲库为空时扫。
     - 接线层订阅 `file.changed`。`/me/音乐`、`/family/audio` 或它们的上级有变化时记一笔，下次进页或 AI 列曲库时只重扫一次。
     - 重扫时按路径加文件大小复用上次读到的标签，只打开新文件和大小变了的文件。
   - 结果：曲库没变时进页 0 次扫描；建完索引后的第一次再进页约 0.13 s，只走索引、不读标签。
   - 测试：
     - music-player restart：曲库没变时进页不扫；录一段音进 `/me/音乐` 后正好扫一次，新歌出现在列表里。
     - music-library：曲库没变时不读标签；只读新的和改过的那两个文件。
     - 撤掉修复，“not scanned again”失败。
   - 没改的：
     - 打开字体每次进页还是 1.4～2.3 s。这是 Mogo 的字体引擎，本任务只许链接、不许改，所以没动。
     - 第一次进页还是超过 3 s，所以 30 s 期限仍然需要。反向对照 timeout 仍按设计失败。

3. **收藏超过 100 首悄悄丢（成立，已修）**
   - 集合上限改成和曲库一样的 200 首（`MUSIC_MAX_TRACKS`），只放宽、格式不变，旧数据不受影响。
   - `music_port_favorite_store` 改为回报集合收没收下。`music_favorite_toggle` 先写集合，写成了才改内存。
   - 满了的时候：
     - 能力回 FAILED，说“收藏满了，先取消几首再收藏”，歌词行也显示这句；
     - 内存和集合不会对不上，再点一次也不会被当成取消收藏。
   - 顺手：`music.favorites` 快写满结果缓冲时停下并带 `"more":true`；结果缓冲从 3072 B 改成 6144 B（放在 PSRAM）。原先 100 首收藏或长路径会溢出，回成功却没有内容。
   - 测试：music-player restart 填满集合，检查 AI 被告知、行不变、页面说明原因、点按也不变、再问一次仍拒绝、腾出位置后能收藏且提示消失。music-library 加了“集合拒收时什么都不变”。撤掉修复，四条失败。

4. **一个操作刷了不止一次（成立，已修）**
   - 原因：放新歌和下一首时，媒体服务中间先停一下（IDLE），页面为此多刷了一次播放键。
   - 改法：音乐自己要求换歌时，计时和状态监听先不画，等 media.play 的回执到了只画一次。播放、暂停、继续的回执一律走“同一首只刷播放键、换歌刷整页”这条路，回执和状态变化谁先到都一样。
   - 测试改严：`n >= 1 && n <= 2` 改成 `n == 1`。撤掉修复，“play costs one refresh (2)”和“next … (2)”失败。

5. **胶水超预算（一半成立）**
   - 成立的部分：报告没写两项行数，现在写在下面。
   - 不成立的部分：app_music.c 末尾的接线入口在 kiikoread 文件里，已经算进“原文改动”那一项。再算进胶水就是同一行算两次。
   - 现在的数：
     - 胶水：apps/music 下全部 .c/.h 共 846 行，预算 850。
     - 原文改动：新写或改过的行 553，预算 600。
     - 删掉的 628 行另列。要是删掉的也算进 600，就超了，列进“要用户定”。

6. **对照表几行的证据不对（成立，已补测试和更正）**
   - 第 12 行：减、加各点一次，`audio.volume` 变 ±6，每次只刷音量那一行。
   - 第 5 行：点输出胶囊后，顶层是蓝牙页 SetBluetooth。
   - 第 19 行：盖板的下一页、上一页键在播放页翻页，在全部歌曲里也翻页；每个键一次能力调用、一次刷新。
   - 第 25 行：录一段 Opus 进 `/me/音乐`，放的时候拔卡，快照写“读不了这首歌”，屏上歌词行也有这句。
     - 查出一个代码问题：原先只改了快照、屏上不刷。advanced_bg 现在会刷歌词行。
     - 媒体服务只对 Opus 报 read_failed，所以测试用 Opus，原因见“顺手发现”。
   - 第 3 行更正：这台设备没有实体返回键，状态栏的返回直接离开音乐页，“返回键同样有效”是错的。退一层只靠页面上的“上一级”按钮，走查里已测。
   - 新增第 35 行（扫描规则）和第 36 行（收藏写不进）。

---

# music-3 报告（含复核修复）

## 结果一句话
新音乐挂在嫁接接口上。页面和曲库规则是 kiikoread 改版的原文：拷进 third_party/kiikoread_music，35 个函数一字没改，其余改动逐条记在 SOURCE.md；按 108 px、主题令牌、随字号缩放改过。播放全交给媒体服务。人和 AI 走同一套 music.* 能力。旧音乐的名字、能力、收藏和上次的歌都接过来了，旧音乐在一个单独的提交里删掉。

## 验收表

| # | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 照 kiikoread 走一遍，触摸一遍、AI 一遍 | 通过【实测】 | 标签曲名、歌词随位置换句、点进度条跳转、行内收藏、收藏循环接收藏、单曲循环重放、列表循环到头回第一首、随机 20 次不重复当前、清空收藏退回列表、手动上下首按全曲库、全部歌曲进出文件夹和翻页；13 个检查点触摸和 AI 逐字段相同 | ctest -R music-player |
| 2 后台照样接着放；盖住再关掉页面原样 | 通过【实测】 | 离开页面加锁屏、打开新阅读后都接下一首；回音乐显示正在放的那首；来电卡和 AI 面板盖上再关，画面 CRC 和快照不变 | 同上 |
| 3 续播和家人；重启后还在 | 通过【实测】 | 大人放到 60 s，换孩子放自己的，换回大人按播放从约 60 s 接着放，模式和收藏还在；孩子看不到大人的歌，也放不了 /members/owner/…；带同一份闪存重启后曲目、45 s 位置、模式都在 | music-player（main、restart 两段） |
| 4 迁移 | 通过【实测】，格式没变，不用迁移 | 按旧音乐格式写的收藏和上次的歌都认得；反向对照 compat 失败 | regressions.py --case compat |
| 5 不再重复绑定 | 通过【实测】 | 200 次进度刷新后事件回调数不变，再点一次播放只发 1 次能力调用；反向对照 rebind 失败 | regressions.py --case rebind |
| 6 刷新 | 通过【实测】，复核后更严 | 放歌时只有局部推屏，只在进度条带和歌词行；改模式只刷模式胶囊；收藏只刷那一行；音量只刷音量行；屏上进度条没有灰缝；每个主要操作正好 1 次刷新（测试断言 ==1） | music-player |
| 7 缺东西 | 通过【实测】 | 缺字体出说明页且能放歌；空曲库用共用空状态说明歌放哪；m4a 列出并说明不能放；收藏满了在歌词行和回执里说明；拔卡时歌词行写“读不了这首歌” | music-player-missing / -empty / music-player |
| 8 截图和点按区 | 通过【实测】 | docs/tasks/music-3/ 下的截图，单张 8～45 KB；所有点按区不小于 108 px 且在屏内 | music-player、music-product |
| 9 全量 | 见下 | | |

## 全量测试、内存检查、固件
- 全量 ctest（复核修复后，清 test-out 并重新配置，分三段）：
  - `99% tests passed, 2 tests failed out of 160`：unit-s31-cell、unit-s31-cell-started 负载高时异常退出，单独重跑通过。
  - `100% tests passed out of 150`
  - `98% tests passed, 3 tests failed out of 146`：ai-history-photo、autotest-cap 是相机相关的基线失败；unit-s31-cell-negative 单独重跑通过。
  - 音乐相关 29 项（music-*、ui-scale、integration-apps 的应用数 23、integration-ai-surface、capability-table、sdk-boundary、i18n-*）全过。
- 反向对照：original 通过；timeout、skip、rebind、compat 都按要求失败。复核新加的四个（进度条、换歌刷新、收藏满、重扫）撤掉修复都失败。
- ASan：上一轮做过，音乐、嫁接、媒体相关 14 项里 13 项过；mogo-flow 并行时和 mogo-page 抢同一个字体文件，单独跑通过。这次只改了应用层逻辑，没有重跑 ASan。
- S31：build.sh 通过，日志里有“factory screen output layer @690a023”，“没有屏幕输出层”0 行，音乐文件没有新警告，链接没有重复符号。
  - 镜像 5,160,928 B，比复核前多 720 B；应用分区余量 81,952 B（0x14020，约 80 KB，很紧）。
  - 内部静态 RAM：音乐三个目标文件不变；PSRAM 静态多 3,072 B（结果缓冲）。
  - 相对删旧音乐前的净增：上一轮估算 flash 约 +23 KB，现在再加约 0.7 KB。
- gcccheck：本轮 5 个文件 0 个问题。
- ui_scale_check：0 违规。
- 每个主要操作刷几次屏（AI 调用后计数，测试断言正好 1 次）：

  | 操作 | 刷新次数 |
  | --- | --- |
  | 播放新歌 | 1 |
  | 暂停 | 1 |
  | 继续 | 1 |
  | 跳转 | 1 |
  | 下一首 | 1 |
  | 改模式 | 1 |
  | 收藏 | 1 |
  | 取消收藏 | 1 |
  | 翻页 | 1 |
  | 进全部歌曲 | 1 |
  | 进文件夹 | 1 |
  | 上一级 | 1 |
  | 回播放页 | 1 |
  | 音量加或减 | 1 |
  | 盖板键翻页 | 1 |

  放歌时进度每长 8 px 刷一窄条，换句刷歌词行。

## 代码量（复核补上）
- 产品代码（胶水：接线、端口层、替身头文件、能力、迁移）：apps/music 下全部 .c/.h 共 846 行，预算 850。
  - music.c 623 行
  - port/draw.c 82 行
  - port/kiikoread.h 71 行
  - port/music_page.h 34 行
  - port/epdiy.h 6 行
  - port/esp_err.h 4 行
  - 13 个一行替身头文件，含 freertos/ 两个，共 26 行
- 构建文件另有 music.cmake、translations.cmake 和 en.tsv。
- kiikoread 原文改动（`git diff --numstat 79b1b417`）：

  | 文件 | 新增或改过 | 删掉 |
  | --- | --- | --- |
  | app_music.c | 435 | 490 |
  | music_library.c | 103 | 64 |
  | music_player.h | 15 | 74 |
  | 合计 | 553（预算 600） | 628（另列） |

  - app_music.c 末尾的接线入口算在这里，不算胶水。
  - 删掉的 628 行是去掉的功能和换掉的写死坐标：底栏、返回箭头、重连胶囊、三层框、圆键、按下反色、NVS、开机续播、没实现的声明。
  - 35 个整函数原样，verify_kiikoread.py 对固定副本核对通过。

## 反向对照（都按要求失败）
- tests/music/regressions.py 的 timeout、skip、rebind、compat；original 通过。
- 厂商两处修复各自撤掉：标题空指针让测试崩溃，FLAC 时长让断言失败。
- 随机规则撤掉“不选当前”那行，music-library 3 项失败。
- 复核新增四个：进度条覆盖规则、换歌不画中间停顿、收藏写失败不改内存、只在文件夹变了时重扫。撤掉任何一个，music-player 都失败。

## 没做到的
- 真机文件层上确认两个目录的标签：不碰设备，没做。模拟器上两处都读到了。
- 卡上必需文件清单登记字体：auto-provision-1 没合入，没登记。
- 重连胶囊、AAC 解码、开机自动续播：见“要用户定”。
- OGG Vorbis 的标签在列表里不显示，显示文件名。
- 改前截图用的是 music-1 的存档。
- 每次进页打开字体要 1.4～2.3 s（模拟器），这段时间页面还没出来。这是 Mogo 的字体引擎，本任务不许改。
- 第一次进页（这位家人开机后头一回）建文件索引约 2.1 s，所以能力的 30 s 期限仍然需要。
- 歌放完自动接下一首不算用户操作，没有单独计数，可能是 2 次刷新：先播放键变回“播放”，再整页换歌【推断】。
- 删旧音乐的提交 7736b270 不是最后一个提交。后面有两个上一轮的文档提交和本次复核提交 0cc4a819，都不碰 system/apps/music，主会话要留住旧音乐可以单独去掉 7736b270。

## 和设计不一致的
- media.finished 订阅放在 BG 执行器，不在媒体执行器：应用框架不许用服务锁；放在 BG 上和用户的播放命令同一条队列。代价是 BG 忙时（进页打开字体）接歌会晚一点。
- music.play 带路径时，只有“上次的歌”从记住的位置接着放，别的歌从头放（kiikoread open_track 的规则）。
- 随机数：应用框架没有随机接口，用时钟搅动的 xorshift。
- 扫描时机：原文开机和插卡时扫；我们第一次用时扫，之后只在音乐文件夹变了（file.changed）时重扫，并复用没变文件的标签。
- 收藏上限 200（原旧音乐 100），满了明说。
- 返回：这台设备没有返回键，状态栏的返回离开音乐页，退一层用页面上的“上一级”按钮。

## 改过的已有测试
- tests/music/product.c：触摸改成嫁接画面的点按表，所有 AI 断言和数值不变（上一轮）。
- tests/music/run.py：全家字体、内容变体、多阶段同一闪存镜像（上一轮）。
- tests/music/regressions.py：timeout、skip 改指新文件，新增 rebind、compat（上一轮）。
- tests/music/formats.c：只加了两条（上一轮）。
- tests/integration/ai_surface.c：末尾追加 music.mode、music.favorites（上一轮）。
- 复核这一轮：
  - tests/music/player.c：
    - “每个主要操作刷几次”由 `n >= 1 && n <= 2` 改成 `n == 1`，更严；
    - 新加进度条无灰缝、音量键、输出胶囊、盖板键翻页、重扫规则、收藏满、拔卡提示几组检查；
    - 读屏上像素的辅助函数顺手记下图像指针；
    - 其余断言不动。
  - tests/music/library.c：
    - 假的 `music_port_favorite_store` 跟着接口从 void 改成回报 bool；
    - 假的文件索引带上文件大小，并数读了几次标签；
    - 新加三条：缓存两条、拒收一条；
    - 原有断言不动。

## 新增的机制
- BG 画、UI 推屏的流水，一帧在路上时新改动并到下一帧（上一轮）。
- 页面开着且没被盖住时的 1 s 计时（上一轮）。
- music.mode 集合（上一轮）。
- media.state 监听和 media.finished 订阅，都在 BG（上一轮）。
- 测试探针（上一轮）；复核加了 `music.rescan`。
- ink_music_native 静态库（上一轮）。
- 复核这一轮：
  - `file.changed` 订阅（BG）：只记一笔，不扫描。
  - 换歌期间不画中间停顿的标记 `B.switching`。
  - 扫描的标签缓存（`music_track_t.size`、`s_tracks_was`）。

## 嫁接接口缺口
- 没有计时：音乐自己建 lv_timer。
- 同一时间只挂一个客户端。
- 成员文件只许 /me/ 加 /family/fonts/。
- 没有“这帧已推上屏”的回调：复核后改为按脏区判断进度条上没上屏。
- S31 的 apps 组件不定义 INK_PSRAM_BSS，并按 apps/*.c 通配。
- 应用框架没有随机数接口。

## 要用户定的
1. 重连胶囊：要不要在蓝牙服务加对应能力。
2. 开机是否自动接着放。
3. AAC 解码：应用分区只剩约 80 KB。
4. 输出胶囊：要不要加一步切到扬声器或耳机的能力。
5. 旧音乐删除（7736b270）合不合：D-135 写的是并存。
6. “只有上次的歌接着放、别的歌从头放”这条 kiikoread 规则是否保留。
7. 原文改动 600 行的算法：按新增或改过的行是 553，没超；把删掉的 628 行也算上就超了。
8. 每次进页打开字体要 1.4～2.3 s：要不要让 Mogo 字体引擎在音乐页关掉后不立刻释放，或者单独开一个任务做（要改 apps/mogo，本任务不许改）。

## 顺手发现
- 媒体服务的位置记忆：听到结尾后再放，从快结尾处开始（上一轮已记）。
- 媒体服务只对 Opus 报 read_failed。mp3、flac、wav、ogg 放到一半拔卡，读不出来就当成“放完”（codec_formats.c 的 `ink_format_next_packet` 读到 0 帧就返回 0）。音乐会接着放下一首，这一步又悄悄失败，页面上没有提示。要改得在媒体服务里分清“读到头”和“读不了”，不归本任务。
- mogo-page 和 mogo-flow 并行时抢着生成同一个字体文件。
- ai-history-photo、autotest-cap 在基线上就失败；unit-s31-cell 一组和 transfer-pages 在机器负载 11～12 时偶尔失败，单独跑都通过。
- system/services/file/transfer.c 还会建空的 /me/music（归传书）。
- “正在进行”卡想显示标签曲名要改 activity_card.c（归 shell-sweep-1）。
- 隐私检查：改动里没有 /Users/、/private/tmp、MAC 样式的字符串。

## 要改的文档
docs/tasks/backlog.md 记：应用分区余量约 80 KB；Mogo 大静态表在 S31 上进了内部 RAM；媒体服务位置记忆的现象；媒体服务对非 Opus 格式拔卡当成放完；音乐每次进页打开字体 1.4～2.3 s。

## 复核修复
1. **进度条灰缝**：`third_party/kiikoread_music/app_music.c` 的 `music_page_presented(EpdRect a)` 只在本帧脏区盖住“屏上进度”到“画出的进度”之间那一段时，才调原文的 `present()`；`apps/music/music.c` 的 flush 把合并后的脏区传进去。原文也有这个问题，已记进 SOURCE.md。测试：“the bar on the panel has no gap”。
2. **进页重扫**：
   - `on_enter` 改回原文“曲库为空才扫”；
   - 接线层订阅 `file.changed`，音乐文件夹变了才在下次进页或 AI 列曲库时重扫一次；
   - `scan_dir` 按路径加大小复用上次的标签。
   - 实测（模拟器）：曲库没变时进页 0 次扫描（原先每次约 0.24 s，200 首时按比例更长）；建完索引后的第一次再进只走索引约 0.13 s。
   - 测试：music-player restart 两条，music-library 两条。
   - 打开字体的时间没变，见“要用户定”第 8 条。
3. **收藏悄悄丢**：
   - 上限 200；
   - `music_port_favorite_store` 回报结果，`music_favorite_toggle` 先写后改；
   - 满了在歌词行和回执里说“收藏满了，先取消几首再收藏”（en.tsv 已加英文）；
   - `music.favorites` 写不下时带 `"more":true`，结果缓冲改成 6144 B。
   - 测试：restart 阶段“full: …”四条加一条腾出位置后的检查；music-library “a refused favorite changes nothing”。
4. **一个操作两次刷新**：
   - `B.switching` 让换歌期间不画媒体服务的中间停顿；
   - 回执统一走“同一首刷播放键、换歌刷整页”。
   - 测试由“1～2 次”改成“正好 1 次”，全部 13 个操作都通过。
5. **胶水预算**：两项行数写进报告和 SOURCE.md。胶水 846 行，预算 850；原文新增或改过的 553 行，预算 600；删掉的 628 行另列，怎么算交给用户定。驳回“胶水约 991 行”：那是把 app_music.c 里的接线入口在两项里各算一次。
6. **对照表的证据**：
   - 第 5、12、19、25 行补了专门测试，第 21 行加了进度条无灰缝的证据；
   - 第 3 行更正“返回键同样有效”；
   - 新增第 35、36 行；
   - 修了第 25 行的屏上不刷（advanced_bg 现在会刷歌词行）。
- 改动的文件：
  - apps/music/music.c
  - apps/music/port/music_page.h
  - apps/music/en.tsv
  - third_party/kiikoread_music/app_music.c
  - third_party/kiikoread_music/music_library.c
  - third_party/kiikoread_music/music_player.h
  - third_party/kiikoread_music/SOURCE.md（改动说明和三个哈希已更新）
  - tests/music/player.c
  - tests/music/library.c
  - docs/tasks/music-3/对照表.md
  - docs/system/capabilities.md（重新生成，多了 `file.changed` 事件）
- 全部在提交 0cc4a819。build-s31-own 已删除，build-asan 这一轮没有建，build 保留。
