# reader-epub-1：阅读器能读 EPUB（原厂解析内核原文 + CrossPoint 目录解析 + 接进现有阅读器）

先读 `docs/tasks/vendor-common.md`（公共规矩，第 1、2、3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。

用户 2026-11-11：“这些功能我希望你能尽量走移植复用 不要重复造轮子 改造和适配我们的系统”“特别是底层逻辑可以充分复用集成 + AI接入能力改造 + UI风格尽量统一”“但是界面也不一定要重写 …… 只在有明显风格或交互冲突的情况下改造UI”。阅读是本机核心，用户勾的功能里 EPUB、目录、书签、统计都排在它后面。

输入：

- `docs/tasks/reuse-crosspoint-1/REPORT.md`：第 131～133 行（原厂 EPUB/ZIP/HTML、TXT 编码和章节）、第 267 行（阅读器“合并，局部换掉 TXT 限定的内容源和书架模型”）、“换入代码怎样接上 AI 与软总线”表第一行；`port-queue.json` 的 R01、R02、R03、R08、R10（本任务把它们收成一个端到端任务）。
- `docs/tasks/ui-audit-2/PAGES-APPS.md` 的 B01、B02、B05、B06（书架页头、分页、中文可行动错误、书架快照）和 `COMPONENTS.md` 的组件规格：本任务碰到的书架和目录页照它做，别的不碰。
- 现在的阅读器：`apps/reader/reader.c`（666 行，只读 TXT、最多 16 本、整本装进 PSRAM 不超过 2 MiB；预渲染下一页、连翻合并、按成员续读、能力和页面快照已接）。

## 要做的

1. **原厂解析内核原文入库**：从原厂 874538e 的 `main/reader/` 整文件拷 `zip_reader.{h,cc}`、`epub_document.{h,cc}`、`html_content.{h,cc}`、`text_encoding.{h,cc}`、`gb18030_tables.inc`、`txt_chapter.{h,cc}`，以及它们要的 `reader_types.h`、`book_read_error.h` 等头文件，放进 `apps/reader/vendor/metalio_core/`，原文不改。宿主机编译缺的 IDF 头（`esp_log.h`、`esp_heap_caps.h`、`freertos/*`、`ff.h`）用 `apps/reader/vendor/shim/` 下的兼容头补，只给宿主机用；固件用真的 IDF 头。图片解码（`image_stream` 一系）是后续任务：能不拷就不拷，正文里的图片块先显示成“［图片］”一行，在报告里写明。
2. **解压库**：原厂用 zlib 接口。用 `oss/crosspoint-reader/lib/miniz/third_party/` 里固定的 miniz 3.1.2（无版权限制，文件尾有声明）原文，加一个 `zlib.h` 兼容头；宿主机和固件用同一份，不新增需要联网下载的 IDF 组件。
3. **目录（NCX/NAV）**：原厂 EPUB 类没有完整目录。用 CrossPoint `lib/Epub/Epub/parsers/TocNcxParser.cpp`、`TocNavParser.cpp` 原文和它固定的 `lib/expat`（MIT，保留许可证），胶水替换它依赖的 `BookMetadataCache`、`FsHelpers`、`Logging`、`XmlParserUtils`。如果这些依赖牵得太深，改用 kiikoread（S05）`main/book/book_epub.c` 里文件头为 Apache-2.0 的 NAV/NCX 函数；二选一，报告写理由。**不许自己写 XML 或目录解析器。** TXT 的章节名用原厂 `txt_chapter.cc` 的规则。
4. **小 C 接口**：`apps/reader/reader_source.h`（纯 C，不出现 C++ 或 IDF 类型）：打开一本书（EPUB 或 TXT）得到标题、作者、章节数；目录条目（标题、章节号、章内位置）；把某一章的正文流式读进调用方给的缓冲（有上限，可取消）；关闭；错误码。实现在 `apps/reader/epub_source.cc`，用原厂 `StreamChapterBlocks` 流式读，不整章放栈上。
5. **接进 `apps/reader/reader.c`**（唯一接入点）：
   - 书架扫 `.epub` 和 `.txt`，不再限 16 本，用现有列表组件分页；EPUB 显示书里的标题；
   - 打开 EPUB 后按章读：一章正文进 PSRAM（每章有上限，超长章在段落边界切成几段），继续用现在的分页、预渲染、连翻合并和刷新调度；章末下一页进下一章，章首上一页回上一章最后一页；
   - 位置记成（章、章内偏移），照现在的方式按成员保存；旧的 TXT 位置照常能续读；
   - 加“目录”页（现有列表组件，点一项跳过去），从阅读页能进；
   - 能力（人和 AI 共用同一套）：保留现有 `reader.*`，新增 `reader.chapters`（列出目录）和 `reader.goto_chapter`（跳到第几章）；页面快照加书名、章名、章内第几页；
   - 打不开的书给中文原因（文件损坏、加密、不是 EPUB），走 i18n。
6. **界面**：阅读页保持现在的样子；书架和目录页照 ui-audit-2 的组件规格用我们的主题令牌和统一组件。原厂书架/详情界面（`main/display/screen/book_screen/`）和 BookSession 不在本任务搬。

## 范围和预算

- 只改：`apps/reader/**`、新增的 miniz 和 expat 拷贝目录（放 `apps/reader/vendor/` 下或 `third_party/` 下，二选一）、`CMakeLists.txt` 里阅读器和测试的几行（宿主机构建要能编 C++：没开图形模拟器时也要启用 CXX）、`ports/esp32s31/main/CMakeLists.txt` 里加源文件和头文件路径的几行、新增 `tests/reader_epub/**`、生成的 `docs/system/capabilities.md`、`docs/tasks/reader-epub-1/**`。
- 同时在跑的任务改的是 4G、电源、蓝牙、相机和短信电话的文件，不要碰。
- 胶水和适配新增不超过 900 行（拷来的原文、测试和测试用书不算）。原厂文件用宽松的警告选项只给 vendor 目录，我们自己的胶水用项目的完整警告。CI 用 Linux gcc（看 codex-rules 第 8 节）。
- 测试用 EPUB 自己做（几段原创文字，几 KB），提交在 `tests/reader_epub/fixtures/`：EPUB2 带 NCX、EPUB3 带 NAV、中文章名、没有目录的、ZIP 损坏的各一本。

## 验收

1. 宿主机测试（新测试加反向对照）：两本测试书的标题、作者、章节、目录跳转位置正确；翻页跨章前后都对；关掉再开回到原处；两个成员各自的位置不串；AI 调 `reader.read_page`、`reader.chapters`、`reader.goto_chapter` 结果和页面一致；损坏和没有目录的书有中文提示或明确回落；旧 TXT 续读、快速翻页、`reader_basic`、`reader_continuous` 等现有测试不放宽照样过。
2. 主工程普通全量 ctest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件增大了多少、应用分区还剩多少。
3. `SOURCE.md` 写清每个拷来文件的来源、提交、SHA-256、改了哪几行；报告写“上机核对清单”（主会话在真机打开一本 EPUB、翻过章节、跳目录、重启续读，看日志和每页耗时）。
