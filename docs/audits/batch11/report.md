# batch11 独立复查报告

日期：2026-11-10；新加坡时间 09:32 开始，09:56 完成，未超过一小时。范围：`32cff5a9..c34d323bc6e8c1d11a5b34b30251862cffd445cb`。当前检出为范围终点。只读产品代码和已有测试；不提交、不接触设备。一次性实验和构建证据放在忽略的构建目录。

本轮确认 **2 条中等级问题**，未发现有证据的严重、高或低等级问题。普通版和 ASan/UBSan 全量均 **301/301**，S31 编译通过；两条问题来自全量尚未覆盖的索引异常路径。结论标签：【实测】本轮执行，【实现】源码核查，【估算】主机模型或静态推算。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 正确性和成员隔离 | 存在 B11-01、B11-02 | 完整扫描、拔卡 / 换卡及重启的已有回归通过；额外清理途中 Flash 断电恢复通过 | 全量与下方补充实验 |
| 2 确认及秘密字段 | 已查路径未发现规则放松 | 钩子在原门禁后运行；失败不入队，确认后仍复核通话；儿童、PIN、凭据掩码及确认落盘回归通过 | `integration-confirm`、`audit5-b5-01`、`audit7-b7-01`、`confirm-durable`、`confirm-pages`、`phone-calls` 均在全量中 |
| 3 能力和页面快照 | 两条旧问题的修复成立 | 短信页持续打开时，屏幕和快照出现入库消息；网络设置第二页不列隐藏按钮，返回第一页可点击且仍进确认 | `audit10-b10-01`、`audit10-b10-02` 与原 `sms` 均通过 |
| 4 资源上限 | 已有预算和 S31 编译通过；余量仍紧 | 能力 / 订阅表未满；没有新增产品常驻任务；主机栈估计及静态内存见资源节 | 全量指标、容量探针、`build-s31/{size,stack}.txt` |
| 5 合并配合 | 存在 B11-02 的旧查询 / 新清理配合问题 | 短信主会话补丁已包含在本轮代码和回归中；未发现重复确认实现；索引手动扫描会提前结束新增清理阶段 | 公开 `file.index` 的独立实验 |
| 6 Linux / GCC | 已做交叉检查；原生 Linux 未做 | 11 个变更 C 文件通过 RISC-V GCC 15.2 `-O2 -Werror` 检查；S31 完整编译通过 | `build/batch11/gcc_check.py`、`ports/esp32s31/build.sh`；边界见验证记录 |

## 发现

### B11-01【中】一次文件状态查询失败会永久删掉仍然存在的文件编号

- 位置：`system/services/file/file_service.c:1756–1766`；相关 HAL 实现 `ports/sim/hal/hal_storage_sim.c:569–584`。
- 【实现】清理将 `hal_fs_size(path) < 0` 一律放入待删除列表。该接口的模拟器实现对所有 `stat()` 错误都返回 `-1`，并不能区分路径不存在与 I/O / 访问失败。卡仍挂载时，后面的 `hal_fs_card_present()` 不会拦住误删；已经完整扫描过该路径也没有被用来保护它。
- 【实测，故障注入】沿用 `index-prune` 的 60 个合成文件、离线删除 20 个和跨根改名 5 个操作，只在所有根扫描完成后，让仍存在的 `/content/SYNTH_0010.txt` 一次 `hal_fs_size()` 返回 `-1`，之后恢复正常。模拟器结束时状态已 ready、落盘已完成、脏标记已清除，但索引只有 **39 条，实际应为 40**；该文件仍有 11 B，原编号 **30** 已不存在，按路径找编号为 0。`test.fail=2`，失败是数量和保号断言。未修改产品文件、正式测试或暂停线程。
- 对照：同一驱动不注入失败时退出 0，58 条断言通过，索引 40 条、该文件仍为编号 30。故障场景模拟器本身退出 0，但指标明确有 2 条失败，不能将进程退出码当作验收通过。
- 【实测】用故障后的同一卡和 Flash 镜像重启，不再注入任何错误，完成后仍只有 39 条，原文件编号仍为 0；数量断言再次失败。命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/reboot_stat.py`；结果 `build/batch11/prune-kind-1/restart-summary.json`。这证明普通重启不会自行纠正这个完成标记。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/compile_driver.py --target sim-index-prune --replace tests/index_prune/device.c=build/batch11/prune_extra.c --replace tests/index_prune/boundary.c=build/batch11/prune_boundary.c --output build/batch11/prune_extra`；随后 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/run_prune_extra.py`。首次使用全新 `build/batch11/prune-kind-*` 目录。驱动生成方式在 `build/batch11/prepare_experiments.py`；命令、指标及日志在 `prune_extra.commands.json`、`prune-extra-summary.json`、`prune-kind-1/exercise.log`。
- 影响：文件内容未删除，但稳定编号及其缓存被清理，依赖编号的查找失效，完成标记还把不完整索引当作成功；不将此扩大描述为文件内容丢失或跨成员泄露。
- 最小修法：在清理的同一处只接受“明确不存在”作为删除依据。通过 HAL 的错误可区分查询，或成功的父目录枚举确认缺失；I/O 错误应终止本次清理并保留未完成状态，不能落完成标记。补单次查询错误的保号回归，无需新增后台任务或独立重试机制。

### B11-02【中】清理中查询空目录会取消后续清理，旧记录继续占用索引

- 位置：`system/services/file/file_service.c:1823–1852` 的新增分批清理与 `:1698–1702`、`:1729` 的原同步扫描完成逻辑未协调；公开入口为 `system/services/file/file_caps.c:339`、`:379–380`。
- 【实现】各根目录扫完之后，清理借用 `storage.saving=true` 保持未完成，并依赖 `FS.index_at` 调度下一批。普通 `file.index` 首次查询会调用 `index_scan()`；即使查询的只是一个空子目录，末尾的 `index_done(..., true)` 仍把同一个 `storage.saving` 清零，并取消后台定时器。此时所有根的扫描标志已齐，剩余清理因此失去调度。
- 【实测】60 个文件建索引；离线删除 20 个、跨根改名 5 个后归还。观察到各根已扫描、首批清理后写盘空闲时，经正常 AI 能力入口执行一次 `file.index({"path":"/family/SYNTH_EMPTY","limit":1})`，请求成功。随后仍沿用原用例的 90 秒完成等待，最后为 **57 条，实际应为 40**，17 个已删除文件的旧编号还在数据库里。共 19 个失败断言（完成等待、数量、17 个旧编号），5 次改名仍保号。模拟器退出 0，`test.fail=19`，不能误报通过。
- 【实测，对照与状态核对】同一空目录夹具不发查询时，58 条断言通过、40 条索引，脏标记清除。发查询时，最终 `file.index_state=ready`、索引任务和定时器均空闲，却保留脏标记与 57 条索引；新增只读观测再次得到相同 19 条失败。证据 `build/batch11/prune-empty-final-summary.json`。
- 实验只增加只读阶段观测和这次公开查询；没有故障注入、挂起线程、占住锁或改产品实现。普通较长的 `/family` 查询实验通过，说明是否已有后台任务入队会影响结果，不能仅用该路径排除空目录问题。
- 复现（含对照和最终状态）：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/compile_driver.py --target sim-index-prune --replace tests/index_prune/device.c=build/batch11/prune_empty_final.c --replace tests/index_prune/boundary.c=build/batch11/prune_empty_query_boundary.c --output build/batch11/prune_empty_final`；随后 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/run_prune_empty_final.py`。使用全新 `build/batch11/prune-empty-final-kind-*` 目录。最终指标在 `prune-empty-final-summary.json`，日志在 `prune-empty-final-kind-{0,3}/exercise.log`；初次复现另保留 `prune-empty-query-summary.json`。
- 影响：本次没有删错内容，但清理不能完成，已失效编号继续占用固定上限；文件查询、搜索、选文件等既有读取路径不能假定后台清理已经收尾。
- 最小修法：集中修正索引完成判定，让局部同步扫描不能结束尚未完成的整卡清理、取消其调度；只有清理游标到尾并且落盘完成后才能置 ready。补“清理批次间查询空目录”的断言，不在各 UI 调用方补丁式绕过。

## 没做到的

- 未运行 Ubuntu 原生 GCC / glibc 构建或远端 CI；交叉 GCC 通过不能覆盖它们的全部诊断和链接差异。
- 未做实机、真实通信、实际存储卡电气故障或所有断电时刻的验证。状态查询错误是模拟 HAL 边界注入，不能据此估算真机故障概率。
- 两条问题按本轮任务只报告，未修产品代码；实验源文件与产物仅保留在忽略的构建目录，未提交。

## 和设计不一致的

【实现】B11-01 不符合“仅删除已经不存在的路径”的约束；B11-02 不符合整卡清理完成后释放旧记录的验收。其余已查改动的架构边界成立：确认策略调用可选钩子，各能力提供绑定和说明；索引维护归文件服务；短信使用既有事件和 UI 执行器；Wi-Fi 触摸按钮和 AI 仍共用 `wifi.forget`。没有新增应开放却未开放的用户动作，凭据输入和人工确认边界未改变。

## 改过的已有测试

本轮无。被审计差异只给 `tests/integration/confirm.c` 追加测试能力与断言，其他测试是新增文件；未删除原断言、增加原用例等待、抬高资源预算或依赖新的历史提交才能运行 CTest。

## 要用户定的

暂无。

## 顺手发现

没有另外评级的发现。RAM / 栈的紧余量是既有风险，见资源节；未证实的推测不计入问题数。

## 新增的机制

本轮不修改产品实现。对被审计批次：

- `ink_cap_t.prepare_confirm`：confirm-hooks 任务明确要求的可选钩子；复用既有确认队列、参数和摘要，没有第二套确认门禁。
- 索引清理游标及 `full` / `pruned` 标志：index-prune 任务所需的既有扫描状态扩展；复用原后台执行器、16 条批次、数据库和卡上脏标记，不改持久格式和 1024 条上限。
- 短信页增加原子读取的已显示版本，网络页增加按钮页码；都复用既有页面和执行器，没有新增常驻任务、配置项或队列。

## 要改的文档

- 主会话按本轮发现更新索引清理的验收状态，并补充查询失败边界；本轮不修改共享文档。
- `docs/tasks/audit10-fix/REPORT.md` 保留了开发方停止时 B10-01 未完成的历史状态，最终修复另见同目录 `REVIEW.md`。已核对最终源码与测试，不能只读前者判断合并结果。建议接续入口注明这两个记录的先后关系。

## 资源证据

- 【实测，模拟器】普通全量中，目标 Plus 档案、无告警现场的最低内部可用总量（含 DMA 保留池）为 `media` 的 **33,424 B**，距 32 KiB 参考线 **656 B**。这是模型预算，不能当作真机余量。
- 【实测，主机栈估计】UI 峰值 **9,664 / 10,240 B**，后台执行器 **6,376 / 8,192 B**，媒体执行器 **7,552 / 8,192 B**；后两列是观测值 / 配置值。紧余量属于既有风险，不据此重复评级。原始现场分别为 `build/test-out/notify/`、`sms-compare/Sms/`、`audit3-b3-06/out/`，汇总在 `build/batch11/resource-summary.json`。
- 【实现】本批没有新增能力条目、事件订阅或状态订阅，也没有提高 JSON 的 128 token 上限。两处确认钩子重建参数后分别仅 3 项 / 2 项，低于参数表 8 项上限。
- 【实测，启动占用】能力 **368/512**、事件订阅 **33/64**、状态 **48/64**、状态订阅 **54/64**、设置 **22/48**、设置订阅 **7/64**、数据库定义 **20/32**；状态订阅仅余 10 项。这是整机启动时的计数，不是所有页面的峰值。只读探针 `build/batch11/capacity{,_state,_db}.c`；完整编译命令 `capacity.commands.json`，运行输出 `capacity-out/metrics.json`，退出 0、无告警。
- 【实测 / 静态】S31 链接统计内部 DIRAM **83,050 B**、外部 BSS **507,808 B**；内部数值含新增钩子对两张可写中枢卡模板的影响。`index_background` 编译帧 **1,120 B**（包含内联清理），短信 `received` **48 B**，两个确认准备钩子各 **32 B**。编译帧和 `stack.txt` 不覆盖完整间接调用、RTOS 上下文等，不能当作运行期栈上界。

## 验证记录

- 开工时 HEAD 为 `c34d323b`；工作区只有本轮未跟踪的任务目录，无产品代码差异。
- 已读取任务书、执行规则和三个开发任务的报告及主会话复查记录；以合并后的实际代码为准。
- 【实测】主机 Clang `INK_WERROR=ON` 配置及全量构建退出 0；日志 `build/batch11/configure.log`、`build/batch11/build.log`。使用 `$INK_DEPS` 的字体、Python、中枢虚拟环境和本地 LVGL / SDL2 / ImGui 源码，未修改依赖。
- 【实测】`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`：退出 0，301/301 通过，无跳过，210.95 秒。日志 `build/batch11/ctest.log`；原始现场保留在 `build/test-out/`。
- 【实测】ASan/UBSan 配置沿用普通版、保留 `INK_WERROR=ON`，仅换 `build-asan` 并加 `INK_SANITIZE=ON`；核对实际编译参数包含 `-fsanitize=address,undefined`。构建退出 0；同一全量命令换测试目录、加 `UBSAN_OPTIONS=halt_on_error=1`，退出 0，301/301 通过、无跳过，366.98 秒。日志 `build-asan/batch11/{configure,build,ctest}.log` 和 `build-asan/Testing/Temporary/LastTest.log`，未检出 sanitizer 诊断。
- 【实测】RISC-V GCC 15.2 对本批全部 **11 个变更 C 文件（6 个产品、5 个测试）** 按主机原警告选项及 `-O2 -Werror` 编译到汇编，均退出 0、无诊断。命令 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/batch11/gcc_check.py`；逐文件命令和结果 `build/batch11/gcc/results.json`。该检查使用不同目标和 libc，不等于 Ubuntu 原生构建或 glibc 的所有检查。
- 【实测】`INK_DEPS` 已设置，`taskpolicy -c utility bash ports/esp32s31/build.sh`：退出 0，完成 S31 编译、链接、尺寸和静态栈报告；日志 `build/batch11/s31-build.log`，报告 `build-s31/{size,size-components,stack}.txt`。未执行刷写或设备访问。
- 【实测】确认策略静态扫描没有具体 `call.*` / `system.*` 能力名或 `comm.h`；两处钩子都丢弃调用方伪造的私有绑定。原有确认、人工回答、家长密码、通话代次、成员隔离及秘密字段回归在两套全量中原样通过。
- 【实测】补充清理中断电：首条索引删除成功后，用现有 `sim_storage_arm_power_cut(1)` 撕裂下一次 Flash 写入，`flash.torn_ops=1`；从同一镜像重启后，29 条断言通过，恢复为 40 条，5 次跨根改名全部保留原编号、删除编号查不到。仅验证这一个实际切断点，不宣称穷举全部断电时刻。证据 `build/batch11/prune-kind-2/{exercise,restart}/metrics.json`，命令同 B11-01 的实验运行器。
- 【实测】构建 / 全量测试逐轮串行，统一 utility 优先级，最大并行度 2；未启动带窗口的模拟器。结束时受跟踪文件的 `git diff --stat` 为空，仅本轮任务目录未跟踪；没有产品或已有测试差异。
- 【实测】指定基线差异的隐私扫描只有 1 处命中：旧 `docs/audits/batch10/report.md:50` 引用规则指定的合成地址；无本机绝对路径。新报告扫描无命中。记录 `build/batch11/privacy.json`；差异格式、报告引用文件及末尾计数检查通过。

SEVERE=0 HIGH=0 MEDIUM=2 LOW=0
