# kid-1 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。复查修复提交：`b7ceac70`。状态：两项复查问题已修；两轮全量均已执行，唯一未通过项是共享能力表文档校验。上轮缺口保留。

## 第 1 轮复查修复（2026-11-09，已完成）

本次接续 `3b2f64aa` 的复查结果，只修 K1-R1（高）及 K1-R2（中）。【实现】页面保存现有成员代次，锁屏或身份变化时集中关闭表单、清除两份凭据；输入和提交也检查会话，异步失败结果不能重开已失效页面。等待结束复用 LVGL 的一次定时回调恢复普通提示，关页或重画时取消定时，不逐秒刷新。未改成员策略、共享文档或设计要求。

【实测】普通构建退出 0；定向测试七阶段共 1246 条断言通过（退出 0），包括真实电源键、121 秒虚拟等待、触摸选孩子、尝试替换成人 PIN，以及等待到期前后可见文本。命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/kid_1.py --build build`。日志：`build/kid-1-fix-targeted.log`；证据：`build/kid-1-0ekjfpjq/`。全任务产品代码按基线全部新增行计为 **275/300 行**，本轮只改一个产品文件。

【实测】反向对照分别用上述命令加 `--mutant session`、`--mutant expiry`，只在构建目录编译，均编译链接成功后退出 1：session 有 20 条失败，直接复现孩子替换 PIN 并进入成人空间；expiry 有 3 条失败，直接检查到期页面和提示未更新。session 中 8 条快照断言来自旧页面快照被拒绝为空，不是凭据泄露，其余为隔离断言及失效后续控件检查。日志：`build/kid-1-fix-session-mutant.log`、`build/kid-1-fix-expiry-mutant.log`；证据：`build/kid-1-1xmqjurp/`、`build/kid-1-nhppswc8/`。

【实测】普通及内存检查构建均退出 0，命令为 `taskpolicy -c utility cmake --build build -j2`，内存检查换 `build-asan`。沿用已配置的同一套依赖，均保留 `INK_WERROR=ON`，后者启用 `INK_SANITIZE=ON`；编译参数核对含 `-fsanitize=address,undefined`。构建日志：两目录中的 `kid-1-fix-build.log`。

【实测】两轮全量各执行 233 项、232 项通过，均退出 8；普通耗时 275.65 秒，内存检查耗时 449.06 秒。唯一失败均为 `capability-table-doc`，没有跳过。`kid-1`、成员唤醒及两项确定性测试均通过。命令见验收表第 5 项；日志：两目录中的 `kid-1-fix-full.log`。扫描内存检查全量日志和 `Testing/Temporary/LastTest.log`，sanitizer 错误为 0。两轮 `kid-1` 均为 1246 条断言，最低内部 RAM 余量均为 47,840 字节（下限 32,768）。全量中的功能证据：`build/kid-1-9ew7rvo2/`、`build-asan/kid-1-5of6u4_b/`。

【实现】新页面在 `system/ui/shell/member_pages.c`；成员门只接入口，PIN 的哈希格式、尝试次数和等待策略未改。新增产品代码保守按全部新增行计为 275 行（含头文件、构建接线，未抵扣删除行）。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 错误计数、第三次等 30 秒、等待不计数、重启仍等待、正确 PIN 进入 | 上述正向命令；verify、wait、recover |
| 2 | 【实测】通过 | 真实触摸设置和修改八位测试 PIN；独立进程重启后旧 PIN 失败、新 PIN 成功 | 同上；write、verify |
| 3 | 【实测】验证通过，重置缺口见下 | 错误家长 PIN 计数；正确 PIN 只报告家长已验证，不切换身份、不删除 PIN | 同上；recover |
| 4 | 【实测】通过 | 各步快照和文本日志无测试 PIN；AI 提交被拒绝 | 同上；七阶段逐字检查 |
| 5 | 【实测】未全通过：普通和内存检查各 232/233；【实现】275/300 行 | 两轮唯一失败均为共享能力表文档；无跳过，sanitizer 错误为 0 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存检查同命令换 `build-asan` |
| K1-R1（高） | 【实测】已修 | 失效表单及凭据清除；孩子不能修改成人 PIN，原 PIN 仍有效；撤掉修复后复现越权 | 正向命令；`--mutant session` |
| K1-R2（中） | 【实测】已修 | 到期后无需提交即恢复普通页面和可见提示，首个数字可输入；撤掉重画后 3 条断言失败 | 正向命令；`--mutant expiry` |

## 没做到的

复查列出的两项均已修，复查未列严重问题。以下是上轮遗留，未计为本轮修复：忘记 PIN 后的重置未做；现有成员接口只有旧 PIN 修改和家长核验，没有核验后重置接口，本任务禁止改存储格式和尝试规则，页面明确提示缺口，没有伪造成功。

视觉对照未达到 12/255；未在 Linux 上执行，本次环境为 macOS。共享文档校验不能称为通过，原因和合并后的处理见“要改的文档”。

## 和设计不一致的

上轮四屏已查看设计图并用 `tools/ui_compare.py --reuse` 实测：MemberSwitch 28.298、PinWait 27.370、PinForgot 30.379、PinSet 29.473（单位 /255），均超过目标 12。本轮未改布局，核看 PinWait 设计图及实际截图，未重跑视觉评分。对照产物在 `build/kid-1-compare/`，未提交。

- MemberSwitch 用通用面板和真实空状态，只写切换的实际影响，未硬编码示例书名和备忘。
- PinWait、PinSet 按现有主题放大后的数字键和字体布局，增加“确定”保留策略的四至八位 PIN；不照画板自动提交四位。等待数字不逐秒刷新，AI 快照按虚拟时钟计算；本轮增加到期后的一次重画，恢复输入提示。
- PinForgot 按任务书接本机家长验证；画板里的服务器重置没有对应接口，改为如实说明。图标用现有锁图标。姓名、时间、电量和灰度也与设计样例不同。

## 改过的已有测试

1. `ui_shell_scenes.c`：Members 场景直接进入选人页，预览另由新测试覆盖；Pin 快照应含“密码内容不可见”，成员门仍应含“请选择家人”。保留原有触摸解锁、错误及布局检查。
2. `ui_shell_regressions.c`：选孩子前补走换人预览的继续能力，原有隐私断言不变。
3. `integration/ai_surface.c`：登记新增导航能力和仅本人可调用的提交能力，没有放宽旧能力权限。
4. 本轮仅给 `kid_1.c` 追加 session、expiry 两个回归阶段，给 `kid_1.py` 追加对应运行及变体；没有改旧断言、期望、预算、超时或跳过条件。

## 要用户定的

本轮无新增待定事项。沿用上轮的重置后续方案：A（推荐，当前保守做法）接家庭服务器重置协议；B 增加家长核验后本机重置的受限策略接口。两者都需明确授权范围；本次只核验和提示。

## 顺手发现

模拟器重启会重置 RTC 基准，持久化等待可能比刚触发的 30 秒长；测试同时验证首次等待恰为 30 秒，以及重启后遵守现有最长 900 秒规则，未改策略。

## 新增的机制

页面内输入阶段（原密码→新密码、家长核验）：任务书明确要求。没有新增常驻任务、持久格式、配置项、后台重试、缓存或队列；慢校验复用现有执行器。

本轮按复查 K1-R1 复用现有成员代次及状态监听；按 K1-R2 复用 LVGL 调度，新增的只是等待页存活期间的一次定时回调，关页即取消，没有新增任务或调度机制。

【实现】架构审视：显示和本机输入留在 UI，校验与持久化仍归成员策略；导航和提交均过能力关口。五个新导航能力对 AI 开放，提交为 owner-only，数字输入不经过总线；四页均有不含凭据的快照，已有 AI 能力面测试覆盖这六项权限。

为什么不能更小：超过 100 行的原因是四页的呈现、异步结果回 UI、凭据清除和两阶段修改都需覆盖。看过直接复用 `pinpad.c` 的更小方案，但旧控件只有单次输入，没有等待禁输、家长入口及修改阶段，外观改动也会牵涉共用的确认卡键盘。本次只在新文件内共用一套输入界面。

## 要改的文档

按总规则第 7 节，本分支不改共享文件：主会话需在合并时重新生成 `docs/system/capabilities.md`（新增 6 个能力），更新 `docs/system/ui-dependencies.md` 中本任务四页的实际状态并保留重置缺口。未改 backlog、需求、想法、决策及调研资料。

【实测】临时能力表已生成到 `build/kid-1-capabilities.md`，共 330 个定义；运行时能力、权限与登记检查均通过。不能通过改测试或跳过检查来消除共享文档差异。

## 本轮结束核对

【实测】本轮仅修改 4 个相关文件（一个产品文件、两个测试文件及本报告）；相对用户基线共 13 个文件，包含原任务和复查记录。已核看差异与统计；`git diff --check` 退出 0，任务规则第 6 节的隐私正则扫描完整基线差异为 0 命中（grep 退出 1）。没有提交构建产物或设计稿，仅本地提交；未推送、开 PR、碰设备或调用付费接口。

## 上轮验证记录（截至 da704af2，保留）

【实测】初轮全量暴露锁定后通用页面绑定不执行 PIN 提交的回归。只修本页经能力关口调用，7 项针对性复测全部通过，未修改通用导航绑定。最终普通全量耗时 348.64 秒，232/233 通过；唯一失败为 `capability-table-doc`。中枢依赖使用已有选项 `-DINK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，因此 3 项中枢测试均执行通过，没有跳过。

【实测】`kid-1` 五个独立进程阶段共 816 条断言，内部 RAM 最低余量 47,840 字节（下限 32,768）。保留证据：`build/kid-1-u15brd0k/`；普通全量与构建日志：`build/kid-1-verification/`。

【实测】反向对照：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/kid_1.py --build build --mutant <名称>`。只在构建目录编译变体，不改工作树；均退出 1 并触发断言失败，未把编译错误当证据：

| 名称 | 撤掉的关键行为 | 实际失败 |
| --- | --- | --- |
| unlock | 不把本人输入交给解锁校验 | verify，3 条 |
| set | 不把新密码交给设置接口 | write，11 条 |
| parent | 把家长核验错接为成员解锁 | recover，1 条 |
| privacy | 让输入内容进入快照 | write，3 条 |
| locked | 改回普通页面按钮绑定 | verify，2 条 |

五份日志在 `build/kid-1-negatives/`。正确版本随后完成普通全量。

【实测】画板复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/kid-1-compare --scenes MemberSwitch PinWait PinForgot PinSet --reuse`。四帧均由实际服务与触摸流程产生，并等到面板提交完成。

【实测】内存检查按任务书同配置加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`，同样使用共享中枢 Python 依赖；构建与测试并行度均为 2，命令均带 `taskpolicy -c utility`。全量耗时 511.67 秒，232/233 通过，唯一失败为同一个 `capability-table-doc`；`Testing/Temporary/LastTest.log` 中 sanitizer 错误扫描为 0。新功能仍为 816 条断言、最低内部 RAM 47,840 字节。证据：`build-asan/kid-1-odyknh20/`、`build-asan/kid-1-verification/`。

【实测】结束核对：相对基线仅 12 个相关文件；`git diff --check` 无问题。按任务书第 6 节的隐私正则扫描基线至 HEAD 的完整差异，0 命中（grep 返回 1 表示无匹配）；未提交绝对本机路径、MAC、构建产物或设计稿。全部变更仅本地提交，未推送、开 PR、碰设备或调用付费接口。
