# semantics-shell-1：确认卡、打断卡、电源菜单、家人门禁、锁屏标记、关机画面迁到控件语义

总规则先读 docs/tasks/codex-rules.md，第 7、8 节优先。报告写到 docs/tasks/semantics-shell-1/REPORT.md。

## 背景

页面给 AI 的快照应该由控件语义自动导出（F1，写法见 `docs/system/app-framework.md`、`docs/system/app-framework-design.md`，实现在 `system/ui/ui_semantics.c`，`ink_ui_page_spec_t` 加 `ink_ui_semantics_snapshot`）。缺口检查 `docs/tasks/phone-gap-2/REPORT.md`（A05，第 36、109、110 行）查出还有 12 个系统壳文件在手写快照（`grep '.snapshot ='`）。手写快照容易和界面脱节，也绕开了语义合同里的规矩：秘密只给“有没有”，动作要核对版本。

## 要做的

预算：产品代码净增不超过 250 行，迁移后能删的手写代码要删掉。

范围：`system/ui/shell/confirm_card.c`、`interrupt_card.c`、`power_menu.c`、`member_gate.c`、`system/ui/lock_mark.c`、`system/ui/shell/off_picture.c`。

1. 每个页面改成用 `ink_ui_page_spec_t` 描述字段和动作，快照由语义导出，删掉手写的快照函数。
2. 秘密（PIN、密码框内容、通话按键）只给“有没有”，照语义合同的规矩。动作带版本，过期的点按被拒，和其他已迁移页面一致。
3. 中枢看到的快照契约不变：字段名、动作名和必要的结构照旧，hub 的契约测试照原样通过。有必须改的，逐条列进报告并说明原因。
4. 规则第 8 节：页面只在真正影响这一页的事件上重画。迁移时顺手检查这些页面的事件订阅，发现整页重画会丢点按的，记进报告；属于这一项的就修。
5. 不改 `ui_semantics.c` 的公共接口；确实缺能力时，写进报告的“要用户定”，先用最小的办法绕过。

## 验收

1. 范围里的文件都没有手写快照了（`grep '.snapshot ='` 对照迁移前的清单）；每个页面有一个测试：快照由语义导出，秘密只给有没有，过期动作被拒。
2. 原有测试照原样通过，期望不改；hub 契约测试通过；全量测试和内存检查通过；S31 能编过；gcccheck 通过。
3. 报告列出每个文件迁移前后的行数。
