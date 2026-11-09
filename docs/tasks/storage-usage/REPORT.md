# storage-usage 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。状态：功能与专项验证完成，全量与内存检查进行中；仅在本地任务分支提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 两位成人分别放入 17/23/31/7 与 19/29/37/11 字节的四类文件，逐类核对个人与设备总量；重复查询不调用目录遍历 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^storage-usage$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 索引未完成时真实页面显示“正在整理”，索引遍历期间观察到 organizing，完成后无需重开页面即可读到数值 | 同上 |
| 3 | 【实测】通过 | 5 字节临时文件、11 字节缩略图缓存夹具、7 字节半截文件共 23 字节被移除；普通文件、以 .tmp 结尾的目录内普通文件、备份均保留；缓存免确认，残留须确认，预览过期时拒绝删除 | 同上；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/storage_usage/reverse.py --build build` |
| 4 | 【实测】通过 | 页面与 AI 读取同一结果，真实触摸与 AI 使用同一能力及 revision；孩子从导航、直接页面接口、AI 和触摸能力均不能清理 | 同上 |
| 5 | 待验证 | 全量、内存检查、250 行预算 | 待补 |

## 没做到的

- 全量与 ASan/UBSan 检查尚在执行。
- 未测实机；模拟器卷空闲值为固定值，释放量的证据是被移除文件的实际字节和清理后预览归零，不声称验证了 FAT 簇分配。

## 和设计不一致的

- 【实测】SetStorage 的 1/4 灰度差为 **51.184/255**，未达到 ≤12。查看了生成图：现有主题放大后的标题、灰色卡片、底部分页控件、空间卡缺少总容量和比例条，均与画板不同；真实备份状态、两种统计口径及新增的清理说明也改变文字。未改备份行及公共绘制样式。既有 settings-a 报告为 52.353/255，场景数据不同，不把两个分数当作严格前后对照。
- 【实现】沿用设计的“照片和其他”合并行；`file.usage` 中照片、其他仍分别返回，页面合计展示。

## 改过的已有测试

- `tests/integration/ai_surface.c`：仅在 `FOR_AI` 末尾增加四项，`file.usage`、`file.cleanup_preview` 无输入，`file.cache_clear`、`file.cleanup` 必须有 `revision`。原因：四个新增能力需要被完整列入工具暴露检查；不改原条目、断言和预算。首轮全量因此报 6 个断言失败，补齐后复跑结果见验证记录。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- 一个索引完成版本标记（内存中的内容 revision 和卡标识）：由本任务“不要每次遍历整张卡”要求；只在内容变化或换卡后重建目录索引，不新增文件格式、任务、线程、队列、配置项或持久汇总。
- 其余复用文件编辑锁、后台执行器、确认卡、文件变化事件与 F1/F4 页面语义。新增四个原子能力：`file.usage`、`file.cleanup_preview`、`file.cache_clear`、`file.cleanup`。

## 要改的文档

- 合并时更新 `docs/system/ui-dependencies.md` 的 SetStorage 状态及生成的 `docs/system/capabilities.md`；遵守任务规则，不直接修改共享文档。

## 验证记录

- 【实测】开工时工作区干净，HEAD 等于给定基线，分支为 `claude/codex-storage-usage`。
- 【实测】专项 138 项检查通过，内部 RAM 最低余量 47,624 字节，内外部堆分配失败为 0。证据：`build/test-out/storage-usage-sim-storage-usage/device/metrics.json`。
- 【实测】反向对照 `member-scope` 产生 9 个断言失败，`user-directory` 产生 3 个断言失败，两个进程均完成测试；原产品文件哈希未变。证据：`build/storage-reverse/summary.json`，对应日志在同目录。
- 【实测】画板命令：`INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/storage-compare --scenes SetStorage --reuse`；证据：`build/storage-compare/scores.json` 与 `SetStorage.png`。

## 为什么不能更小

- 【实现】当前约 200 行产品改动，分别承担索引汇总、限定范围的清理预览与执行、四个能力注册及现有存储页接线。检查过更小方案：直接调用 `file.index` 会每次重新扫描目录；直接用通用目录删除会扩大删除范围；仅在页面累计用量或删除文件会绕过 AI 共用能力与权限。因此保留这几个必要增量。
- 【实现】现有 `file.index` 持久记录只存编号、卡标识和路径，`file.lookup` 的类型与大小来自扩展名和卡元数据。本次沿用同一分类函数、按索引路径取文件大小，不改持久格式；补充电子书扩展名到既有文本类型。该查询不读取文件正文，索引未变化时不扫描目录。
