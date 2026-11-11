# lock-faces-2：把锁屏表盘接通——设置里能选、分钟和跨日自动重画、翻译补齐

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条。

本任务从分支 `lock-faces-1-work` 开始（= `lock-faces-1` 的成果 + 最新主分支）。`lock-faces-1` 已画好简约时钟、待机时钟、月历（农历、今天反白）、待办和继续阅读小组件、透明壁纸叠加（见 `docs/tasks/lock-faces-1/REPORT.md`、`SCREENSHOTS.md`），但受文件范围限制没有接通。本任务把它做成用户能用的功能。

## 要做的

1. **设置里能选**：`display.lock_style` 的定义在 `system/ui/shell/navigation.c`（约 1999 行），设置行在 `system/ui/shell/settings_family.c`（约 44、255 行，现在三档循环）。把新表盘加进这个设置的取值和名称；设置行改成点进去是一页带小预览的单选列表（照 iOS“锁屏”样式选择），不是循环切换；小组件（待办、继续阅读）的开关放同一页。人和 AI 用同一个 `settings.set`。
2. **自动重画**：表盘的分钟变化和跨日（月历）只在内容变了时局部重画，挂在现有时间服务的事件上（不新增定时器或常驻循环）；锁屏所有者变了（换家人）重建。`lock-faces-1` 的跨日反向对照没跑完（等新日期超时），修好让它在模拟时间里触发并按预期失败/通过。
3. **翻译和能力表**：给 `lock-faces-1` 留下的 31 处中文加 `TR`/`TR_NOOP`，在 `assets/i18n/en.tsv` 补英文；重新生成 `docs/system/capabilities.md`；让 `i18n-literals`、`i18n-shell-remaining`、`i18n-apps`、`capability-table-doc` 通过。
4. **节气**：现有 `system/services/time/lunar.c` 没有节气接口。如果能在 `lunar.c` 里用成熟的公开数据（例如 1900～2100 年的节气表，许可证写清楚）加一个查询接口，就在日历表盘上显示当天节气；没有许可清楚的来源就照实写缺口，不做近似公式冒充。

## 范围和预算

- 只改：`system/ui/lock_faces/**`、`system/ui/lock_mark.c`、`system/ui/shell/wallpaper.c`、`system/ui/shell/navigation.c` 和 `system/ui/shell/settings_family.c` 里锁屏设置的那几处（这两个文件别的任务刚改过，只动锁屏相关的行）、`system/services/time/lunar.c`（只加节气查询）、`assets/i18n/en.tsv`、生成的 `docs/system/capabilities.md`、锁屏测试和截图基线、`docs/tasks/lock-faces-2/`。
- 胶水新增不超过 300 行；不新增常驻任务、定时循环、文件格式。

## 验收

1. 测试：在设置里选每种表盘后锁屏显示对应表盘；AI 用 `settings.set` 选表盘结果一致；分钟变化只刷时间那块，同一分钟不刷；跨日月历重画（反向对照能按预期失败）；换家人重建、孩子看不到大人的私密待办；`lock-faces` 原有 343 条断言照样过。
2. 主工程普通全量 ctest 过（含 `ui-scale`、各 i18n 检查、`capability-table-doc`）；S31 编译主会话合并时跑。
3. 截图：设置页的表盘选择、每种表盘常规和 120%（`docs/tasks/lock-faces-2/`，单张不超过 300 KB）。
