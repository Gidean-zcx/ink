# file-index-1 执行报告

基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`。分支：`claude/codex-file-index-1`。状态：实现和定向验证完成，全量与内存检查进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】改名、整个目录移动、删除重建；跨进程重启后、首次查询前移动仍保号 | T1 |
| 2 | 通过 | 【实测】PNG/JPG、Opus、TXT/MD、BIN；扩展名不区分大小写 | T1 |
| 3 | 通过 | 【实测】目录树按类型查询；C 接口及 AI 能力首末页、数字游标 | T1 |
| 4 | 通过 | 【实测】拔卡返回 NO_CARD/unavailable；换卡返回 CARD_CHANGED/changed；两张合成卡同路径编号不同，换回恢复原号 | T1 |
| 5 | 通过 | 【实现】设备集合 `file.index` 存于 `/user/sys/file.index.*`，由数据服务在 PSRAM 保存记录与索引；卡上只存 16 字节随机身份 `/content/.file-index-card`。所有卡合计最多 512 个活跃文件，记录内容最多 110592 字节 | `file_service.c` 的 `INDEX_DEF`、`index_save`；`datastore.c` 的 `mem_get` |
| 6 | 验证中 | 【实现】注册 `file.index`、`file.lookup` 和 `file.index_state`；新增产品代码 217 行，未改预算 | T2、T3 待完成 |

T1：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/run.py --build build file_index`。首次四组全部通过，共 564 个检查；后续补充家庭目录范围检查。

反向对照：仅临时在 `index_edit` 函数首部返回 0，撤去移动/删除元数据维护；同一 T1 退出 1，改名、目录移动、重启前置移动、满额恢复共四项确实失败。随后逐字节恢复源码。证据：`build/file-index-reverse.log`。

## 没做到的

- 未碰设备；未做真 FAT、实机性能或 Linux 运行验证。
- 电脑在 U 盘模式下直接改名/删除重建的身份追踪、两卷间断电原子性不在本次验收中，未做。
- 扩展名用于分类，不检测文件内容，也不声明格式可解码。

## 和设计不一致的

本任务要求的索引补上详细设计 §4 的历史缺口；没有修改设计合同。扫描沿用已有 8 层目录限制，超深或超长返回 PATH；容量满返回 INDEX_FULL，不淘汰旧编号。首次扫描在现有后台执行器同步完成，后续页按已分配编号读取。

## 改过的已有测试

- `tests/integration/ai_surface.c`：AI 可用清单新增 `file.index`（必填 `path`）、`file.lookup`（必填 `id`）。原断言、预算均保留；首次全量运行正因缺这两项产生四个失败。
- `tests/services_finish/run.py`：仅增加 file_index 的四组持久化场景，已有场景参数和期望保持原样。

## 要用户定的

尚无。

## 顺手发现

默认字体 Python 环境没有中枢测试依赖，第一次全量跳过三项；已发现依赖目录内现成的 `hub/.venv`，用 `INK_HUB_PYTHON` 指向它重跑，不安装或升级依赖。

## 新增的机制

- 设备级 `file.index` 二进制记录：任务明确要求持久编号和媒体索引；复用现有数据服务的格式、日志、单调编号、持久化和 PSRAM 分配。
- 卡上 16 字节身份标记：任务明确要求换卡隔离；临时写入、同步后改名，文件管理器不展示隐藏文件。
- `file.index_state`：任务明确要求不可用、已换卡状态；使用现有状态表。
- 未新增任务、线程、执行器、队列、后台重试或配置项。

为什么不能更小：需要同时覆盖跨重启编号、目录移动时更新子项、删除后不复用、按类型分页、卡身份、权限和能力注册。路径哈希无法保留改名后的编号；纯内存表无法跨重启；只按卡内计数会让不同卡编号重合。复用数据服务和现有变更锁后，产品代码为 217 行，低于 250 行上限。

## 要改的文档

- 合并时更新 `docs/system/ui-dependencies.md` §4.1 和 `docs/tasks/backlog.md` 的对应缺口。
- 合并时运行 `tools/capabilities.py`，生成两项新能力和一项状态。为本地目录一致性测试临时生成，最终不提交生成文档。
- 详细设计 §4 可补充接口、集合位置、512 条全局上限、扩展名分类与 8 层扫描边界。
