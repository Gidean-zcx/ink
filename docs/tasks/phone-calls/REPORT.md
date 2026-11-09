# 电话任务报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。验收 1–4 通过，5 部分通过；仅限模拟器 AT 模型。

本轮复修（2026-11-09）：复查的严重 0、高 1、中 1 中，PC-R1-01（高）、PC-R1-02（中）均已修复；回归和撤掉修复的反向对照有效。常规及 ASan/UBSan 全量各 224/225 通过，仍仅共享能力文档检查失败，原因见“没做到的”。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| PC-R1-01（高） | 【实测】已修复 | 通话接通后回主页，再从电话入口恢复到当前成员的 `PhoneCall`；页面有原号码和 `call.hangup`，实际点按钮挂断 | T1、T7 |
| PC-R1-02（中） | 【实测】已修复 | 保持最近通话页打开，接听并挂断后直接读取原页面：集合和快照均为 3 条，包含新来电号码；未先重新进入 | T1、T7 |
| 1 | 【实测】通过 | AT 拨出、接通、挂断；接听与拒接各记一条；新进程读回大人 5 条、孩子 1 条 | T1 |
| 2 | 【实测】通过 | 真实联系人编辑页点“选择这个号码”，`contacts.select` 返回后填入拨号盘，再点拨出 | T1 |
| 3 | 【实测】通过，含反例 | 去掉读取时的成员绑定，孩子记录数断言失败；恢复后通过 | T2 |
| 4 | 【实测】通过 | 三页快照能放进 3072 字节 AI 上下文；按钮与 AI 同用 `call.dial` / `call.hangup`；AI 拨号先待确认，AI 不能自己确认 | T1 |
| 5 | 【实测】部分通过 | 常规及 ASan/UBSan 各 224/225 通过，无跳过；唯一失败为共享生成能力表未更新。新操作已注册且源码/运行时一致；产品改动 288 行 | T3、T4、T6 |

T1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(phone-calls|contacts)$' --output-on-failure -j2`，本轮 2/2 通过。电话五阶段分别为 184、33、14、20、20 个断言，共 271 个；证据 `build/phone-r1-focused.log` 和 `build/test-out/phone-calls/*/metrics.json`。

T2：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/phone_calls/reverse.py build`，本轮退出 0。反例把唯一一处 `ink_db_open_cached_member(&HISTORY, P.member)` 改为固定读大人，恰好 3 个隔离断言失败；脚本恢复源码、重新构建后全五阶段 271 个断言通过，产品源码与提交完全一致。证据：`build/phone-r1-isolation-reverse.log`、`build/phone-reverse.log`、`build/phone-restored.log`。

构建使用任务书第 5 节的全部参数，两套构建均额外指定现成的 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；ASan 仅再加 `-DINK_SANITIZE=ON`。两套 `taskpolicy -c utility cmake --build <目录> -j2` 均退出 0。

T3：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`，本轮退出 8，224/225 通过，无跳过，134.08 秒；唯一失败为 `capability-table-doc`。构建退出 0；证据 `build/phone-r1-build.log`、`build/phone-r1-ctest-full.log`。

T4：同 T3，仅目录换为 `build-asan`，本轮退出 8，224/225 通过，无跳过，245.14 秒；唯一失败仍为 `capability-table-doc`。构建退出 0；证据 `build-asan/phone-r1-build.log`、`build-asan/phone-r1-ctest-full.log`。无内存或未定义行为错误。电话五阶段共 271 个断言通过，两套测试内部 RAM 最低余量均为 **47,840 字节**，门槛 **32,768 字节**，内部、外部及 DMA 分配失败均为 0；预算未放宽。

T5：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/phone-calls --scenes Phone PhoneRecents PhoneCall --reuse`，本轮退出 0，分数与上轮一致；已逐张打开三页实际截图核对。证据为 `build/phone-r1-ui-compare.log`、该目录的 `scores.json`、三张拼图及各页面的 `snap-scene.png`；保留证据，未提交设计稿或产物。

T6：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/phone-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`，本轮退出 0，285 个定义通过，运行时匹配 224 项能力/事件及 16 项设置；证据 `build/phone-r1-capabilities.log`。`git diff f0b0c539..HEAD --numstat` 计数产品改动。

T7：【实测】在复修前版本 `15ac0ca5` 的产品代码上，仅补充本轮回归并构建 `sim-phone-calls`，运行 `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/phone_calls/run.py build/sim-phone-calls . build/assets build/test-content build/test-out/phone-r1-before`，退出 1；写入阶段 180 个断言通过、恰好 PC-R1-01 和 PC-R1-02 两项失败，`test.finished=1`。保留相同测试，加入修复后 T1 通过。证据 `build/phone-r1-before.log` 和 `build/test-out/phone-r1-before/write/metrics.json`；没有放宽断言或预算。

修复后再做一次撤回对照：用 `git show 15ac0ca5:system/apps/phone/phone.c` 导出原实现到 `build/test-out/phone-r1-reversed-copy/phone.c`，让当前测试副本引用它；复用 `build/CMakeFiles/sim-phone-calls.dir/{flags.make,link.txt}` 编译、链接，构建退出 0。以该副本运行同一 `tests/phone_calls/run.py`，退出 1，仍恰好上述两项失败、180 个断言通过；未修改工作区源码。证据为该目录的 `build.log`、`negative.log` 和 `run/write/metrics.json`。

## 没做到的

- 全量测试仍未全绿：`docs/system/capabilities.md` 仍写 213 项，源码及运行时为 224 项；总规则第 7 节明确禁止本分支修改共享生成能力表，须留给主会话合并后更新，因此保留 `capability-table-doc` 失败。未跳过或放宽检查；两项编号问题均已修复，无未处理的严重或高问题。
- 通话静音、通话键盘、耳机通话路由和通话录音没有可用接口；页面如实说明，未伪造操作。
- 通话页没有持续更新的分钟数；已结束记录保存按单调时钟计算的整分钟数。满额、通话中断电等超出验收的情况未另加机制。
- 本机仅验证 macOS；没有可用 Linux 执行环境，不声称 Linux 已实测。

## 和设计不一致的

- 电话入口从联系人列表改为拨号盘；联系人仍由原页面和 `contacts.select` 提供。
- 为接通现有入口，联系人文件除末尾增补接口外，还修改了应用入口并在选择成功处发事件。这两处不能仅靠末尾注册完成，未加入通用拦截或替换机制。
- 【实测】`tools/ui_compare.py --reuse`：Phone **32.474/255**、PhoneRecents **52.382/255**、PhoneCall **25.851/255**，均超过 12，未宣称达标。输入框为 F2 控件，数字键按现行主题 163；列表为 F4 四行，页码在列表下方；通话页显示实际号码及缺失能力的文字，没有画板的联系人头像、姓名与四个可用按钮。现行主题的白底、`#888888`、1.2 倍尺寸也与旧画板不同。

## 改过的已有测试

- `tests/contacts/contacts.c`：原入口断言改为电话入口能打开，再调用 `contacts.open` 进入联系人；原联系人 CRUD、选择、隔离和确认断言保留。
- `tests/integration/ai_surface.c`：在 AI 预期清单追加 8 个新操作，核查 `phone.open` 的可选号码参数、`phone.key` 的必需参数 `key`，并追加 3 个事件注册断言。原断言不变；首轮漏登导致 10 个断言失败，补登后实测通过。
- 本轮 `tests/phone_calls/phone_calls.c`：在既有首次外呼后插入主页往返并实际点挂断；在已有来电接听前打开最近通话页，结束后先检查原页，再保留原历史检查。大人 5 条、孩子 1 条等既有期望不变；反例分支仅在“无法回到通话”断言已经失败后清理模拟通话，避免连带失败。

## 要用户定的

- 暂无；按任务书既有确认规则实施。

## 顺手发现

- 【实现】既有 `call.dial` 只限制号码长度，没有校验字符；本任务未改通信参数验证策略。
- 【实现】能力注册表上限 256，当前 224；主会话合并并发分支后需复核容量。
- 【实测】默认 Python 缺少中枢测试依赖，首轮有 3 项跳过；改选依赖检出中现成的中枢 Python 后，`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 全部通过，未安装或升级依赖。

## 新增的机制

- `phone.calls` 成员数据集合：任务书要求保存通话记录；复用数据服务现有 JSON 日志格式及默认 256 KiB 限额，不自动删除历史。
- `call.dialed`、`call.connected`、`contacts.selected` 事件：让电话页面接收到已有能力的实际结果，接通事件来自模拟 AT 的 `+CLCC`。
- 无新增常驻任务、线程、后台重试、队列、缓存、配置项或独立文件格式。
- 本轮无新增机制：PC-R1-01 复用已有通话 UI 事件状态及活动页 `settle` 钩子；PC-R1-02 按复查要求，在 `ink_db_put` 成功后复用 UI 执行器投递刷新，只有仍持有同一集合且属于当前成员的最近通话页会重画。

## 要改的文档

- 按总规则第 7 节，主会话合并后运行 `python tools/capabilities.py` 更新共享能力表；本分支只在 `build/phone-capabilities.md` 生成核查副本。源码与运行时校验通过：224 个能力、16 项设置，共 285 个定义。
- `docs/system/ui-dependencies.md`：Phone / PhoneRecents / PhoneCall 可标为模拟器已接线；通话附加控件、姓名头像和画板分数差距仍需保留。共享文件未修改。

## 规模与边界

- 当前相对基线的产品改动 **286 行新增、2 行替换删除，共 288 行**（含 CMake 接线及空行），低于 350 行。本轮只改电话模块 **16 行新增、4 行删除**。累计超过 100 行的原因是三页、F1/F2/F4 接入、事件归属与落盘缺一不可；更小的纯占位页不能满足验收，没有加入通用框架。
- 服务增量仅为：通信服务发布拨出/接通事件、模拟蜂窝 HAL 识别外呼接通的 `+CLCC`、联系人选择成功事件与页面入口。未改模组固件或配置路径。
- 【实现】页面只经能力总线执行通信，记录只经成员数据接口；BG 订阅负责落盘，UI 订阅负责页面。无模拟器头文件进入产品代码。
- 【实测】`git diff --check` 通过。本轮仅改电话模块、电话测试、报告三份文件；相对任务基线为 14 份开发文件加既有复查报告，无其他任务文件、共享需求/决策/生成文档或构建产物。按总规则第 6 节扫描基线至 HEAD，原始扫描退出 0，唯一命中为既有 `REVIEW.md:98` 的扫描表达式本身，人工核对无真实本机路径或 MAC；本轮相对 `15ac0ca5` 的改动无匹配（退出 1）。
- 工作仅本地提交，没有推送、开 PR、访问设备、真实通信或调用付费接口。
