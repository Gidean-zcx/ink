# reminder-edit 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。分支：`claude/codex-reminder-edit`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测】 | 创建 327 项、重启 51 项检查，均零失败；真实触摸、通知提示音、改时、重复、删除、AI 互通和成员隔离 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/reminder_edit/run.py --build build` |
| 2 | 进行中 | 原有测试、全量、ASan、S31、gcccheck 待执行 | 待执行 |
| 3 | 通过【实测】 | `build/reminder-edit/add.png`、`detail.png`、`modified.png`、`picker.png`；逐张视觉检查，无裁切、遮挡 | 同上 |

## 没做到的

- 全量、ASan 和 S31 正在执行；未连接设备，当前证据均为模拟器或编译结果。

## 和设计不一致的

- 无。日期时间选择页在 `system/apps/calendar/calendar.c` 的 `editor_create`，复用既有圆形按钮和胶囊按钮，年、月、日、时、分分别选择；合并时可由主会话与 time-settings 统一。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】现有参数校验未落实 `minLength`；提醒入口已直接拒绝空内容，没有改通用校验器。

## 新增的机制

- 无新增任务、线程、状态机、持久格式或配置项。任务要求的 `reminder.update` 与六个编辑导航／保存能力复用既有能力表、短输入和页面机制。

## 要改的文档

- 能力表按任务重新生成到构建目录，合并时由主会话更新共享的 `docs/system/capabilities.md`。

## 执行记录

- 【实测】开工工作区干净，HEAD 与指定基线一致；已读总规则和任务书。构建及测试使用 `taskpolicy -c utility`，并行度不超过 2。
- 【实测】定向证据：`build/test-out/reminder-edit-mnifzdeu/`。提醒按既有设计走高优先级通知和短提示音，一次性提醒到点移除；测试核对两次 `reminder.fired`、通知正文和实际播放计数，没有改成闹钟浮层。
- 【实现】新增 `reminder.update` 和编辑页面的导航能力；保存仍走同一提醒提供者，按成员检查原 ID，沿用原存储格式和排程器。重复提醒的首次发生不早于选定日期。所有新界面字符串使用 `TR()`，补齐英文。
- 为什么不能更小：需要两个有 AI 快照的页面、短输入框、五项日期时间选择和四种重复选项；仅补能力或继续用语音入口不能满足手动编辑，直接输入时间字符串也不满足选择组件要求。没有另建选择控件框架或排程机制，产品代码仍低于 300 行预算。
- 【实测】反向对照：临时把三个产品 C 文件替换为指定基线、重编 `sim-reminder-edit` 后，`run.py --negative` 得到 `exit=1, pass=9, fail=1, finished=1`（缺手动入口）；随后完整恢复文件并重编成功。日志：`build/reminder-edit/reverse/{baseline-build,baseline-test,restored-build}.log`。该手动比较不注册进 ctest，不要求 CI 保留历史。
- 【实测】`taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/calendar/calendar.c system/services/time/time_caps.c system/services/time/time_service.c tests/reminder_edit/reminder_edit.c`：4 个文件、0 个问题；UI 尺寸检查 0 违规，翻译表校验通过。最终验证后还会核对结果。
