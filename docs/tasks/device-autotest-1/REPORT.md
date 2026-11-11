# device-autotest-1 执行报告

基线：`517fedc5cee5290adf31251c22556d9e0f061915`。只在当前任务分支提交，不推送，不连接设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过（宿主机） | 【实测】4/4 专项；395 项普通全量覆盖原脚本；真实总线打开相机、危险调用拒绝；输入边沿/坐标/长按/脱敏/确认拦截；默认/OFF 不编入执行器 | `ctest --test-dir build --output-on-failure -j2 -R '^autotest-'` |
| 2 | 通过（macOS 全量/内存检查及 S31 构建） | 【实测】普通和 ASan/UBSan 全量均 395/395、ctest 无跳过；S31 正常版/测试版均编过；大小、关闭后字节比对见下 | `ctest --test-dir build{,-asan} --output-on-failure -j2`（两目录分别跑）；`INK_AUTOTEST=OFF/ON bash ports/esp32s31/build.sh` |
| 3 | 已完成文档 | 【实现】[使用说明](USAGE.md)，包括三个脚本前提、trial_run 的恢复行为与正常版重装、日志解释 | 本任务未运行设备命令 |

## 没做到的

- 本任务禁止连接、烧写和操作设备；真机结果由主会话复核。
- 未在 Linux 主机运行全量测试；本机为 macOS，使用指定的 GCC 核对工具补查编译警告，不把它记作 Linux 运行结果。

## 和设计不一致的

- 来源沿用现有 `INK_SRC_APP`，日志明确记 `source=test`，没有新增能力来源枚举；能力头文件不在本任务可改范围。`ink_cap_invoke_json` 与 `ink_cap_invoke` 共用内部调用实现，保留 JSON schema 和成员权限检查。
- note/mark 正文统一不回显，用源文件行号识别标记，避免脚本正文混入隐私日志。
- 未增加硬件参数或驱动；输入调用现有服务 sink，再进原 UI 队列。没有脱离 HAL，也未添加测试专用页面或能力。

## 改过的已有测试

- 没有修改已有测试和断言。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】`system/apps/remote/remote.c:805` 起的远程网页能力（包括提交和点按）标为导航类，仍可能对外操作；本任务仅在集中拒绝清单中挡住 `remote.*`，未修改远程网页模块。

## 新增的机制

- 任务书第 1、5 项：固件 `INK_AUTOTEST`（默认 OFF）、`INK_AUTOTEST_SCRIPT` 及编译时字节数组；无新持久格式。
- 任务书第 2 项：设备 `autotest` 短命任务，低优先级、4096 字节 PSRAM 栈，结束释放动作表并返回；通过已有 UI 执行器调度动作，不阻塞输入任务。
- 任务书第 2 项：模拟器只在出现 `cap` 时建立一个 4096 字节栈的短命运行器，把调用从 ISR 上下文转交已有 UI 执行器；不含 cap 的旧脚本不建此任务。
- 任务书第 4 项：测试固件通过链接包装复用现有启动、输入、能力与确认入口，集中拒绝危险操作；未改驱动或其他应用。

## 要改的文档

- 暂无。

## 规模与更小方案

- 产品 C/头文件：diff 新增 526、删除 279，净增 247 行（含原解析搬移）；连同全部构建/测试注册配置净增 290 行，低于 400 行预算。测试、说明和合成读物另计。
- 为什么不能更小：任务同时要求共用解析、设备时序执行、统一输入、安全拦截与默认关闭构建；复制一份解析会破坏共用要求，直接调用页面会绕过输入/能力，借 UI 执行器等待时间会阻塞界面。因此保留原解析分支，只增加短命任务和入口包装。

## 过程与证据

- 【实测】开工时工作区干净，HEAD 与指定基线相同。
- 【实测】正常 S31 构建成功，镜像 4,369,472 字节；`compile_commands.json` 中 `system/autotest/` 翻译单元为 0。
- 【实测】最终测试 S31 构建成功，嵌入 `camera.script`，镜像 4,379,856 字节（增加 10,384）；自动测试翻译单元恰为 `device.c`、`script.c`。两种编译均没有编译器 warning/error；ESP-IDF 自带的组件依赖配置警告仍有。
- 【实测】固定 `SOURCE_DATE_EPOCH=1794405426` 后从 ON 切回 OFF，`cmp` 与首次 OFF 镜像一致；两者 SHA256 均为 `eef928a026408668ec2ebb6a765d5d4fde8cd2a6c77855524ef578dad1d0e9c3`。未改任何供普通固件编译的既有 C 文件。
- 上述字节比对保持相同提交版本元数据；后续本地提交会改变固件版本标记，不能把不同版本元数据的镜像混比。
- 【实测】`nm` 检查：正常 ELF 无 `__wrap_ink_main`，测试 ELF 有该符号；正常固件动作解析与执行翻译单元为 0，测试固件为 2。
- 【实测】S31 栈使用文件：`run.part.0=752`、`dispatch=368` 字节（单函数静态栈，不代表完整调用链峰值）；任务配置栈 4096 字节 PSRAM。未做实机高水位测量。
- 【实测】专项测试原话：`100% tests passed out of 4`。三个现成脚本在模拟器中：相机保持 20 秒、取景更新能力调用 7 次；阅读 `reader.page_turns=20`；备忘键盘实际输入 `hello`、五次退格清空，11 次触摸均到驱动。
- 【实测】反向对照 `tests/device_autotest/negative.py build`：`without-cap-guard: expected failure exit=-6`、`without-input-forwarding: expected failure exit=-6`；原文件未改。
- 【实测】`ink-codex/tools/gcccheck.py` 最终结果：`gcccheck: 5 compiled, 0 with findings`。检查共用解析、模拟器脚本及三个 C 测试入口；设备执行器也通过包含它的宿主机测试入口核查，并已用 S31 GCC 编过。
- 普通全量首次使用默认字体 Python 环境，已有 hub 测试因缺依赖跳过；已停止该轮，配置现有只读 `$INK_DEPS/hub/.venv/bin/python` 重跑。未放宽断言、未安装或升级依赖。
- 【实测】普通最终全量原话：`100% tests passed out of 395`，`Total Test time (real) = 682.05 sec`，没有跳过。命令前均有 `taskpolicy -c utility`，构建和 ctest 均为 `-j2`。
- 【实测】内存检查用同一组字体/LVGL/家庭中枢依赖，目录 `build-asan`，`INK_WERROR=ON`、`INK_SANITIZE=ON`；编译全部目标，运行全部 395 项。原话：`100% tests passed out of 395`，`Total Test time (real) = 1467.62 sec`；ctest 无跳过，无 sanitizer 失败。
- 【实测】收尾专项再跑原话：`100% tests passed out of 4`，6.31 秒；两项反向对照仍分别以 -6 退出。集成用真实注册名 `phone.emergency`，另由单元测试验证低风险标记的电话/远程能力仍会被拒绝。
- 【实测】差异只涉及 21 个任务范围内文件；文档链接和 `git diff --check` 通过；按总规则第 6 节的表达式检查完整基线差异，隐私命中为 0。构建产物和本机路径未提交。
