# ui-fix-g：扫码结果、电源菜单和打断卡片照审计整改

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条（只在明显冲突处改界面，用我们的主题令牌和统一组件）。

用户 2026-11-11：“我要求每个功能和页面都参考现代UI的设计审计 我非常不满意 这些都要重新搞”。审计在 `docs/tasks/ui-audit-2/`；本任务做 `TASKS.md` 的 **UX07** 和 **UX17 里电源和打断的部分**（锁屏 `lock_mark.c` 那部分已由 `lock-faces-1` 做了，不要再改 `lock_mark.c`），照问题编号（V06～V08；S11～S18、S27～S32、Z06、Z07，详见 `PAGES.md`、`PAGES-SYSTEM.md`、`PAGES-APPS.md`、`COMPONENTS.md`）。

## 要做的

1. **UX07 扫码结果**（`system/apps/camera/scan.c`）：网址不再说“设备没有浏览器”；有现成的网页能力（`remote.open` 等，先确认存在）就用它打开，没有就明确说限制；结果静态分页；失败时保留重新扫的路径。`camera-fps-1` 已合入（取景会话常开），扫码用同一个会话，不新增识别线程；不实际联网或拨号。
2. **UX17 电源和打断**（`system/ui/shell/power_menu.c`、`power_pages.c`、`off_picture.c`、`interrupt_card.c`、`airplane_card.c`）：先做前三个直接影响使用的问题——不可点的东西不要画成按钮样子、电量为空时的文案不自相矛盾、“记录问题”等按钮不贴边；照 iOS/Android 的关机菜单：选项少、主次分明、危险操作（关机、重启）有区分。保持现有唤醒和打断行为不变。

## 范围和预算

- 只改上面列的文件、本任务专用的测试（新增测试文件或这几个页面已有的测试）和截图基线、`assets/i18n/en.tsv`（只加本任务新文案的英文）、`docs/tasks/ui-fix-g/`。不改 `lock_mark.c`、共享主题、`textfield.c`、`keyboard.c`、电源服务和驱动、公共测试文件；别的任务在改的文件不碰。
- 产品代码合计不超过 250 行；不新增机制、状态机、文件格式；不改睡眠、电源时序和刷新频率。

## 验收

1. 每项附常规和 120% 字号的前后截图（`docs/tasks/ui-fix-g/`，单张不超过 300 KB）；每项有针对性测试和撤掉修复会失败的反向对照。
2. 主工程普通全量 ctest 过（含 `ui-scale`）。
3. 报告逐项写对应的审计编号、改了什么、没改的低优先项。
