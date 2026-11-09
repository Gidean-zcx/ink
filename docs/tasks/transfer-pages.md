# transfer-pages：无线传入和连电脑的页面

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/transfer-pages/REPORT.md。

## 要做的

预算 300 行产品代码。后端已经有了，这次只做页面和入口：

- `Transfer`：从手机或电脑传入。显示二维码（手机扫）、电脑上打开的地址和一次性数字，写明“只能放到‘传进来的’。关掉这页就断开。”；按钮“开始”“关闭”。二维码用 LVGL 自带的 `lv_qrcode`（lv_conf 里已经打开），不要另引库。
- `TransferLive`：正在传，显示收到了几个、多大，按钮“断开”。传完回到 `Transfer` 或文件列表时，新文件在“传进来的”里能看到。
- `TransferUsbBusy`：要把存储卡给电脑，但有应用正在用卡：说清楚谁在用，先停下。
- `TransferUsbOn`：存储卡给了电脑，按钮“结束”；结束或拔线后重新整理文件。
- `TransferUsbFail`：电脑读不了卡时如实说，**不自动格式化**。
- 入口：文件应用首页（system/apps/files/files.c）加“从手机或电脑传入”和“连电脑”两行，按 FileList 画板的位置。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 Transfer、TransferLive、TransferUsbBusy、TransferUsbOn、TransferUsbFail、FileList；规格 docs/product/system-ui-spec.md 第 7.9 节。
- 无线传入：system/services/file/transfer.c（`transfer.start`、`transfer.status`、`transfer.stop`，一次性数字、只放“传进来的”、10 分钟没动静断开、输错三次关闭）。
- U 盘：文件服务的 `usb.status`、`usb.mount`、`usb.unmount` 和事件 `usb.changed`、`usb.refused_files_in_use`、`usb.disk_stopped_bad`。
- 页面写法参考 system/apps/files/files.c 和 system/ui/shell/backup_pages.c。

## 边界

- 不改 transfer.c 和 USB 后端的行为；发现后端缺什么，写进报告，最多补一个只读的状态字段。
- 连电脑要大人确认（D-72），照现有确认规则；孩子看不到这两行。
- 同时在做的有 settings-b（修复中）、kid-2、remote-app、activity-card、fw-f8、storage-usage，不要改它们的文件；共用文件只在末尾加行，files.c 里只加入口那两行和最少的接线。

## 验收

1. `Transfer` 显示的地址和数字来自 `transfer.status`；关页后服务断开（测试）。
2. 模拟一次传入：`TransferLive` 显示进度，传完文件出现在“传进来的”，文件应用里能看到（测试）。
3. 有应用占着卡时进 `TransferUsbBusy`，停下后进 `TransferUsbOn`；结束后文件重新整理；读不了卡进 `TransferUsbFail` 且没有格式化（测试，反例：模拟坏卡，确认没有任何写入）。
4. 孩子看不到入口，直接调用被拒（测试）；五页都有 AI 快照，按钮和 AI 走同一个能力。
5. 全量测试和内存检查通过；产品代码不超过 300 行；截图留在构建目录。
