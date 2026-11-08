# file-index-1：稳定的文件编号、类型和媒体索引

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/file-index-1/REPORT.md。

## 要做的

backlog 的 file-index 一节（预算 500 行）拆成两半，这是前一半，预算 250 行产品代码：文件服务缺口（docs/system/ui-dependencies.md §4.1）里的稳定文件编号和类型、媒体索引，以及卡拔出和换卡的状态。缩略图、图片解码和抖动是 file-index-2，正在另一个克隆里同时做，这里不做。

## 从哪里看起

- 文件服务 system/services/file/，能力注册在同目录的 `*_caps.c`；存储 system/services/storage/。
- 现有测试：tests/services_finish/ 里的 `file_*.c`，tests/integration/ 里文件相关的。
- ui-dependencies.md §4.1：界面需要文件服务给什么。

## 边界

- 编号存在卡上或数据服务里：改名、移动不变；删除后不复用。
- 索引放 PSRAM（`OSAL_MEM_EXTERNAL`）或卡上，不占内部 RAM；条目数有上限，满了如实返回。
- 卡拔出、换了一张卡：状态明确（不可用、已换卡），旧编号不会指到新卡上别的文件。
- 用已有的执行器和数据服务，不新建常驻任务。
- 同时在跑的还有 member-quiesce（system/policy）、file-index-2（新目录，图片和缩略图）、ui-semantics（system/ui）。不要改它们的文件。

## 验收

1. 改名、移动后编号不变；删除后再建同名文件，编号不同（测试）。
2. 类型至少分图片、音频、文本、其它（测试）。
3. 媒体索引：按类型列出，可分页（测试）。
4. 拔卡、换卡：状态明确，旧编号查不到新卡的文件（测试）。
5. 报告写清楚索引放在哪、上限多少。
6. 新操作进能力表（G2，在代码里注册；生成的文档不提交）。全量测试和内存检查通过；产品代码不超过 250 行。
