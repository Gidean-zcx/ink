# reader-more-1 执行报告

基线：`d9ece7ad488d0a5ec74ed289c1c03e63719d538d`。状态：实现、24 项定向回归及 GCC 检查通过，完整构建和内存验证进行中；只在本地任务分支提交，不连接设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 收件箱书架、续读和成员隔离 | 通过【实测】 | reader-more 三阶段通过；EPUB/TXT、排除项及两个家人 | `ctest --test-dir build -R "^reader-more$" --output-on-failure` |
| 2 原厂书签模型、操作和重排定位 | 通过【实测】 | 256 条、768 字节、半开区间模型测试及整机重排跳转通过；来源和适配见 [SOURCE.md](../../../apps/reader/vendor/metalio_bookmarks/SOURCE.md) | `ctest --test-dir build -R "reader-(more|bookmark-model)" --output-on-failure` |
| 3 书签隔离、重启和 AI 一致性 | 通过【实测】 | 当前页标记、菜单增删、列表删除、按成员和重启保存 | 同上 |
| 4 反向对照和原有阅读回归 | 通过【实测】 | 定向回归原话：`100% tests passed out of 24`；三项反向对照均检测到预期失败 | `ctest --test-dir build --output-on-failure -j2 -R "阅读回归集合"`，精确集合在最终报告补全 |
| 5.1 普通全量 | 待验证 | — | — |
| 5.2 内存检查 | 通过【实测】 | ASan + UBSan：`100% tests passed out of 24`，61.90 秒；包括临时对象对照和分段测试 | 相同阅读回归集合，目录换为 `build-asan`，加 `-DINK_SANITIZE=ON` |
| 5.3 GCC | 通过【实测】 | `gcccheck: 6 compiled, 0 with findings`，含 C++ 胶水 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build apps/reader/reader.c apps/reader/bookmarks.cc apps/reader/epub_source.cc tests/reader_more/device.c tests/reader_more/model.cc tests/integration/ai_surface.c` |
| 6.1 S31 宿主机 | 通过【实测】 | `100% tests passed out of 75`，153.17 秒 | `cmake -S ports/esp32s31/tests -B build-s31-tests`；构建和 ctest 均为 utility、`-j2` |
| 6.2 S31 固件 | 通过【实测】 | bin **4,907,616 字节**；5 MiB 分区余 **335,264 字节（6.39%）**。编译器告警 0；SDK CMake 提示 6 条，数量与首轮相同 | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 7 常规及 120% 截图 | 通过【实测】 | 六张 clean 帧；最大 50,243 字节；逐张检查，120% 删除按钮完整 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R '^reader-more$' --output-on-failure` |

## 没做到的

尚未做设备实测；完整主工程、S31 和内存检查执行中，完成后在验收表给出结果。

## 和设计不一致的

原厂模型头保持原文；持久化按任务要求改为成员数据集合，移除原厂 BKM1 文件格式；ID 改由成员数据库分配，避免不同书籍编号相撞。其余范围与上限逻辑保留，具体差异见 SOURCE.md。

## 改过的已有测试

只在 `tests/integration/ai_surface.c` 登记四个新能力及参数要求；没有修改已有断言或预算。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

- 成员数据集合 `reader.bookmarks`：任务第 2 条要求，复用现有 JSON 记录和持久化服务，按书籍路径筛选。
- 书签列表页和四个能力：任务第 2、3 条要求，复用系统分页、能力调用、页面快照和已有跳转路径。
- 收件箱沿用现有进度文件判断“新”；没有新增配置项、常驻任务、状态机、缓存、后台重试或持久文件格式。

## 为什么不能更小

收件箱必须沿用已有打开和进度路径，书签必须接到成员数据库、原厂源位置、分页列表、菜单与四个 AI 能力。只加页码标签会在重排后定位错误；直接使用原厂存储会引入新的文件格式。产品代码新增 **314 行**（含 CMake、翻译、AI 登记和原文适配的 8 行；不以删除行抵扣），低于 400 行。

## 要改的文档

生成能力表按总规则第 7 条保护，不直接修改。需登记 `reader.bookmarks`（可选 open）、`reader.add_bookmark`、`reader.remove_bookmark`（可选 id，默认当前页）、`reader.goto_bookmark`（必选 id）及成员数据集合 `reader.bookmarks`。

## 上机核对清单

1. 从手机或电脑分别传入 TXT、EPUB，当前家人书架显示“新”；非书、目录及传输中的 .part 不显示；回书架、续读和目录、阅读设置可用。
2. 在 EPUB 第二章中段及 TXT 第二个目录位置加书签，确认页角标记、时间、章名和摘录；点击列表返回原句，菜单和列表都能删。
3. 连续改变字号和行距，书签仍回原句；核对定位耗时、内部 RAM/PSRAM 及刷新残影。
4. 切换两个家人，收件箱和同一共享书的书签各自独立；重启后进度与书签保留。
5. 常规与 120% 系统字号核对三行条目和“删除”按钮，没有裁字或重叠。

截图：[书架 100%](shelf-100.png) / [120%](shelf-120.png)，[书签 100%](bookmarks-100.png) / [120%](bookmarks-120.png)，[正文标记 100%](marked-100.png) / [120%](marked-120.png)。模拟器 clean 帧不代表真实屏幕残影。
