# services-mid 执行报告

2026-11-06；基线 `2c844243`，分支 `claude/codex-services-mid`。只处理 [任务清单](../backlog.md) 的 services-mid，遵守 [总规则](../codex-rules.md)。以下【实测】均为本次执行；实现提交 `3e69d052`，最终验证后仅补本报告。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| AUD18-106 | 基线已修，本次复验通过 | 【实测】失败版本迁移、第二次回滚和重启后历史保留；旧代码 3 项功能断言失败 | V1 `update_history`；V2 |
| AUD18-206 | 基线已修，本次复验通过 | 【实测】显示提交失败不报关机图已显示，重试成功；旧代码 2 项功能断言失败 | V1 `offpanel`；V2 |
| AUD18-207 | 基线已修，本次复验通过 | 【实测】长选区、两侧标记和拼音位置映射；旧代码 3 项功能断言失败 | V1 `text_window`；V2 |
| AUD18-402 | 基线已修，本次复验通过 | 【实测】普通/数字/密码键盘与页面组合快照，目标版本及密码掩码；旧代码 9 项功能断言失败 | V1 `keyboard_snapshot`；V2 |
| AUD18-404 | 已修 | 【实测】补齐 8 个能力、1 个状态、1 个事件；新增检查解析文档的对应列，旧文档准确检出这些漏项 | V1 `services-mid-capability-doc`；V2 |
| services-finish 其余中级 | 无额外未修项 | 【实现】当前第 6 轮 REVIEW 的中级均列已修，之后 REPORT 也确认保留；【实测】R1-06 的 `undo`、R2-04 的 `empty_removal`、S4-DET 的两项确定性和栈计量均通过 | V1；V3 |
| hub-step1 R4 | 已修 | 【实测】同进程 80 次开关（72 次成功、8 次连接失败）、线程/描述符启动失败、退出清理；每次关闭后描述符回到 3，144 个迟到回调安全，80 个当前回调正常；旧代码保留连接和待发消息，测试退出 1 | V1 `hub-bridge-lifecycle`、原 HAL 代次及桥端到端；V2 |
| G1 | 通过 | 【实测】最终普通 94/94、ASan/UBSan 94/94，均无跳过；预算不变 | V1 |
| G2/G5 | 通过范围检查 | 【实现】只改桥接生命周期、HAL 失效句柄及测试；没有新增用户操作、页面、设备任务或内部 RAM 缓冲。设计改动仅修任务明确指派的 404 | 源码差异 |
| G3 | 已更新 | 【实现】ui-dependencies 仅修改文本、页面快照、升级、电源四行的状态单元格，保留原判断 | 文件差异 |
| 提交和隐私 | 通过 | 【实测】本地提交；基线差异 10 文件，隐私 0 匹配，空白和链接检查通过 | V4 |

V1：两套均按总规则完整参数配置，保留 `INK_WERROR=ON`，内存套仅另加 `INK_SANITIZE=ON`；中枢测试使用 `work/hub-venv/bin/python`，以指定 Python 3.11.15、uv 0.9.5 按 `hub/uv.lock` 冻结安装。逐条串行执行：

```sh
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

均退出 0。最终普通 94/94（41.65 秒），ASan/UBSan 94/94（122.34 秒），无跳过；日志 `build/services-mid-ctest-final.log`、`build-asan/services-mid-ctest.log`。新增 `services-mid-regressions` 复用原四项专项及 `undo empty_removal`，每套 8 次启动、64 项检查、8 次 test.finished=1、0 失败；证据 `build/services-finish-ru55h_rw/`、`build-asan/services-finish-ullva7l0/`。文档检查两套均覆盖 95 个 AI 调用、6 个限制调用、37 个状态、10 个设置、20 个事件的对应列。桥接回归将子进程描述符上限设为 64，条件变量固定交接；原 HAL 代次测试和实际模拟中枢端到端也通过。gui-smoke 使用 SDL dummy，未启动带窗口的模拟器。

V2：`UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_mid/reverse.py`。只从 Git 读旧源码到忽略的构建目录，不撤回工作区。R4/404 对照 `2c844243`，四项已有修复对照 `6f689264^`；脚本退出 0，六项旧行为各按预期被拒绝。四项服务共 6 次模拟器启动均退出 0 且 test.finished=1，共 17 项功能断言失败；编译错误和崩溃不算复现。本次反向使用普通构建。证据 `build/services-mid-reverse-jcnrfkk1/`、`build/services-finish-3yrhlf8z/`，日志 `build/services-mid-reverse.log`。

V3：`"$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/stack_meter.py`，内存套增加 `--build build-asan`，均退出 0、栈计量 0 失败；证据 `build/services-stack-x36_mlq3/`、`build-asan/services-stack-2iwgo4qk/`。

V4：【实测】`git diff --stat 2c844243..HEAD` 核对 10 文件；`git diff --check 2c844243..HEAD` 退出 0。按总规则原隐私正则检查完整提交差异，0 匹配；全部变更文件小于 5 MB，本地 Markdown 链接无缺失；受保护需求、想法、决策、研究、简报和审计原文均未改。没有新增忽略返回值的 write/read/fgets 调用，新增 send/recv 检查返回值。结果 `build/services-mid-closure.json`；本轮未 push、开 PR、改远端或触碰设备。

## 没做到的

本节无未完成项。只有 macOS/Apple clang、模拟器和回环合成中枢实测；没有 Linux gcc、实机、真实厂商或真实家庭数据结果。

## 和设计不一致的

没有改变设计合同迁就代码。404 仅补已有接口并说明文档检查的真实覆盖；自动生成完整能力总表仍归 capability-table 任务。

桥接回收由宿主线程和连接句柄共同控制：句柄关闭不等待线程，线程结束清 socket 和待发队列，双方都结束再释放对象；迟到事件只保留原 owner/代次的独立副本，继续走原 HAL 校验。模拟器退出时才在宿主侧等待所有线程收尾。开发桥仍默认关闭。

## 改过的已有测试

未改任何已有断言、期望、预算、场景、等待阈值或跳过规则。仅新增 CTest 登记，复用原专项；新增桥接回归、文档检查和可复现的反向脚本。桥接测试用条件变量固定线程交接，无 sleep。

## 要用户定的

无。

## 顺手发现

当前 services-finish 的 REVIEW 是第 6 轮复查结论，REPORT 已记录其后修复；四项中级在基线已修，不重复改实现。配置命令的中/粗字体变量目前仍未被 CMake 使用；保留规定参数，没有扩展字体管线。
