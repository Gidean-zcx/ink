# kid-1 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。状态：实现和检查已完成；未通过项与缺口如下。

【实现】新页面在 `system/ui/shell/member_pages.c`；成员门只接入口，PIN 的哈希格式、尝试次数和等待策略未改。新增产品代码保守按全部新增行计为 246 行（含头文件、构建接线，未抵扣删除行）。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 错误计数、第三次等 30 秒、等待不计数、重启仍等待、正确 PIN 进入 | `ctest --test-dir build --output-on-failure -j2 -R '^(kid-1|ui-scene-(big-)?Pin(Wrong)?|integration-member-wake|integration-apps)$'`：7/7 |
| 2 | 【实测】通过 | 真实触摸设置和修改八位测试 PIN；独立进程重启后旧 PIN 失败、新 PIN 成功 | 同上；`tests/kid_1.py` 的 write、verify |
| 3 | 【实测】验证通过，重置缺口见下 | 错误家长 PIN 计数；正确 PIN 只报告家长已验证，不切换身份、不删除 PIN | 同上；recover |
| 4 | 【实测】通过 | 各步快照和文本日志无测试 PIN；AI 提交被拒绝 | 同上；各阶段逐字检查 |
| 5 | 【实测】未全通过：两轮各 232/233；246/300 行 | 普通与 ASan/UBSan 唯一失败均是共享能力表文档；无跳过；未检出 sanitizer 错误 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存检查同命令换 `build-asan` |

## 没做到的

忘记 PIN 后的重置未做：现有成员接口只有旧 PIN 修改和家长核验，没有核验后重置接口；本任务禁止改存储格式和尝试规则，页面明确提示缺口，没有伪造成功。

视觉对照未达到 12/255；未在 Linux 上执行，本次环境为 macOS。共享文档校验不能称为通过，原因和合并后的处理见“要改的文档”。

## 和设计不一致的

四屏已查看设计图并用 `tools/ui_compare.py --reuse` 实测：MemberSwitch 28.298、PinWait 27.370、PinForgot 30.379、PinSet 29.473（单位 /255），均超过目标 12。对照产物在 `build/kid-1-compare/`，未提交。

- MemberSwitch 用通用面板和真实空状态，只写切换的实际影响，未硬编码示例书名和备忘。
- PinWait、PinSet 按现有主题放大后的数字键和字体布局，增加“确定”保留策略的四至八位 PIN；不照画板自动提交四位。等待数字在提交重试时更新，AI 快照按虚拟时钟计算，屏幕不逐秒刷新。
- PinForgot 按任务书接本机家长验证；画板里的服务器重置没有对应接口，改为如实说明。图标用现有锁图标。姓名、时间、电量和灰度也与设计样例不同。

## 改过的已有测试

1. `ui_shell_scenes.c`：Members 场景直接进入选人页，预览另由新测试覆盖；Pin 快照应含“密码内容不可见”，成员门仍应含“请选择家人”。保留原有触摸解锁、错误及布局检查。
2. `ui_shell_regressions.c`：选孩子前补走换人预览的继续能力，原有隐私断言不变。
3. `integration/ai_surface.c`：登记新增导航能力和仅本人可调用的提交能力，没有放宽旧能力权限。

## 要用户定的

重置有两个后续方案：A（推荐，当前保守做法）接家庭服务器重置协议；B 增加家长核验后本机重置的受限策略接口。两者都需明确授权范围；本次只核验和提示。

## 顺手发现

模拟器重启会重置 RTC 基准，持久化等待可能比刚触发的 30 秒长；测试同时验证首次等待恰为 30 秒，以及重启后遵守现有最长 900 秒规则，未改策略。

## 新增的机制

页面内输入阶段（原密码→新密码、家长核验）：任务书明确要求。没有新增常驻任务、持久格式、配置项、后台重试、缓存或队列；慢校验复用现有执行器。

【实现】架构审视：显示和本机输入留在 UI，校验与持久化仍归成员策略；导航和提交均过能力关口。五个新导航能力对 AI 开放，提交为 owner-only，数字输入不经过总线；四页均有不含凭据的快照，已有 AI 能力面测试覆盖这六项权限。

为什么不能更小：超过 100 行的原因是四页的呈现、异步结果回 UI、凭据清除和两阶段修改都需覆盖。看过直接复用 `pinpad.c` 的更小方案，但旧控件只有单次输入，没有等待禁输、家长入口及修改阶段，外观改动也会牵涉共用的确认卡键盘。本次只在新文件内共用一套输入界面。

## 要改的文档

按总规则第 7 节，本分支不改共享文件：主会话需在合并时重新生成 `docs/system/capabilities.md`（新增 6 个能力），更新 `docs/system/ui-dependencies.md` 中本任务四页的实际状态并保留重置缺口。未改 backlog、需求、想法、决策及调研资料。

【实测】临时能力表已生成到 `build/kid-1-capabilities.md`，共 330 个定义；运行时能力、权限与登记检查均通过。不能通过改测试或跳过检查来消除共享文档差异。

## 验证记录

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
