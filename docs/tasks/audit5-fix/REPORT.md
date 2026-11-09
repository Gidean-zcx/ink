# audit5-fix 执行报告

基线：`8d2a1d9a368bf9d50084dbc1810cea33e1705dde`。四项指定场景均有正反向证据，但 B5-02 仍有两个旧回归未解决，**整体未达到验收标准**。产品代码截至 `18fd11bf`，之后仅更新报告；全部本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B5-01 确认说明 | 通过【实现＋实测】 | 在统一确认说明中列出实际清卡范围；`audit5-b5-01` 核对 true / false / 省略参数的真实待确认列表，均取消，家长密码和 AI 不可代确认保持。换回基线 `confirm.c` 后 4 条断言失败 | 下文定向及反向命令，case `confirmation` |
| B5-02 索引持久性 | 指定场景通过，旧回归未通过【实测】 | `audit5-b5-02`：成功后 1 μs 断电，新进程恢复；新增文件有编号，统计 8 B；改名原编号仍可用，统计 3 B。换回基线后分别失败 3 / 2 条；容量扫描和 BG 清理旧回归仍失败 | 下文命令，case `index`；`build/audit5-fix/final-regressions.log` |
| B5-03 回执成员 | 通过【实现＋实测】 | 回执默认当前家人，日志行显式传 `member`，与日志共用鉴权；`audit5-b5-03` 测孩子日志→主页→独立回执、真实点击孩子日志行及儿童越权拒绝。换回基线 `ai_pages.c` 后 4 条断言失败 | 下文命令，case `receipts` |
| S31 等待换算 | 定向通过【实测】 | `unit-s31-osal-ticks`：相对时长向上取整加 1，保留 0、永久等待和饱和；起点差及 tick 回绕不改变结果；基线实现断言失败 | 下文定向命令 |
| 普通全量 | 未通过【实测】 | 最终 267/271，退出 8，280.52 秒；容量扫描、BG 清理及两项基线 AI 场景失败；四项指定回归通过 | 下文全量命令；`build/audit5-fix/ctest-final.log` |
| ASan/UBSan 全量 | 未通过【实测】 | 268/271，退出 8；容量扫描及两项基线 AI 场景失败；四项指定回归通过，sanitizer 诊断 0 匹配 | `build-asan/audit5-fix/{build,ctest,sanitizer-scan}.log` |
| S31 编译 | 通过【实测】 | `build.sh` 退出 0，GNU 15.2.0；未操作设备 | `taskpolicy -c utility bash ports/esp32s31/build.sh`；`build/audit5-fix/s31-build.log` |
| 范围、行数、隐私 | 通过【实测】 | 仅 11 个相关文件；产品 C/H 增 55、删 40，净增 15 行；另有 9 行 CTest 注册，合计净增 24，均低于 150。差异格式检查通过，任务指定隐私扫描 0 匹配 | `git diff --stat`、`git diff --numstat`、`git diff --check`（均相对基线）；`build/audit5-fix/{final-checks.json,privacy-scan.log}` |

【实现】B5-02 在新路径成功前持久化当前卡的完成标记失效，只有存在未落盘版本才等待；复制在任务启动前失效，改名/删除在 `index_edit` 末尾统一等一次。完成标记已删除后无需再次写盘；重启因此不再复用旧完成记录。没有修改 `index_background` 的批次落盘。

## 没做到的

- 三轮调整后 `file-index-regressions` 的容量用例仍未通过：应返回 `INK_FS_ERR_INDEX_FULL`，实际返回 IO；诊断副本测得等待版本 707，3 秒后仅持久到 675，数据库错误码为 0、存储还有 2 项排队。`integration-races` 仍在上一场景启动后台删除后、下一场景等待 BG 卡点的 2 秒窗口失败。基线对照两项均通过，属本次未解决回归。按总规则 §7 不再继续尝试；不改后台批次落盘、存储服务、断言或预算。证据 `capacity-wait-diagnostic.log`、`final-regressions.log`（均在 `build/audit5-fix/`）。
- 首次全量暴露的换卡状态副作用已修正；`backup-restore-recovery` 的 4 KiB 合成任务栈告警已通过只读取完成记录的 16 字节卡身份修正，原测试通过。没有抬高栈预算。
- 普通全量与 ASan/UBSan 全量均未全通过；内存版 268/271、退出 8、427.19 秒，未检出 sanitizer 诊断，不能据此声称全量通过。普通版的 BG 清理失败在内存版没有复现。
- 没有运行 Linux 宿主 GCC、远端 CI 或设备；断电均为模拟器断电。

## 和设计不一致的

【实现】未修改设计合同。确认说明仍由统一策略层生成；按钮与 AI 共用回执能力和成员校验。文件持久性集中在失效处理和 `index_edit` 末尾；`index_background` 的批次写盘完全未改。

## 改过的已有测试

- `tests/esp32s31/osal_ticks.c`：1 μs 期望 1→2 ticks，10,001 μs 期望 2→3 ticks；这是任务明确要求的“向上取整后加 1”，不是放宽预算。新增起点差、整 tick、回绕及频率用例。
- `tests/file_index_3/run.py`：两个反向变体的源码匹配串跟随失效调用的新位置和参数更新，断言、预算不变。

## 要用户定的

暂无产品决定；本次未解决回归交主会话复核，不能按全量通过合入。

## 顺手发现

【实测】把三个改动过的模拟器产品文件均替回基线后，`scenario-ai_page_version-metalio-plus` 仍为 done=5、没有 stale；`scenario-ai_tools-metalio-plus` 仍丢 90 ms 麦克风数据。两个相同失败与本次修复无关，不改断言或预算。证据 `build/audit5-fix/*-all-baseline.log`。对照使用相同构建参数、真实 `sim-configured` 入口及新输出目录。

## 新增的机制

无新增任务、状态机、文件格式或配置项。`durable` 仅区分现有文件操作和不应阻塞的平台回调；后台调度机制不变。

## 要改的文档

- 主会话复核后同步 B5-01、B5-03、S31 换算及 B5-02 的最终状态；B5-02 不能直接标为全部修复。原审计记录本轮未改。
- `shell.ai_receipts` 的可选 `member` 参数需主会话重生成共享能力清单；未修改生成的 `docs/system/capabilities.md`。

## 复现命令与证据

- 配置沿用任务书字体、Python、LVGL，额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。中枢测试使用本检出代码：`PYTHONPATH="$PWD/hub"`。
- 全量：`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版将目录改为 `build-asan`。两轮均执行全部 271 项，无跳过；构建与全量测试串行，并行度均不超过 2。
- 定向：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R 'audit5-|unit-s31-osal-ticks'`，4/4 通过，退出 0。证据 `build/audit5-fix/fixed-tests.log`。
- 反向（case 分别为 `confirmation`、`index`、`receipts`）：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit5_fix/run.py --build build --case CASE --baseline 8d2a1d9a368bf9d50084dbc1810cea33e1705dde`。每次只替换对应产品文件，保持其余修复和最终测试；三次均按业务断言失败退出 1。证据 `build/audit5-fix/reverse-{confirmation,index,receipts}.log`。测试驱动检查实际 `test.fail`，不把模拟器退出 0 误当通过。
- 首次定向执行曾因尚未生成字体/图标未进入业务场景；生成资源后重新运行。此启动错误不计入反向证据。
- 两个已有索引反向变体：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/file_index_3/run.py --build build --mutant file-change`（以及 `card-change`），均检出预期失败，驱动退出 0；证据 `build/audit5-fix/reverse-existing-*.log`。
- 内存版按任务书使用 `build-asan` 和 `-DINK_SANITIZE=ON`，其余配置相同；编译参数核对包含 `-Werror -fsanitize=address,undefined`，全量仍使用 `-j2` 和 utility 优先级。诊断扫描覆盖 CTest 输出及 `Testing/Temporary/LastTest.log`，0 匹配。
- S31 反向：把基线头文件放在 `build/audit5-fix/osal_ticks.h`，将同一单测的 include 改指向它，`cc -Wall -Wextra -Werror -UNDEBUG` 编译后运行，起点差断言触发 SIGABRT；证据 `build/audit5-fix/reverse-ticks.log`。可复现如下：

```sh
git show 8d2a1d9a:ports/esp32s31/osal_ticks.h > build/audit5-fix/osal_ticks.h
sed 's#../../ports/esp32s31/osal_ticks.h#osal_ticks.h#' tests/esp32s31/osal_ticks.c > build/audit5-fix/ticks_reverse.c
taskpolicy -c utility cc -Wall -Wextra -Werror -UNDEBUG build/audit5-fix/ticks_reverse.c -o build/audit5-fix/ticks-reverse
taskpolicy -c utility build/audit5-fix/ticks-reverse
```
