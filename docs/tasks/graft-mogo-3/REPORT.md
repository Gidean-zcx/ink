# graft-mogo-3 执行报告

基线：`d703befe3e5e7b3e51c0483862c7feb2ef44aa61`。日期：2026-11-12。**模拟器正文与翻页已接通；普通全量尚有能力文档同步检查未过，不能报告全部验收通过。** 仅本地提交，没有连接设备或推送。数字与哈希见 [verification.json](verification.json)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 新阅读、GBK 正文与截图 | 通过 | 【实测】系统默认大小，正文 48 px；[第一页](gbk-page-1.png)、[第十页](gbk-page-10.png)、[返回首页](gbk-return.png)、[EPUB 正文](epub-page-1.png)，单张 54～60 KB | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility python3 tests/graft_mogo_page/run.py --build build` |
| 2 前后十页自动测试及反向对照 | 通过 | 【实测】最新 47 条断言通过，含 BG 清理完成；真实触摸、翻页键交替各前后十次，首页页码、文字及 L8 像素一致；断开输入后原测试退出 1 | 上述正向命令；`INK_KEEP_EVIDENCE=1 taskpolicy -c utility python3 tests/graft_mogo_page/reverse.py --build build` |
| 3 全量、内存检查、S31 宿主及固件 | 部分通过 | 【实测】内存 15/15、S31 宿主 77/77；两份固件编译成功，无编译器警告。全量首轮 4 失败，修正后三项接入检查通过；最新定向 9/10，剩禁止修改的生成能力文档 | 普通 `ctest --test-dir build --output-on-failure -j2`；内存 `ctest --test-dir build-asan -R '^(mogo-\|graft-carrier$)' --output-on-failure -j2`；`bash ports/esp32s31/build.sh`；实际调用均前缀 `taskpolicy -c utility` |
| 4 空实现及完整路径缺口 | 已列清 | 【实现】下表和“没做到的”；不能作为完整 Mogo 应用验收 | `apps/mogo/port/ink_page_port.h`、`runtime.c`、`settings.h` |

场景自动生成 `build/mogo-content`，可作为窗口复核时的内容目录；点“新阅读”后从文件选择器打开当前成员的 `/me/books/星星邮差GBK.txt` 或同目录 EPUB。书来自仓库原创示例，扩展首章用于十页往返；未下载图书。字体由既有工具生成到 `/me/fonts/InkSC.ttf`，3,897,328 B，随附 OFL；书和字体不提交，只提交截图及哈希。

## 没做到的

- 未接原目录、书签、阅读设置、书架、全文搜索、字典、摘句、阅读统计；没有把这些认定为不需要。任务可见核心完成后，时间用于必须的验证。
- 从“新阅读”进入系统文件选择器，可打开当前成员的 TXT/EPUB；还没有 Mogo 原书架。完整路径还缺原目录导航、书签与进度存储、设置及偏好存储、书架，再补它们的能力与快照。
- EPUB 本轮只验正文；插图解码、内嵌字体、题头装饰、竖排、阅读背景和原生浮层未接。图页可能留白，不能宣称完整 EPUB 阅读体验。
- 偏好暂取固定默认值，进度不持久化；没有调用或伪造 NVS 成功。成员文件走 `ink_graft_file_open`，本轮没有验收完整家人切换与浮层恢复。
- 排版途中退出、快速重开及系统字号/语言变化后的页面重建未覆盖；不能把稳定状态下关闭通过当作完整生命周期验收。
- 新阅读仅链接模拟器，尚不能在 S31 上阅读；没有测量实机耗时、功耗或原应用在 S31 的容量。

## 和设计不一致的

【实现】本轮保留 `app_book.c` 的 21 个完整原函数，包括 `open_book_impl`、`load_chapter_at`、`turn_page`、`draw_reader` 及正文几何；从固定提交整函数复制到 `third_party/mogo/main/apps/ink_book_page.c`，原文件不改。`reader_open`、`reader_chapter`、`reader_turn`、`reader_session`、TXT、EPUB、排版和字体文件实际编译进模拟器。不是另写分页算法，也没有用旧阅读画面替代。

完整应用的外围页面尚未接入；正文沿用原全屏阅读与进度线，顶部由系统状态栏承接，横排题头装饰等留空。`ui_fill_round_rect` 当前以 L8 矩形原语承接极细的进度线。书内字体和排版选项尚未接设置。不能把这些差异写成设计已完成。

架构审视：原解析/排版/绘制只在现有 BG 执行器串行运行，完成帧在 UI 执行器交 `ink_graft_present`；没有新增原生预绘线程。翻页点按及实体键都经嫁接输入调用 `mogo.next`/`mogo.previous`，AI 调相同能力；开书能力为 `mogo.open`。快照含书名、章节、本章页码、稳定字节位置、字号和可见文字。未接页面的能力留待后续，不以这三个能力代替完整 R-061 验收。

## 空实现表

以下均只在新阅读的模拟器适配中生效。原外壳、旧阅读和真实硬件服务没有替换成空实现。

| 函数或入口 | 当前行为及原因 | 后续连接 |
| --- | --- | --- |
| `save_progress`、`book_progress_load`、`pending_restore/find/discard/mark_latest/reserve` | 不保存/恢复；只让开书事务继续，尚无成员进度存储 | 当前成员的嫁接偏好或数据服务，恢复原进度队列语义 |
| `book_index_cache_prepare/open_read/open_write/finish_write`、`ink_mogo_fwrite` | 不建索引缓存、不写文件；书源仍按原算法扫描 | 成员内可重建索引目录及原子写入 |
| `app_settings_*` | 字号接系统缩放的默认 48；其余是默认值，无设置写入 | `ink_graft_preferences_open` 和原阅读设置 |
| `reader_prefetch_discard`、`invalidate_prep`、`lock_draw/unlock_draw` | 无预绘，现有 BG 执行器串行访问，所以无需另建锁/线程 | 接预绘时沿用原生命周期，不在本轮新建后台任务 |
| `board_sd_get_info`、`wifi_transfer_get_status` | 板级路径分支提供挂载占位/离线状态；实际文件访问由成员文件服务判定，未访问原 SD 或 Wi-Fi 驱动 | 文件服务的卡状态、已有网络服务 |
| `app_font_activate_reading/system` | 不切槽；适配入口已打开当前成员的 `InkSC.ttf` | 原字体上下文和字体设置 |
| `_binary_builtin_ttf_start/end` | 32 个零字节，仅满足未启用的内建回退链接；不是真字库，缺少 `InkSC.ttf` 时不能阅读 | 后续接真实系统字体回退；本轮正文实际使用生成的文件字体 |
| `heap_caps_get_largest_free_block` | 框架只提供可用 PSRAM 总量，暂以总量返回；分配仍由真实池决定是否成功，不当作连续块实测 | 需要时补框架连续块诊断 |
| `paint_reading` | 只返回原刷新意图，原 `draw_reader` 在同一 BG 工作中绘图，UI 统一提交 | 已经接 `ink_graft_present`，不接原厂同步推屏 |
| `prepare_inline_image`、`release_page_images`、`reader_image_dims`、`draw_reader_images` | 不解码/绘制插图，尺寸查询返回失败 | 原图片准备流程和系统图片能力 |
| `draw_reader_header`、`draw_vertical_heading`、`draw_reader_title_style`、`reader_chrome_data/draw_footer_row` | 原页眉、题头装饰及非全屏页脚未接 | 原绘制模块与系统外观适配 |
| `draw_reader_panel`、`ui_reader_dictionary_*`、`ui_reader_notice_draw`、`dictionary_cancel/dismiss` | 原设置、字典与提示层未接 | 原页面、系统弹窗及原子能力 |
| `ui_reader_background_paint`、`ui_nav_status`、`ui_text_set_system_scale` | 不画阅读背景或第二个状态栏；本轮只做正文，系统状态栏由承载层创建 | 阅读背景设置；继续共用系统状态栏 |
| `book_excerpt_begin`、`reader_excerpt_reposition`、`app_reading_journal_record` | 不记摘句/统计 | 成员数据服务 |
| `invalidate_covers`、`delete_retry_find`、`shelf_hidden_load/save`、`book_title_from_path` | 书架、删除清理和书名覆盖未接；书名沿用原文件名回退 | 原书架/文件管理/书名元数据 |
| `ESP_LOG*`、`TD_LOGI`、`uxTaskGetStackHighWaterMark` | 原厂诊断不输出，原任务栈查询占位；本轮以模拟器自身报告验证 | 系统日志及真实任务诊断 |

显示只交承载层，不调用墨水屏、触摸、电源、BLE 或震动驱动；原来的这些依赖未引入产品链接路径。

## 改过的已有测试

断言和期望数据没有修改。能力目录导出夹具的 CMake 编译增加 `INK_MOGO_SIM=1`，与生产模拟器相同，导出实际新增的三个能力；不是缩小检查范围。新增页面场景、来源校验和两种手动反向对照；不依赖 Git 历史。没有删断言、扩大预算或用 sleep 掩盖失败。

## 要用户定的

暂无。目录、书签、设置、书架仍是后续待接工作；没有自行改变产品范围。

## 顺手发现

【实测＋实现】承载层保存的状态栏对象会被外壳刷新替换（`system/ui/shell/navigation.c:1986` 起），退出时再删旧指针会卡住。仅在 `system/graft/graft.c` 移除这份所有权，状态栏交给页面生命周期。未改外壳文件。反向恢复旧指针时，场景完成正文往返后停在关闭，15 秒宿主超时；正向正常退出。

首次运行暴露 C 流适配不完整：`clearerr`/`ferror` 仍试图把成员文件句柄当宿主 `FILE`。已集中补齐流错误和单字节读取，不修改各书源算法。首个短首章夹具翻到第八次即跨章，随后只扩展原创测试书首章，保留页码和像素断言。退出卡住期间的未完成运行均不计通过；临时诊断代码已经撤掉。

### 规模与来源

见 [line-budget.json](line-budget.json)。本方 `apps/mogo/` 531 行（含翻译表），注册和根 CMake 8 行，S31 CMake 2 行，嫁接修复增删共 6 行，合计 547 行，低于 800；原函数子集另有 28 行包装、注释与分隔空行，计入本方也只有 575 行。Mogo 已有原文件本轮修改 0 行；新增四个整文件和 445 行整函数原文不计改写预算。

超过 100 行的原因：需要承接原页面上下文、成员文件流/字体文件描述符、PSRAM、L8 绘图、执行器和输入生命周期。更小的只调 `book_layout` 自画页面不能覆盖原开书/翻页路径；整份编译 7896 行 `app_book.c` 又会引入本轮不接的整套书架和设备依赖。本轮采用逐字的原函数子集，不重写排版。文件与函数哈希在 `third_party/mogo/page-upstream-files.json`，新测试逐项核对；许可证继续随原文件保留。

### 验证记录

普通全量首轮原话：`99% tests passed, 4 tests failed out of 439`，`Total Test time (real) = 1544.70 sec`；3 项 Hub 测试跳过。失败项为翻译标记、生成能力文档、能力运行时目录和 SDK 边界。三项接入代码问题已分别复查修正，能力文档按规则不改。重新配置后最新定向原话 `90% tests passed, 1 tests failed out of 10`，耗时 `40.09 sec`，唯一失败 `capability-table-doc`；不将局部复跑写成一次全量全绿。

内存检查：普通配置仅增加 `INK_SANITIZE=ON`，构建 `sim-mogo-page`、`sim-graft-carrier` 和八个 Mogo 宿主目标。专项原话 `100% tests passed out of 15`，耗时 `27.44 sec`，无跳过，含新页面 47 条断言和真实字体引擎。按 vendor-common 第 7 条未跑 ASan 全量。

S31 宿主独立工程原话 `100% tests passed out of 77`，耗时 `120.93 sec`，无跳过。命令：`taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host`，构建同目录 `-j2`，再用依赖 Python 的 `ctest --test-dir build-s31-host --output-on-failure -j2`。

S31 当前与独立基线工作树的 `build.sh` 均退出 0；当前 **4,268,080 B**，基线 **4,268,416 B**，差 **−336 B**，5 MiB 应用槽剩 **974,800 B**。两份均 **0 条编译器警告**，各有 6 条既有 CMake 提示。新阅读未进入 S31 源码列表，不能把不同构建路径/版本元数据产生的差值归因于阅读器优化。基线使用指定提交的独立工作树、`INK_S31_BUILD` 指向 `build-s31-baseline`、内容资产复用未改的模拟器资产；未移动只读上游 HEAD。

首次当前固件构建被 `apps/*.c` 通配收进新模拟器模块，因缺 `page.h` 失败；在 S31 应用 CMake 用两行明确排除此模块后重编成功，不通过删原有功能或改分区凑容量。

GCC 最终结果：`gcccheck: 7 compiled, 0 with findings`。命令：`taskpolicy -c utility python3 ../../tools/gcccheck.py build apps/mogo/mogo.c apps/mogo/port/runtime.c system/graft/graft.c third_party/mogo/main/apps/ink_book_page.c third_party/mogo/main/book/reader_open.c third_party/mogo/main/book/reader_chapter.c tests/graft_mogo_page/device.c`。未运行 Linux CI。

主构建存在既有重复静态库链接提示，不写成零警告。没有运行窗口版模拟器。

【实现】现有 CI 工作流没有显式声明已有字体转换工具所需的 `fontTools`；本机可用且真实运行成功，未运行远端 CI。新页面测试缺依赖时明确失败，不增加跳过；主会话合并时需确保 Linux/macOS CI 的工具 Python 也有该依赖。工作流不在本任务允许修改范围。

## 新增的机制

- 原生 Mogo 页面客户端、三个能力和快照：任务第 1 项及嫁接契约要求。
- 成员文件/C 流、字体文件描述符和 L8 原语适配：任务第 2、3 项要求；复用现有文件、PSRAM 和 BG/UI 执行器。
- `INK_MOGO_SIM` 编译标记与 S31 源码排除：实现任务允许的“只在模拟器链接”，不是运行时设置；翻译复用已有生成流程。
- 新的后台线程、状态机、持久文件格式、配置项、重试或缓存：无。原索引缓存和进度持久化暂未连接，未新造替代格式。

## 要改的文档

由主会话统一更新 UI 依赖、能力目录和接续记录：模拟器新增“新阅读”入口，`mogo.open/next/previous` 及正文快照可用；其余 Mogo 页面及 S31 链接仍未完成。本任务未改共享文档。

## 上机核对清单

本轮新阅读不在固件中，不把模拟器截图当真机效果。后续完成 S31 链接后，先核对存储卡字体的装载和 PSRAM，再验真实 GBK/EPUB、实体键与触摸、十页往返、退出、浮层及成员切换；本轮没有自行设定硬件参数。

## 过程记录

- M0：基线、范围及前两轮证据核对；建立并提交报告。
- M1：链接原函数、开书/章节/翻页/字体原文件及新阅读注册。
- M2：补齐文件流适配、修承载层退出，46 条断言及截图完成；输入和退出反向对照均符合预期。
- M3：内存专项、S31 宿主、当前与基线固件、GCC 核查完成；定向复查仅余禁止修改的能力文档未同步，保留该结论。

收尾：【实测】文件范围、本地链接、单文件大小与 `git diff --check` 通过；指定基线至交付内容的隐私模式扫描 **0 匹配**。旧阅读、外壳、共享需求/决策/能力文档和历史调研未改；没有提交字体、设计稿、固件或构建产物。验证日志位于忽略的 `work/graft-mogo-3/`，摘要纳入本报告目录。

全量首轮发现的接入问题：新应用三个翻译键、私有 C 接口头的引用约定，以及能力导出夹具未启用模拟器新阅读的同一编译开关。现已在新应用、其翻译表和相关 CMake 中修正，未放宽检查。生成的 `docs/system/capabilities.md` 仍需主会话运行 `python3 tools/capabilities.py` 同步新增三项；该文件被总规则明确禁止本任务修改，不能报告这项检查已过。
