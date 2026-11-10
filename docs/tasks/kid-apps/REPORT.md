# kid-apps 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 名单 | 【实测】通过 | 移除时钟后，主页及所有“全部”页无入口；AI、触摸来源、直接应用能力拒绝并解释原因；两个孩子独立 | `tests/kid_apps.py --build build` |
| 1.2 时段 | 【实测】通过 | 普通应用到 KidTimeUp；标为可用的时钟可开，电话和短信保留 | 同上 |
| 1.3 日历边界 | 【实测】通过 | 周一 21:00→周二 06:59；07:00 解除；周二 08:00–16:00；未选星期不限制 | 同上 |
| 1.4 叠加 | 【实测】通过 | 一分钟日限额耗尽后，时段豁免的时钟仍被拒绝 | 同上 |
| 1.5 家长设置 | 【实测】通过 | 真实按钮经家长密码修改应用/星期；孩子触摸、AI、确认来源均不能改；不能读兄弟姐妹规则 | 同上 |
| 1.6 重启 | 【实测】通过 | 独立进程保留应用模式、两段时间及星期选择，并继续执行限制 | 同上 |
| 2.1 原有用例 | 【实测】通过 | kid-1～kid-3、成员、主页原样通过；备份恢复等回归通过 | 普通全量，见下文 |
| 2.2 普通全量 | 【实测】317/318 | 唯一失败为共享能力总表过期；未跳过或放宽测试 | `ctest --test-dir build --output-on-failure -j2` |
| 2.3 内存检查 | 运行中 | ASan + UBSan 全量 | `ctest --test-dir build-asan --output-on-failure -j2` |
| 2.4 S31 | 待编译 | 只编译，不连接设备 | `bash ports/esp32s31/build.sh` |
| 2.5 GCC | 【实测】本次 8 文件通过 | 扩展检查的两个既有测试提示见“顺手发现” | `gcccheck.py build`，文件列表见日志 |
| 3 截图与快照 | 【实测】通过 | 两个新页面有语义快照；中英四张截图已查看，路径见下文 | 专项设置 `INK_KEEP_EVIDENCE=1` |

## 没做到的

- 按总规则第 7 节不改共享生成文档，因此普通全量仍有 `capability-table-doc` 一项失败；没有达到全量零失败。能力表已生成到构建目录，供合并更新。
- ASan/UBSan、S31 进行中；未连接设备，也未在 Linux 宿主运行。

## 和设计不一致的

- 两个页面无画板（D-117），复用已有列表、翻页、确认和密码组件；没有修改设计文档或声称做过画板分数对比。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记六个新能力及其必填参数，保留完整覆盖校验。
- `tests/settings_b.c`：旧“阅读、AI、主页”文字概要已被真实应用名单入口取代；改为验证入口、实际名单中的阅读/电话及电话时段豁免，另验证返回。未删除其它断言。
- `tests/files_app/files_app.c`、`tests/transfer_pages/device.c`：保留所有文件和成员隔离断言；仅增加真实家长确认后开放文件应用的前置条件。新默认名单与原孩子主页一致，默认不开放文件。公用测试前置条件在 `tests/child_app_fixture.h`，不改产品权限。

## 要用户定的

- 时段数量暂定 4 段；界面每次点按将时刻增加半小时，能力支持任意分钟，星期逐个选择。步进未增加配置项；是否改成时刻选择器留给评审。

## 顺手发现

- 扩展 `gcccheck` 到 11 文件：退出 1，`gcccheck: 11 compiled, 2 with findings`。`tests/settings_b.c:213,235` 的 `%u` 与 `tests/files_app/files_app.c:220` 的 `sscanf` 在 S31 GCC 的 `uint32_t=unsigned long` 类型下提示不匹配；对应语句在基线已存在。本次未改这些语句，未关闭警告。日志 `build/kid-apps-gcccheck-final.log`；本次产品及新增测试 8 文件检查退出 0，日志 `build/kid-apps-gcccheck-product.log`。
- 复用的 KidTimeUp 仍沿用“明天接着来”的既有文案，未细分白天禁用时段的解除时间。

## 新增的机制

- 成员记录增加可选策略尾部：保存应用模式和禁用时段，任务 1、2 及重启验收要求；兼容旧长度，未配置时维持旧长度。
- 配置：每应用“不开放／可用／禁用时段也能用”，任务 1、2 要求；必需入口不受名单影响。
- 配置：每个孩子 4 段本地时刻及星期选择，任务 2 要求；跨午夜按开始日的星期计算。
- 无新增任务、线程、状态机、后台重试、缓存、队列或独立文件格式；复用现有分钟检查点和软总线能力。

## 要改的文档

- 能力表须重新生成到构建目录，合并时更新共享的 `docs/system/capabilities.md`。
- 合并时在 `docs/system/ui-dependencies.md` 标记 SetKid 两个页面；本分支遵守规则不改共享文档。

## 截图路径

- `build/kid-apps-nnju1ozj/policy/snap-SetKidApps.png`
- `build/kid-apps-nnju1ozj/policy/snap-SetKidPeriods.png`
- `build/kid-apps-nnju1ozj/locale/snap-SetKidAppsEnglish.png`
- `build/kid-apps-nnju1ozj/locale/snap-SetKidPeriodsEnglish.png`

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
- 【实测】第二轮普通全量原话：`99% tests passed, 1 tests failed out of 318`；903.21 秒，退出码 8。唯一失败 `capability-table-doc`，按任务规则不修改共享生成文档。`build/kid-apps-ctest-final.log` 及 CTest 的完整日志可复核；kid-1～kid-3、成员、主页、备份等均通过。
- 【实测】补齐应用名单中的“接下来”英文标题，专项新增英语页面阶段：policy 205、restart 12、locale 33，共 250 条断言通过；逐页检查所有应用/星期的译文计数，未新增缺译文。最新中英文截图与证据在 `build/kid-apps-kug3638c`，四张截图均已查看。译文及 CMake 也计入预算后为 293 行。
- 【实测】补齐时段例外的真实按钮路径：限时未耗尽时 Home/返回可去主页选择豁免应用；日限额耗尽时仍拦。复用已有卡片版本通知，仅在日额度耗尽状态变化时通知，处理“时段已禁用、随后日限额耗尽”的页面切换并避免通知回环；保留原模态页关闭规则。最新专项 policy 219、restart 12、locale 33，共 264 条断言通过，证据 `build/kid-apps-nnju1ozj`；最终产品差异含翻译、CMake 共 297 行。
- 【实测】原有 kid-1～kid-3 在最终补丁后再次原样通过：`100% tests passed out of 3`，52.77 秒，`build/kid-apps-kid-regression-final.log`。
- 【实测】最终五项反向对照失败数为 17、7、4、4、1；最后两项分别撤掉返回主页路径、撤掉叠加到时通知，均命中新增断言，所有变体正常执行到结束。`build/kid-apps-reverse-final.log`、`build/kid-apps-reverse/summary.json`。最终 8 文件 GCC 检查仍为 0 项发现。
- 【实测】首轮 ASan/UBSan 全量原话 `99% tests passed, 1 tests failed out of 318`，974.71 秒，仅文档失败。Home/通知补丁后的最终版本正在重新构建和复验。
