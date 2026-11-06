# device-events 执行报告

日期：2026-11-06。任务基线 `f1f72ae9`；本轮从 `83a0ac90` 接续，将开工时 FETCH_HEAD 的集成提交 `a51a804c` 合入当前分支（合并提交 `fb4dec90`），并修复[最新复查](REVIEW.md)的 DE-R5、DE-R6。范围沿用[任务书](../backlog.md#device-events)与[两端约定](../../hub/contracts-step1.md)。本报告替代上一轮报告，历史证据保留在 Git 提交中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| M1 集成冲突 | 【实现＋实测】已合并、构建通过 | 三个冲突文件逐处合并；保留固定公钥连接、配对专用响应、连接代次、完整已发送身份、发送前校验。修正自动合并留下的重复原始请求函数和配对夹具的旧接口调用 | `git merge --no-edit FETCH_HEAD`；`git merge-base --is-ancestor a51a804c HEAD`；T1 |
| DE-R5 页面上下文（高） | 【实现＋实测】已修 | 页面、浮层和输入会话绑定成员与记录代次，读取前后复核身份。交接期间拒绝旧上下文及其页面能力／输入编辑，新问题只带自己的文字，page_gen=0、actions=[]、state={}；同成员重新验证后可继续读取原页，关闭空间时只提供本身份浮层的脱敏快照 | T2；`private` 场景及原 member-wake／people-reset／power-menu |
| DE-R6 已完成朗读（高） | 【实现＋实测】已修 | 待播流、解码 PCM 与已提交输出保留原身份；声音类型决定锁屏策略，私密语音同时校验认证与 epoch，身份过期即丢缓存并停止输出。最后扬声器写入前再查；AI 打开声音流后再查原轮身份 | T2；`private`：真实语音已结束、缓存超过 20 帧、UI 忙、成员发布暂停；200 ms 在途窗口后观察 800 ms，旧声音增加 0 ms |
| DE-R1～R4 既有修复 | 【实测】定向回归通过 | 原握手、重连、双向切换、A→B→A、认证变化、发布窗口、声音错误、浮层清理均保留 | T2：两次启动各 94 项、握手／声音 87 项、身份隔离 108 项，零失败 |
| DE-1 事件 | 【实现＋实测】已实现部分回归通过 | seq／boot／member、ACK 与重连重发、重启 boot 递增、实际接收受众检查保留；仍为内存事件环 | T2 |
| DE-2 成员交付 | 【实现＋实测】本轮隔离缺口已修 | 既有入站、排队、执行、RPC、实时会话保护保留，补齐新请求的旧上下文与已完成回答的声音出口 | T2 |
| DE-3 时间与背压 | 【实测】普通全量通过 | 剩余 budget、排队 deadline、30 秒录音上限、声音背压和错误回执保留，无期限或预算改动 | T2；全量场景预算 |
| DE-4 能力目录 | 【实测】定向回归通过 | 全分页、同 rev 原子替换、不透明及重复 cursor、失败保留旧表、重复名与 32 项上限保护保留 | T2 |
| DE-5 两端契约 | 【实测】普通全量通过相关项 | sim_hub、合成中枢契约、JSON 交叉检查及端到端通过；配对 hello 使用一致成员快照并携带 epoch，上线前仍强制同步身份 | 全量中的 hub-*、pairing-*；配对 hello 与在线身份断言 |
| DE-6 D-105 | 【实现】部分，未完成 | asked、ask_by_hand、活动记录保留；undo_ms=3000 和 asked 短信／拨号仍明确拒绝，未实现三秒后执行 | T2；原缺口继续披露 |
| DE-7 反向对照 | 【实测】52/52 检出，另有修前源码 5/5 | 既有 26 项、新增 5 项、配对 21 项全部检出指定失败。新五项 test.finished=1；开工基线对应源码也检出同样失败。最初场景修前四条失败、旧声音增加 810 ms；能力／编辑旁路在修前另有两条失败 | T3；`python3 tests/device_events/reverse.py --build build`；`python3 tests/pairing/reverse.py --build build` |
| G1 全量与内存 | 【实测】普通 105/105；内存待执行 | 普通最终 64.84 秒，零失败、零跳过；快照误拒脱敏浮层／同成员解锁的问题已修，音频复用原播放锁后通知竞态通过。ASan+UBSan 尚未完成 | T1、T4 |
| G2 原子能力／AI 原生／分层 | 【实现】符合本轮范围 | 没有新用户动作、能力旁路、常驻任务或锁。页面与输入仍由 UI 任务读取；音频出口在原服务内检查。新增身份元数据放 PSRAM；PIN 不入快照 | 源码差异、分层检查与既有能力回归 |
| G3 依赖状态 | 【实现】已更新 | ui-dependencies 只更新成员隔离状态格和报告引用，不改设计判断 | `git diff -- docs/system/ui-dependencies.md` |
| G4／G5 范围与资源 | 【实现＋实测】按约束执行 | 无新页面；构建最多 -j4，ctest 最多 -j3，逐轮串行。未启动窗口模拟器，原 gui-smoke 使用 SDL dummy；未触设备、付费接口或远端写入 | 命令记录；最终范围／隐私检查待完成 |

命令与证据（产物均在忽略的构建目录）：

- T1：按总规则 §5 配置 `build`，`INK_WERROR=ON`；`cmake --build build -j4`。日志 `build/device-events-gate-config.log`、`device-events-gate-build.log`。
- T2：`python3 tests/device_events/run.py --build build`；`ctest --test-dir build --output-on-failure -j3 -R '^(device-events|integration-races|integration-member-wake|capability-table-policy.*)$'`。新 private 场景 33 项；所有场景 test.finished=1、test.fail=0。日志 `device-events-gate-final-focused.log`。
- T3：`python3 tests/device_events/private_reverse.py --build build`；同命令加 `--baseline 83a0ac90`。编译、链接、模拟器均退出 0，指定断言失败，脚本退出 0 表示成功检出破坏。基线 UI 尚无页面归属查询，兼容函数明确返回 true，模拟旧行为，不伪装产品实现。日志 `device-events-gate-private-reverse.log`、`device-events-gate-private-baseline.log`；首个修前场景指标 `device-events-gate-before-metrics.json`，能力旁路修前指标在 `test-out/device-events/private-before-actions/`。
- T4：普通与内存检查均沿用字体、Python、LVGL 及锁定中枢环境 `INK_HUB_PYTHON=$PWD/work/hub-python/bin/python`。内存版只另加 `INK_SANITIZE=ON`；两套构建／全量按序执行，均不跳过测试。

## 没做到的

本轮只修集成和两项新复查，不将以下原任务缺口标为完成：三秒自动撤销窗口及显示回执；中枢成员表全部分页替换、member.reset_pin、正式成人解锁证明；sync/*、一致清单、原子快照安装、history_gap 对账和数据集元数据；末端探针线上格式。内容包、远程画面新动作、真 Opus／JPEG、可选 rt.close reason／heard_frames、计划短连未新增。配对由集成分支带入，不等于这些缺口已经实现；确认持久化与块传输仍是本任务排除范围。

仅在本机 macOS／Apple clang、模拟器与合成中枢执行；未实测 Linux gcc、真机、真实厂商或真实 TLS。新增 C 行没有 write／read／fgets 调用；不将本机结果写成 Linux 验证。

## 和设计不一致的

上述缺口保持披露，没有改设计迁就实现。目录仍限制 32 项，超限保留旧表；事件仍为 8 条内存环，不能代替持久结果库；boot 损坏或不能落盘仍启动失败。proto1 开发握手仍固定合成儿童，客户端自报认证不能取得正式成人权限。

## 改过的已有测试

本轮没有删除测试、放宽断言、提高预算、增加产品等待或跳过测试。run.py 仅追加 private 程序；ai_boundary.c 增加真实朗读已经完成且仍有缓存的前置断言；private 另含旧页面能力与输入编辑的反向拒绝及当前成员的正常调用对照。配对夹具补必需 epoch 实参、hello 字段及在线身份断言；配对反向脚本适配合并后的发送接口与完整成员校验，追加 hello epoch 反向。用于观察的 200＋800 ms 只在测试中，不影响产品期限。

前轮已有变更继续保留：ai_tools 精确要求 done=1、answers=3、切换及旧轮取消各 1；races 用实际接收连接判断受众；starts／actions 适配绑定身份 RPC；policy 补身份和真实握手前置条件；handoff 显式关闭成人认证以匹配 D-95。细节可从开工提交的旧报告重建。

## 要用户定的

无新增产品选择；没有等待答复。旧功能缺口仍按原设计需要建设。

## 顺手发现

- 原复查的 speech-case 在合并后当前调度下未复现泄露，不能声称旧脚本本轮失败；新增场景先验证真实回答完成且有缓存，再保持 UI 忙并暂停成员发布，修前稳定测到 810 ms，修后为 0 ms。
- 音频初版额外抢一次锁影响了低优先级通知交接进度；对照旧音频源码可恢复，最终将身份检查合并进原 staged_wanted 临界区，保留竞态原断言而不扩大通知服务修改范围。
- 快照修复后的源码复核又发现同一交接窗口的页面能力与输入编辑旁路，补回归证实后在统一能力策略和文本目标校验中修复；没有只在 AI 请求拼装处隐藏旧内容。
- 最终文件／链接、隐私、全量与内存检查及提交状态待补。仅本地提交，未 push。
