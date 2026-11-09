# confirm-pages 开发报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。状态：功能及普通全量检查完成，正在内存检查；只在本地任务分支提交，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 网页、家长、电话、设置、长内容均出现、同意、拒绝、真实计时过期；过期页可关闭，旧编号不能执行；错误密码不能同意 | `python tests/confirm_pages/run.py --build build` |
| 2 | 【实测】通过 | 四项先进先出，第五项拒绝；第二项同意执行，其余拒绝，各有对应回执；快照随队列变化 | 同上，`StressConfirm` |
| 3 | 【实测】通过 | AI、系统、应用、远端、已确认来源均不能直接回答；AI 不能代翻说明。仅移除人类来源检查后，设置场景失败 16 条，退出 1 | `python tests/confirm_pages/run.py --build build --mutant ai-source --scene ConfirmSetting` |
| 4 | 【实测】通过 | 六类快照均含页面身份、家人、确认编号、阅读进度和队列长度；密码页无密码。六场景共 498 条断言 | `python tests/confirm_pages/run.py --build build` |
| 5 | 部分通过，内存检查中 | 【实测】普通构建通过；全量 233 项中 229 通过、3 项中枢环境依赖跳过、1 项生成能力目录校验失败；产品文件新增 137 行，连同 5 行测试注册保守计 142 行，小于 300 | `cmake --build build -j2`；`ctest --test-dir build --output-on-failure -j2`；`git diff --numstat` |

以上命令均加 `taskpolicy -c utility`，Python/CTest 使用 `$INK_DEPS/build-deps/pyenv/bin/`，构建参数与总规则一致。

【实测】反向对照：只把确认卡替换回基线版本，网页场景失败 8 条，退出 1。命令：`python tests/confirm_pages/run.py --build build --baseline 3fce2beea07d9e25a814cd24c64e8d5305ea9222 --scene ConfirmWeb`。两种反例均只编译构建目录中的副本，不改工作树。

【实测】各场景内部 RAM 最低余量均为 47,840 字节，未放宽 32,768 字节预算。证据保留在 `build/test-out/confirm-pages-current/`，反例在相邻 `confirm-pages-ai-source/`、`confirm-pages-baseline/`。

【实测】普通全量耗时 198.68 秒，CTest 退出 8。唯一失败是 `capability-table-doc`；`confirm-pages`、原确认集成、确认持久化、两种屏幕配置的 UI 回归、确定性、分层和预算检查均通过。3 项跳过是 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，没有修改它们的跳过条件。

## 没做到的

- 过期页没有“再来一次”：当前确认服务过期后不保留可重放的原始参数，也没有重新发起接口。显示“请重新发起”和“关闭”，不绕过原事务绑定。
- 无独立支付、网页登录状态、联系人名称、设置前后值接口；显示服务提供的标题和完整说明，不编造画板中的数据。
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

过期重发的服务接口及其重新绑定方式；网页支付元数据和设置前后值是否向页面提供。任务内采取上述可逆显示方式。

## 顺手发现

能力目录检查因本页新增对 `confirm.result`、`confirm.pending`、`interrupt.active` 的实际订阅/读取而要求更新生成目录。基线该检查通过，当前失败差异仅为该模块的两项状态依赖和一项事件依赖。按总规则第 7 节，任务内不改共享目录，不隐藏或跳过此失败。

## 新增的机制

- 常驻任务、后台重试、队列、持久格式、配置项：均未新增。
- 过期页的可见标记及 `confirm.result` 界面订阅：任务书明确要求 `ConfirmExpired`；只在原 UI 执行器运行。
- 关闭过期说明复用已有 `confirm.page` 的 `delta=0`，仍为仅设备操作的能力；确认队列、PIN 验证、成员/参数/内容版本检查不变。

## 为什么不能更小

仅改按钮不能补齐过期结果页、密码页事务说明和六类快照。已比较六个独立页面（重复更多）和修改服务提供新模型（越界）；最终把外观集中在已有确认卡，共新增 137 行产品文件，没有新增页面框架。

## 要改的文档

- 合并时将 `docs/system/ui-dependencies.md` 中六个页面的状态改为实际情况，并保留本报告列出的接口和视觉差异。
- 合并时运行 `python tools/capabilities.py`，同步确认卡的状态/事件依赖；能力 id 没有增加。

## 结束检查

- 【实测】`git diff --check` 无错误；基线到当前提交的 `git diff --stat` 只有确认卡、尾部测试注册、四个新增测试文件和本报告。
- 【实测】按总规则指定表达式检查基线到 HEAD 的提交差异，零匹配（grep 退出 1）。最终提交后再核对一次。
