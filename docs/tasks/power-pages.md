# power-pages：电量和关机画面（power-update-pages 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/power-pages/REPORT.md。

## 要做的

预算 250 行产品代码：`BatteryLow`（低电量提醒）、`BatterySaving`（省电模式）、`Charging`（充电中）、`PowerOffEmpty`（电量耗尽的关机画面）。接电源服务（`battery.level`、`battery.low`、`battery.charging` 等状态和省电设置）。

## 从哪里看起

- 这四页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 电源服务 system/services/power/；现有的关机画面和锁屏（system/ui/shell/off_picture.c、system/ui/lock_mark.c）；打断卡片 system/ui/shell/interrupt_card.c（低电量走打断通道）。

## 边界

- 只做这四页；升级、备份和恢复的页面是后面的任务。
- 新页面放新文件；共用的文件只在末尾加行。同时在做的有 phone-calls、sms、settings-a、memo，以及在修的 clock-alarm、ai-chat，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟器里电量降到阈值时弹出低电量提醒；插上电显示充电中；省电模式的开关生效；电量耗尽显示关机画面（测试）。
2. 每页有 AI 快照（测试）。
3. 全量测试和内存检查通过；产品代码不超过 250 行。
