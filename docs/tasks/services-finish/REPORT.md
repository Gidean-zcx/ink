# 系统服务收尾报告

2026-11-06，第 6 轮接续。开工 `4a792e9`，已将集成分支 `FETCH_HEAD=39a349d` 合入当前分支（`c3c217c`）。修复 `ad3eb9e`，集成测试准备修正 `d79c8b4`。依据[复查报告](REVIEW.md)、[任务书](../services-finish.md)、[总规则](../codex-rules.md)。本表【实测】均为本轮执行；旧缺陷重跑正向，本轮 R6 修复另做正反向，历史反向不冒充本次结果。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 合并 | 完成 | 【实测】唯一冲突在总规则配置命令，采用集成分支完整参数；保留 gcc 返回值要求及并发限制，FETCH_HEAD 为当前 HEAD 祖先 | V4 |
| R6-01 高：数据、时间能力串成员 | 已修 | 【实测】原探针 4 个换人分支失败；修复后真实写入、闹钟、提醒及儿童取消保持原身份/权限，原成人内容不进入新成员空间 | V0；V2/V3 `member_caps` |
| R6-01 同类：确认删除、删除与复用 | 已修 | 【实测】真人确认的数据删除只删原空间同编号记录；时间能力等待期间已删除/复用则拒绝。数据与删除共用后台执行器，按真实顺序完成，旧内容不串到家长或复用成员 | V2/V3 `member_caps` |
| R6-02 高：迟到通知标题泄漏 | 已修 | 【实测】持久写入后暂停生产者，真人删除并复用编号；恢复后本地订阅及实际模拟中枢都收不到旧标题，新成员收件箱为空，同成员控制正常 | V0；V2/V3 `member_caps` |
| R6-02 同类：时间事件及派生通知 | 已修 | 【实测】真实闹钟/提醒到期处理保留原代次；迟到事件不进入新 AI、本地订阅或派生收件箱，同成员控制正常 | V2/V3 `member_caps` |
| R5-01 高 | 保留修复 | 【实测】文件句柄、短操作、任务与录音/播放启动事件保持原身份；旧代次、目录及编号复用回归通过 | V2 `file_events file_reuse` |
| R5-02 高 | 保留修复 | 【实测】源/目标、stat、作业准备共用原成员权限；四类路径、目录拷贝、儿童权限及私有任务标题回归通过 | V2 `file_paths` |
| R3-01/R4-01 高 | 保留修复 | 【实测】阅读销毁、定时和入队窗口，成员写者/集合引用、文件、应用、媒体、通知与活动的迟到保存通过 | V2 `reader_late member_writes late_writers file_late member_inventory` |
| R3-02 高 | 保留修复 | 【实测】Wi-Fi 保存失败、闹钟/提醒版本及延迟撤销归属通过 | V2 `wifi_undo undo_schedules undo_owner` |
| R2-01 严重 | 保留修复 | 【实测】成员集合、文件、卡目录、闹钟、缓存、迟到写者、删除复用和重启通过 | V2 `member_inventory removal_alarm removal reader_late late_writers file_late` |
| R2-02 高 | 保留修复 | 【实测】请求号绑定成员代次、来源、能力及参数，在途重复与迟到回执隔离通过 | V2 `replay` |
| R2-03 高 | 保留修复 | 【实测】旧数据、文件、通知、时间与 Wi-Fi 撤销拒绝覆盖新版本，未修改控制可撤销 | V2 `undo_data` |
| R2-04 中 | 保留修复 | 【实测】空白卡清理正常，占用或失败仍保留编号 | V2 `empty_removal removal` |
| R1-01 高 | 保留修复 | 【实测】有效槽提交前后断电、提交失败及恢复四链通过 | V2 `upgrade_crash` |
| R1-02 高 | 保留修复 | 【实测】TTS/听书缓冲交接隔离、同成员锁屏播放通过 | V2 `playback`；V1 |
| R1-03 高 | 保留修复 | 【实测】1,600 次动作、单成员积压及恢复通过 | V2 `activity` |
| R1-04 严重 | 保留修复 | 【实测】权限、儿童上限、真人确认/拒绝、换人及权限撤销通过 | V2 `permissions policy` |
| R1-05 高 | 保留修复 | 【实测】卡、Flash、缓存、唯一 PIN 故障及在途写者清理通过 | V2 `removal member_writes file_late` |
| R1-06 中 | 保留修复 | 【实测】连续撤销、过时版本及过期通过 | V2 `undo` |
| AUD18-101 高 | 保留修复 | 【实测】不可读文件根不报清理成功 | V2 `files` |
| AUD18-102 高 | 保留修复 | 【实测】目录和短写持卡，USB 独占不穿透 | V2 `files` |
| AUD18-103 中 | 保留修复 | 【实测】作业先完成仍结束任务 | V2 `files` |
| AUD18-104 高 | 保留修复 | 【实测】状态写失败不切槽，缺状态不标有效 | V2 `update upgrade_crash` |
| AUD18-105 高 | 保留修复 | 【实测】资源恢复事务不被新包覆盖 | V2 `update` |
| AUD18-106 中 | 保留修复 | 【实测】失败升级历史迁移、跨重启及人工重试通过；同步旧文档状态 | V2 `update_history` |
| AUD18-107 中 | 保留修复 | 【实测】正常包及八类坏包通过 | V2 `update` |
| AUD18-108 中 | 保留修复 | 【实测】下载固定原包，两条 offer 变化链通过 | V2 `update` |
| AUD18-109 高 | 保留修复 | 【实测】遗忘集合不破坏独立定时器 | V2 `storage` |
| AUD18-201 高 | 保留修复 | 【实测】拥堵时停止采集与上传 | V2 `audio voice` |
| AUD18-202 高 | 保留修复 | 【实测】OOM 后不重做旧文本 | V2 `text` |
| AUD18-203 高 | 保留修复 | 【实测】录音结束且持久完成后关机 | V2 `shutdown` |
| AUD18-204 高 | 保留修复 | 【实测】耳机掉线不外放旧媒体 | V2 `audio` |
| AUD18-205 中 | 保留修复 | 【实测】通话后保留闹钟焦点 | V2 `audio` |
| AUD18-206 中 | 保留修复 | 【实测】拒绝提交不冒报关机图，恢复后正常关机；同步旧文档状态 | V2 `offpanel`；V1 |
| AUD18-207 中 | 保留修复 | 【实测】长选区窗口、右标记和拼音映射通过；同步旧文档状态 | V2 `text_window` |
| AUD18-208 中 | 保留修复 | 【实测】AI 不能提交隐藏字段 | V2 `text` |
| AUD18-209 中 | 保留修复 | 【实测】初始 UTF-8 截断保留完整字符 | V2 `text` |
| AUD18-401 高 | 保留修复 | 【实测】字段、内容及光标的过期会话/版本均拒绝 | V2 `bindings` |
| AUD18-402 中 | 保留修复 | 【实测】键盘组合快照保留目标/版本，凭据掩码；同步旧文档状态 | V2 `keyboard_snapshot` |
| AUD18-403 高 | 保留修复 | 【实测】未保存的低电量关机不冒报全部存好，另跑保存控制 | V2 `lowbattery` |
| S6-01 高 | 保留并补全 | 【实测】缓冲/传输旧事件、关闭空间和文件/通知/时间生产者原代次回归通过 | V2 `handoff replay file_events file_reuse member_caps` |
| S6-02 高 | 保留修复 | 【实测】采集、迟到帧和缓冲播放隔离通过 | V2 `isolation playback` |
| S4-DET 中 | 保留修复 | 【实测】全量确定性比较与栈计量通过 | V1/V2 |
| S1 | 本轮范围通过 | 【实测】R6 两高先复现后修复，同类均有正反向；旧严重、高和中逐组重跑通过 | V0/V1/V2/V3 |
| S2 | 保留修复 | 【实测】D-105、儿童权限、真人确认、凭据绑定、活动和撤销通过 | V2 `policy permissions activity undo undo_data undo_owner` |
| S3 | 保留修复 | 【实测】两条真实 AI 请求的完整 actions 六检查及预算通过 | V2 `actions`；V1 |
| S6/R-061 | 本轮范围通过 | 【实现】身份归总线，集合/时间修改和事件生命周期归原服务；按钮与 AI 共用能力。代次入口是私有机制，不让 AI 选择身份；没有新增常驻任务或内部 RAM 缓冲 | 源码；V2/V3 |
| S4 | 通过 | 【实测】普通 91/91（34.15 秒）、ASan/UBSan 91/91（93.29 秒），无跳过；两套专项各 190 次启动、2,188 项检查，另各 5 项保存控制、栈计量 0 失败；中枢端到端连续 5 次通过 | V1/V2 |
| S5 | 检查完成 | 【实测】修复差异 18 个文件均小于 5 MB；隐私 0 匹配，无预算/受保护需求改动；本轮空白及链接通过，完整分支继承的审计 EOF 空行列在后文 | V4 |

V0【实测】：在合并后的旧源码上执行复查报告内的独立探针，10 次模拟器均正常退出且 test.finished=1；5 个同成员控制通过，5 个换人/复用分支共 6 项功能断言失败（88 项检查），脚本退出 1。证据 `build/services-review6-wns0_bcz/`，日志 `build/services-r6-before.log`。

V1：按总规则全参配置，普通与内存目录仅相差构建目录和 `-DINK_SANITIZE=ON`，均保留 `-DINK_WERROR=ON`。另设 `-DINK_HUB_PYTHON=$PWD/build/hub-venv/bin/python`，使用 Python 3.11.15、uv 0.9.5 从 hub/uv.lock 冻结安装，uv lock --check 退出 0。配置记录分别为两目录的 services-r6-configure.log。实际执行：

```bash
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build -R '^hub-simulator-e2e$' --repeat until-fail:5 --output-on-failure -j1
```

【实测】最初普通 CTest 90/91：中枢 proto1 固定儿童 default，设备默认成人 owner，工具调用被新身份检查拒绝。采用只在测试目标使用的 e2e_device.c，运行原产品启动入口，经真实成员能力与真人确认准备 default 儿童空间，保持原脚本及全部 11 条断言，再加准备成功断言。单项及最终两套全量、连续 5 次均通过。没有放宽生产身份检查。首次失败保留在 `build/services-r6-before-fixture-ctest-final.log`，修正后单项在 `build/services-r6-hub-fixed.log`。

V2：两套均执行，内存套增加 --build build-asan；组别、编译与运行均串行：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py member_caps file_events file_paths file_reuse file_late member_writes late_writers reader_late member_inventory wifi_undo undo_schedules undo_owner replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel files storage update text bindings lowbattery audio voice handoff isolation shutdown actions policy permissions undo playback activity upgrade_crash removal
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

【实测】39 组各 190 次启动，其中 12 次为测试指定的提前重启，其余 178 次均 test.finished=1；2,188 项功能检查、保存分支及栈计量全部通过。新增 member_caps 每套 22 次启动、277 项检查。正向证据 `build/services-finish-vmnybeah/`、`build-asan/services-finish-3n1cihl7/`；保存控制 `build/services-finish-l5wo8g8z/`、`build-asan/services-finish-e_elfv8c/`；栈计量 `build/services-stack-9uecc_2s/`、`build-asan/services-stack-gnycs3m9/`。

V3：两套均执行下列反向命令，内存套增加 --build build-asan。只从 Git 读取配套的旧生产源码到忽略目录，不改工作区，不用兼容桩伪造结果。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline c3c217c member_caps
```

【实测】两套均按预期退出 1；各 22 次模拟器均退出 0 且 test.finished=1，8 个控制通过，14 个分支触发 24 项功能断言失败（共 277 项检查）。正反向没有 sanitizer 诊断，没有把崩溃、编译错误或缺失结果当作复现。反向证据 `build/services-finish-88jchyao/`、`build-asan/services-finish-xyok5l6d/`。

【实测】两套 PQBN AI 场景内部总 RAM 最低空闲均 32,880 字节；原门槛仍 32,768，余量 112。内部、DMA、外部失败分配均 0。没有抬高预算、跳过测试、关闭确定性比较或 sanitizer；始终最多构建 -j4、ctest -j3，同一时间只有一轮构建或测试。gui-smoke 使用 SDL dummy，未启动带窗口的模拟器。

V4【实测】：git diff --stat c3c217c..HEAD；git diff --check 4a792e9..HEAD 和 c3c217c..HEAD 退出 0。按总规则原隐私正则扫描 4a792e9..HEAD、c3c217c..HEAD、FETCH_HEAD..HEAD，均 0 匹配；本轮修复的 18 个文件均小于 5 MB，相对文件链接有效。原 38 组专项定义一致，原端到端 11 条断言全部保留；未改预算、需求、想法、决策、设计稿或既有审计原文。docs/audits/README.md 是任务允许的处理表；合并带入的研究资料未另行改动。

最终命令、退出码和耗时为 `build/services-r6-validation.json`，汇总 `build/services-r6-results.json`，闭合检查 `build/services-r6-closure.json`。两目录日志均为 services-r6-ctest-final.log、services-r6-regressions-final.log、services-r6-saved-final.log、services-r6-stack-final.log；连续端到端为 build/services-r6-hub-repeat-final.log，反向为 build/services-r6-reverse.log 和 build-asan/services-r6-reverse-final.log。最终验证后只更新本报告，源码及测试与 d79c8b4 一致。

## 没做到的

- 没有未修的本轮已确认严重或高问题。低级 AUD18-404（既有能力文档总表漏项）按原任务仅列后续。
- 只有 macOS/Apple clang 的模拟器及本机回环示例中枢实测；未运行 Linux gcc、真机、真实厂商或真实家庭数据。新增 C 代码无 libc write/read/fgets 调用；不冒称 Linux CI 已通过。

## 和设计不一致的

- 本轮没有修改设计合同迁就实现。接受的操作可在换人后于原空间收尾，回执仍按现有规则拒绝过期；删除/复用后生命周期失效的调用及事件拒绝。
- 保留原边界：成员清理遇在途写者/引用/作业或 I/O 失败仍保留 removing 编号并需重试；运行期代次不承诺跨重启；未承诺物理取证擦除、正式成员协议或事件必达。

## 改过的已有测试

- run.py 新增 member_caps 及矩阵；原 38 组定义、预算、断言和等待阈值不变。缺失 metrics 现在记录为失败并继续报告，仍不能通过。
- 新增三个服务边界包装与 member_caps.c；只暂停真实查询、修改锁及事件提交。时间生产者用真实到期处理，避免阻塞承担落盘的计时任务；通知派生完成通过执行器屏障等待，未给产品加 sleep。
- 中枢端到端改用原产品加合成儿童准备的入口；原 11 条断言逐项保留，新增准备成功断言。CMake 是总规则允许的验收阻挡项最小例外；未改中枢、生产默认成员或正式协议。没有更改任何已有测试期望。

## 要用户定的

无。没有设备、付费、采购、远端写入、push、PR 或发布。

## 顺手发现

- 第 6 轮复查指出的 AUD18-106/206/207/402 文档状态落后于实现，已在允许的设计 §15 和审计处理表同步。
- FETCH_HEAD..HEAD 的完整空白检查仍退出 2，仅继承的 docs/audits/2026-11-05-codex-services-a/report.md:133 的 EOF 空行；开工前已存在，按规则不改既有审计原文。本轮差异空白检查通过。
- 总规则新增的中/粗字体配置参数当前 CMake 未使用，会给配置警告；本轮保留原参数，未扩展字体管线。README、background 的历史构建示例仍有超出本任务资源上限的并发，本次没有照其执行。
