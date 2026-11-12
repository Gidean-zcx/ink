# graft-mogo-4 执行报告

基线 `346d0ce9e8c76dd784b9193726a207c9f62d744e`，2026-11-12。优先项进度、跳章、书签和 S31 链接已接通；原设置与书架尚有缺口，普通全量有一次既有 AT 测试失败，不能报告全部验收通过。仅本地提交，未连接设备、未推送。数字与截图哈希见 [verification.json](verification.json)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 六种页面、两档系统文字大小 | 通过截图验收，原 UI 完整度见缺口 | 【实测】下表 12 张，均小于 300 KB；已检查正文、列表和最大字号面板 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/graft_mogo_flow/run.py --build build` |
| 2 阅读、成员隔离、按钮与能力一致 | 通过 | 【实测】142 条断言，0 失败；含五页翻动、加书签、第二章跳转、关闭重开、书签返回、家人隔离、八项重排与字号按钮一致性；最大系统字号下 90 逻辑字号为 108 px，重开保留 | 同上 |
| 2 保存反向对照 | 通过 | 【实测】仅在临时对象中撤掉进度写入；相同场景退出 1，139 条通过、3 条失败，包含“saved progress restores”；未改产品文件或断言 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/graft_mogo_flow/reverse.py --build build` |
| 3 普通全量 ctest | 未取得无失败的全量结果 | 【实测】原话：`99% tests passed, 1 tests failed out of 440`，707.74 秒；436 通过、3 跳过，`unit-s31-cell` AT 超时中止；同一二进制定向复跑 1/1 通过（11.63 秒） | `taskpolicy -c utility cmake --build build -j2`；完整 `ctest --test-dir build --output-on-failure -j2`；复跑加 `-j1 -R '^unit-s31-cell$'` |
| 3 内存检查 | 通过定向检查 | 【实测】`100% tests passed out of 16`，48.57 秒；ASan/UBSan 构建的实际阅读流程、正文、嫁接层及解析/排版/字体/存储可执行程序，另含来源与绘制脚本校验 | 同构建参数，目录改 `build-asan`，加 `-DINK_SANITIZE=ON`；该目录 ctest 选择 `mogo-` 与 `graft-carrier`，`-j2` |
| 3 S31 宿主机测试 | 通过 | 【实测】`100% tests passed out of 77`，112.00 秒 | `taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests`，构建后运行该目录 ctest |
| 3 S31 固件及链接证据 | 通过编译 | 【实测】构建退出 0、编译器警告 0；ELF 含 `ink_appdef_mogo`、`ink_mogo_init`、`ink_mogo_page_command`、`book_toc_render`、`book_progress_save` | `taskpolicy -c utility bash ports/esp32s31/build.sh`；交叉工具链 `nm --print-size build-s31/ink-s31.elf` |
| 3 GCC 与隐私检查 | 通过 | 【实测】`gcccheck: 8 compiled, 0 with findings`；按总规则第 6 节正则检查无匹配 | `taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build` 加本次 8 个 C 文件；隐私命令见下 |

| 页面 | 默认系统字号 | 最大系统字号 |
| --- | --- | --- |
| 书架 | [截图](default-shelf.png) | [截图](maximum-shelf.png) |
| 正文 | [截图](default-body.png) | [截图](maximum-body.png) |
| 目录 | [截图](default-toc.png) | [截图](maximum-toc.png) |
| 书签列表 | [截图](default-bookmarks.png) | [截图](maximum-bookmarks.png) |
| 设置 | [截图](default-settings.png) | [截图](maximum-settings.png) |
| 改阅读字号后 | [60 px](default-body-size60.png) | [72 px](maximum-body-size60.png) |

书与字体由既有夹具生成到 `build/mogo-content`，只用仓库原创故事；当前家人分别持有相同路径的独立副本。测试不依赖目录排序或 Git 历史。构建资源全部从任务指定的只读依赖目录读取，未改依赖。

## 没做到的

- 普通全量没有取得一次完全无失败的结果，不能把定向复跑通过写成整轮通过；具体失败与证据见验收表及“顺手发现”。

- 第 4 项只接通字号、行距、段距、统一边距、字重、缩进和页眉/页脚开关及其保存、重排；**原滑杆、四向独立边距、六槽页眉页脚等子面板没接完**。部分原 UI 间距/图标尚未完整跟随系统字号；没有将这些认定为不需要。
- 第 5 项接了原书架条目模型、排序与成员目录扫描，能从书名卡片打开书；**原封面解码、完整书架布局、逻辑分类管理没接完**。
- 原目录的十页快跳与百分比选择浮层未完整连接；本次验收的是原目录行到章节锚点的跳转。书签批量管理/删除未接。
- 第 6 项已有 22 个 `mogo.*` 能力（原 3 个、新增 19 个），同一入口承接触摸、按键与 AI；快照含书名、章名、稳定位置、可见正文和当前面板。**书签条目、目录列表与设置值的完整结构化快照还不齐**。全局能力名已由旧阅读占用，保留新阅读命名空间，旧阅读未改。
- EPUB 插图、内嵌字体、搜索、字典等前轮缺口仍在；本轮不以正文与目录可用代替完整应用验收。快速退出/重开、异常断电和实机性能未验证。
- 没有烧录或检查设备卡上是否已有字体；固件能链接不等于设备已能使用。

## 和设计不一致的

【实现】原 `book_toc.c/.h`、进度存储、书签核心和书架排序模型复用原文；来源文件/行号、原始与适配后哈希在 [来源清单](../../../third_party/mogo/flow-upstream-files.json)，出处与许可见 [SOURCE.md](../../../third_party/mogo/SOURCE.md)。书签列表仅隐藏未连接的管理按钮；开书字号校验对原 18～90 范围应用系统缩放。未重写 TXT/EPUB 解析、分页或字体栅格化。

设置当前为原面板壳加整行增减控件，书架为书名卡片；这两项尚未达到完整原界面嫁接合同。系统字号的 90/100/110/120% 与儿童 110% 在 UI 侧捕获，再交 BG 使用，避免在后台访问 LVGL。上述缺口均保留，不改设计文档迁就实现。

架构审视：仍使用现有 BG/UI 执行器，BG 排版与存储，UI 只提交画面；所有已接按钮经能力表。成员文件用 `ink_graft_file_open`，原 NVS 用 `ink_graft_preferences_open`，操作结束后再回复能力结果。未增加线程、显示直通或通用框架；AI 快照的剩余缺口见上。

## 固件、字体和内存

- 【实测】未接新阅读的基线映像 4,267,664 B，新映像 4,426,878 B，增加 159,214 B。实际 `.bin` 从 4,268,080 B 到 4,427,296 B，增加 **159,216 B**；5 MiB 分区按 `.bin` 计还剩 **815,584 B**，无需改分区或删功能。
- 【实测】链接静态 DIRAM 从 165,350 B 到 180,334 B，增加 **14,984 B**（BSS +14,928、data +56）；静态外部 RAM 均为 746,504 B。编译器警告均为 0；IDF 自身 CMake 组件提示仍存在，不算新编译器警告。
- 【实测】模拟器整体内部 RAM（含 DMA）最低余量、PSRAM 最低余量见 `verification.json`；BG 任务标签峰值约 4.92 MB，包含字体、排版与文件读入，不能等同实机测量。
- 【估算】两个 L8 表面占 `2 × 684 × 1154 = 1,578,672 B` PSRAM；测试字体占 **3,897,328 B**，另有字形/页表及数据库开销。字体每位家人虚拟路径为 `/me/fonts/InkSC.ttf`（卡上 `.m/<成员>/fonts/InkSC.ttf`），OFL 文件随字体放置。没有把这份字体嵌入应用分区；不能用 32 字节的内建占位代替字体。

## 改过的已有测试

没有修改既有测试、断言或预算；新增 `tests/graft_mogo_flow/`。原页面来源校验仍通过。新增测试早期修正了文件索引返回值理解（成功返回 1）与系统设置参数名（`id`），未放宽断言或增加延时掩盖问题。

内存检查集合：`mogo-page`、`mogo-flow`、`graft-carrier`、`mogo-source`、`mogo-utf16`、`mogo-layout`、`mogo-progress`、`mogo-store`、`mogo-policy`、`mogo-html`、`mogo-font-file`，以及 fixtures、shelf-bookmarks、session、settings-drawing、provenance 五项来源/绘制脚本。

## 要用户定的

暂无新增产品决定。未完成的原界面、分类和快照仍按原任务范围保留。

## 顺手发现

【实测】全量中的 `unit-s31-cell` 在 `ports/esp32s31/tests/cell.c:16` 因 AT 超时断言中止；随后相同二进制定向复跑通过，独立 S31 工程也 77/77 通过。`CMakeLists.txt:45`～57 的该目标不链接新阅读，本轮未改调制解调器源文件；已有注释也提到假对端在负载下可能错过 ACK。只确认存在间歇失败，未定位根因，未加 sleep、放宽断言或改驱动，应由主会话归口复核。

图标检查器会合并不同文件同名的局部指针，新增通用变量名使别名解析递归展开；改用本应用专用前缀后恢复 `0 errors`，没有修改检查器。全量检查发现新增标签缺少国际化标记，补齐本应用标签与翻译，没有改测试。视觉检查发现数学减号不在现有字体子集中，控件改用可显示的 ASCII `-`。

## 新增的机制

- 成员集合内的 NVS 键/类型/原始值封装：任务第 1 项要求承接原进度与书签；底层仍用现有数据服务。
- 阅读偏好记录：任务第 1、4 项要求按家人保存八项设置；没有新存储引擎。
- 新线程、后台重试、队列、缓存、配置框架：无。

## 为什么不能更小

本方接线改动 **685/800 行**（产品增加与删除均计），Mogo 原文适配为书签管理入口删除 6 行、字号校验替换 1 行；原样文件和函数复制单独记录，不算改写。新增测试和文档另计。NVS、灰度绘制与原页面能力/生命周期都是任务要求；保留固定默认值或另写小阅读器不能完成进度隔离与整块嫁接，未采用这些更小方案。

## 要改的文档

本任务明确授权的能力表已重新生成（含前轮未同步的 3 项）；新增项使生成章节锚点顺延。其余共享文档未改。完整原设置、书架和 AI 快照缺口供合并主会话续接。

## 上机核对清单

由主会话放置各成员的字体与 OFL；检查桌面旧/新阅读并存、从书架开书、目录第二章、书签跳回、关闭续读和换家人隔离；核对排版耗时、内部堆水位、PSRAM、异常断电后的已确认保存。此处均未实机执行。

最终核对：差异空白、允许范围、685/800 行、12 张截图（最大 58,326 B）、报告相对链接及总规则第 6 节隐私正则均通过。只有本地提交，不作“远端已保存”承诺。
