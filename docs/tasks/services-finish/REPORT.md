# 系统服务收尾报告

2026-11-05，第 5 轮；开工基线 `9b976aa`，工作区干净。依据 [复查报告](REVIEW.md)、[任务书](../services-finish.md)和[总规则](../codex-rules.md)。实现提交 `969cf95`、`55d27bc`；复查报告原文保留，历史报告在 Git 中。本轮确认的两项高问题及下列同类问题已修复；最终全量复跑及结束检查正在核对。以下【实测】为本次执行，历史反向结果不计入本次。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R5-01 高：跨成员关闭文件泄漏路径 | 已修 | 【实现】句柄关闭释放槽前复制原成员、代次和虚拟路径；文件事件通过带原代次的总线入口发送。【实测】真实句柄及真实录音的同成员/换人收尾均通过；旧实现把成人路径标为孩子并投递给实际模拟中枢和本地订阅者 | V2/V3 `file_events` |
| R5-01 同类：短写、建目录、删除、移动、作业事件 | 已修 | 【实现】短操作沿用原上下文，file.changed 和 file.job_done 保留原代次。【实测】八种操作共 16 个控制/换人分支；撤回文件实现，八个换人分支均触发功能断言 | V2/V3 `file_events` |
| R5-01 同类（高）：录音启动期间换人 | 已修 | 【实测】原文件事件修好后，真实 audio.record 仍在打开文件期间换人，把 audio.record_done 的成人路径标为孩子并送到中枢；单独复现 2 项隔离失败。【实现】录音固定原调用成员/代次，文件打开及失败清理绑定原身份；启动完成后补查换人，采集回调及结束事件检查原代次。当前真实录音结果仍只发一次且归原成员 | V0；V2/V3 `file_events` 的 `record_start` |
| R5-01 同类（高）：播放启动期间换人 | 已修 | 【实测】真实 media.play 在打开成人录音文件期间换人，旧实现把 media.finished 的私有路径归孩子；单独复现 2 项隔离失败。【实现】软总线及内部媒体命令保留原成员/代次，阻塞读完后失效即拒绝，流打开及帧入队复查身份；状态快照不暴露另一成员路径，结束事件保留原代次 | V0；V2/V3 `file_events` 的 `play_start/play_api_start` |
| R5-02 高：源/目标路径混用不同成员 | 已修 | 【实现】一次操作固定成员、代次和成人权限，两条路径共用快照；修改仍与成员卡目录清理共锁。【实测】真实 file.move/file.copy 在源解析后暂停并换人，成人内容保留在原空间，孩子经真实 file.read 无法读出；儿童换到成人后仍不能向家庭目录写入 | V2/V3 `file_paths` |
| R5-02 同类：能力提供者、stat 与拷贝交接 | 已修 | 【实现】总线提供接受调用时的成员/代次，八个文件路径能力绑定它；stat 与后续作业共用上下文。【实测】个人、应用、指定成员、家庭四类路径，以及 stat、源解析、作业准备三个暂停阶段，共 36 分支通过，包含真实目录拷贝 | V2/V3 `file_paths` |
| R5 同类：任务标题和结束事件归属 | 已修 | 【实现】文件作业显式登记原成员代次；私有任务列表及结束事件检查原代次。【实测】在两条路径均解析、作业打开源之后换人，孩子的 job.list 看不到成人文件名；同成员控制正常；旧实现暴露原任务标题 | V2/V3 `file_paths file_events` |
| R5 同类：释放槽后删除并复用编号 | 已修 | 【实测】真实关闭在文件槽释放后、事件投递前暂停；真人确认删除成人并将编号复用为孩子，恢复关闭。旧路径不会进入新 AI 或本地订阅，旧代次任务登记被拒绝；新成员事件及文件能力正常 | V2/V3 `file_reuse` |
| R5 同类：目录句柄的代次和权限 | 已修 | 【实现】虚拟根与共享目录也保留原调用上下文。【实测】旧目录不能继承复用身份，儿童目录换到成人后不增加成人根项；旧实现三个断言失败，当前控制正常 | V2/V3 `file_reuse` |
| R4-01/R3-01 高：迟到阅读保存 | 保留修复 | 【实测】销毁、定时、校验后入队前暂停的九分支，换人、删除及复用通过 | V2 `reader_late` |
| R4-01 同类：写者、集合与延迟持有者 | 保留修复 | 【实测】写者/集合引用、应用状态写入和删除、媒体 Flash/卡断点、通知及偏好、活动待记槽、卡上短写及打开句柄的生命周期回归通过 | V2 `member_writes late_writers file_late member_inventory` |
| R3-02 高：Wi-Fi 撤销及同类归属 | 保留修复 | 【实测】满额保存失败、闹钟/提醒版本、延迟提供者撤销票据归属通过 | V2 `wifi_undo undo_schedules undo_owner` |
| R2-01 严重：成员数据残留 | 保留并补全 | 【实测】闹钟/提醒、成员集合与文件、卡目录、缓存、迟到写者、删除复用和重启通过；本轮补文件事件及路径时序 | V2 `member_inventory removal_alarm removal reader_late late_writers file_late`；V3 |
| R2-02 高：重复/迟到调用 | 保留修复 | 【实测】请求号绑定成员代次、来源、能力和参数；在途重复、迟到回执及关闭空间通过 | V2 `replay` |
| R2-03 高：旧撤销覆盖新内容 | 保留修复 | 【实测】记录、文件、通知、时间及 Wi-Fi 的过期撤销拒绝，未修改控制可撤销 | V2 `undo_data` |
| R2-04 中：空白卡清理 | 保留修复 | 【实测】空白卡删除未使用成员；占用和失败仍保留编号 | V2 `empty_removal removal` |
| AUD18-106 中 | 保留修复 | 【实测】失败升级历史迁移、跨重启及人工重试规则通过 | V2 `update_history` |
| AUD18-206 中 | 保留修复 | 【实测】拒绝提交不冒报关机图，恢复后正常关机 | V2 `offpanel`；V1 |
| AUD18-207 中 | 保留修复 | 【实测】长选区窗口、右标记及拼音映射通过 | V2 `text_window` |
| AUD18-402 中 | 保留修复 | 【实测】输入快照保留目标/版本，凭据掩码通过 | V2 `keyboard_snapshot` |
| R1-01 高：升级提交断电 | 保留修复 | 【实测】有效槽提交前/后断电及提交失败四链通过 | V2 `upgrade_crash` |
| R1-02 高：缓冲播放隔离 | 保留修复 | 【实测】TTS/听书交接及同成员锁屏播放通过 | V2 `playback`；V1 |
| R1-03 高：活动积压 | 保留修复 | 【实测】1,600 次动作、单成员积压和恢复通过 | V2 `activity` |
| R1-04 严重：策略及权限 | 保留修复 | 【实测】权限、儿童上限、人工确认/拒绝/换人及权限撤销通过 | V2 `permissions policy` |
| R1-05 高：成员清理失败 | 保留修复 | 【实测】卡、Flash、缓存、唯一 PIN 四种故障及在途写者通过 | V2 `removal member_writes file_late` |
| R1-06 中 | 保留修复 | 【实测】连续撤销、过时版本及过期通过 | V2 `undo` |
| AUD18-101 高 | 保留修复 | 【实测】不可读文件根不报清理成功 | V2 `files` |
| AUD18-102 高 | 保留修复 | 【实测】目录和短写持卡，USB 独占不穿透 | V2 `files` |
| AUD18-103 中 | 保留修复 | 【实测】作业先完成仍结束任务 | V2 `files` |
| AUD18-104 高 | 保留修复 | 【实测】升级状态写入失败不切槽，缺状态不标有效 | V2 `update upgrade_crash` |
| AUD18-105 高 | 保留修复 | 【实测】资源恢复事务不被新包覆盖 | V2 `update` |
| AUD18-107 中 | 保留修复 | 【实测】正常包及八类坏包通过 | V2 `update` |
| AUD18-108 中 | 保留修复 | 【实测】下载绑定原包，两条 offer 变化链通过 | V2 `update` |
| AUD18-109 高 | 保留修复 | 【实测】遗忘成员集合不破坏独立定时器 | V2 `storage` |
| AUD18-201 高 | 保留修复 | 【实测】拥堵时停止采集/上传 | V2 `audio voice` |
| AUD18-202 高 | 保留修复 | 【实测】OOM 后不能重做旧文本 | V2 `text` |
| AUD18-203 高 | 保留修复 | 【实测】录音结束且持久完成后关机 | V2 `shutdown` |
| AUD18-204 高 | 保留修复 | 【实测】耳机掉线不外放旧媒体 | V2 `audio` |
| AUD18-205 中 | 保留修复 | 【实测】通话结束保留闹钟焦点 | V2 `audio` |
| AUD18-208 中 | 保留修复 | 【实测】AI 不能提交隐藏字段 | V2 `text` |
| AUD18-209 中 | 保留修复 | 【实测】初始 UTF-8 截断保留完整字符 | V2 `text` |
| AUD18-401 高 | 保留修复 | 【实测】字段、内容及光标的过期会话/版本均拒绝 | V2 `bindings` |
| S6-01 高 | 保留并补全 | 【实测】旧缓冲/传输事件及关闭空间回归通过；本轮文件生产者补原代次 | V2 `handoff replay file_events file_reuse` |
| S6-02 高 | 保留修复 | 【实测】采集、迟到帧和缓冲播放隔离通过 | V2 `isolation playback` |
| S4-DET 中 | 保留修复 | 【实测】确定性比较及栈计量通过 | V1；V2 `stack_meter.py` |
| S1 | 本轮范围已修，最终复跑待核对 | 【实测】复查两项高先复现；另复现并修复录音/播放启动两处同类高问题，同类均有正反向；此前严重、高和中逐项重跑正向通过 | V1/V2/V3 |
| S2 | 保留修复 | 【实测】D-105、儿童权限、人工确认、凭据绑定及撤销通过 | V2 `permissions policy undo undo_data undo_owner` |
| S3 | 保留修复 | 【实测】实际能力表 actions 六检查与 AI 场景预算通过 | V2 `actions`；V1 |
| S6/R-061 | 本轮模拟器范围通过 | 【实现】路径及授权上下文归文件服务，接受调用和事件归属归总线，私有任务归任务服务；按钮和 AI 继续调用共同能力。录音/播放通过文件服务的代次入口保留接受身份，Ogg 长度取已打开的句柄。新增入口是内部身份机制，不允许 AI 自选成员或代次；应用/缓存路径仍不在 AI 能力范围 | V2/V3；相关源码 |
| S4 | 最终复跑待核对 | 两套完整配置、构建、全量、服务回归、低电量保存及栈计量；不放宽资源预算 | V1/V2 |
| S5 | 结束检查待核对 | 提交范围、隐私、文件和链接检查 | V4 |

V0【实测】：开工时运行复查留下的两个脚本，正常文件/录音同成员控制通过；换人事件泄漏 4 项断言失败，路径混用 3 项断言失败，均退出 1。日志 `build/services-r5-before-events.log`、`build/services-r5-before-paths.log`。这两个临时脚本不作为交付依赖，完整可复建的回归已加入 `tests/services_finish/`。继续排查后，`build/services-r5-record-start-before.log`、`build/services-r5-play-start-before.log` 分别确认录音及播放启动的同类路径事件泄漏；没有声称已实测录到孩子声音或成人声音实际外放。

V1：两套均按总规则完整配置，内存目录仅增加 INK_SANITIZE=ON；普通目录未启用 sanitizer。配置参数：

```bash
cmake -S . -B build -DINK_WERROR=ON -DINK_CJK_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 -DPython3_EXECUTABLE=$INK_DEPS/build-deps/pyenv/bin/python -DFETCHCONTENT_SOURCE_DIR_LVGL=$INK_DEPS/build/_deps/lvgl-src
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：两套均执行，内存套给脚本增加 --build build-asan。所有组别串行运行，低电量保存分支另跑：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py file_events file_paths file_reuse files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel reader_late member_inventory wifi_undo undo_schedules undo_owner late_writers member_writes file_late
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

V3：两套执行下列命令，内存套增加 --build build-asan。预期退出 1，要求模拟器正常退出且 test.finished=1，失败来自隔离/授权的功能断言。仅用 git show 读取旧文件服务、文件能力、媒体和 Ogg 四份真实源码到忽略目录，保持旧接口配套；总线、成员和任务服务保留当前版本。没有用兼容桩伪造旧结果。包装只暂停真实 HAL/事件边界，不改路径、I/O 结果、业务授权或断言。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 9b976aa file_events file_paths file_reuse
```

最终结果、证据目录、RAM 数字和 V4 待最后核对后填写。命令、退出码及耗时在 `build/services-r5-validation.json`；每套日志为 services-r5-configure-final.log、services-r5-build-final.log、services-r5-ctest-final.log、services-r5-regressions-final.log、services-r5-saved-final.log、services-r5-stack-final.log、services-r5-reverse-final.log。

## 没做到的

- 本轮没有未修的已确认严重或高问题；旧中级修复均保留并重跑。低级 AUD18-404 是既有能力文档漏项，按用户范围保留。
- 只有 macOS 模拟器实测，没有 Linux 执行环境、真机、真实中枢或窗口外观验证。修改没有新增 libc write/read/fgets 调用；不冒称 Linux gcc 已通过。
- 全量中的既有 gui-smoke 使用 SDL dummy，没有启动带窗口的 ink-sim-gui。

## 和设计不一致的

- 本轮没有为迁就实现而修改设计。一次已接受的文件操作可在换人后于原空间收尾，回执仍经现有过期检查；生命周期已失效的调用/事件拒绝。目录权限也按打开时的快照处理。
- 保留既有边界：文件撤销按全卡修改序号保守失效；失败升级历史有 128 KiB 上限；成员删除遇在途写者、引用、作业或 I/O 失败仍保留 removing 编号并需重试。运行期代次不承诺跨重启，不承诺物理取证擦除。

## 改过的已有测试

- run.py 只新增三组及矩阵，原组别、预算、断言、等待阈值不变；没有改既有测试期望。
- 新增 file_events.c、file_paths.c、file_reuse.c 和文件、媒体、Ogg 的边界包装。应用私有路径使用测试程序的内部调用，产品能力表未增加测试入口。
- 并发窗口由测试扣住/释放任务固定；没有给产品加 sleep、跳过测试或伪造返回值。目录补充检查后重新执行两套最终验证。

## 要用户定的

无；没有采购、硬件操作、付费、远端写入、push、PR 或发布。

## 顺手发现

- 同类的短操作事件、stat/作业交接、录音及播放启动、内部媒体命令、任务标题、目录权限和释放槽后编号复用均已纳入本轮修复与正反向验证。
- 证据为确定性的模拟器并发窗口及真实服务链路；没有据此声称真机自然触发频率、整个产品的所有并发窗口或物理擦除已经验证。
