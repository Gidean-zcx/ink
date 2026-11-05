# services-mid 执行报告

2026-11-06；基线 `2c844243`，分支 `claude/codex-services-mid`。只处理 [任务清单](../backlog.md) 的 services-mid，遵守 [总规则](../codex-rules.md)。以下【实测】均为本次执行，内存检查尚在进行。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| AUD18-106 | 基线已修，本次复验通过 | 【实测】失败版本迁移、第二次回滚和重启后历史保留；旧代码 3 项功能断言失败 | V1 `update_history`；V2 |
| AUD18-206 | 基线已修，本次复验通过 | 【实测】显示提交失败不报关机图已显示，重试成功；旧代码 2 项功能断言失败 | V1 `offpanel`；V2 |
| AUD18-207 | 基线已修，本次复验通过 | 【实测】长选区、两侧标记和拼音位置映射；旧代码 3 项功能断言失败 | V1 `text_window`；V2 |
| AUD18-402 | 基线已修，本次复验通过 | 【实测】普通/数字/密码键盘与页面组合快照，目标版本及密码掩码；旧代码 9 项功能断言失败 | V1 `keyboard_snapshot`；V2 |
| AUD18-404 | 已修 | 【实测】补齐 8 个能力、1 个状态、1 个事件；新增检查解析文档的对应列，旧文档准确检出这些漏项 | V1 `services-mid-capability-doc`；V2 |
| services-finish 其余中级 | 无额外未修项 | 【实现】当前第 6 轮 REVIEW 的中级均列已修，之后 REPORT 也确认保留；【实测】R1-06 的 `undo`、R2-04 的 `empty_removal`、S4-DET 的两项确定性和栈计量均通过 | V1；V3 |
| hub-step1 R4 | 已修 | 【实测】同进程 80 次开关（72 次成功、8 次连接失败）、线程/描述符启动失败、退出清理；每次关闭后描述符回到 3，迟到回调安全；旧代码保留连接和待发消息，测试退出 1 | V1 `hub-bridge-lifecycle`；V2 |
| G1 | 普通通过，内存待验证 | 【实测】首轮普通 94/94，无跳过；预算不变 | V1 |
| G2/G5 | 通过范围检查 | 【实现】只改桥接生命周期、HAL 失效句柄及测试；没有新增用户操作、页面、设备任务或内部 RAM 缓冲。设计改动仅修任务明确指派的 404 | 源码差异 |
| G3 | 已更新 | 【实现】ui-dependencies 仅修改文本、页面快照、升级、电源四行的状态单元格，保留原判断 | 文件差异 |
| 提交和隐私 | 收尾待执行 | 空白、链接、范围和指定隐私扫描 | V4 |

V1：两套均按总规则完整参数配置，保留 `INK_WERROR=ON`，内存套仅另加 `INK_SANITIZE=ON`；中枢测试使用 `work/hub-venv/bin/python`，以指定 Python 3.11.15、uv 0.9.5 按 `hub/uv.lock` 冻结安装。构建 `cmake --build build -j4`，全量 `SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3`。新增 `services-mid-regressions` 调用原 `tests/services_finish/run.py` 的四项专项及 `undo empty_removal`。首轮普通 94/94（62.94 秒）；随后增强迟到回调测试，两项专项通过（17.60 秒）。日志 `build/services-mid-ctest.log`、`build/services-mid-focused.log`。最终两套全量结果待更新。

V2：`UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_mid/reverse.py`。只从 Git 读旧源码到忽略的构建目录，不撤回工作区。R4/404 对照 `2c844243`，四项已有修复对照 `6f689264^`；脚本退出 0，六项旧行为各按预期被拒绝。四项服务共 6 次模拟器启动均退出 0，共 17 项功能断言失败；编译错误和崩溃不算复现。证据 `build/services-mid-reverse-jcnrfkk1/`、`build/services-finish-3yrhlf8z/`，日志 `build/services-mid-reverse.log`。

V3：`"$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/stack_meter.py`，退出 0，栈计量 0 失败；证据 `build/services-stack-x36_mlq3/`。

V4：待最终范围、隐私、链接检查及提交。

## 没做到的

内存全量及最终收尾尚在执行。只有 macOS/Apple clang、模拟器和回环合成中枢实测；没有 Linux gcc、实机、真实厂商或真实家庭数据结果。

## 和设计不一致的

没有改变设计合同迁就代码。404 仅补已有接口并说明文档检查的真实覆盖；自动生成完整能力总表仍归 capability-table 任务。

桥接回收由宿主线程和连接句柄共同控制：句柄关闭不等待线程，线程结束清 socket 和待发队列，双方都结束再释放对象；迟到事件只保留原 owner/代次的独立副本，继续走原 HAL 校验。模拟器退出时才在宿主侧等待所有线程收尾。开发桥仍默认关闭。

## 改过的已有测试

未改任何已有断言、期望、预算、场景、等待阈值或跳过规则。仅新增 CTest 登记，复用原专项；新增桥接回归、文档检查和可复现的反向脚本。桥接测试用条件变量固定线程交接，无 sleep。

## 要用户定的

无。

## 顺手发现

当前 services-finish 的 REVIEW 是第 6 轮复查结论，REPORT 已记录其后修复；四项中级在基线已修，不重复改实现。配置命令的中/粗字体变量目前仍未被 CMake 使用；保留规定参数，没有扩展字体管线。
