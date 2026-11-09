# confirm-pages 开发报告

## 复查后的修复

2026-11-09：三项中级问题均已修复；复查没有严重、高级或低级问题。以下为本轮结果，后文保留初次交付记录。

| 问题 | 修复与回归 | 反向对照 |
| --- | --- | --- |
| CP-R1-01 | 【实测】确认卡订阅已有成员、电源和打断状态，在 UI 执行器复用 `hidden()` / `close_card()` 收起过期卡。真实过期后，来电结束不再恢复旧卡；锁屏 121 秒、唤醒后触摸选孩子，旧卡消失，孩子的 AI 快照非空且没有旧确认内容 | 只换回受审确认卡，`ConfirmExpired` 失败 2 条、退出 1，分别检出打断后遗留和孩子空快照 |
| CP-R1-02 | 【实测】密码页增加可触摸的“取消”，复用 `on_answer` 的拒绝路径；输入一位后取消不执行。错误密码、正确密码、密码遮蔽及 HOME 拒绝仍通过 | 只换回受审确认卡，`ConfirmParent` 失败 1 条、退出 1，检出触摸取消未拒绝 |
| CP-R1-03 | 【实测】逐项检查回执 token、decision、status，以及执行次数和实际参数；只有第二项 `n=11` 执行一次 | 第二项改按取消：失败 5 条、退出 1；真正把执行参数替换成 `n=12`：失败 4 条、退出 1 |

【实测】普通专项六场景共 546 条断言通过；内部 RAM 最低余量为 42,200 字节（新增来电回归），高于原预算 32,768 字节。密码页截图已实际查看，取消、说明、密码提示与数字键均可见，没有重叠。仅去掉人类来源检查的原反例仍失败 16 条、退出 1。

【实现】本轮产品只改 `system/ui/shell/confirm_card.c`，11 行新增、1 行删除；相对原任务基线产品文件共 147 行新增、18 行删除，加 CMake 注册 5 行，即使把增删都算入也只有 170 行，低于 300 行。新增机制：无；复用已有状态通知、UI 执行器和拒绝路径，没有新任务、轮询、队列或服务接口。

已有测试调整：`tests/confirm_pages/pages.c` 保留原 HOME 取消检查并新增触摸取消，原有断言未放宽；回执夹具每次清空可选的 `status`，避免沿用上条结果；失败诊断只打印页面名，避免长中文快照被截成无效 UTF-8。`pages.script` 的合成卡从短信套餐改为语音套餐，以支持真实来电；结束时间从 240 秒改为 420 秒，容纳三次真实过期和锁屏 121 秒，未增加宿主等待或修改产品超时。

环境跳过原因已定位：普通构建 Python 缺少 `pytest==8.4.2`、`pytest-asyncio==1.2.0`、`aiohttp==3.12.15`、`PyYAML==6.0.3`。`$INK_DEPS/hub/.venv/bin/python` 已具备这些锁定版本和 Python 3.11.15，构建选择该 `INK_HUB_PYTHON`；测试设置 `PYTHONPATH="$PWD/hub"`，已确认加载本工作树的中枢源码。没有安装或升级依赖，没有修改依赖目录。

全量与 ASan/UBSan 验证进行中；生成能力表校验按用户补充留给合并会话，不修改或掩盖该检查。

可重跑的专项与反例（命令均加 `INK_KEEP_EVIDENCE=1 taskpolicy -c utility`，Python 使用 `$INK_DEPS/build-deps/pyenv/bin/python`）：

```sh
python tests/confirm_pages/run.py --build build
python tests/confirm_pages/run.py --build build --baseline 132c3969 --scene ConfirmExpired
python tests/confirm_pages/run.py --build build --baseline 132c3969 --scene ConfirmParent
python tests/confirm_pages/run.py --build build --mutant queue-reject --scene StressConfirm
python tests/confirm_pages/run.py --build build --mutant queue-target --scene StressConfirm
python tests/confirm_pages/run.py --build build --mutant ai-source --scene ConfirmSetting
```

反例仅编译构建目录副本。证据为 `build/confirm-review-negative-*.log`；两份旧卡结果另存 `build/test-out/confirm-review-before-expired/`、`confirm-review-before-parent/`，队列反例在 `confirm-pages-queue-reject/`、`confirm-pages-queue-target/`。

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。状态：本地交付；功能、全量、内存和视觉检查均已跑完，未全通过的项目如下。未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 网页、家长、电话、设置、长内容均出现、同意、拒绝、真实计时过期；过期页可关闭，旧编号不能执行；错误密码不能同意 | `python tests/confirm_pages/run.py --build build` |
| 2 | 【实测】通过 | 四项先进先出，第五项拒绝；第二项同意执行，其余拒绝，各有对应回执；快照随队列变化 | 同上，`StressConfirm` |
| 3 | 【实测】通过 | AI、系统、应用、远端、已确认来源均不能直接回答；AI 不能代翻说明。仅移除人类来源检查后，设置场景失败 16 条，退出 1 | `python tests/confirm_pages/run.py --build build --mutant ai-source --scene ConfirmSetting` |
| 4 | 【实测】通过 | 六类快照均含页面身份、家人、确认编号、阅读进度和队列长度；密码页无密码。最终普通及内存专项各 500 条断言，全部通过 | `python tests/confirm_pages/run.py --build build`；ASan 同场景 |
| 5 | 【实测】未全通过；代码预算通过 | 普通与 ASan/UBSan 构建均通过；两轮全量各 233 项：229 通过、3 项中枢环境依赖跳过、1 项生成能力目录校验失败；产品新增 137 行，含 5 行测试注册保守计 142 行，小于 300 | `cmake --build build -j2`；`ctest --test-dir build --output-on-failure -j2`；内存检查换 `build-asan`、加 `INK_SANITIZE=ON`；`git diff --numstat` |

以上命令均加 `taskpolicy -c utility`，Python/CTest 使用 `$INK_DEPS/build-deps/pyenv/bin/`，构建参数与总规则一致。

【实测】反向对照：只把确认卡替换回基线版本，网页场景失败 8 条，退出 1。命令：`python tests/confirm_pages/run.py --build build --baseline 3fce2beea07d9e25a814cd24c64e8d5305ea9222 --scene ConfirmWeb`。两种反例均只编译构建目录中的副本，不改工作树。

【实测】各场景内部 RAM 最低余量均为 47,840 字节，未放宽 32,768 字节预算。证据保留在 `build/test-out/confirm-pages-current/`，反例在相邻 `confirm-pages-ai-source/`、`confirm-pages-baseline/`。

【实测】普通全量耗时 198.68 秒，CTest 退出 8。唯一失败是 `capability-table-doc`；`confirm-pages`、原确认集成、确认持久化、两种屏幕配置的 UI 回归、确定性、分层和预算检查均通过。3 项跳过是 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，没有修改它们的跳过条件。

【实测】ASan/UBSan 全量耗时 493.25 秒，CTest 退出 8；同样只失败 `capability-table-doc`，同样 3 项跳过。`build-asan/CMakeCache.txt` 确认 `INK_SANITIZE=ON`、`INK_WERROR=ON`；编译参数包含 `-fsanitize=address,undefined`。`build-asan/Testing/Temporary/LastTest.log` 没有 AddressSanitizer/UBSan 错误；确认页专项 500 条断言均通过，包括密码页中按 HOME 拒绝。普通构建随后重新跑过该专项，500 条断言通过，并重新保留全部截图及评分。

## 没做到的

- 全量尚未全绿：生成能力目录依任务约定留给合并会话同步；中枢锁定 Python 环境缺失，三个已有测试按原条件跳过。未删除、跳过或放宽失败项。
- 过期页没有“再来一次”：当前确认服务过期后不保留可重放的原始参数，也没有重新发起接口。显示“请重新发起”和“关闭”，不绕过原事务绑定。
- 无独立支付、网页登录状态、联系人名称、设置前后值接口；显示服务提供的标题和完整说明，不编造画板中的数据。
- 【实现】基线 `system/policy/confirm.c` 将家长标记设为 `parent_pin && ink_member_any_pin()`；没有设置任何大人密码时会成为普通确认。本次保留该规则；本报告的家长验收是在已设置密码的条件下实测，未把无密码情形说成已满足“家长确认要密码”。
- 未做实机验证或 Linux 实跑；本机是 macOS clang，使用 `INK_WERROR=ON`。

## 和设计不一致的

【实测】已逐张看六张画板，并用 `tools/ui_compare.py --reuse --out build/test-out/confirm-pages-current --scenes ConfirmWeb ConfirmParent ConfirmCall ConfirmSetting ConfirmExpired StressConfirm` 测量。全部超过 12/255，未改评分口径。

| 页面 | 1/4 灰度平均差 |
| --- | ---: |
| ConfirmWeb | 39.942/255 |
| ConfirmParent | 31.066/255 |
| ConfirmCall | 35.202/255 |
| ConfirmSetting | 35.631/255 |
| ConfirmExpired | 36.402/255 |
| StressConfirm | 43.802/255 |

- 画板尚是旧尺寸和浅灰；按规范第 1 节及现有主题使用 1.2 倍尺寸、深灰内容块和淡化背景，故字体、面板高度、键盘与底图明显不同。未改共享主题或主页。
- 测试底图使用真实空书架；联系人、收件人和内容用合成资料，和画板示例不同。
- 家长确认先完整阅读，再打开带事务标题和说明的现有密码键盘；保留 4～8 位输入、删除和确定，HOME/返回取消。没有照旧稿自动提交四位密码。
- 过期页只提供关闭；长说明沿用现有分页条，确认按钮必须等全部说明显示后才启用。

## 改过的已有测试

无；全部新增在 `tests/confirm_pages/`，已有测试和预算未修改。

## 要用户定的

过期重发的服务接口及其重新绑定方式；网页支付元数据和设置前后值是否向页面提供；无大人密码时，家长确认是否必须先设置密码。任务内采取上述可逆显示方式，并保留原策略。

## 顺手发现

能力目录检查因本页新增对 `confirm.result`、`confirm.pending`、`interrupt.active` 的实际订阅/读取而要求更新生成目录。基线该检查通过，当前失败差异仅为该模块的两项状态依赖和一项事件依赖。按总规则第 7 节，任务内不改共享目录，不隐藏或跳过此失败。

## 新增的机制

- 产品常驻任务、后台重试、队列、持久格式、配置项：均未新增。
- 过期页的可见标记及 `confirm.result` 界面订阅：任务书明确要求 `ConfirmExpired`；只在原 UI 执行器运行。
- 关闭过期说明复用已有 `confirm.page` 的 `delta=0`，仍为仅设备操作的能力；确认队列、PIN 验证、成员/参数/内容版本检查不变。
- 架构审视：【实现】确认规则与结果仍归策略服务；页面在 UI 执行器读取状态、订阅结果并调用既有确认接口；按家人绑定的快照供 AI 读取，密码内容不进入快照。

## 为什么不能更小

仅改按钮不能补齐过期结果页、密码页事务说明和六类快照。已比较六个独立页面（重复更多）和修改服务提供新模型（越界）；最终把外观集中在已有确认卡，共新增 137 行产品文件，没有新增页面框架。

## 要改的文档

- 合并时将 `docs/system/ui-dependencies.md` 中六个页面的状态改为实际情况，并保留本报告列出的接口和视觉差异。
- 合并时运行 `python tools/capabilities.py`，同步确认卡的状态/事件依赖；能力 id 没有增加。

## 结束检查

- 【实测】`git diff --check` 无错误；基线到当前提交的 `git diff --stat` 只有确认卡、尾部测试注册、四个新增测试文件和本报告。
- 【实测】按总规则指定表达式检查基线到 HEAD 的提交差异，零匹配（grep 退出 1）。截图、构建产物和设计稿均未提交。
