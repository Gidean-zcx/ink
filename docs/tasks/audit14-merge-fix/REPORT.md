# audit14-merge-fix 执行报告

- 基线：`bebb20b98de31214ffa8bbd0742ad8071899db46`。
- 范围：只修 `boot-flow` 和 `hub-simulator-e2e` 的合并回归；仅宿主机测试与 S31 编译，不接触设备。
- 状态：修复和验证结束；验收 2 的严格全流程零警告未达到，原因见下。仅本地提交，不推送、不接设备。产品代码只替换 1 行（+1/-1）。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 普通及 ASan/UBSan 原话均为 `100% tests passed out of 345`，分别 452.52、529.12 秒，退出均为 0，无跳过；专项 6/6；撤回一行后二者均重现原失败；测试未改 | C1–C3；`focused.log`、`ctest.log`、`ctest-asan.log`、`reverse-boot.log`、`reverse-hub.log` |
| 2 | 【实测】部分通过 | S31 构建、镜像、size、stack 退出 0，镜像 3191312 B，产品编译/链接 0 警告；仍有 5 条 SDK 配置警告和 1 条 shell 补全警告。gcccheck 原话 `gcccheck: 1 compiled, 0 with findings`，退出 0、无跳过。隐私扫描 0 匹配，diff 检查通过 | C4–C6；`gcccheck.log`、`s31.log`、`s31-summary.json`、`privacy.log`；`build-s31/export.log` |
| 3 | 【实现】【实测】原因已定位 | `cap.c:122` 的短路求值跳过原来的电源锁读取；只恢复读取顺序，两项均通过 | 下节及 C1 |

## 原因和修法

- 【实现】合并前 `1390e166` 的 `cap.c` 在普通页面上先执行 `ink_power_state()`，再判断具体能力。`3ba30618` 把 `c->locked_owner_only` 放到了它前面；普通能力的该标记为假，因 `&&` 短路，不再执行这次读取。`ink_power_state()` 不是无副作用的字段读取：`system/services/power/power_service.c:478–482` 会获取和释放电源互斥锁；模拟器的 `ports/sim/osal/osal_sim.c:522–525` 对锁操作计费并允许调度。因此布尔结果相同，任务交错不同。这是两项失败共有的触发点。两分支共同基线为 `41d7f251`；主线随后加入的 `log-1`（`a0e058ad`，能力日志实现在 `35721739`）改变了当前日志、存储和刷新负载，旧审计分支的结果没有覆盖这一组合。
- 【实测】先只替换旧 `gate()`，两项通过；再保持新 `gate()`，仅交换该表达式的前两个操作数，两项仍通过。最终只改 `system/policy/cap.c:122` 这一行，不改测试、超时、预算或模拟器计费。保留 `.locked_owner_only`、`ink_ui_locked_activity()` 和 `.app`，不恢复能力名前缀特判。
- 【实测】`hub-simulator-e2e` 的产品初始化时间两边同为 2107.476 ms；失败版成员准备到 5056.542 ms 才结束，3 秒点按先进入 `BootWifi`，后续点按没有进入阅读器，所以模型没有 `reader.next_page` 可调，`tool_calls=0`。修复后准备时间为 2417.653 ms，原定 3 秒点按及工具调用断言通过。证据：`baseline-e2e-metrics.json`、`baseline-e2e.txt`、修复后 `build/test-out/hub-e2e/`。
- 【实现】【实测】恢复期间密集的 `job.list` 来自 `activity_card.c:207–209` 对作业进度的订阅；恢复结束到 `BootHello` 截图之间，最近一次调用在 58835 ms，没有继续密集轮询（后续离开设置、切换成员仍会正常调用）。`BootHello` 于 58846 ms 打开；`time_service.c:198–203` 的分钟通知经 `navigation.c:1838–1851` 刷新状态栏，恰与截图相撞。trace 显示 60311.778–60329.761 ms 界面任务正在运行，60334.604 ms 又提交一帧，原截图断言在 60321 ms 正确拒绝未呈现完成的画面。修复恢复原有交错后，fresh/restore 的 11 张截图全部通过。证据：`baseline-boot.txt`、`current-restore-trace/trace.json`。
- 【实现】这是恢复既有电源锁读取顺序的兼容修复；没有将偶然等待改成延时或增加同步机制，也不声称消除了所有可能的调度敏感窗口。
- 【实测】反向对照只在临时副本撤回这一行，断言副本与基线 `cap.c` 逐字相同，按现有编译参数重编该对象并链接原测试。`boot_flow_check.py` 退出 1，重现 60.321 秒的同一断言；`e2e.py` 退出 1，重现 `tool_calls=0`。原工作文件未变。复核者也可在基线单独检出，用 C0/C1 复现；无需修改任何测试。
- 【实现】架构审视（R-061）：改动只在能力网关，按钮和 AI 仍走同一条原子能力路径；成员的应用归属与锁屏声明保持不变，没有新增接口或绕过服务边界。

本节未加目录前缀的诊断文件均在 `work/audit14-merge-fix/`；它们被 Git 忽略，不提交构建产物或本机路径。

内存检查覆盖全部 345 项（包括本机中枢环境），编译命令已核对包含 `-fsanitize=address,undefined` 和 `-Werror`，运行设置 `UBSAN_OPTIONS=halt_on_error=1`。两轮 CTest 和 `LastTest.log` 中均无 ASan/UBSan 错误诊断。

S31 编译命令表已核对：用实际 RISC-V GCC 编译当前检出的 `system/policy/cap.c`，不是复用旧产品对象。提交范围仅该文件和本报告；无超过 5 MiB 的变更文件，无构建产物或共享文档。

## 复现命令

使用任务给定的 `INK_DEPS`、`INK_DESIGN`。中枢复用只读依赖目录中已锁定的 Python 3.11.15 环境（aiohttp 3.12.15、PyYAML 6.0.3、pytest 8.4.2）；`PYTHONPATH` 指向当前检出，运行的是当前分支的中枢代码。

```sh
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1
# C0：配置遵照总规则第 5 节，另加：
# -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
# C1：专项
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^(boot-flow|hub-simulator-e2e|audit14-locked|audit14-ownership|lock-camera|kid-apps)$' --output-on-failure -j2
# C2：普通全量
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# C3：按 C0 同样配置 build-asan，另加 -DINK_SANITIZE=ON，保留 -DINK_WERROR=ON
taskpolicy -c utility cmake --build build-asan -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C4：仅编译 S31
taskpolicy -c utility bash ports/esp32s31/build.sh
# C5：实际 GCC 检查唯一改动的产品文件
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" ../../tools/gcccheck.py build system/policy/cap.c
# C6：总规则第 6 节的隐私模式，检查 bebb20b9..HEAD 的完整 diff
```

## 没做到的

- 【实测】没有达到严格的 S31 全流程 0 警告。固定 SDK 提交 `fff9895c82d744c7237be8847347bdd1b07c6643` 的 `tools/cmake/component_validation.cmake:106` 产生 5 条警告：`wpa_supplicant` 使用 Wi-Fi 的三个私有目录，`esp_wifi` 使用 supplicant 的两个私有目录。这与 `docs/tasks/port-wifi-1/REPORT.md:40` 已记录的问题相同。另有系统 Bash 3.2.57 不能载入 SDK 补全的 1 条警告。SDK 位于本任务可写范围外；遵照总规则不修改或升级依赖，也没有关闭校验、过滤日志或压制警告。
- 没有进行实机验证；本任务仅编译与宿主机测试。

## 和设计不一致的

- 暂无。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 【实测】宿主机的 Apple ld 在基线及修复后构建均提示既有的重复库链接：`ignoring duplicate libraries: 'libink_port_sim.a'`。未扩大范围修它；S31 警告另按它自己的构建日志统计。

## 新增的机制

- 无。

## 要改的文档

- 无。没有增删能力或修改能力声明，无需重新生成能力清单。
