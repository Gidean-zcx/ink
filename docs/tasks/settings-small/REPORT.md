# settings-small 执行报告

基线：`424fc6b206bbeca53438680d0973c9f2e6f338dc`。执行结束：功能专项通过，全量未全绿；仅本地提交，未推送、未操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 已存网络 | 【实测】专项通过 | 三个合成加密网络、真实点按连接与忘掉、确认及撤销、恢复密码可连接、八网络翻页、快照与日志无密码、儿童拒绝 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build` |
| 1b 文字重做 | 【实测】专项通过 | 点按与 AI 恢复结果相同；新输入失效；禁用状态不接受点按，黑白删除线可见；专项共 175 通过、0 失败 | 同上；`build/settings-small-evidence/fixed/metrics.json` |
| 2 普通全量 | 【实测】未全绿 | 首轮原话：`99% tests passed, 4 tests failed out of 324`，903.76 秒；失败项见下 | `taskpolicy -c utility cmake --build build -j2`；`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；`work/settings-small/full.log` |
| 2 回归复核 | 【实测】本次听写回归已修 | `settings-small`、`integration-dictation`、`integration-dictation-handsfree` 通过；剩余两项只报下述基线问题 | 同上 CTest 加 `-R '^(settings-small|integration-dictation|integration-dictation-handsfree|integration-ai-surface|audit12-m1)$'`；`work/settings-small/regression.log` |
| 2 内存检查 | 【实测】全量未全绿，无内存错误诊断 | 最终代码运行全部 324 项，原话：`99% tests passed, 4 tests failed out of 324`，1098.45 秒；专项 175/0，普通及免按键听写通过；AddressSanitizer/UBSan 错误诊断 0 | `taskpolicy -c utility cmake --build build-asan -j2`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2`；`work/settings-small/full-asan.log`、`build-asan/Testing/Temporary/LastTest.log` |
| 2 S31 | 【实测】编译、链接通过 | 脚本退出 0，镜像 1,402,806 B，ELF、尺寸与静态栈报告已生成；仅编译 | `INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh`；`work/settings-small/s31.log`、`build-s31/{size,size-components,stack}.txt` |
| 2 GCC | 【实测】通过 | `gcccheck: 5 compiled, 0 with findings`，无跳过；`work/settings-small/gcccheck-final.log` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/ui/shell/navigation.c system/ui/shell/keyboard.c system/services/net/wifi.c tests/settings_small.c tests/integration/ai_surface.c` |
| 3 中英文截图 | 【实测】已生成并目检 | 下列 6 张图 | 同 1a，保留证据 |

【实测】手动反向对照：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/settings_small.py --build build --reverse`。分别替换成基线 `navigation.c`、`keyboard.c`、`wifi.c`，最终复跑均完成并以退出 1 命中对应缺口（失败断言 1、6、8）；日志 `work/settings-small/reverse-final.log`。反向模式不注册 CTest，不影响浅克隆 CI。

配置采用任务书字体和只读 LVGL 依赖，另启用 `CMAKE_EXPORT_COMPILE_COMMANDS`（供 GCC/反向验证）并指定现有 `INK_HUB_PYTHON` 环境；内存构建增加 `INK_SANITIZE=ON`，复用普通构建内相同版本的 SDL 源码。构建和测试均加 `taskpolicy -c utility`，最多 `-j2`，没有构建或全量测试重叠。普通全量后的修正经定向复核及最终代码的完整 ASan/UBSan 回归验证；最后重新构建普通目标供复核，未再重复普通全量。

## 没做到的

全量通过的验收未达到。最终检测构建剩 4 项：`capability-table-doc`（生成文档未同步）、`services-mid-capability-doc`（服务设计未登记逆操作）、`integration-ai-surface`（基线相机漏登记）、`audit12-m1`（基线飞行模式断言）。前两项待主会话同步文档，后两项见“顺手发现”。没有删除测试、放宽断言或调整预算。

首轮普通全量曾检出本次菜单遮住话筒的回归（听写 26 条失败），已在 `keyboard.c` 一处收窄菜单显示条件；原有两项听写测试修正后在普通及检测构建均通过。

## 和设计不一致的

【实现】基线 `wifi.forget` 只有确认，没有任务书所说的撤销。补齐服务侧逆操作，接入现有 `system.undo`；凭据不放能力参数或回执。

英文图中的“大写、删除、空格”等是基线尚未迁移的键帽文案；本次已存网络页和编辑菜单提供中英译文，没有扩做整套键盘翻译。

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

按总规则第 7 节不改共享文档。合并时运行 `python3 tools/capabilities.py`，登记 `shell.known_networks` 和 `wifi.restore`；当前 `capability-table-doc` 会因生成文档过期失败。`docs/system/system-services-design.md` §1.3 的 Wi-Fi 行还须登记仅供撤销的 `wifi.restore`（必需 `revision`、凭据不外传），否则 `services-mid-capability-doc` 报 `NOT_FOR_AI: wifi.restore`；本任务不改设计规范来迁就实现。底层依赖清单可将已存网络管理和输入法重做更新为已接入。

## 规模与边界

产品差异 **+147/-13，共 160 行**（包括构建配置和 12 行译文，不含测试、报告），未超过 200 行。超过 100 行的原因：新增整页、异步读取及快照、服务中原先缺失的撤销、英文菜单宽度及禁用样式。更小的方案（仅暴露 `wifi.list`、只加“重做”文字、把密码放撤销参数）分别缺少操作页面、无可用状态或泄露凭据，未采用。

模块审视【实现】：界面只通过 `wifi.list/connect/forget` 与 `text.undo/redo` 读取和执行；凭据及逆操作归 Wi-Fi 服务；AI 经同一能力和 `system.undo` 操作，新增页面提供仅含名称、状态和动作的快照。

最终差异检查【实测】：仅 10 个相关文件；`git diff --check` 通过。按任务书第 6 节对基线至提交的差异检查地址与设备标识，`grep` 退出 1（无匹配），日志 `work/settings-small/privacy.log`。布局检查：`ui-scale: 50 files, 0 violations`（`work/settings-small/ui-scale.log`）。未运行 Ubuntu 原生测试；已执行要求的 GCC 检查和 S31 交叉编译。

## 截图路径

```text
build/settings-small-evidence/fixed/clean-networks-zh.png
build/settings-small-evidence/fixed/clean-networks-en.png
build/settings-small-evidence/fixed/clean-redo-zh.png
build/settings-small-evidence/fixed/clean-redo-en.png
build/settings-small-evidence/fixed/clean-redo-disabled-zh.png
build/settings-small-evidence/fixed/clean-redo-disabled-en.png
```
