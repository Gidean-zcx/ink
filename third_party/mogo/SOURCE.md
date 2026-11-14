# Mogo 阅读模块与主机测试副本

来源：`https://github.com/chopper1026/mogo-e4p`，固定提交
`864f2f9dc75980a67d2d6b67b17365020b7a50ce`（v1.1.0）。
取自任务指定的只读副本 `$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`，核对日期 2026-11-12。
上游 URL 已核对该副本的 Git 元数据；本次未联网取其它版本。

每个上游文件的原始 SHA-256、路径和字节数见 [upstream-files.json](upstream-files.json)。
文件保留原版权与 SPDX 声明，许可全文见 [LICENSE](LICENSE)、[licenses](licenses/Apache-2.0.txt)
及 [第三方声明](THIRD_PARTY_NOTICES.md)。仓库是分范围许可，不能统称 MIT 或 Apache-2.0；
标注 `LicenseRef-Kiikoread-Personal-Study-1.0` 的内容适用保留的个人学习研究测试许可。

## 本次用途与边界

- 整文件保留 TXT/GBK、EPUB/ZIP/HTML、排版、阅读会话、进度、书库及其主机测试。
- `main/apps/app_book.c` 供上游主机测试抽取原有书架、书签和阅读助手；**没有编译进产品**。
- `main/ui/ui_reader_settings.c` 用于原有设置绘制基准测试；**没有挂到产品页面**。
- `tools/*_stubs/` 只用于独立主机测试，绝不进入 `ink_apps` 或 `ink_system`。
- MOBI 不在本任务范围；其解码器带 LGPL-3.0 声明，本次不纳入。
- 本目录不是已完成的完整阅读应用移植；显示、文件、NVS、字体与应用主页面的生产接线尚未完成。

## 改过的上游文件

| 文件 | 改动原因 |
| --- | --- |
| `main/book/book_source.c` | 移除 MOBI 分派及引用，只保留本任务的 TXT/EPUB 分支；未重写二者解析算法。 |
| `main/book/book_source_internal.h` | 给原有 TXT 书源记录 UTF-16 大小端，位置仍为原书字节偏移。 |
| `main/book/book_txt.c` | 增加 BOM 识别、UTF-16 码元解码与代理对；沿用原来的流式索引和章节加载。索引缓存版本从 2 升到 3，旧缓存可重建。 |
| `tools/book_shelf_test_code.py` | 测试代码：拼目录链改用 `memmove`/`memcpy`，原来用同一个二维数组的上一行做 `snprintf` 参数，Linux 的 gcc 报截断和 `restrict` 重叠（`-Werror`），CI 失败。 |
| `tools/book_ui_host_test.py` | 测试代码：三处 `snprintf` 改为检查返回值，逐级拼深目录先把上一级拷到临时缓冲；Linux 的 gcc 把同一二维数组的两行看成重叠（`-Wrestrict`），并对不看返回值的有界写报可能截断（`-Wformat-truncation`），`-Werror` 下 CI 失败。 |

其余上游文件保持逐字节一致。新增 `CMakeLists.txt` 和本来源记录由 ink 项目编写；
测试入口和反向对照在 `tests/graft_mogo/`，通用承载接口在 `system/graft/`。

## graft-mogo-2 字体补充

固定来源仍为 `864f2f9`（v1.1.0），查阅日期 2026-11-12。新增的十个字体文件及其原始 SHA-256 见 [font-upstream-files.json](font-upstream-files.json)，从该提交的 Git 对象复制。`ttf_font.c`、字节源、块缓存、字形缓存、字重过滤及其头文件使用文件头声明的 Apache-2.0；`stb_truetype.h` 选择其文件末尾完整保留的 MIT 许可证。既有 [Apache 许可证](licenses/Apache-2.0.txt)继续随附。

仅两个文件适配，文件内均标明修改日期：`ttf_font.h` 增加会话关闭声明；`ttf_font.c` 末尾增加 `ink_mogo_font_shutdown()`，在绘制和预绘已经停止后交还所有字体槽、保留块缓冲、共享工作区和目录，并清除借用 run 与回调。原 `ttf_font_unload()` 保留原语义。清理入口尚未连接产品生命周期，不能据此宣称换成员隔离完成。适配后的哈希也记在清单里，其余八个文件逐字节保持原文。

mogo-lock-host-1（2026-11-14）又改了 `ttf_font.c` 一处：扫描字体目录时，如果“目录/文件名”放不进目录表的 160 字节路径，就跳过这个字体。原来照样登记一条被截短、打不开的路径；Linux 的 gcc 在 `-O0`/`-Os` 下对这里报 `-Wformat-truncation`（`-Werror` 下 CI 失败），产品构建把 `snprintf` 换成 `ink_mogo_snprintf`，所以只有 Mogo 的锁屏主机测试看得到。适配后的哈希记在清单里。

reader-one-1（2026-11-13）又改了 `ttf_font_source.c` 一行：从卡上读字体时每次读 256 KiB（原来 16 KiB）。阅读每次打开都要把约 3.6 MB 的 glyf 表读进内存，卡的每次请求都有固定开销，按 16 KiB 读要四百多次；改后阅读第一次打开在模拟器上从 1.78 s 降到 1.31 s。读出的字节和顺序都不变，适配后的哈希记在清单里。

这些文件目前只进入 `mogo-font-file` 宿主兼容性测试，没有接入产品应用或设备固件。测试平台头明确放在 `tests/`；不把它们当成生产 HAL、NVS 或成员文件适配。

`tools/mogo_font.py` 使用现有 Noto Sans CJK SC 字面进行子集化和 CFF→TrueType 轮廓转换，保留 OFL/版权记录并将修改版改名为 Ink Reader Sans SC。完整 OFL 随生成字体复制；字体和许可证仅生成到内容目录，不提交字体、不下载资源、不嵌入应用分区。

## graft-mogo-3 正文页面接线

固定来源仍为 `864f2f9`（v1.1.0），查阅日期 2026-11-12。
新增 `reader_open.c/.h`、`reader_chapter.c/.h` 四个完整原文件；
`main/apps/ink_book_page.c` 从同一提交的 `app_book.c` 整函数复制开书、章节交接、
几何、正文绘制和翻页路径，函数本身逐字保留，不编译原文件里尚未接入的书架和设置。
原路径、行号和每个函数的 SHA-256 见 [page-upstream-files.json](page-upstream-files.json)。
原文件 `app_book.c` 保持不变，既有主机抽取测试继续使用它。

本方胶水在 `apps/mogo/`：`port/ink_page_port.h` 提供当前阶段的页面上下文和外围空实现，
`ink_page_exports.h` 只暴露同步开书、翻页、绘制、关闭和快照；`runtime.c` 将书和字体读入
当前成员的 `ink_graft_file_open`，分配走系统 PSRAM，格式化走已有宿主调用出口。
帧缓冲改由 L8 平台绘图原语承接，再经 `ink_graft_present` 交给同一个 UI 和显示服务。
未接入功能及空实现表见 [本轮报告](../../docs/tasks/graft-mogo-3/REPORT.md)。
不改排版、TXT、EPUB、字形栅格化算法。新增原函数仅链接模拟器应用，不进入 S31 固件。

## graft-mogo-4 导航、成员保存与设备构建

固定来源与许可不变，查阅日期 2026-11-12。新增文件与整函数的原始 SHA-256、
原路径和行号见 [flow-upstream-files.json](flow-upstream-files.json)。

- `main/apps/book_toc.c/.h` 整文件原文复用。目录绘制、章节锚点及命中调用原函数。
- `main/apps/ink_book_page.c` 追加原 `app_book.c` 的 15 个整函数，复用书签格式、
  读写、绘制和当前目录项判断。仅 `draw_reader_bookmarks` 隐藏管理入口六行：
  本轮连接添加、列表和跳回，未连接批量删除；适配后哈希单独记录。
- `main/book/book_progress.c` 原文加入产品；原 NVS 调用经 `apps/mogo/port/nvs.h`
  和 `preferences.c` 映射到 `ink_graft_preferences_open` 的成员集合。
  没有访问设备原 NVS。进度保留原版本、完整路径、文件大小和章节内 UTF-8 偏移校验。
- `main/book/ink_reader_open.c` 从原 `reader_open.c` 整文件复制，只修改一行字号边界：
  对 18～90 的逻辑字号应用系统缩放，避免最大系统字号下把 108 px 误判成无效值。
  原文件继续供原主机测试和既有来源检查使用。
- `main/book/book_shelf_model.c` 原文进入产品，沿用原排序与可见性规则；
  当前成员书籍由文件服务扫描，封面尚未解码，使用书名卡片。分类管理尚未连接。
- `apps/mogo/` 的设置页使用原面板壳，八项设置经原章节加载/排版事务重建，
  尚未完整移植原字号滑杆、四向独立边距及六槽页眉页脚子面板。

同一份 `ink_mogo_build` 接线供模拟器和 S31 使用。无新线程、直接推屏或硬件访问。
画面经原嫁接层合成；输入和 AI 共用 `mogo.*` 能力。字体位于各成员虚拟路径
`/me/fonts/InkSC.ttf`，不嵌入应用映像。这里的设备接线是否编译、容量及实测边界
以 [本轮报告](../../docs/tasks/graft-mogo-4/REPORT.md) 为准。
