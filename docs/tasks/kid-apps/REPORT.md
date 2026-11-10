# kid-apps 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 待验证 | 应用名单、禁用时段、权限、持久化 | 待补 |
| 2 | 待验证 | 原有测试、全量、内存检查、S31、gcccheck | 待补 |
| 3 | 待验证 | SetKid 两个新页面截图 | 待补 |

## 没做到的

- 验证完成后填写。

## 和设计不一致的

- 验证完成后填写。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记六个新能力及其必填参数，保留完整覆盖校验。
- `tests/settings_b.c`：旧“阅读、AI、主页”文字概要已被真实应用名单入口取代；改为验证入口、实际名单中的阅读/电话及电话时段豁免，另验证返回。未删除其它断言。
- `tests/files_app/files_app.c`、`tests/transfer_pages/device.c`：保留所有文件和成员隔离断言；仅增加真实家长确认后开放文件应用的前置条件。新默认名单与原孩子主页一致，默认不开放文件。公用测试前置条件在 `tests/child_app_fixture.h`，不改产品权限。

## 要用户定的

- 暂无；按任务书实施可逆的设置。

## 顺手发现

- 暂无。

## 新增的机制

- 待实现后列明，仅增加任务书要求的名单和时段设置。

## 要改的文档

- 能力表须重新生成到构建目录，合并时更新共享的 `docs/system/capabilities.md`。

## 验证记录

- 【实测】开工时分支为 `claude/codex-kid-apps`，工作区干净，HEAD 与指定基线一致。
- 【实测】第一里程碑：普通 `INK_WERROR=ON` 完整构建退出 0；专项 `tests/kid_apps.py --build build` 的 policy 203 条、restart 12 条断言通过。证据目录 `build/kid-apps-4vkft4gi`。
- 【实测】`gcccheck.py build` 检查 8 个相关 C 文件：`gcccheck: 8 compiled, 0 with findings`。能力表已生成到 `build/kid-apps-capabilities.md`：466 项定义。
- 【实测】首个提交的 C 源码/头文件 210 行新增、45 行删除；加翻译表 36 行和 CMake 5 行后，保守合计 296 行。超过 100 行的原因：同时覆盖每个孩子的持久规则、统一权限、日限额叠加和两个设置页。更小的“只隐藏入口”方案无法阻止直接能力调用；“只加名单”无法覆盖时段。复用成员记录、分钟检查点、列表和确认页。
- 【实测】反向对照撤掉名单、时段、持久化，分别命中 13、4、4 条失败断言，三个模拟器均正常完成；`build/kid-apps-reverse/summary.json`。
- 【实测】首轮全量原话：`98% tests passed, 7 tests failed out of 318`，耗时 691.43 秒。日志 `build/kid-apps-ctest-initial.log`。保留失败记录，不作为验收通过。
- 【实测】修正空策略记录也扩大的回归：未配置成员继续写旧长度，避免原来不会触发的后台压缩。备份 quick 七阶段全部通过；种子阶段的压缩次数从 3 降为 0，导出断言从失败恢复通过，证据 `build/backup-restore-8ow3er4g`。未改备份产品代码或测试。
- 【实测】名单拒绝只处理名单规则，原有能力权限回执保留；补齐与新默认名单相符的测试前置条件。六项复验（中枢、设置、拒绝回执、文件、传入页、本任务）全部通过，原话 `100% tests passed out of 6`，49.33 秒，日志 `build/kid-apps-regressions.log`。
- 【实测】第二里程碑产品差异：加翻译和 CMake 后保守合计 292 行；最终全量、ASan/UBSan、S31 待记录。
