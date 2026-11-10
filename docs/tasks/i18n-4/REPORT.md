# i18n-4 执行报告

基线：`4eabca66395ff1da6610526d79e1c3dfde9cc98b`。工作分支：`claude/codex-i18n-4`。本报告随里程碑更新。

产品代码目前新增 292 行、删除 256 行（按基线 diff 统计，包含构建文件，不含测试、文档和译文表），预算 300 行。为什么不能更小：需要覆盖 31 个服务、策略文件的既有文字；已将确认模板、设置回执和无线传输集中翻译，未增加产品机制。农历改用英文数字日期，日历仅加一处短日期适配，字号不变。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 服务、策略英文及中文回切、排版、缺译计数 | 部分实测通过，继续补查 | 【实测】新增服务测试通过，10 张实页截图，缺译计数为 0；原有 i18n 两项通过 | `ctest --test-dir build -R '^i18n-(services\|tables\|lang)$' --output-on-failure -j2` |
| 2 原有测试、全量测试、内存检查、S31、gcccheck | 验证中 | 首轮全量 348 项，唯一失败为已修复的源码行号检查；内存、S31 和 gcccheck 待运行 | 待记录 |
| 3 文件迁移计数、保留中文原因、英文截图 | 进行中 | 正在清点 | 待记录 |

| 文件 | 新迁移键数（文件内去重） |
| --- | ---: |
| `system/policy/cap.c` | 2 |
| `system/policy/confirm.c` | 5 |
| `system/policy/members.c` | 10 |
| `system/policy/settings.c` | 4 |
| `system/policy/system_actions.c` | 14 |
| `system/services/ai/ai_service.c` | 20 |
| `system/services/audio/audio_service.c` | 15 |
| `system/services/audio/media_service.c` | 4 |
| `system/services/backup/backup_service.c` | 19 |
| `system/services/bt/bt_service.c` | 2 |
| `system/services/camera/camera_service.c` | 1 |
| `system/services/comm/comm_service.c` | 31 |
| `system/services/data/data_caps.c` | 1 |
| `system/services/display/light.c` | 2 |
| `system/services/file/file_caps.c` | 5 |
| `system/services/file/file_service.c` | 8 |
| `system/services/file/transfer.c` | 26 |
| `system/services/hub/pairing.c` | 13 |
| `system/services/log/log.c` | 4 |
| `system/services/net/net_service.c` | 7 |
| `system/services/net/wifi.c` | 4 |
| `system/services/notify/notify_service.c` | 25 |
| `system/services/power/power_service.c` | 8 |
| `system/services/res/task.c` | 1 |
| `system/services/text/text_caps.c` | 7 |
| `system/services/text/text_service.c` | 1 |
| `system/services/text/voice_input.c` | 11 |
| `system/services/time/lunar.c` | 36 |
| `system/services/time/time_caps.c` | 4 |
| `system/services/time/time_service.c` | 3 |
| `system/services/update/update_service.c` | 18 |

计数为新增的字面量键；无线传文件为经集中翻译入口处理的 26 个键。确认卡显示标题另在显示时翻译，能力表原文不改。`datastore.c`、`interrupt.c`、`notify_caps.c` 无需新迁移；`pinyin.c` 的汉字是输入内容，保留。其它服务文件没有待迁移中文。

## 没做到的

尚未声称整个系统界面都已英文。截图中确认卡标题、拒接、闹钟电源键说明、低电量专页和公历格式仍来自范围外的 UI 文件；更新、备份等全部状态页面尚无逐页英文截图。服务生成文字的迁移和代表性页面已验证。

## 和设计不一致的

【实现】低电量弹层由 `system/ui/shell/power_pages.c` 重新生成标题和正文，覆盖服务提供的文字；该界面本身仍是中文，属于界面迁移范围。本任务验证服务通过 `interrupt.list` 提供英文，保留截图并明确此边界。

## 改过的已有测试

- `tests/log/log.c`：独立日志测试增加中文直通的 `ink_tr` 桩；未改断言或期望。
- `tests/unit/lunar_test.c`：独立农历算术测试增加中文语言和翻译桩，原有日期、生肖、干支期望不变。
- `tests/i18n_tables.py`：数组结束符按独占一行的 `};` 识别。无线传文件 HTML 内的 JavaScript 也含 `};`，原解析误截断为 15 条；未改数量、排序或坏表断言。

## 要用户定的

暂无。已存通知按生成时的语言保留，语言切换后新通知使用新语言。

## 顺手发现

【实测】首次全量测试的源码片段检查发现 `settings.c` 新增头文件使文档行号落到空行；将该头文件放进原有空行，保留原行号，检查恢复通过。

【实现】确认卡、来电、闹钟等周边 UI 还有自行生成的中文（如拒接、确认卡标题、闹钟电源键说明）；服务提供的文案已经迁移，完整界面英文须由界面任务合并。

## 新增的机制

无产品机制；沿用现有 `TR()` / `TR_NOOP()` 和语言设置。新增的反向对照编译开关只用于测试。模块边界、原子能力和权限规则保持原样，按钮和 AI 仍调用相同能力。

## 要改的文档

主会话合并时更新任务进度，并汇总界面迁移的剩余中文。共享文档未改。
