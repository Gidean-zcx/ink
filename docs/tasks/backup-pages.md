# backup-pages：备份和恢复页面（power-update-pages 的第三块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/backup-pages/REPORT.md。

## 要做的

预算 300 行产品代码：`Backup`（备份状态、立刻备份）、`BackupFail`、`Restore`（选备份、预览）、`RestoreRun`（恢复中）、`RestoreDone`、`RestoreFail`。接备份服务 system/services/backup/（`backup.now`、`backup.status`、`backup.manifest`、`restore.preview`、`restore.apply`）。读预览时带上 `summary` 参数，只取创建时间和条目数：完整条目列表会超出界面里固定大小的 JSON 解析器。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 备份服务；开机配网里恢复那一页的写法（system/ui/shell/boot_flow.c 的 BootRestore）。

## 边界

- 恢复会覆盖现有资料，照现有的确认规则。
- 不改备份服务的格式和流程；缺的写进报告。
- 新页面放新文件；共用的文件只在末尾加行。同时在做的有 settings-b、camera-gallery、search，还有上一批在收尾的六个任务，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟器里走完 备份 → 恢复 的整个流程（测试）。
2. 断网、备份失败、恢复失败的分支都能到达，也能返回（测试）。
3. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
4. 全量测试和内存检查通过；产品代码不超过 300 行。
