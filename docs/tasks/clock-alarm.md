# clock-alarm：时钟和闹钟页面（memo-calendar-clock 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/clock-alarm/REPORT.md。

## 要做的

backlog 的 memo-calendar-clock 一节拆成几块，这是第一块，预算 300 行产品代码（查缺口报告 B14）：`Clock` 页面——闹钟列表（时间、名称、重复、开关），新建、编辑、删除；响铃时的稍后响和关掉。时间服务里闹钟表、重复规则、稍后响、响铃打断都已经有了，这里做页面和接线。

## 从哪里看起

- `Clock` 的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题（system/ui/theme.c）和比例为准。
- 时间服务 system/services/time/（time_service.c、time_caps.c）。
- 列表用 F4（`ink_ui_list`），名称输入用 F2，页面语义用 F1；报告在 docs/tasks/ui-semantics、ui-input-host、ui-list-paging。

## 边界

- 只做页面和接已有的时间服务能力。时间服务本身缺的写进报告，一两行能修的小错可以修。
- 秒表、倒计时、世界时钟不做（查缺口报告里是建议，还没定）。
- 页面放在自己的文件里；共用的列表（应用目录、CMake、能力注册）只在末尾加行。同时在做的还有 contacts、ui-file-picker。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 新建、编辑、删除、开关闹钟，重启后还在（测试）。
2. 重复（每天、工作日、自选星期）显示和保存都对（测试）。
3. 响铃时稍后响、关掉走已有的能力；页面和 AI 快照一致（测试）。
4. 全量测试和内存检查通过；产品代码不超过 300 行。
