# update-pages：系统升级页面（power-update-pages 的第二块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/update-pages/REPORT.md。

## 要做的

预算 300 行产品代码：`Update`、`UpdateReady`、`UpdateBlocked`、`UpdateFail`、`UpdateInstall`、`UpdateDone`，以及 `BootRecovery`。接升级服务（system/services/update/）。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 升级服务 system/services/update/；现有的升级卡片 system/ui/shell/update_card.c。

## 边界

- 不改升级的校验、签名和回滚逻辑；缺的写进报告。不碰设备。
- 新页面放新文件；共用的文件只在末尾加行。同时在做的有 kid-1、ai-pages-2、confirm-pages、calendar、files-app-1，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟器里走一遍：有新版本 → 下载 → 准备好 → 安装 → 完成（测试）。
2. 电量或存储不够时出现 `UpdateBlocked`；失败时出现 `UpdateFail`，能重试；启动失败进 `BootRecovery`（测试）。
3. 每页有 AI 快照（测试）。
4. 全量测试和内存检查通过；产品代码不超过 300 行。
