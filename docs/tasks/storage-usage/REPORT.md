# storage-usage 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。首轮验收 1～4 通过，第 5 项保留生成文档检查失败，画板差异未达目标；本轮复查修复与验证见末节。仅在本地任务分支提交。

证据范围：macOS 模拟器、合成文件和本机工具；未操作实机。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 两位成人分别放入 17/23/31/7 与 19/29/37/11 字节的四类文件，逐类核对个人与设备总量；重复查询不调用目录遍历 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^storage-usage$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 索引未完成时真实页面显示“正在整理”，索引遍历期间观察到 organizing，完成后无需重开页面即可读到数值 | 同上 |
| 3 | 【实测】通过 | 5 字节临时文件、11 字节磁盘缓存夹具、7 字节半截文件共 23 字节被移除；另经 file.thumb 生成并清除实际 PSRAM 缩略图缓存，原图保留；普通文件、以 .tmp 结尾的目录内普通文件、备份均保留；缓存免确认，残留须确认，预览过期时拒绝删除 | 同上；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/storage_usage/reverse.py --build build` |
| 4 | 【实测】通过 | 页面与 AI 读取同一结果，真实触摸与 AI 使用同一能力及 revision；孩子从导航、直接页面接口、AI 和触摸能力均不能清理 | 同上 |
| 5 | 【实测】未全绿；行数通过 | 普通与 ASan/UBSan 均为 241/242，无跳过、无内存错误；两套唯一失败均是按规则未更新的生成能力文档。产品新增 200 行，连删除行和 CMake 计入为 214 行 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存检查改为 `--test-dir build-asan` |
| G4 | 【实测】未达到画板差异目标 | 51.184/255，目标 ≤12，差异原因见下文 | `INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/storage-compare --scenes SetStorage --reuse` |

## 没做到的

- 生成的 `docs/system/capabilities.md` 尚未更新，因此干净检出的 `capability-table-doc` 会失败；按总规则第 7 节保留给主会话统一生成，没有临时替换文档来取得通过结果。
- Linux/gcc 未执行，本机使用 Apple clang 和 `INK_WERROR=ON`。
- 未测实机；模拟器卷空闲值为固定值，释放量的证据是被移除文件的实际字节和清理后预览归零，不声称验证了 FAT 簇分配。
- 清理仅覆盖 `/cache` 的可见文件，以及 `/content` 普通目录和 `.m` 成员目录内的保留后缀 `.tmp` / `.part` 文件；隐藏的备份、升级及索引元数据目录不清理。
- 【实现】清理预览单独扫描上述目录，不缓存结果；分类查询不重复遍历的证据仅针对 `file.usage`。索引完成标记跟随文件服务 revision 和卡标识；未验证离线新增文件后重插同一张卡的情况。

## 和设计不一致的

- 【实测】SetStorage 的 1/4 灰度差为 **51.184/255**，未达到 ≤12。查看了生成图：现有主题放大后的标题、灰色卡片、底部分页控件、空间卡缺少总容量和比例条，均与画板不同；真实备份状态、两种统计口径及新增的清理说明也改变文字。未改备份行及公共绘制样式。既有 settings-a 报告为 52.353/255，场景数据不同，不把两个分数当作严格前后对照。
- 【实现】沿用设计的“照片和其他”合并行；`file.usage` 中照片、其他仍分别返回，页面合计展示。

## 改过的已有测试

- `tests/integration/ai_surface.c`：仅在 `FOR_AI` 末尾增加四项，`file.usage`、`file.cleanup_preview` 无输入，`file.cache_clear`、`file.cleanup` 必须有 `revision`。原因：四个新增能力需要被完整列入工具暴露检查；不改原条目、断言和预算。首轮全量因此报 6 个断言失败，补齐后复跑结果见验证记录。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】确认卡标题缓冲含终止符共 48 字节，最初清理标题恰为 48 字节，会截断最后一个汉字。本任务标题改为“清理临时文件”，详细范围留在确认正文；反向恢复长标题准确触发 1 条失败。

## 新增的机制

- 一个索引完成版本标记（内存中的文件服务 revision 和卡标识）：由本任务“不要每次遍历整张卡”要求；只在服务 revision 变化或换卡后重建目录索引，不新增文件格式、任务、线程、队列、配置项或持久汇总。
- 其余复用文件编辑锁、后台执行器、确认卡、文件变化事件与 F1/F4 页面语义。新增四个原子能力：`file.usage`、`file.cleanup_preview`、`file.cache_clear`、`file.cleanup`。
- 架构审视：【实现】统计与清理由文件服务持有，经 HAL 访问卡；界面与 AI 共用能力和结构化结果，清理结果经 `file.changed` 通知；页面语义与确认卡均沿用统一框架，儿童沿用策略和页面入口限制。

## 要改的文档

- 合并时更新 `docs/system/ui-dependencies.md` 的 SetStorage 状态及生成的 `docs/system/capabilities.md`；未修改总规则第 7 节列出的共享文件。
- 已在 `docs/system/system-services-design.md` §1.3 表末追加本任务的四个增量能力，保留文档与运行时的清单一致性检查；没有改既有设计约束。

## 验证记录

- 【实测】开工时工作区干净，HEAD 等于给定基线，分支为 `claude/codex-storage-usage`。
- 【实测】最终普通与 ASan/UBSan 专项均为 135 项检查通过，内部 RAM 最低余量均为 47,624 字节，内外部堆分配失败为 0。证据：两套构建目录的 `storage-compare/SetStorage/metrics.json`。
- 【实测】反向对照 `member-scope` 产生 9 个断言失败，`user-directory` 产生 4 个断言失败，`confirmation-title` 产生 1 个断言失败，三个进程均完成测试；原产品文件哈希未变。证据：`build/storage-reverse/summary.json`，对应日志在同目录。
- 【实测】画板命令：`INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/storage-compare --scenes SetStorage --reuse`；证据：`build/storage-compare/scores.json` 与 `SetStorage.png`。
- 【实测】最终普通全量：241/242，无跳过，413.20 秒，CTest 退出 8，仅 `capability-table-doc` 失败；日志：`build/ctest-full.log`。AI 工具总表、服务设计清单、确定性、所有既有预算和三项中枢测试均通过。
- 【实测】最终 ASan/UBSan 全量：241/242，无跳过，725.74 秒，CTest 退出 8，仅 `capability-table-doc` 失败，无 sanitizer 报错；日志：`build-asan/ctest-full.log`。
- 【实测】不改共享文档的替代核查：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/storage-capabilities.md`，再加 `--check --runtime build/test-out/capability-table/capability-inventory.json`，两条均退出 0；368 条定义，运行时 306 项能力和 16 项设置一致。此项不能替代未更新的文档检查。
- 【实测】构建沿用总规则第 5 节，普通与内存构建都加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，内存构建另加 `-DINK_SANITIZE=ON`；均保留 `INK_WERROR=ON`，`taskpolicy -c utility` 下构建最多 `-j2`，CTest 最多 `-j2`，串行执行构建与全量测试。日志：两套构建目录的 `configure.log`、`build.log`、`ctest-full.log`。
- 【实测】`tools/ui_scale_check.py`：40 个文件、0 违规；`git diff --check` 通过。差异仅含文件服务、存储页接线、测试、CMake、设计清单表尾一行及本报告；没有构建产物或设计稿。基线到最终提交按总规则第 6 节的隐私正则扫描，0 匹配，grep 退出 1。

## 为什么不能更小

- 【实现】产品代码新增 200 行；按更保守口径，连删除行和 CMake 也计入，共 214 行，低于 250 行。分别承担索引汇总、限定范围的清理预览与执行、四个能力注册及现有存储页接线。检查过更小方案：直接调用 `file.index` 会每次重新扫描目录；直接用通用目录删除会扩大删除范围；仅在页面累计用量或删除文件会绕过 AI 共用能力与权限。因此保留这几个必要增量。
- 【实现】现有 `file.index` 持久记录只存编号、卡标识和路径，`file.lookup` 的类型与大小来自扩展名和卡元数据。本次沿用同一分类函数、按索引路径取文件大小，不改持久格式；补充电子书扩展名到既有文本类型。该查询不读取文件正文，索引未变化时不扫描目录。

## 复查后的修复

- 复查基线：`b40d6f52`；主会话提出 SU-M1、SU-L1，本轮只处理这两项。
- SU-M1：【实现】移除 `file.usage` 内的目录扫描，由现有 `file.index` 流程标记共享目录和成员根目录的索引完成情况；未完成时返回 `organizing`，页面显示“正在整理”。索引完成沿用 `file.changed` 刷新页面。统计缓存按卡、成员与文件 revision 复用；逐文件查大小在编辑锁外进行，结束后核对 revision 和索引版本，过期结果不发布。
- SU-L1：【实现】存储页分类、清理及剩余空间统一使用 KB、MB、GB；换算沿用现有的 1024 进位。
- 【实测】专项 `storage-usage` 148 项检查通过：未建索引不扫描、索引完成刷新、写后只重算一次、第二次零次已索引文件大小查询、KB/MB/GB 分类与剩余空间的真实页面快照，以及统计时发生写入后丢弃旧结果、下次重算并缓存。命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^storage-usage$' --output-on-failure -j2`。
- 【实测】锁探针共 5 次统计：普通查询每次只进入编辑锁 2 次，注入写入的一次含写操作共 3 次；查已索引文件大小时持锁次数为 0，最长单次持锁 12.148 毫秒。两次缓存命中均查 0 个已索引文件大小；非缓存查询各查 11 个，期间不扫目录。卡标识文件的固定元数据校验仍保留，不计入已索引文件探针。证据：`build/storage-compare/SetStorage/metrics.json`。过期写入由大小查询探针确定性注入普通写文件 API；未使用线程暂停或 sleep，锁探针作为本次并发要求的证据。
- 【实测】9 个反向对照均按预期失败并完成场景：原有 3 项为成员口径 13 条、用户目录 4 条、确认标题 1 条；本轮为禁用缓存 2 条、恢复统计内扫描 1 条、去掉过期检查 1 条、恢复分类单位 3 条、恢复剩余空间单位 2 条、恢复全程持锁 6 条。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/storage_usage/reverse.py --build build`；证据：`build/storage-reverse/summary.json`、`build/review-reverse.log`。两份产品文件的哈希前后相同，编译变体仅放在忽略的构建目录。
- 改过的已有测试：原来“首次 `file.usage` 自动建索引”和“统计扫描期间索引状态为 organizing”的断言改为“未建索引只返回 organizing、不读目录，现有 `file.index` 流程完成后页面自动刷新”。这是 SU-M1 明确要求改变的职责；原分类精度、成员隔离、清理范围、确认、儿童限制及预算断言均保留。
- 【实现】完成标记要求共享根目录和所有现有成员根目录都经过既有索引流程；未建立索引的新文件仍由该流程纳入，统计入口不主动补扫。未新增全卡自动建索引调度，也未验证实机性能；本轮不会把部分索引冒充整机结果。
- 【实测】相对任务基线，产品代码新增 225 行；连删除行和 CMake 计入为 241 行，均低于 250 行。全量与内存检查正在执行，最终结果待补充。
- 新增的机制：仅 SU-M1 明确要求的内存统计缓存及现有索引完成标记；没有新增任务、线程、文件格式、配置、重试或队列。
