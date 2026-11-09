# thumb-wiring：缩略图接上文件编号（file-index 的收尾）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足。报告写到 docs/tasks/thumb-wiring/REPORT.md。

## 要做的

预算 120 行产品代码。把 file-index-2 的缩略图（system/services/media/，缓存键是“对象编号＋版本”）和 file-index-1 的稳定文件编号接起来：给一个文件编号就能拿到缩略图；文件内容变了（大小或修改时间变了），版本跟着变，旧缩略图不再用；文件删了，缩略图跟着清。做成能力（例如 `file.thumb`），界面和 AI 走同一条路。

## 从哪里看起

- docs/tasks/file-index-1/REPORT.md、docs/tasks/file-index-2/REPORT.md，以及两边的代码（system/services/file/、system/services/media/）。

## 边界

- 不改解码和抖动本身，不做相册页面。
- 同时在做的还有 contacts、clock-alarm、ui-file-picker、ai-chat，不要改它们的文件；共用的列表（CMake、能力注册）只在末尾加行。

## 验收

1. 按编号取缩略图；改名、移动后，同一编号还能取到（测试）。
2. 文件内容变了，旧缩略图不再用；文件删了，缩略图被清掉（测试）。
3. 缓存总量的上限仍然有效（测试）。
4. 新操作进能力表；全量测试和内存检查通过；产品代码不超过 120 行。
