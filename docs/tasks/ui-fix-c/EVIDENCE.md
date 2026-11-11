# 专用验证证据

使用 `taskpolicy -c utility`，不启动窗口、不使用真实服务器或设备。完整命令见 [测试说明](../../../tests/ui_fix_c/README.md)。

| 模式 | 场景 | 进程退出 | 通过断言 | 失败断言 | 正常结束标记 |
| --- | --- | --- | --- | --- | --- |
| before | panel-100 | 0 | 51 | 0 | 1 |
| before | panel-120 | 0 | 51 | 0 | 1 |
| before | records-100 | 0 | 36 | 0 | 1 |
| before | records-120 | 0 | 36 | 0 | 1 |
| before | gallery-100 | 0 | 37 | 0 | 1 |
| before | gallery-empty-100 | 0 | 14 | 0 | 1 |
| before | gallery-120 | 0 | 37 | 0 | 1 |
| before | gallery-empty-120 | 0 | 14 | 0 | 1 |
| after | panel-100 | 0 | 70 | 0 | 1 |
| after | panel-120 | 0 | 70 | 0 | 1 |
| after | records-100 | 0 | 67 | 0 | 1 |
| after | records-120 | 0 | 67 | 0 | 1 |
| after | gallery-100 | 0 | 64 | 0 | 1 |
| after | gallery-empty-100 | 0 | 16 | 0 | 1 |
| after | gallery-120 | 0 | 64 | 0 | 1 |
| after | gallery-empty-120 | 0 | 16 | 0 | 1 |
| reverse | panel-100 | 1 | 55 | 11 | 1 |
| reverse | panel-120 | 1 | 55 | 11 | 1 |
| reverse | records-100 | 1 | 49 | 9 | 1 |
| reverse | records-120 | 1 | 49 | 9 | 1 |
| reverse | gallery-100 | 1 | 45 | 13 | 1 |
| reverse | gallery-120 | 1 | 45 | 13 | 1 |

反向命令自身退出 0，表示六个被测基线进程均按预期退出 1 并触发断言；不是编译失败或中途崩溃。基线重编译只发生在构建目录，没有撤回或覆盖工作树中的修复。

关键反向断言：

- `panel-100`：
  - 10.220s CHECK FAILED (tests/ui_fix_c/panel.c:23) first page warns that the answer was cut
  - 10.220s CHECK FAILED (tests/ui_fix_c/panel.c:24) first page announces more pages
  - 10.220s CHECK FAILED (tests/ui_fix_c/panel.c:29) first replay label does not claim it was heard
  - 10.220s CHECK FAILED (tests/ui_fix_c/panel.c:30) AI sees the same truncation boundary
- `records-100`：
  - 5.530s CHECK FAILED (tests/ui_fix_c/records.c:32) receipt has actual setting name, result and object
  - 5.530s CHECK FAILED (tests/ui_fix_c/records.c:34) snapshot identifies whose records these are
  - 5.530s CHECK FAILED (tests/ui_fix_c/records.c:35) member name is visible at the page header
  - 6.820s CHECK FAILED (tests/ui_fix_c/records.c:41) expired receipt explains why undo is absent
- `gallery-100`：
  - 6.710s CHECK FAILED (tests/ui_fix_c/gallery.c:17) thumbnail press has a visible outline
  - 6.710s CHECK FAILED (tests/ui_fix_c/gallery.c:17) thumbnail press has a visible outline
  - 6.710s CHECK FAILED (tests/ui_fix_c/gallery.c:20) snapshot offers manual reload and camera actions
  - 9.850s CHECK FAILED (tests/ui_fix_c/gallery.c:33) current photo is identified by filename and album position

## 原测试的当前面板重链接检查

| 路径 | 退出 | 结论 | 断言 |
| --- | --- | --- | --- |
| ai-chat | 0 | 通过 | 105 |
| ui-answer-regressions-metalio-plus | 0 | 通过 | 106 |
| ui-answer-regressions-desktop-lcd | 0 | 通过 | 106 |
| ai-chat-snapshots | 0 | 通过 | 原脚本内检查 |
| ai-history-photo/replay | 0 | 通过 | 原脚本内检查 |

`ai-history-photo/replay` 原脚本报告 25 条断言通过；`ai-chat-snapshots` 原脚本报告五张 F1 快照通过。这些测试与断言文件未改。

GCC：`gcccheck: 3 compiled, 0 with findings`。UI 令牌：`ui-scale: 52 files, 0 violations`。

## 全量与构建边界

- 普通全量原话：`97% tests passed, 11 tests failed out of 401`；`Total Test time (real) = 1242.78 sec`；退出 8，三个中枢测试跳过。
- 普通构建、ASan 构建和 S31 配置均因缺少英文词条未完成；ASan/UBSan 运行测试为 0 项，S31 未编过。
- 五项重链接回归是当前面板检查，不等于最终全量通过。
- 词表补丁保存在任务目录；正式词表、共享生成能力文档保持任务要求的禁止修改范围。
