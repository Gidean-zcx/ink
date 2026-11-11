# reader-more-1 执行报告

基线：`d9ece7ad488d0a5ec74ed289c1c03e63719d538d`。状态：实现、24 项定向回归及 GCC 检查通过，继续做完整构建和内存验证；只在本地任务分支提交，不连接设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 收件箱书架、续读和成员隔离 | 通过【实测】 | reader-more 三阶段通过；EPUB/TXT、排除项及两个家人 | `ctest --test-dir build -R "^reader-more$" --output-on-failure` |
| 2 原厂书签模型、操作和重排定位 | 通过【实测】 | 256 条、768 字节、半开区间模型测试及整机重排跳转通过；来源和适配见 [SOURCE.md](../../../apps/reader/vendor/metalio_bookmarks/SOURCE.md) | `ctest --test-dir build -R "reader-(more|bookmark-model)" --output-on-failure` |
| 3 书签隔离、重启和 AI 一致性 | 通过【实测】 | 当前页标记、菜单增删、列表删除、按成员和重启保存 | 同上 |
| 4 反向对照和原有阅读回归 | 通过【实测】 | 定向回归原话：`100% tests passed out of 24`；三项反向对照均检测到预期失败 | `ctest --test-dir build --output-on-failure -j2 -R "阅读回归集合"`，精确集合在最终报告补全 |
| 5 普通全量、内存检查及 GCC | 待验证 | — | — |
| 6 S31 宿主机和固件大小 | 待验证 | — | — |
| 7 常规及 120% 截图 | 待验证 | — | — |

## 没做到的

执行中，尚未作完成结论。

## 和设计不一致的

待核查。

## 改过的已有测试

只在 `tests/integration/ai_surface.c` 登记四个新能力及参数要求；没有修改已有断言或预算。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

仅接入任务要求的收件箱书籍和书签；不新增常驻任务或持久文件格式。

## 为什么不能更小

收件箱必须沿用已有打开和进度路径，书签必须接到成员数据库、原厂源位置、分页列表、菜单与四个 AI 能力。只加页码标签会在重排后定位错误；直接使用原厂存储会引入新的文件格式。完成后统计实际新增行数。

## 要改的文档

生成能力表按总规则第 7 条保护，不直接修改。需登记 `reader.bookmarks`（可选 open）、`reader.add_bookmark`、`reader.remove_bookmark`（可选 id，默认当前页）、`reader.goto_bookmark`（必选 id）及成员数据集合 `reader.bookmarks`。

## 上机核对清单

模拟器与编译结果不代替实机验证；待功能完成后列出具体步骤。
