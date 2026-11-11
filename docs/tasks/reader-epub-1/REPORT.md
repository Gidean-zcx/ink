# reader-epub-1 执行报告

基线：`3fe7e334fdf1c0bdd343b4244bc95e6c9ead350e`。只在本地分支提交，未推送、未开 PR、未接触设备。证据：【实测】本次命令；【实现】代码核查；【估算】待上机。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 标题、作者、spine、NCX/NAV、中文目录和锚点 | 通过【实测】 | 两本原创 EPUB；锚点对应第二章内正文偏移；原厂和 CrossPoint 文件逐字节核对 | `ctest --test-dir build -R '^reader-epub' --output-on-failure -j2` |
| 1.2 双向跨章、超长章有界分段、连续章内页码 | 通过【实测】 | 普通版 351 条首次运行断言；2 KiB 正文缓冲变体 356 条，真实跨分段；大于 2 MiB 的原创章按段落切分后字节不丢 | 同上，`reader-epub-segments` |
| 1.3 关书、重开、目录跳转后续读、重启 | 通过【实测】 | 首次运行及同一闪存重启；普通版和分段版重启各 25 条断言 | 同上 |
| 1.4 两成员位置隔离 | 通过【实测】 | 成人第一章第二页、孩子第二章第一页，关书和重启后分别恢复 | 同上 |
| 1.5 AI 与页面一致 | 通过【实测】 | `read_page` 的书名、章名、章内页和正文；`chapters` JSON；`goto_chapter` 与真实点按目录均走能力表 | 同上 |
| 1.6 损坏、加密、非 EPUB、无目录 | 通过【实测】 | 合成损坏 ZIP、加密标志、错误 mimetype 均拒绝；无目录以 spine 回落；损坏回执和页面同为中文；新增文字进 i18n | 同上 |
| 1.7 超过 16 本、书架分页和快照 | 通过【实测】 | 24 本填充 TXT 加测试书逐页可达；统一列表快照含对象和分页 | 同上 |
| 1.8 TXT 续读、快速翻页及原有阅读测试 | 部分通过【实测】 | 定向 23 项中 22 项通过，包含 basic、continuous、rapid、reader-continue、成员唤醒、备份恢复、确定性；空书错误页一项计数断言未过，见下文 | `ctest --test-dir build --output-on-failure -j2 -R 'reader|integration-member-wake$|backup-restore-recovery|audit12-l7'` |
| 1.9 反向对照 | 通过【实测】 | 临时对象撤销 EPUB 接入，154 条断言失败；撤销目录位置的 dirty 标记，恰好 1 条“TOC navigation persists”失败；产品文件不改，ctest 无历史依赖 | `reader-epub-reverse`、`reader-epub-toc-save-reverse` |
| 2.1 普通全量 ctest | 最终复跑中 | 首轮原话：`96% tests passed, 15 tests failed out of 393`；其中回执兼容、C 链接驱动、翻页点位和重复保存问题已修复并定向通过；最终结果待补 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |
| 2.2 S31 宿主机测试 | 待跑 | 待补 | `cmake -S ports/esp32s31/tests -B build-s31-tests`，构建及 ctest 均 `-j2` |
| 2.3 S31 固件、增量、分区余量 | 初编通过，最终复编与基线对比待跑 | 【实测】原厂解析、miniz、expat 和胶水均编过；初编 bin 4,550,736 字节；未烧录 | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2.4 内存检查 | 待跑 | 待补，不把普通测试算作内存检查 | `build-asan`，`-DINK_SANITIZE=ON` |
| 2.5 GCC 严格告警 | 通过【实测】 | 原话：`gcccheck: 4 compiled, 0 with findings`；阅读器、C++ 内容源、兼容端口、集成测试均检查 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build apps/reader/reader.c apps/reader/epub_source.cc apps/reader/vendor/shim/reader_port.cc tests/reader_epub/device.c` |
| 3.1 SOURCE、许可、提交、哈希 | 通过【实测·实现】 | [SOURCE.md](../../../apps/reader/vendor/SOURCE.md) 列每个拷贝文件；50 个整文件逐字节相同；FsPaths 只抽整函数并加标准头和 namespace；均有宽松许可 | Python SHA-256 / 原文比较；最终重核待补 |
| 3.2 上机清单 | 已提供【实现】 | 见末节；没有实机通过结论 | 主会话执行 |

## 没做到的

- 空书错误页：实际打开、NEXT/PREV 拒绝和 HOME 都成立；`scenario-reader_empty-metalio-plus` 仍报 `ui.open_to_stable_ms.count: 1.000 < min 2.000`。三次处理未消除这个旧计数断言，按规则停止；没有改预算或跳过测试。
- 共享验收登记未改：生成的能力文档、既有 AI 能力清单和 SDK 静态边界脚本尚不识别新增能力或原文 vendor 头文件；本任务范围不允许改这些共享文件。最终全量失败清单和数量待补。
- 图片解码按任务书不移植，正文显示“［图片］”。单段自身超过 2 MiB 时只能退到 UTF-8 字边界，本次长章样本是多段。
- 未在 Linux 主机跑完整工程，采用交叉 GCC 严格告警检查，不能替代 Linux 全量或实机验证。
- 原厂 `sdkconfig.defaults:54` 选 CP936，本项目生成的 S31 配置仍是 CP437；`metalio_core/text_encoding.cc:176` 的双字节 GB18030 转换依赖 FatFS CP936。宿主机 iconv 用例通过，S31 的该编码路径不能据此宣称通过；配置不在本任务可改范围。

## 和设计不一致的

- 书架和目录用 C02 紧凑单行规格；为保留既有脚本的点按位置，页头实际留高 106 个画布像素，C10 规定为 125。返回控件和标题不相互覆盖，规范文件没有改，需主会话复核此差异。
- TXT 保留原来的全文分页与旧位置键；内容源将 TXT 作为一个正文流，目录用原厂规则定位全文中的标题。没有把 TXT 强行重排成 EPUB spine。
- 没有搬入原厂 BookSession、书架 UI 或持久缓存；miniz 的首次 FINISH 与 zlib 不同，由兼容头转换；原厂解析整文件没有改动。

## 改过的已有测试

- 没有改已有断言、期望、预算和测试源文件。
- `i18n-literals` 的 CMake 调用增加 `--root build/reader-i18n-check`：system/apps 链接到全部原源码，翻译表使用与编译相同的“系统表＋阅读器表”；原检查器和它的反向对照原样执行，未排除源文件。
- 纯 C 可执行文件保留 C 链接驱动；CMake 仍链接 C++ 运行库，修正既有测试脚本用链接驱动编译 C 时误取 c++ 的问题。

## 要用户定的

无采购、花钱、设备或发布事项。页头留高差异由主会话按设计与旧场景点位一起复核；建议合并时同步调整场景点位后恢复 C10 留高。GB18030 建议后续使 S31 的 FatFS 代码页与原厂 CP936 一致，不在此任务修改 SDK 配置。

## 顺手发现

- 【实现】原厂 ZIP 整条目读取没有与流式路径相同的加密标志/CRC 检查；胶水先对 mimetype 开流检查，正文仍用有校验的 `StreamChapterBlocks`。未改原文或另造 ZIP 解析器。
- 【实现】宿主机 C++ STL 元数据分配不计入模拟器设备内存池；主要正文、页表、miniz 和 expat 工作区走应用 PSRAM。EPUB 解析 CPU 成本没有单独校准，模拟器耗时不能当实机性能结论。

## 新增的机制

- EPUB/TXT 小 C 内容源和可取消、有界读取：任务书第 4 条要求。
- 章与分段导航、累计章内页码、旧位置文本增加 chapter/file 字段：任务书第 5 条要求；没有新增持久文件种类。
- 动态书架、目录页、能力与页面快照：任务书第 5、6 条要求；复用统一列表和现有软总线。
- 宿主机 IDF/stdio、miniz zlib、CrossPoint 数据接头与定长翻译表拼接：任务书的复用、双端编译与 i18n 要求。
- 无新增常驻任务、线程、后台重试、缓存、队列或用户配置项。

## 为什么不能更小

原文之外需要连接 C++ 解析、C 应用、两端文件访问、目录、成员位置和统一列表。没有搬入原厂 BookSession（会带入原厂 UI/缓存策略），没有自写 XML/目录解析器，也没有替换现有分页、预渲染和连翻合并。产品/适配新增行数最终重算，预算 900 行；测试、原创书与原文不计。

## 要改的文档

合并时重新生成 `docs/system/capabilities.md`：新增 `reader.chapters`、`reader.goto_chapter`、`reader.show_chapters`、`reader.list_next`、`reader.list_prev`，并登记 `open_book` 的可选 index。需要同步 `tests/integration/ai_surface.c` 的能力登记，以及 SDK 静态检查对独立 vendor 目标、模块内头和 JSON 工具的识别；不改 requirements/ideas/decisions。

## 上机核对清单

1. 各打开原创 EPUB2/EPUB3：标题与作者正确，中文正文可读，图片只显示占位行；缺目录、损坏及加密书给对应中文提示或明确回落。
2. 下一页进入第二章，上一页回第一章末；从书名旁“目录”进入，点“中途码头”，AI 的目录与读页结果和画面一致。
3. 成人停在第一章第二页，孩子停在第二章第一页；关书、目录跳转、重启和切换成员后各自恢复，日志不含成员正文。
4. 打开大于 2 MiB 的长章，跨分段前后不丢字、章内页码连续；记录 `reader.open_ms`、`reader.page_render_ms`、空闲 RAM/PSRAM 与每次稳定刷新耗时。固件编译和宿主机预算不能替代这一步。
