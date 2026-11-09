# file-index-3：插卡后在后台把整张卡的索引建好，第一次建索引别那么慢

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/file-index-3/REPORT.md。

## 要做的

预算 250 行产品代码：

- 现在文件索引只在有应用查某个目录时（`file.index`）才顺手建，没人查就一直不全；存储页的分类占用（`file.usage`）因此在新卡上一直显示“正在整理”。改成：开机或插卡后，文件服务在后台低优先级地把 `/family` 和每个成员目录的索引建一遍，建完 `file.index_state` 变成 ready、发 `file.changed`，存储页自动出数。
- 建的时候不要一直拿着 `FS.edits`：一批一批做（例如每批几十个文件），批与批之间放开锁，让用户的文件操作能插进来；卡被拔掉或换了就停。
- 第一次建索引现在每个文件单独写一次盘、逐条查重（docs/tasks/backlog.md 性能一节记过）：改成一批一次写、查重用批内的现成结果，写盘次数降一个数量级。
- 已建好的卡（卡标识和索引版本都对得上）开机不重建。

## 从哪里看起

- system/services/file/file_service.c：`index_scan`、`index_open`、`fs_index_for`、`storage_usage`（storage-usage 刚改过，它只读索引、不扫卡）；索引存在数据存储里（`ink_db_*`）。
- 状态 `file.index_state`、事件 `file.changed`；存储页 system/ui/shell/settings_pages.c 已经订阅了 `file.changed`。

## 边界

- 不改索引的存储格式；不改 `file.usage` 的口径。
- 后台建索引不能让界面卡顿：用 BG 执行器、低优先级，单批持锁时间要有探针记录。
- 同时在做的有 privacy-records、kid-3、factory-pages、fw-f13、port-skeleton，不要改它们的文件。

## 验收

1. 新卡上开机后不开任何应用，过一会儿 `file.usage` 就有数，存储页从“正在整理”变成数值（测试）。
2. 建索引期间写一个文件不用等索引建完；单批持锁最长时间有探针数，写进报告（测试）。
3. 1000 个文件的卡，第一次建索引的写盘次数比现在少一个数量级（前后对比写进报告，测试里断言上限）。
4. 已建好的卡重启不重建；拔卡中途停下、插回接着建（测试）。
5. 全量测试和内存检查通过；产品代码不超过 250 行。
