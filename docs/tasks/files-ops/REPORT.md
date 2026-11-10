# files-ops 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。工作分支：`claude/codex-files-ops`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 新建文件夹、文本编辑、多选、AI、隔离 | 【实测】核心场景通过，补充检查随全量运行 | 两进程 123 + 176 个检查通过，`build/files-ops-v2bvggj5/summary.json` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build` |
| 2 回归、全量、内存、S31 | 全量进行中；内存与 S31 待运行 | GCC：5 compiled, 0 with findings | 见验证记录 |
| 3 截图 | 【实测】已生成并检查 | 下列截图路径；最终版本随全量测试重录 | 同验收 1 |

## 没做到的

编辑页复用现有长文本输入上限：4095 UTF-8 字节，超出时提示并拒绝编辑。多选针对文件；文件夹仍用于浏览和选择目标。大量、不连续的选择受现有能力参数表容量限制；没有扩容公共参数表。

## 和设计不一致的

新命名页、编辑页、列表工具栏按 D-117 自行补齐；删除沿用不可撤销操作的人工确认。未修改设计合同。

## 改过的已有测试

无。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

- 批量调用上下文和 `file.batch_progress` 事件：任务 3、4 要求逐项进度与结果，复用现有文件作业回调和任务展示。
- `files.drafts`：任务 2 的断电恢复要求，在已有成员数据库内存路径和草稿编号，正文仍由现有长文本草稿机制保存。
- 命名、编辑两页：任务 1、2 要求，复用键盘托管输入和长文本页。无新增 OS 线程、配置项或独立文件格式。

## 要改的文档

按总规则 §7 不修改共享的 `docs/system/capabilities.md`。已用 `python3 tools/capabilities.py --output work/files-ops/capabilities.md` 生成 468 项定义，主会话合并时重新生成正式表；因此原来的 `capability-table-doc` 报告过期。运行时目录核对已通过。

## 验证记录

工作区起始干净，HEAD 与指定基线一致。未访问设备、密钥或项目外资料。

【实测】`taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/files/files.c system/services/file/file_caps.c system/services/file/file_service.c system/services/text/text_service.c tests/files_ops/files_ops.c`：`gcccheck: 5 compiled, 0 with findings`。

【实测】命名、保存与重开、断电恢复、三文件拷贝/移动/删除、部分冲突与成员隔离均使用真实页面和服务；不依赖目录或编号顺序。点按先等待输入驱动确认收到按下，再抬起，避免闪存写入期间丢掉测试点按。

## 截图

- 新建文件夹：`build/files-ops-v2bvggj5/write/snap-NewFolder.png`。
- 写文件：`build/files-ops-v2bvggj5/write/snap-WriteText.png`。
- 多选：`build/files-ops-v2bvggj5/recover/snap-MultiSelect.png`。
- 恢复草稿：`build/files-ops-v2bvggj5/recover/snap-RecoveredDraft.png`。

## 为什么不能更小

需同时补三组页面行为、长文本草稿引用、显式替换写入和批量能力。只在界面直接读写会绕过能力与成员权限；把正文塞进普通参数表会受 416 字节上限影响。采用现有编辑页和草稿引用、现有原子写入、既有文件作业回调，没有另写编辑器或后台调度器。

产品改动行数和最终全量、内存、S31、反向对照、隐私扫描结果在收尾时补齐。
