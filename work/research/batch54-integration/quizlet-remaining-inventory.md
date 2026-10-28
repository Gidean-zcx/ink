# Quizlet 剩余“仅目录”条目本地盘点

核查日期：2026-10-27。任务范围：只读核对既存目录、`evidence.jsonl` 和 `work/quizlet-controls-deepening`；未联网，未修改正式资料。

## 结论

- 原目录共有 68 行，其中 `child_topic` 64 行；严格按 `coverage == 仅目录；正文未在本例展开` 筛得 21 行。
- 21 项的 `article_url` 均为空。
- 21 项在 `docs/research/interaction-library/evidence.jsonl` 中均无“标题字段完全相同”的证据记录。
- 已保存的 16 个 `parsed/articles-*.txt` 正文，其一级标题均不与这 21 项相同。
- 21 个标题虽然可在章节目录、类别目录或其他文章的“Related Articles”中出现，但这只是链接文字；本地捕获没有保存这些链接的目标 URL，不能据此认定正文已读，也不能仅凭标题拼接 URL。
- 因此，这 21 行应继续解释为“本目录未更新且在本次指定本地范围内没有同题正文”；不能扩大为“全项目或互联网范围从未读取”。

## 逐项结果

| order | child_topic | CSV article_url | 同题 evidence | 同题已存正文 | 本地明确文章 URL |
|---:|---|---|---|---|---|
| 3 | Studying with Study Groups | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 5 | Becoming a top creator on Quizlet | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 7 | What are flashcard sets? | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 9 | Adding images to sets from the free gallery | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 10 | Adding your own images to sets | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 11 | Recording custom audio | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 14 | Creating diagram sets | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 21 | Rating a set | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 22 | Changing a rating on a set | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 23 | Viewing your sets' ratings | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 26 | Finding flashcard sets | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 27 | Finding your teacher's class | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 28 | Navigating your home feed on mobile devices | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 29 | Trouble accessing a set | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 30 | Sharing sets and folders | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 31 | Changing a set's editing permissions | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 33 | Protecting a set with a password | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 35 | Embedding sets | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 37 | Dropping a class | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 67 | Organizing study content with tags | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |
| 68 | Organizing study content with folders | 空 | 无 | 无 | 无；仅有目录/相关文章链接文字 |

## 已保存正文交叉核对

以下 16 个正文一级标题是本地实际保存的文章正文；它们均不属于上表 21 项：

- `work/quizlet-controls-deepening/parsed/articles-18312306436365.txt`：Studying with Study Guides
- `work/quizlet-controls-deepening/parsed/articles-25946589648013.txt`：Studying with Practice Tests
- `work/quizlet-controls-deepening/parsed/articles-360029004272.txt`：Removing sets from your Recent activity feed
- `work/quizlet-controls-deepening/parsed/articles-360029768912.txt`：Editing draft sets
- `work/quizlet-controls-deepening/parsed/articles-360029977151.txt`：Creating sets by importing content
- `work/quizlet-controls-deepening/parsed/articles-360030171011.txt`：Deleting a set
- `work/quizlet-controls-deepening/parsed/articles-360030212131.txt`：Editing sets
- `work/quizlet-controls-deepening/parsed/articles-360030255191.txt`：Changing a set's visibility
- `work/quizlet-controls-deepening/parsed/articles-360030642972.txt`：Studying with Test
- `work/quizlet-controls-deepening/parsed/articles-360030645752.txt`：Studying with Spell mode
- `work/quizlet-controls-deepening/parsed/articles-360030990531.txt`：Studying with Write mode
- `work/quizlet-controls-deepening/parsed/articles-360031172312.txt`：Studying most-missed terms with stars
- `work/quizlet-controls-deepening/parsed/articles-360048313652.txt`：Using grading options (US)
- `work/quizlet-controls-deepening/parsed/articles-360048314692.txt`：Setting up a study path
- `work/quizlet-controls-deepening/parsed/articles-360048803491.txt`：Using Progress for targeted studying
- `work/quizlet-controls-deepening/parsed/articles-360048806511.txt`：Resetting study path progress

## 检查文件

- `docs/research/interaction-library/batches/quizlet-controls-deepening-inventory.csv`：SHA-256 `9ec5054a71e6a0afd70c728546afb3fd1bd68cab3b16fa12e9869f8ea0d3e90c`
- `docs/research/interaction-library/evidence.jsonl`：SHA-256 `c3b9dac706270080691f5f6dca0a0c408dada29275627e6250c79166af6932cf`
- `work/quizlet-controls-deepening/`：递归检查 42 个文件，其中 16 个 `parsed/articles-*.txt`。

## 命令与退出状态

- `python3`：检查三个目标路径是否存在、文件大小及工作目录文件数；退出码 0。
- `python3`：读取 CSV 表头、行数与 `coverage` 分布；退出码 0。
- `python3`：列出 `work/quizlet-controls-deepening` 文件；退出码 0。
- `rg -n ... work/quizlet-controls-deepening/parsed/articles-*.txt | head -120`：定位正文标题及目录型提及；退出码 0。
- `python3`：筛选 21 行，提取 16 个正文一级标题，并按标题字段核对 `evidence.jsonl`；退出码 0。
- `cat` / `head` / `tail`：检查三个目录捕获及原始 web-tool 响应的保存格式；退出码 0。
- `python3`（本命令）：生成本报告，并断言剩余条目数为 21、同题证据和同题正文命中均为 0；退出码 0。
- `python3`（首次复核）：确认表格 21 行与报告 SHA，但错误断言批次目录仅有本报告；因根代理已并行创建 `before-manifest.json` 和 `turn-audit.json`，退出码 1。未产生文件修改。

## 未知与限制

- 本地目录捕获只保留链接编号和可见标题，没有链接编号到目标 URL 的映射；不能从现有文件恢复21篇文章的确切URL。
- 未核对网络当前状态，也未验证文章是否改名、迁移、合并或下线。
- `evidence.jsonl` 的核对采用标题字段完全匹配；正文文件核对采用一级标题完全匹配。其他产品资料中若以不同标题描述相近功能，不在本任务的“同题正文”判定范围内。

## 状态变化

- 仅新增本报告 `work/research/batch54-integration/quizlet-remaining-inventory.md`。
- 正式资料、目录 CSV、证据索引和既存抓取文件均未修改。
