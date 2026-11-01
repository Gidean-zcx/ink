# Batch49 紧急存储盘点

日期：2026-10-27。范围仅 `/home/workspace/reader`；本轮未删除、压缩或修改任何既有文件。

## 最少且最有效候选

若根代理决定执行清理，单一路径即可明显超过当前紧急缺口：

- `work/research/batch38-week/desktop-profile/Default/Cache/`
  - 83 个文件；文件 `st_size` 合计 **26,318,084 bytes**。
  - 文件块合计 **26,501,120 bytes**；`du -B1 -s`（含目录块）为 **26,517,504 bytes**。
  - `rg -F` 未发现仓库内对该精确路径的引用；`git ls-files` 无输出，未被 Git 跟踪。
  - batch44 审计已将该目录列为 Chromium 纯派生缓存，明确要求保留同一 profile 的 Cookies、Local Storage、IndexedDB、Preferences 和会话状态。候选仅限这个 `Cache/` 目录，不扩大到其父目录。

这是最小路径数（1）且足以恢复空间的候选。盘点时可用空间先为 8,871,936 bytes，后复核时为 4,128,768 bytes；变化来自本轮之外的并行工作。本轮没有执行清理。

## `before` 同 SHA 副本

当前逐文件将 `*/before/<相对路径>` 与仓库根同路径文件比较：先比较长度，再流式计算双方 SHA-256。结果为 **40 文件 / 16,725 bytes**，其中有效字节全部来自以下五份相同 `AGENTS.md`：

- `work/research/batch39-integration/before/AGENTS.md`
- `work/research/batch40-integration/before/AGENTS.md`
- `work/research/batch41-integration/before/AGENTS.md`
- `work/research/batch42-integration/before/AGENTS.md`
- `work/research/batch43-integration/before/AGENTS.md`

每份 3,345 bytes，均与正式 `AGENTS.md` 的 SHA-256 `566d6da6db07d26f8d15ae3031db746e96bb6d304d40694786c64f4db795c9ef` 相同。其余 35 项均为空文件，SHA-256 为 `e3b0c442...b855`，释放价值为零。该组可按单文件恢复，但总量不足以解决本次紧急空间。

复核办法示例：

```bash
sha256sum AGENTS.md \
  work/research/batch{39,40,41,42,43}-integration/before/AGENTS.md
```

不要整删任何 `before/`：未匹配当前正式文件的内容可能是唯一历史基线。

## 既有归档与引用

`work/research/batch44-storage-audit/cleanup-result.json` 记录：3,067 份同正式资料的副本（原始 40,818,534 bytes）已归档后从展开快照移除；归档为 `redundant-before-copies.tar.gz`，大小 9,577,250 bytes、3,067 members。现场复核归档 SHA-256 为 `d824d249db77f526656ebe5ba1b3bdec9f5ae865eb0718e37357c60cdc37f3a0`，与记录一致，`tar -tzf` 可正常列目录。

正式 `docs/research/interaction-library/verification-log.md:978` 明确引用该归档和日志。因此不把归档本身列入最小清理候选；移除会让正式引用失效，并丢掉当前可能已与后续正式版本不同的历史快照。batch44 的安全方法是：只按清单选取当时与正式文件同 size、同 SHA 的单文件，先建归档并逐成员核验，再移除展开副本；禁止整目录删除。

## 命令、退出状态与证据

以下命令均 exit 0：

1. `cat AGENTS.md`；并读取 `README.md`、`docs/requirements.md`。
2. `du -a work | sort -nr | head -80`、`df -B1 .`：检查各层占用和实时可用空间。
3. `find work/research/batch44-storage-audit ...`、读取 `report.md`、`cleanup-result.json`、`build_audit.py`：核对既有方法与执行结果。
4. Python 只读扫描全部 `work/**/before/*`，按对应正式路径比较 size 和流式 SHA-256：40 项 / 16,725 bytes。
5. Python 统计八个既有 Chromium 缓存目录的 `st_size`、`st_blocks`、文件数；`du -B1 -s`复核目录块占用。
6. `rg -F <精确Cache路径> .`：无匹配（命令以 `|| true` 汇总，`rg` 本身无匹配语义为 1）；`git ls-files -- <Cache路径>`：无输出。
7. `sha256sum ...redundant-before-copies.tar.gz`、`tar -tzf ... | wc -l`：哈希吻合、3,067 members。

早先一次大范围 `rg` 输出因命中大量证据记录而被工具截断，但命令 exit 0；未据该截断输出作候选判断，随后均用精确路径复核。

## 未知与状态变化

实际释放量由文件系统记账决定；预计接近 `du` 的 26,517,504 bytes。未启动 Chromium 实机验证缓存删除后的首次加载行为，但目录语义和 batch44 既有审计均支持其可再生性。没有检查或触碰工作区之外的数据。

状态变化仅新增本报告 `work/research/batch49-integration/storage-inventory.md`；没有修改任何既有文件。
