# 开源实现证据索引

查阅日期：2026-11-12。只读固定提交的文本源码；没有执行第三方代码、下载固件或游戏。S 编号仅用于本任务。已读文件的 SHA-256、字节数与行数见 [evidence.json](evidence.json)。

这里记录“代码实现了什么”；[报告](REPORT.md)另行判断哪些固件确实引用了它，哪些只是替代候选。许可证不作本任务筛选条件。

<a id="s01"></a>

## S01 CrossPoint

仓库：[crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)；固定提交：[`0333cbbe4649f80afafe83061bf1afe9fd372fcf`](https://github.com/crosspoint-reader/crosspoint-reader/tree/0333cbbe4649f80afafe83061bf1afe9fd372fcf)。

文件：[`README.md:19`](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/README.md#L19)；[`lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:111`](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp#L111)；[`src/activities/reader/EpubReaderActivity.cpp:1`](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/reader/EpubReaderActivity.cpp#L1)。

【实现】成熟阅读功能的公开基准：EPUB、字体排版、目录、脚注、书签等有实现入口。它是 CrossMux 和 CrossLink 家族已声明的上游。当前树支持 Metalio E-Ink4，不能把这个名称当成 Plus 非官方移植的代码证据。

<a id="s02"></a>

## S02 CrossMux：微读、五子棋、OPDS 的应用端

仓库：[0x1abin/crossmux](https://github.com/0x1abin/crossmux)；固定提交：[`a6fd569f7552db4ea1a6550ced7bc06568ae0e61`](https://github.com/0x1abin/crossmux/tree/a6fd569f7552db4ea1a6550ced7bc06568ae0e61)。

文件：[`README.md:5`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/README.md#L5)；[`src/activities/apps/weread/README.md:17`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/weread/README.md#L17)；[`src/activities/apps/weread/README.md:109`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/weread/README.md#L109)；[`lib/WeReadWebApi/src/WeReadClient.cpp:1`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/WeReadWebApi/src/WeReadClient.cpp#L1)；[`src/activities/apps/gomoku/GomokuAI.cpp:499`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuAI.cpp#L499)；[`src/activities/browser/OpdsBookBrowserActivity.cpp:122`](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/browser/OpdsBookBrowserActivity.cpp#L122)。

【实现】README 明确为 CrossPoint fork；微读文档给出登录→书架→离线 EPUB 的流程。此固定版本的进度同步不上传本地阅读时长，不能把它当成 ThingOS 所称时长同步的现成完整实现。致谢明确指向 S14/S15，并明确仅为参考，不证明逐行复制或组织隶属。五子棋有棋盘、难度与搜索代码；OPDS 页面与底层 S16 配合。

<a id="s03"></a>

## S03 kiikoread Pico

仓库：[wegooo-cell/read-pico-reader](https://github.com/wegooo-cell/read-pico-reader)；固定提交：[`ea371a7d5998880265aea09f9d44b26b619a8289`](https://github.com/wegooo-cell/read-pico-reader/tree/ea371a7d5998880265aea09f9d44b26b619a8289)。

文件：[`README.md:11`](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/README.md#L11)；[`main/book/book_source.c:61`](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/book/book_source.c#L61)；[`main/book/book_source.c:136`](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/book/book_source.c#L136)；[`main/book/book_layout.c:130`](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/book/book_layout.c#L130)。

【实现】README 明确源于 MindReset 官方演示；现有书籍入口、章节读取和排版实现可以检查。Mogo 官网和注册记录明确指向这一仓库；不能用 S31 适配仓替换它的实际来源。它包含完整阅读产品流程，不能因为许可变化而排除本次候选。

<a id="s04"></a>

## S04 kiikoread S31

仓库：[wegooo-cell/Metalio-S31-kiikoread](https://github.com/wegooo-cell/Metalio-S31-kiikoread)；固定提交：[`83942eb45d13064a47aca7ff04c2faf8fd51f98a`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/tree/83942eb45d13064a47aca7ff04c2faf8fd51f98a)。

文件：[`README.md:3`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/README.md#L3)；[`main/book/book_source.c:25`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_source.c#L25)；[`main/book/book_source.c:97`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_source.c#L97)；[`main/book/book_layout.c:1`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_layout.c#L1)。

【实现】这是可读的 Plus 阅读适配来源，用于与 S03 对照功能覆盖。没有证据表明 Mogo 或 PageRest 的私有改动等同这棵树。硬件后端与阅读产品分开评价；本次不移植、不编译。

<a id="s05"></a>

## S05 CrossLink 公开旧分支

仓库：[DaisonChun/crosslink](https://github.com/DaisonChun/crosslink)；固定提交：[`107e172994ad2086c414e159795515175681be71`](https://github.com/DaisonChun/crosslink/tree/107e172994ad2086c414e159795515175681be71)。

文件：[`README.md:16`](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/README.md#L16)；[`README.md:20`](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/README.md#L20)；[`platformio.ini:6`](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/platformio.ini#L6)；[`platformio.ini:10`](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/platformio.ini#L10)。

【实现】团队列出 Whimco、风眼；感谢 CrossPoint 和 allocated/crosspoint_chinesetype。构建配置仍是 CrossPoint 1.2.0、ESP32-C3。可证明家族旧源码存在，不能证明当前 Pixel、经典 2.0、Metalio S3/S31 发布源码齐备。Gitee 固定提交 eaeecd22761dd2d2884cbdb02acd066d4bd71bc4 的完整树仅 LICENSE、README.en.md、README.md；五个 GitHub fork 的 refs 与两个不同 fork 的配置检查见 evidence.json。这个 Gitee 发布仓的中文 README 另指向真正源码仓，已继续读到，见 S26。

<a id="s06"></a>

## S06 ChineseType：CrossLink 指出的另一来源

仓库：[icannotttt/crosspoint-chinesetype](https://github.com/icannotttt/crosspoint-chinesetype)；固定提交：[`c8e4100ed7fe59c4586cd1752f8c39544e493b81`](https://github.com/icannotttt/crosspoint-chinesetype/tree/c8e4100ed7fe59c4586cd1752f8c39544e493b81)。

文件：[`README.md:10`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/README.md#L10)；[`README.md:74`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/README.md#L74)；[`README.md:78`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/README.md#L78)；[`lib/EpdFont/CustomEpdFont.cpp:1`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/lib/EpdFont/CustomEpdFont.cpp#L1)；[`src/managers/FontManager.cpp:1`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/src/managers/FontManager.cpp#L1)；[`src/activities/reader/EpubReaderActivity.cpp:1`](https://github.com/icannotttt/crosspoint-chinesetype/blob/c8e4100ed7fe59c4586cd1752f8c39544e493b81/src/activities/reader/EpubReaderActivity.cpp#L1)。

【实现】公开代码含中文阅读与自选字体相关实现；作者说明源于 CrossPoint 1.0.0，且公开展示版本不含其写好的 TXT、图片及后续功能。因此它是部分来源，不能从功能介绍外推当前 CrossLink 的网盘、高清漫画和 Pixel 增量已公开。Gitee allocated 同系链接与 GitHub 同系仓库均已核查。此前按许可排除的结论不用于本任务筛选。

<a id="s07"></a>

## S07 RickyOS 旧版应用源码

仓库：[chinoryunqin/RickyOS](https://github.com/chinoryunqin/RickyOS)；固定提交：[`ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae`](https://github.com/chinoryunqin/RickyOS/tree/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae)。

文件：[`README.md:3`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/README.md#L3)；[`README.md:7`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/README.md#L7)；[`platformio.ini:1101`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/platformio.ini#L1101)；[`src/activities/home/HomeActivity.cpp:687`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/src/activities/home/HomeActivity.cpp#L687)；[`src/activities/apps/standby/CalendarClockFace.cpp:77`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/src/activities/apps/standby/CalendarClockFace.cpp#L77)；[`src/activities/apps/weread/README.md:109`](https://github.com/chinoryunqin/RickyOS/blob/ecfc977e6dd2fa93ef3a1f237ea0c0e48ad292ae/src/activities/apps/weread/README.md#L109)。

【实测／实现】匿名取到应用、库、构建配置及测试目录，绝非只放了说明。README 为 1.1.2/1.1.3-dev，配置已写 1.1.6-dev，公开 Release 元数据至 v1.1.5；官网明确从 1.2.0 起不公开源码，故不能用此树覆盖注册记录 v1.2.14。README 里“私有仓库”的旧描述与本次匿名可读事实矛盾，以实际访问结果和版本边界为准。

<a id="s08"></a>

## S08 Metalio-WhiteAI

仓库：[446599/Metalio-WhiteAI](https://github.com/446599/Metalio-WhiteAI)；固定提交：[`153d0967e97e1cf308403252cfc2839d6d65fb08`](https://github.com/446599/Metalio-WhiteAI/tree/153d0967e97e1cf308403252cfc2839d6d65fb08)。

文件：[`README_zh.md:3`](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/README_zh.md#L3)；[`README_zh.md:6`](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/README_zh.md#L6)；[`docs/AI_SYSTEM_MCP.md:1`](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/docs/AI_SYSTEM_MCP.md#L1)；[`main/application.cc:148`](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/main/application.cc#L148)。

【实现】源码本来就公开，并不是本次新找到的闭源解套。说明明确使用原生 1-bit 帧缓冲，当前目标不链接 LVGL；不得按 UI 外观或小智名称把所有固件都推成 LVGL／Brookesia。已有 AI 状态与工具文档可比较；列表旧版本与当前树没有二进制对应验证。

<a id="s09"></a>

## S09 retro-go-tab5：多核心游戏系统

仓库：[andjiang0083/retro-go-tab5](https://github.com/andjiang0083/retro-go-tab5)；固定提交：[`4e8f0f2f4e75b2102016c433b34ee1f23e17e471`](https://github.com/andjiang0083/retro-go-tab5/tree/4e8f0f2f4e75b2102016c433b34ee1f23e17e471)。

文件：[`README_CN.md:22`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/README_CN.md#L22)；[`README_CN.md:85`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/README_CN.md#L85)；[`README_CN.md:295`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/README_CN.md#L295)；[`retro-go-p4/gbsp/main/main.c:52`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/retro-go-p4/gbsp/main/main.c#L52)；[`retro-go-p4/gbsp/main/main.c:341`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/retro-go-p4/gbsp/main/main.c#L341)；[`retro-go-p4/retro-core/components/gnuboy/gnuboy.c:19`](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/retro-go-p4/retro-core/components/gnuboy/gnuboy.c#L19)。

【实现】已确认 retro-go 前端、gpSP GBA、gnuboy GB/GBC，以及 HowBoyAdvance RISC-V 后端来源。代码有存档、加载、输入/音频和运行循环；60fps 属作者在 P4/Tab5 上的声明，不是本次实测。根 GPL 不再导致本任务排除。F33 直接链接此仓；F26 不因此自动与它同源。

<a id="s10"></a>

## S10 retro-go-claw4：独立 Claw4 分叉

仓库：[andjiang0083/retro-go-claw4](https://github.com/andjiang0083/retro-go-claw4)；固定提交：[`f775a34d56f7346057c1193d106f041c7d9b17e8`](https://github.com/andjiang0083/retro-go-claw4/tree/f775a34d56f7346057c1193d106f041c7d9b17e8)。

文件：[`README.md:3`](https://github.com/andjiang0083/retro-go-claw4/blob/f775a34d56f7346057c1193d106f041c7d9b17e8/README.md#L3)；[`README.md:32`](https://github.com/andjiang0083/retro-go-claw4/blob/f775a34d56f7346057c1193d106f041c7d9b17e8/README.md#L32)；[`retro-go-p4/components/retro-go/targets/claw4/config.h:1`](https://github.com/andjiang0083/retro-go-claw4/blob/f775a34d56f7346057c1193d106f041c7d9b17e8/retro-go-p4/components/retro-go/targets/claw4/config.h#L1)；[`retro-go-p4/components/retro-go/targets/claw4/touch_layout.h:1`](https://github.com/andjiang0083/retro-go-claw4/blob/f775a34d56f7346057c1193d106f041c7d9b17e8/retro-go-p4/components/retro-go/targets/claw4/touch_layout.h#L1)；[`retro-go-p4/gbsp/main/main.c:341`](https://github.com/andjiang0083/retro-go-claw4/blob/f775a34d56f7346057c1193d106f041c7d9b17e8/retro-go-p4/gbsp/main/main.c#L341)。

【实现】README 明确是从 Tab5 线切出的冻结分支，目标表和实际 config/touch_layout 均有 claw4。可作为同硬件 GBA 的真实开源候选；注册记录 F26 的署名和版本并未指向此仓，仍不能宣称找到了 F26 自身源码。

<a id="s11"></a>

## S11 HowBoyAdvance：gpSP 的 RISC-V 实现链

仓库：[Irak4t0n/HowBoyAdvance](https://github.com/Irak4t0n/HowBoyAdvance)；固定提交：[`d2ed7442e7fbca96146ce043ae127835fa7e4ce8`](https://github.com/Irak4t0n/HowBoyAdvance/tree/d2ed7442e7fbca96146ce043ae127835fa7e4ce8)。

文件：[`README.md:16`](https://github.com/Irak4t0n/HowBoyAdvance/blob/d2ed7442e7fbca96146ce043ae127835fa7e4ce8/README.md#L16)；[`components/gpsp/gpsp_esp.c:82`](https://github.com/Irak4t0n/HowBoyAdvance/blob/d2ed7442e7fbca96146ce043ae127835fa7e4ce8/components/gpsp/gpsp_esp.c#L82)；[`components/gpsp/riscv/riscv_emit.h:1`](https://github.com/Irak4t0n/HowBoyAdvance/blob/d2ed7442e7fbca96146ce043ae127835fa7e4ce8/components/gpsp/riscv/riscv_emit.h#L1)。

【实现】不是仅有名字：有 ARM/Thumb→RISC-V 的代码生成和可执行缓存接线。它可解释 S09 明确承认的动态重编译来源。作者的 P4 性能数字不外推 S31；本次没有读取或试跑任何游戏/固件。

<a id="s12"></a>

## S12 Peanut-GB

仓库：[deltabeard/Peanut-GB](https://github.com/deltabeard/Peanut-GB)；固定提交：[`d0bcca771c83a2638c93a9ae61f3d51f226dc905`](https://github.com/deltabeard/Peanut-GB/tree/d0bcca771c83a2638c93a9ae61f3d51f226dc905)。

文件：[`README.md:9`](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/README.md#L9)；[`README.md:13`](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/README.md#L13)；[`peanut_gb.h:3532`](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h#L3532)；[`peanut_gb.h:3697`](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h#L3697)；[`peanut_gb.h:3826`](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h#L3826)。

【实现】纯 C GB 核心，公开 ROM/存储回调、逐帧运行和显示回调；README 明确只支持原始 GB，且存在准确性限制。Eink4Boy 的“无 GBC 专用游戏”与它相容，但这只是弱线索；gnuboy 也可能被裁剪，不能由此认定血缘。按游戏兼容性评价时，不能因它体积小就排在 gnuboy/retro-go 前面。

<a id="s13"></a>

## S13 Ardens：真正加载 Arduboy 游戏的模拟器

仓库：[tiberiusbrown/Ardens](https://github.com/tiberiusbrown/Ardens)；固定提交：[`e811cc6be72c8a802bd3265a84daf4165c177b93`](https://github.com/tiberiusbrown/Ardens/tree/e811cc6be72c8a802bd3265a84daf4165c177b93)。

文件：[`README.md:13`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/README.md#L13)；[`README.md:43`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/README.md#L43)；[`src/absim_load_file.cpp:253`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim_load_file.cpp#L253)；[`src/absim_load_file.cpp:966`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim_load_file.cpp#L966)；[`src/absim_execute.cpp:1`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim_execute.cpp#L1)；[`src/absim.hpp:1`](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim.hpp#L1)。

【实现】有 AVR/ATmega32U4、显示、声音和游戏文件装载的实现，是 ardmos 的功能替代来源候选。它不是已证实的 ardmos 上游，也没有本次 S31 性能证据。ESP32_Arduboy 这类把游戏源码重新编译的库，与加载既有 HEX/FX 文件的模拟器不是同一功能。

<a id="s14"></a>

## S14 weread.koplugin：完整流程与同步语义参考

仓库：[finlater/weread.koplugin](https://github.com/finlater/weread.koplugin)；固定提交：[`2795349f9992c2c723817330cf4ef2a9f0716f8b`](https://github.com/finlater/weread.koplugin/tree/2795349f9992c2c723817330cf4ef2a9f0716f8b)。

文件：[`README.md:5`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/README.md#L5)；[`README.md:23`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/README.md#L23)；[`weread/lib/qr_login.lua:1`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/weread/lib/qr_login.lua#L1)；[`weread/lib/protocol.lua:1`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/weread/lib/protocol.lua#L1)；[`weread/lib/downloader.lua:78`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/weread/lib/downloader.lua#L78)；[`weread/lib/progress_sync.lua:282`](https://github.com/finlater/weread.koplugin/blob/2795349f9992c2c723817330cf4ef2a9f0716f8b/weread/lib/progress_sync.lua#L282)。

【实现】可读登录、协议、下载恢复、进度同步等 Lua 实现；在 KOReader 上有章节/整本缓存、阅读时长和划线查看的完整流程。公开版与作者另做的 WeReader 闭源版有功能差异，不能把闭源版的精确进度、上传标注等算入公开版。运行依赖 KOReader；可复用流程和协议研究，不等于整套 Lua 可直接装进 ink。

<a id="s15"></a>

## S15 papers3-weread：ESP32 上的微读实现

仓库：[nasonliu/papers3-weread](https://github.com/nasonliu/papers3-weread)；固定提交：[`599ad6b2cc857c33c59a2ebedf9945e703c36c95`](https://github.com/nasonliu/papers3-weread/tree/599ad6b2cc857c33c59a2ebedf9945e703c36c95)。

文件：[`README.md:3`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/README.md#L3)；[`README.md:37`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/README.md#L37)；[`src/weread_login.cpp:25`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/src/weread_login.cpp#L25)；[`src/weread_api.cpp:52`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/src/weread_api.cpp#L52)；[`src/weread_api.cpp:159`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/src/weread_api.cpp#L159)；[`src/weread_api.cpp:860`](https://github.com/nasonliu/papers3-weread/blob/599ad6b2cc857c33c59a2ebedf9945e703c36c95/src/weread_api.cpp#L860)。

【实现】可定位扫码登录、个人书架、章节和进度请求；README 描述离线缓存、续读、取消下载与阅读设置。它与 S14 都被 CrossMux 明确致谢，因而比“凡是微读都猜用了某接口库”更有追源价值。目标是 PaperS3，性能文案未经本次实机验证。

<a id="s16"></a>

## S16 FreeInk SDK 的 OPDS 客户端与解析器

仓库：[Free-Ink/freeink-sdk](https://github.com/Free-Ink/freeink-sdk)；固定提交：[`6a208c0c46cce99bafa334bc4f171f5e558c5195`](https://github.com/Free-Ink/freeink-sdk/tree/6a208c0c46cce99bafa334bc4f171f5e558c5195)。

文件：[`libs/network/Opds/src/OpdsParser.cpp:37`](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/OpdsParser.cpp#L37)；[`libs/network/Opds/src/Opds2Parser.cpp:1`](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/Opds2Parser.cpp#L1)；[`libs/network/Opds/src/OpdsClient.cpp:65`](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/OpdsClient.cpp#L65)。

【实现】Atom XML、OPDS2 JSON 和经 transport 获取目录的实际实现。CrossMux 的应用页面在 S02。当前 CrossMux 已将解析器移至 SDK，不能错引已不存在的 lib/OpdsParser。OPDS 目录不是微信读书接口，也不是 WebDAV 客户端。

<a id="s17"></a>

## S17 Launcher：Moji／lnkboot 的功能候选

仓库：[bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)；固定提交：[`2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba`](https://github.com/bmorcelli/Launcher/tree/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba)。

文件：[`README.md:1`](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/README.md#L1)；[`src/sd_functions.cpp:640`](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/src/sd_functions.cpp#L640)；[`src/sd_functions.cpp:801`](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/src/sd_functions.cpp#L801)；[`src/install_shared.cpp:1`](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/src/install_shared.cpp#L1)。

【实现】有从 SD 安装固件、分区/镜像处理、固件目录等实现与文档；它是同类 ESP32 启动器候选。Moji 只写了“Fork 自 Launcher”，没给作者/仓库/提交；本次没有证据把 bmorcelli 的项目认定为唯一上游。切换整机镜像与 ink 的应用安装契约不同。

<a id="s18"></a>

## S18 ESP-ADF：录音／播放服务候选

仓库：[espressif/esp-adf](https://github.com/espressif/esp-adf)；固定提交：[`12ea304254d8a1efb1d93afa78e82ab3887684c1`](https://github.com/espressif/esp-adf/tree/12ea304254d8a1efb1d93afa78e82ab3887684c1)。

文件：[`README.md:13`](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/README.md#L13)；[`components/esp_audio_capture_service/src/esp_audio_capture_service.c:1`](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_audio_capture_service/src/esp_audio_capture_service.c#L1)；[`components/esp_audio_capture_service/examples/audio_capture/main/simple_record.c:35`](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_audio_capture_service/examples/audio_capture/main/simple_record.c#L35)；[`components/esp_audio_player_service/src/esp_audio_player_service.c:161`](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_audio_player_service/src/esp_audio_player_service.c#L161)。

【实现】所查是 v3.0 线，README 明确与 v2.x 不兼容；不能拿旧版 audio_pipeline 的路径冒充本树。可读录音帧获取/释放、服务创建和播放接线，有 MCP 服务方向；这仍是框架，未提供 ModernClassicOS 音乐管理或有声书章节的完整体验。没有证据表明闭源录音机、布丁或 ModernClassicOS 实际用了它。

<a id="s19"></a>

## S19 ESP-Brookesia：系统界面与音频服务候选

仓库：[espressif/esp-brookesia](https://github.com/espressif/esp-brookesia)；固定提交：[`65d66c810ad706c9e2f82fe9177b11816acee546`](https://github.com/espressif/esp-brookesia/tree/65d66c810ad706c9e2f82fe9177b11816acee546)。

文件：[`README.md:11`](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/README.md#L11)；[`README.md:47`](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/README.md#L47)；[`service/media/brookesia_service_audio/src/audio_encoder.cpp:12`](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/src/audio_encoder.cpp#L12)；[`service/media/brookesia_service_audio/src/audio_playback_control.cpp:68`](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/src/audio_playback_control.cpp#L68)；[`examples/service/audio/main/main.cpp:1`](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/examples/service/audio/main/main.cpp#L1)。

【实现】当前 v0.8 主线具备 GUI、运行时、应用和服务结构，可定位音频编码、音量/静音控制与示例。功能成熟度必须看完整用户流程，不能因框架官方或界面像手机就认定布丁／BoBo 基于它。本次不选择新架构。

<a id="s20"></a>

## S20 LVGL 的编辑、日历、二维码控件

仓库：[lvgl/lvgl](https://github.com/lvgl/lvgl)；固定提交：[`c033a98afddd65aaafeebea625382a94020fe4a7`](https://github.com/lvgl/lvgl/tree/c033a98afddd65aaafeebea625382a94020fe4a7)。

文件：[`src/widgets/textarea/lv_textarea.c:185`](https://github.com/lvgl/lvgl/blob/c033a98afddd65aaafeebea625382a94020fe4a7/src/widgets/textarea/lv_textarea.c#L185)；[`src/widgets/textarea/lv_textarea.c:269`](https://github.com/lvgl/lvgl/blob/c033a98afddd65aaafeebea625382a94020fe4a7/src/widgets/textarea/lv_textarea.c#L269)；[`src/widgets/calendar/lv_calendar.c:1`](https://github.com/lvgl/lvgl/blob/c033a98afddd65aaafeebea625382a94020fe4a7/src/widgets/calendar/lv_calendar.c#L1)；[`src/libs/qrcode/lv_qrcode.c:93`](https://github.com/lvgl/lvgl/blob/c033a98afddd65aaafeebea625382a94020fe4a7/src/libs/qrcode/lv_qrcode.c#L93)。

【实现】固定 v9.3.0 对应提交，可作为记事输入、日历和亮证二维码呈现的能力零件。文本控件不包含 TXT 文件保存、成员隔离、核销服务或浏览器引擎；无证据证明开哥诸应用实际用了 LVGL。

<a id="s21"></a>

## S21 MindReset Read Pico：可读基础来源

仓库：[MindReset/read_pico_firmware](https://github.com/MindReset/read_pico_firmware)；固定提交：[`6a247cc94420b85c650b32a6b2c063a360070eee`](https://github.com/MindReset/read_pico_firmware/tree/6a247cc94420b85c650b32a6b2c063a360070eee)。

文件：[`README.md:21`](https://github.com/MindReset/read_pico_firmware/blob/6a247cc94420b85c650b32a6b2c063a360070eee/README.md#L21)；[`README.md:143`](https://github.com/MindReset/read_pico_firmware/blob/6a247cc94420b85c650b32a6b2c063a360070eee/README.md#L143)；[`main/book/book_source.c:1`](https://github.com/MindReset/read_pico_firmware/blob/6a247cc94420b85c650b32a6b2c063a360070eee/main/book/book_source.c#L1)；[`components/read_pico_transfer/read_pico_transfer.c:601`](https://github.com/MindReset/read_pico_firmware/blob/6a247cc94420b85c650b32a6b2c063a360070eee/components/read_pico_transfer/read_pico_transfer.c#L601)。

【实现】这是 kiikoread 明确承认的基础来源，ThingsOS 亦声明参考其 BSP；PageRest 的历史仓库链接用了同名项目名。README 明确是硬件演示而非成品阅读器，不能将它排成阅读体验最佳。可读书籍入口与传输组件；本次未核硬件参数、不移用 Pico 驱动到 Plus。

<a id="s22"></a>

## S22 开哥项目集：规范和源码包入口

仓库：[Changkai9888/CloudZao-Projects](https://github.com/Changkai9888/CloudZao-Projects)；固定提交：[`90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8`](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8)。

文件：[`README.md:19`](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/README.md#L19)；[`README.md:29`](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/README.md#L29)；[`protocols/固件口袋接入规范与开发指南.md:1`](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/protocols/%E5%9B%BA%E4%BB%B6%E5%8F%A3%E8%A2%8B%E6%8E%A5%E5%85%A5%E8%A7%84%E8%8C%83%E4%B8%8E%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.md#L1)。

【实测／实现】完整树有口袋 source.zip、LGPL relink.zip、亮证 public-compliance.zip 和字体源码。规范不是各个小应用实现；压缩包文件名也不能证明完整应用已开源。本次范围请求不受服务器支持，未下载整包，未读取其中二进制；不为它虚构应用文件和行号。录音机等其余九类应用在该公开树中未出现独立源码。

<a id="s23"></a>

## S23 ESP32-audioI2S：常见音频容器的实现候选

仓库：[schreibfaul1/ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)；固定提交：[`56f27918a8e6d10d56a19ddf0fdd307cdeee8cde`](https://github.com/schreibfaul1/ESP32-audioI2S/tree/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde)。

文件：[`README.md:3`](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/README.md#L3)；[`src/Audio.cpp:750`](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L750)；[`src/Audio.cpp:2193`](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L2193)；[`src/Audio.cpp:5892`](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L5892)；[`src/Audio.h:1`](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.h#L1)。

【实现】公开多格式音频、文件输入、M4A 头解析与按时间定位实现；比只找到底层 I2S 更接近 ModernClassicOS 的本地音乐需求。README 列出 S31 及 PSRAM 条件；仍未经 ink 的预算与输出路径验证。M4A 解析不等于已有 M4B 章节索引，时间定位接口也不等于有声书跨书续播。

<a id="s24"></a>

## S24 xiaozhi-esp32：小智对话与工具的公开基础

仓库：[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)；固定提交：[`026e9b6e55e871089670c3909f85805b2a4b41f7`](https://github.com/78/xiaozhi-esp32/tree/026e9b6e55e871089670c3909f85805b2a4b41f7)。

文件：[`README_zh.md:1`](https://github.com/78/xiaozhi-esp32/blob/026e9b6e55e871089670c3909f85805b2a4b41f7/README_zh.md#L1)；[`main/application.cc:1`](https://github.com/78/xiaozhi-esp32/blob/026e9b6e55e871089670c3909f85805b2a4b41f7/main/application.cc#L1)；[`main/mcp_server.cc:28`](https://github.com/78/xiaozhi-esp32/blob/026e9b6e55e871089670c3909f85805b2a4b41f7/main/mcp_server.cc#L28)；[`main/mcp_server.cc:459`](https://github.com/78/xiaozhi-esp32/blob/026e9b6e55e871089670c3909f85805b2a4b41f7/main/mcp_server.cc#L459)。

【实现】可读对话应用编排、工具注册和 `tools/call` 分发。F31 自称小智，只能指到这个家族，不能证明其日历、壁纸、U盘改动来自本仓某个具体板型。没有证据把名字相同提升成版本或二进制一致。

<a id="s25"></a>

## S25 ThingsOS 明确声明的基础组件

【文档】[THIRD_PARTY_NOTICES](https://thingsos.scholarcn.com/THIRD_PARTY_NOTICES.txt) 明列以下组件；这证明作者声明了依赖，不证明已公开 ThingsOS 的接线和修改。对照公开上游实现如下，不能拿这些零件当成完整应用。

| 组件与公开仓库 | 固定代码文件／行号 | 对应功能与边界 |
| --- | --- | --- |
| [FreeType 2.14.3](https://github.com/freetype/freetype) | [`src/base/ftobjs.c:907`](https://github.com/freetype/freetype/blob/0a0221a1347e2f1e07c395263540026e9a0aa7c7/src/base/ftobjs.c#L907) | 字体装载与字形栅格化入口 |
| [zlib 1.3.1](https://github.com/madler/zlib) | [`inflate.c:590`](https://github.com/madler/zlib/blob/51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf/inflate.c#L590) | 压缩数据展开 |
| [cJSON 1.7.19](https://github.com/DaveGamble/cJSON) | [`cJSON.c:1142`](https://github.com/DaveGamble/cJSON/blob/c859b25da02955fef659d658b8f324b5cde87be3/cJSON.c#L1142) | 有长度边界的 JSON 解析入口 |
| [TinyUSB 0.21.0](https://github.com/hathach/tinyusb) | [`src/class/msc/msc_device.c:472`](https://github.com/hathach/tinyusb/blob/dae3f9a366bfcddbf9dcf1b48d7500286a849539/src/class/msc/msc_device.c#L472) | USB MSC 收发与设备侧读写回调 |
| [pypinyin 0.55.0](https://github.com/mozillazg/python-pinyin) | [`pypinyin/converter.py:34`](https://github.com/mozillazg/python-pinyin/blob/df101577145af2eb1abe5656e592e34e3bb56d23/pypinyin/converter.py#L34) | 汉字转拼音／字典预处理；Python 处理端，不等于设备运行 Python |
| [stb_image（声明未固定版本）](https://github.com/nothings/stb) | [`stb_image.h:1`](https://github.com/nothings/stb/blob/4e9b368bfc071e97206ea5c6adba68dd0cbf8897/stb_image.h#L1) | 图片解码；此处固定的是已知公开代码，未声称与 ThingsOS 的版本相同 |

【实现】stb、FreeType、zlib、cJSON、USB 栈都不能解释微读、ima 投递、录音管理等完整业务的来源；这些业务仍未知。

<a id="s26"></a>

## S26 CrossLink 的 Gitee 源码仓：坚果云、数据胶囊、风眼主题

仓库：[daixinchun/xteink](https://gitee.com/daixinchun/xteink)；固定提交 `5a93c1bc4589cde5b4f6df22b2edc618703b74de`。网页本次404，匿名 API 可读，不等于仓库没有源码。发布仓 [README 的 blob API](https://gitee.com/api/v5/repos/daixinchun/crosslink/git/blobs/51556ad86b5b0e7249b74f9755a1b74cf21dd906) 原文指向本仓：“本代码库只有发行版，源代码在”该地址。[固定完整树 API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/trees/5a93c1bc4589cde5b4f6df22b2edc618703b74de?recursive=1) 有540项，未截断。

【实测／实现】以下8份代码通过 Git blob API取得，解码后 SHA-1 与固定树逐份一致；C2复现命令可解码。源码的构建版本标为20260424、X3/X4 C3目标，因此仍是公开旧版，不能写成注册表当前 Metalio Plus／Pixel 4.3.0T 的对应源码。

| 文件／行号 | 可匿名读取的精确 blob | 实现／边界 |
| --- | --- | --- |
| [`platformio.ini:4`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/platformio.ini#L4) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/66c03e2f49ea2e8dce9b2bcb2f2cdf43100236c3) | 版本与构建目标；不是Plus发布配置 |
| [`src/activities/browser/DataCapsuleBrowserActivity.cpp:720`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/activities/browser/DataCapsuleBrowserActivity.cpp#L720) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/85bb3f305559ab119d0a40282922ebee2f6a0784) | 数据胶囊目录浏览／下载应用代码 |
| [`src/activities/browser/JianGuoBrowserActivity.cpp:52`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/activities/browser/JianGuoBrowserActivity.cpp#L52) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/81a4465921e04e681fd6b5f1eacea00acd003b6d) | 内嵌ESPWebDAV、PROPFIND与坚果云浏览／下载 |
| [`src/activities/reader/JianGuoSyncActivity.cpp:385`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/activities/reader/JianGuoSyncActivity.cpp#L385) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/b34f0262a3c2c7bb95498004cf563708e2d3c69c) | 坚果云阅读进度同步代码；本次未调用服务 |
| [`src/components/themes/fengyan/FengyanTheme.cpp:389`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/components/themes/fengyan/FengyanTheme.cpp#L389) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/e5e9fa71c90930c31e40f7fff92c87972fe53c41) | 风眼主题、书封和列表等绘制实现；不是当前Pixel的版本证明 |
| [`src/managers/FontManager.cpp:39`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/managers/FontManager.cpp#L39) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/88a33bc02e598ef2fabdf44c994177390766eb9c) | 自选字体扫描／装载；不据此承诺当前TTF直读 |
| [`lib/EpdFontLoader/EpdFontLoader.cpp:15`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/lib/EpdFontLoader/EpdFontLoader.cpp#L15) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/9a0651ab835dee8aa2368632805fa4f55988fe89) | SD字体装载接口 |
| [`src/activities/reader/EpubReaderActivity.cpp:1`](https://gitee.com/daixinchun/xteink/blob/5a93c1bc4589cde5b4f6df22b2edc618703b74de/src/activities/reader/EpubReaderActivity.cpp#L1) | [Git blob API](https://gitee.com/api/v5/repos/daixinchun/xteink/git/blobs/64b8bb1bb822f838b9220ca408dd598f9d85f3d1) | 阅读运行与排版交互入口 |

这使“坚果云客户端／数据胶囊只有闭源功能说明”的判断得到实质补充；它们现在有可读候选。完整体验、服务可用性和当前设备版本仍需比较，不能从源码存在推出成熟度胜出。
