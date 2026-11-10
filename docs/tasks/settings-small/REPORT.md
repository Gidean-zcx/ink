# settings-small 执行报告

基线：`424fc6b206bbeca53438680d0973c9f2e6f338dc`。状态：专项 175 项断言通过，普通全量已跑，正在内存检查。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 已存网络 | 【实测】专项通过 | 三个合成加密网络、真实点按连接与忘掉、确认及撤销、恢复密码可连接、八网络翻页、快照与日志无密码、儿童拒绝 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build` |
| 1b 文字重做 | 【实测】专项通过 | 点按与 AI 恢复结果相同；新输入失效；禁用状态不接受点按，黑白删除线可见 | 同上；`work/settings-small/targeted-small.log` |
| 2 普通全量 | 【实测】未全绿 | 首轮原话：`99% tests passed, 4 tests failed out of 324`，903.76 秒；失败项见下 | `taskpolicy -c utility cmake --build build -j2`；`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；`work/settings-small/full.log` |
| 2 回归复核 | 【实测】本次听写回归已修 | `settings-small`、`integration-dictation`、`integration-dictation-handsfree` 通过；剩余两项只报下述基线问题 | 同上 CTest 加 `-R '^(settings-small|integration-dictation|integration-dictation-handsfree|integration-ai-surface|audit12-m1)$'`；`work/settings-small/regression.log` |
| 2 内存检查、S31 | 待验证 | 正在构建 ASan + UBSan | 待补 |
| 2 GCC | 【实测】通过 | 首次 4 个 C 文件 0 处问题；最终含 AI 清单的结果待补 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/ui/shell/navigation.c system/ui/shell/keyboard.c system/services/net/wifi.c tests/settings_small.c tests/integration/ai_surface.c` |
| 3 中英文截图 | 【实测】已生成并目检 | `build/settings-small-evidence/fixed/clean-networks-{zh,en}.png`、`clean-redo-{zh,en}.png`，另有 `clean-redo-disabled-{zh,en}.png` | 同 1a，保留证据 |

【实测】手动反向对照：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build --reverse`。分别替换成基线 `navigation.c`、`keyboard.c`、`wifi.c`，运行均完成并以退出 1 命中对应缺口（失败断言 1、6、8）；日志 `work/settings-small/reverse.log`。反向模式不注册 CTest，不影响浅克隆 CI。

## 没做到的

普通全量未全绿。首轮 4 项失败中：本次菜单遮住话筒的回归已在 `keyboard.c` 一处修正并经两项原有听写测试验证；其余为禁止本任务更新的能力文档、基线相机能力漏登记、基线飞行模式断言（详见“顺手发现”）。没有删除测试、放宽断言或调整预算。

## 和设计不一致的

【实现】基线 `wifi.forget` 只有确认，没有任务书所说的撤销。补齐服务侧逆操作，接入现有 `system.undo`；凭据不放能力参数或回执。

## 改过的已有测试

`tests/integration/ai_surface.c` 仅登记本次新增的 `shell.known_networks`（AI 可用，无参数）和 `wifi.restore`（AI 只能经 `system.undo` 使用，必需参数 `revision`），原断言和预算不变。这是能力清单随新增能力的必要更新，不扩大 AI 权限。

## 要用户定的

无。

## 顺手发现

【实现】忘掉未连接网络原先会清空当前连接状态，本次在同一删除处理处修正。

- 【实现＋实测】基线 `camera.review` 已注册，但 `tests/integration/ai_surface.c` 的两张清单均遗漏；本任务登记自己的两个能力后，该测试只余这 1 条失败，记录于 `work/settings-small/regression.log`。未改相机或替别的任务补清单。
- 【实测】`audit12-m1` 的“then airplane mode is on”断言在 8.130 秒失败。把三处产品源文件都换回基线后，同一断言仍在 8.130 秒失败，19 通过、1 失败。对照日志 `work/settings-small/audit12-baseline.log`，首轮指标副本 `work/settings-small/audit12-original.json`；未修改该测试或飞行模式实现。

## 新增的机制

任务书要求撤销忘掉：Wi-Fi 服务内增加有界凭据槽及仅供撤销调用的逆操作，参数只有版本号，复用现有 10 秒撤销时限及成员策略。没有新增任务、线程、状态机、文件格式或配置项。页面列表和分页沿用现有设置页机制。

## 要改的文档

按总规则第 7 节不改共享文档。合并时运行 `python3 tools/capabilities.py`，登记 `shell.known_networks` 和 `wifi.restore`；当前 `capability-table-doc` 会因生成文档过期失败。底层依赖清单可将已存网络管理和输入法重做更新为已接入。

## 规模与边界

产品差异 **+147/-13，共 160 行**（包括构建配置和 12 行译文，不含测试、报告），未超过 200 行。超过 100 行的原因：新增整页、异步读取及快照、服务中原先缺失的撤销、英文菜单宽度及禁用样式。更小的方案（仅暴露 `wifi.list`、只加“重做”文字、把密码放撤销参数）分别缺少操作页面、无可用状态或泄露凭据，未采用。

模块审视【实现】：界面只通过 `wifi.list/connect/forget` 与 `text.undo/redo` 读取和执行；凭据及逆操作归 Wi-Fi 服务；AI 经同一能力和 `system.undo` 操作，新增页面提供仅含名称、状态和动作的快照。
