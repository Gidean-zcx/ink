# 源码证据索引

查阅日期：2026-11-12。以下均为【实现】，不代表真机或完整运行验证。文件 SHA-256 见 [source-evidence.json](source-evidence.json)，提交见 [sources.json](sources.json)。

| 证据 | 核查内容 | 固定版本文件与起始行 |
| --- | --- | --- |
| P1 | 禁则边界；这不是标点半字宽挤压。 | [crosspoint-reader/lib/Epub/Epub/CjkBreaks.cpp:9](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/CjkBreaks.cpp#L9) |
| P2 | 表格测宽与分列布局；不能外推为浏览器级 CSS。 | [crosspoint-reader/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:979](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp#L979) |
| P3 | CSS 数值解析使用浮点 from_chars；本机构建的系统版本边界。 | [crosspoint-reader/lib/Epub/Epub/css/CssParser.cpp:236](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/css/CssParser.cpp#L236) |
| P4 | TXT 按 UTF-8 输出，换行转 br；此路径没有中文编码转换或自动章目录。 | [crosspoint-reader/lib/Txt/TxtToHtml.cpp:59](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Txt/TxtToHtml.cpp#L59) |
| P5 | KOReader 同步界面和流程。 | [crosspoint-reader/src/activities/reader/KOReaderSyncActivity.cpp:1](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/reader/KOReaderSyncActivity.cpp#L1) |
| P6 | 书摘持久化。 | [crosspoint-reader/src/ClippingStore.cpp:1](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/ClippingStore.cpp#L1) |
| P7 | 上传、新建文件夹、重命名、删除和设置 API。 | [crosspoint-reader/src/network/CrossPointWebServer.cpp:326](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/network/CrossPointWebServer.cpp#L326) |
| P8 | WebSocket 上传与进度交互。 | [crosspoint-reader/src/network/html/FilesPage.html:3335](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/network/html/FilesPage.html#L3335) |
| P9 | 按标题和作者的书库检索，与正文搜索分开。 | [crosspoint-reader/src/activities/library/LibraryListActivity.cpp:1](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/library/LibraryListActivity.cpp#L1) |
| M1 | 完整行两端对齐，末行自然宽度。 | [crossmux/lib/Epub/Epub/ParsedText.cpp:1678](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/Epub/Epub/ParsedText.cpp#L1678) |
| M2 | 表格、插图、ruby 等 EPUB 布局路径。 | [crossmux/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:979](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp#L979) |
| M3 | 脚注导航及返回，不是只提取纯文本。 | [crossmux/src/activities/reader/EpubReaderActivity.cpp:874](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/reader/EpubReaderActivity.cpp#L874) |
| M4 | 微信扫码、书架和离线 EPUB；需要非官方服务协议。 | [crossmux/src/activities/apps/weread/README.md:1](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/weread/README.md#L1) |
| M5 | 每日、每书统计汇总。 | [crossmux/src/util/ReadingStatsAnalytics.cpp:48](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/util/ReadingStatsAnalytics.cpp#L48) |
| M6 | WebDAV 服务端实现；不等于坚果云客户端。 | [crossmux/src/network/WebDAVHandler.cpp:118](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/network/WebDAVHandler.cpp#L118) |
| M7 | 中文传书前端。 | [crossmux/src/network/html/zh-CN/FilesPage.html:1](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/network/html/zh-CN/FilesPage.html#L1) |
| M8 | KOReader 位置映射边界。 | [crossmux/lib/KOReaderSync/ProgressMapper.h:1](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/KOReaderSync/ProgressMapper.h#L1) |
| M9 | 原生模拟器及其固定依赖；Read Pico 不是 Plus 的板级验证。 | [crossmux/platformio.ini:336](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/platformio.ini#L336) |
| K1 | 中文避头尾、后续悬挂与横竖排。 | [kiikoread/main/book/book_layout.c:245](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_layout.c#L245) |
| K2 | 整行字距伸缩与 TTF 栅格化；不是专门的标点配对挤压。 | [kiikoread/main/font/ttf_font.c:3237](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/font/ttf_font.c#L3237) |
| K3 | 有限 CSS 属性集；不能称完整 CSS，也未见网格表格布局。 | [kiikoread/main/book/html_text.c:169](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/html_text.c#L169) |
| K4 | 流式 TXT、编码探测与中文章节。 | [kiikoread/main/book/book_txt.c:1](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_txt.c#L1) |
| K5 | 书名检索及书架操作；不是正文全文搜索。 | [kiikoread/main/apps/app_book.c:1452](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/apps/app_book.c#L1452) |
| K6 | 本次阅读时长、翻页数和最近30天统计；不是测试用 app_reading。 | [kiikoread/main/apps/app_book.c:2970](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/apps/app_book.c#L2970) |
| K7 | 从当前解析章节自动缓存短句；不是自由选区划线或笔记。 | [kiikoread/main/book/book_excerpt.c:1](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_excerpt.c#L1) |
| K8 | 网页目录和重命名、复制、删除；基础版没有改版的前端 TXT 目录生成。 | [kiikoread/components/read_pico_transfer/upload.html:62](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/components/read_pico_transfer/upload.html#L62) |
| X1 | 浏览器内编码探测、TXT 章节规则及原文字节偏移。 | [kiikoread-xiugai/components/read_pico_transfer/upload.html:98](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/components/read_pico_transfer/upload.html#L98) |
| X2 | 设备读取同名 toc.json；目录由手机/电脑浏览器生成。 | [kiikoread-xiugai/main/book/book_txt.c:727](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/book/book_txt.c#L727) |
| X3 | FTP 服务生命周期。 | [kiikoread-xiugai/components/read_pico_transfer/read_pico_transfer.c:2200](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/components/read_pico_transfer/read_pico_transfer.c#L2200) |
| X4 | FTP 只显示接收字节，不伪造未知总量。 | [kiikoread-xiugai/main/apps/app_transfer.c:781](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/apps/app_transfer.c#L781) |
| X5 | 仅同章锚点脚注，不能外推跨文件脚注。 | [kiikoread-xiugai/main/book/html_text.c:77](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/book/html_text.c#L77) |
| X6 | 模拟器声明 rc97、684×1216，Wi-Fi/USB/波形不模拟。 | [kiikoread-xiugai/tools/simulator/README.md:3](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/tools/simulator/README.md#L3) |
| X7 | 编译失败只汇总而不设置非零退出码。 | [kiikoread-xiugai/tools/simulator/build.py:21](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/tools/simulator/build.py#L21) |
| X8 | 要求对应 rc97 的发布提交。 | [kiikoread-xiugai/tools/simulator/source-manifest.json:1](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/tools/simulator/source-manifest.json#L1) |
| I1 | 字体、阅读统计、控件映射与近期九宫格等作者说明。 | [CrossInk/README.md:27](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/README.md#L27) |
| I2 | CJK 禁则和两端对齐实现；不是仅支持英文。 | [CrossInk/lib/Epub/Epub/ParsedText.cpp:75](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/lib/Epub/Epub/ParsedText.cpp#L75) |
| I3 | CrossInk 设备间统计同步。 | [CrossInk/src/activities/network/NearbyStatsSyncActivity.cpp:1](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/activities/network/NearbyStatsSyncActivity.cpp#L1) |
| I4 | 同一浮点 from_chars 最低系统版本问题。 | [CrossInk/lib/Epub/Epub/css/CssParser.cpp:137](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/lib/Epub/Epub/css/CssParser.cpp#L137) |
| L1 | 作者致谢 CrossPoint 与 allocated/crosspoint_chinesetype。 | [crosslink/README.md:16](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/README.md#L16) |
| L2 | 指南是继承文档；不能用它证明当前 Pixel 功能。 | [crosslink/USER_GUIDE.md:1](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/USER_GUIDE.md#L1) |
| L3 | 已有按字符断行代码；指南称不支持 CJK 与代码能力有时差。 | [crosslink/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:811](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp#L811) |
| D1 | 精确 XPointer 与近似进度明确分开。 | [dpreader/firmware/shared/main/book/book_sync_position.c:63](https://github.com/dephia31/dpreader31/blob/84f81c39d92fec50301aacc9f811d238fdb8b26a/firmware/shared/main/book/book_sync_position.c#L63) |
| D2 | 设备端 OPDS 客户端。 | [dpreader/firmware/shared/main/book/opds_client.c:1](https://github.com/dephia31/dpreader31/blob/84f81c39d92fec50301aacc9f811d238fdb8b26a/firmware/shared/main/book/opds_client.c#L1) |
| D3 | 现有位置映射测试带 UBSan；本次运行失败。 | [dpreader/tools/book_sync_position_host_test.py:13](https://github.com/dephia31/dpreader31/blob/84f81c39d92fec50301aacc9f811d238fdb8b26a/tools/book_sync_position_host_test.py#L13) |
| D4 | 现有流式 TXT 编码测试；本次运行失败。 | [dpreader/tools/txt_utf16_host_test.py:1](https://github.com/dephia31/dpreader31/blob/84f81c39d92fec50301aacc9f811d238fdb8b26a/tools/txt_utf16_host_test.py#L1) |
| D5 | 部分所谓截图由 Pillow 绘制，包含示例网络名，不能当真机或模拟器实测。 | [dpreader/tools/render_screenshots.py:90](https://github.com/dephia31/dpreader31/blob/84f81c39d92fec50301aacc9f811d238fdb8b26a/tools/render_screenshots.py#L90) |
| T1 | 在线阅读正文未打通，EPUB kosync 为百分比近似。 | [tingqu/docs/community-release-v2.0.74.md:13](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/docs/community-release-v2.0.74.md#L13) |
| T2 | 实际发送百分比作为 progress，不是 EPUB XPointer。 | [tingqu/main/reader/kosync.cc:334](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/reader/kosync.cc#L334) |
| T3 | 公开的书架服务代码；听趣不是只有闭源说明。 | [tingqu/main/weread/weread_service.cc:1](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/weread/weread_service.cc#L1) |
| T4 | 在线读取适配代码，存在代码不等于正文接口可用。 | [tingqu/main/weread/weread_online.cc:1](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/weread/weread_online.cc#L1) |
