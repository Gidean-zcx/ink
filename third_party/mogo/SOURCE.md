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

其余上游文件保持逐字节一致。新增 `CMakeLists.txt` 和本来源记录由 ink 项目编写；
测试入口和反向对照在 `tests/graft_mogo/`，通用承载接口在 `system/graft/`。

## graft-mogo-2 字体补充

固定来源仍为 `864f2f9`（v1.1.0），查阅日期 2026-11-12。新增的十个字体文件及其原始 SHA-256 见 [font-upstream-files.json](font-upstream-files.json)，从该提交的 Git 对象复制。`ttf_font.c`、字节源、块缓存、字形缓存、字重过滤及其头文件使用文件头声明的 Apache-2.0；`stb_truetype.h` 选择其文件末尾完整保留的 MIT 许可证。既有 [Apache 许可证](licenses/Apache-2.0.txt)继续随附。

仅两处原文适配：`ttf_font.h` 增加会话关闭声明；`ttf_font.c` 末尾增加 `ink_mogo_font_shutdown()`，在绘制和预绘已经停止后交还所有字体槽、保留块缓冲、共享工作区和目录，并清除借用 run 与回调。原 `ttf_font_unload()` 保留原语义。清理入口尚未连接产品生命周期，不能据此宣称换成员隔离完成。适配后的哈希也记在清单里，其余八个文件逐字节保持原文。

这些文件目前只进入 `mogo-font-file` 宿主兼容性测试，没有接入产品应用或设备固件。测试平台头明确放在 `tests/`；不把它们当成生产 HAL、NVS 或成员文件适配。

`tools/mogo_font.py` 使用现有 Noto Sans CJK SC 字面进行子集化和 CFF→TrueType 轮廓转换，保留 OFL/版权记录并将修改版改名为 Ink Reader Sans SC。完整 OFL 随生成字体复制；字体和许可证仅生成到内容目录，不提交字体、不下载资源、不嵌入应用分区。
