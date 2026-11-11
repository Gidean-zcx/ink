# ui-fix-f：天气和计算器、日历提醒、网页远程三处页面照审计整改

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条（只在明显冲突处改界面，用我们的主题令牌和统一组件）。

用户 2026-11-11：“我要求每个功能和页面都参考现代UI的设计审计 我非常不满意 这些都要重新搞”。审计在 `docs/tasks/ui-audit-2/`；本任务做 `TASKS.md` 的 **UX12、UX20、UX22**（它们等的输入框任务 `textfield-1` 已合入，先读合并后的代码，已经修好的行跳过），照各行的“具体交付和验收”和问题编号（E01～E07；W01～W06、Q01～Q03；D01～D06，详见 `PAGES.md`、`PAGES-APPS.md`、`COMPONENTS.md`）。

## 要做的

1. **UX12 网页远程**（`system/apps/remote/remote.c`）：正常页能看出域名；普通字段和秘密字段都显示要填给哪个网站；“已结束，重新打开”和“可以恢复，重新连接”分清；合成网页的输入不泄露秘密。不新增浏览器或会话机制。
2. **UX20 天气和计算器**（`system/apps/weather/weather.c`、`system/apps/calculator/calculator.c`）：天气的旧数据、更新时间和刷新动作能分辨（照 iOS 天气：城市、当前温度最大、更新时间小字）；计算出错时保留算式并说怎么改。不加定位、新运算模式或历史。
3. **UX22 日历提醒**（`system/apps/calendar/calendar.c`）：开始和结束写明对象名，全天和响铃的控件清楚，错误提示和底栏不互相遮挡，120% 字号和键盘弹出时都看得见；照 iOS 日历“新建日程”的分组表单。复用现有日期输入，不改公共日期选择器，不加新的提醒服务。

## 范围和预算

- 只改上面四个文件、`tests/remote_app/remote_app.c`、`tests/weather/weather.c`、`tests/calendar/calendar.c` 和本任务专用的计算器测试、截图基线、`assets/i18n/en.tsv`（只加本任务新文案的英文）、`docs/tasks/ui-fix-f/`。不改共享主题、`textfield.c`、`keyboard.c`、公共测试文件；别的任务在改的文件不碰。
- 产品代码合计不超过 300 行；不新增机制、状态机、文件格式。

## 验收

1. 每项附常规和 120% 字号的前后截图（`docs/tasks/ui-fix-f/`，单张不超过 300 KB）；每项有针对性测试和撤掉修复会失败的反向对照。
2. 主工程普通全量 ctest 过（含 `ui-scale`）。
3. 报告逐项写对应的审计编号、改了什么、没改的低优先项。
