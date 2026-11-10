# settings-small 执行报告

基线：`424fc6b206bbeca53438680d0973c9f2e6f338dc`。状态：专项及三组反向对照通过，正在全量验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 已存网络 | 【实测】专项通过 | 三个合成加密网络、真实点按连接与忘掉、确认及撤销、恢复密码可连接、八网络翻页、快照与日志无密码、儿童拒绝 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build` |
| 1b 文字重做 | 【实测】专项通过 | 点按与 AI 恢复结果相同；新输入失效；禁用状态不接受点按，黑白删除线可见 | 同上；`work/settings-small/targeted-small.log` |
| 2 回归与构建 | 待验证 | 原有测试、全量、内存检查、S31、gcccheck | 待补 |
| 2 GCC | 【实测】通过 | `gcccheck: 4 compiled, 0 with findings`，无跳过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/ui/shell/navigation.c system/ui/shell/keyboard.c system/services/net/wifi.c tests/settings_small.c` |
| 3 中英文截图 | 【实测】已生成并目检 | `build/settings-small-evidence/fixed/clean-networks-{zh,en}.png`、`clean-redo-{zh,en}.png`，另有 `clean-redo-disabled-{zh,en}.png` | 同 1a，保留证据 |

【实测】手动反向对照：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build --reverse`。分别替换成基线 `navigation.c`、`keyboard.c`、`wifi.c`，运行均完成并以退出 1 命中对应缺口（失败断言 1、6、8）；日志 `work/settings-small/reverse.log`。反向模式不注册 CTest，不影响浅克隆 CI。

## 没做到的

尚在执行。

## 和设计不一致的

【实现】基线 `wifi.forget` 只有确认，没有任务书所说的撤销。补齐服务侧逆操作，接入现有 `system.undo`；凭据不放能力参数或回执。

## 改过的已有测试

无。新增独立专项，原有断言、预算和测试期望未改。

## 要用户定的

无。

## 顺手发现

【实现】忘掉未连接网络原先会清空当前连接状态，本次在同一删除处理处修正。

## 新增的机制

任务书要求撤销忘掉：Wi-Fi 服务内增加有界凭据槽及仅供撤销调用的逆操作，参数只有版本号，复用现有 10 秒撤销时限及成员策略。没有新增任务、线程、状态机、文件格式或配置项。页面列表和分页沿用现有设置页机制。

## 要改的文档

按总规则第 7 节不改共享文档。合并时运行 `python3 tools/capabilities.py`，登记 `shell.known_networks` 和 `wifi.restore`；当前 `capability-table-doc` 会因生成文档过期失败。底层依赖清单可将已存网络管理和输入法重做更新为已接入。

## 规模与边界

产品差异 **+147/-13，共 160 行**（包括构建配置和 12 行译文，不含测试、报告），未超过 200 行。超过 100 行的原因：新增整页、异步读取及快照、服务中原先缺失的撤销、英文菜单宽度及禁用样式。更小的方案（仅暴露 `wifi.list`、只加“重做”文字、把密码放撤销参数）分别缺少操作页面、无可用状态或泄露凭据，未采用。

模块审视【实现】：界面只通过 `wifi.list/connect/forget` 与 `text.undo/redo` 读取和执行；凭据及逆操作归 Wi-Fi 服务；AI 经同一能力和 `system.undo` 操作，新增页面提供仅含名称、状态和动作的快照。
