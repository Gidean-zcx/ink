# device-events 执行报告

## 2026-11-06 集成修复（进行中）

本轮从 `83a0ac90` 接续，先将集成提交 `a51a804c`（开工时的 FETCH_HEAD）合入当前分支，保留配对与成员身份保护。最新复查的 DE-R5、DE-R6 一并修复，补正向回归和撤掉修复的反向对照。普通与内存检查均按总规则完整执行，构建最多 `-j4`、ctest 最多 `-j3`，串行运行；未完成前不标通过。下文是上一轮证据，不能替代本轮验收。

日期：2026-11-06；任务基线：`f1f72ae9`，本轮复修基线：`053a4cd5`。范围按[任务书](../backlog.md#device-events)、[两端约定](../../hub/contracts-step1.md)和[设备协议](../../hub/device-protocol.md)核对。证据仅来自本机 macOS 模拟器、`sim_hub` 和合成家庭中枢。

最新[独立复查](REVIEW.md)的 DE-R4（严重）、DE-R3（高）及同类边界已修；复查未单列中级。普通和 ASan+UBSan 全量各 103/103；26 项反向对照及四项修前源码复现均检出指定失败。原任务的未完成功能继续列在“没做到的”，不把本轮修复当作整项产品功能验收。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| DE-R4 AI 展示隔离（严重） | 【实现＋实测】已修，两套通过 | 去掉 AI 换人保留旧浮层的例外；展示前校验一致的成员、认证和代次，关闭时清空旧文字及请求编号。真实语音上传、转写、tools/call 换到孩子后，浮层和回答缓冲都清除；最终 GC16 渲染图也无旧文字。另测旧通知、新成员新轮、新语音启动失败 | `ctest --test-dir build --output-on-failure -j3 -R '^device-events$'`；同命令换 `build-asan` |
| DE-R3 发布窗口（高） | 【实现＋实测】已修，两套通过 | 成员服务在同一临界区更新身份与代次，读取使用一致快照；hello／member 保存连接上实际发送的完整身份。发送和实时回调不再依赖状态通知到达。真实 PIN 工作线程暂停在发布前，双向切换和同成员认证关闭／重开时，旧请求拒收，新事件的实际接收受众正确；另有同步后的正常请求、事件对照 | 同上；新增 `sim-device-events-isolation` 随同执行 |
| DE-R1 握手隔离（前轮高） | 【实测】回归通过 | 首次握手、重连、双向切换、A→B→A、同成员认证变化、身份同步发送失败和序列化分配期间换人均保留；新增校验连接的成员及 incarnation，不能只比较数字 epoch | 同上 |
| DE-R2 声音失败（前轮高） | 【实测】回归通过 | 真实下行 push／stream_open 拒收均中止声音，完成后仍显示失败；扬声器失败、旧通知及首段文字顺序保留。关闭失效浮层的两条路径统一记录探针 | 同上；`ctest --test-dir build --output-on-failure -j3 -R '^integration-listening$'` |
| DE-1 事件 | 【实测】本批实现通过 | seq、boot、member、ACK 和重连重发保留；两次持久 Flash 启动 boot 递增。补测发布窗口内的 owner／kid 事件，受众从接收连接独立记录，不使用 payload.member 自证隔离 | `device-events` |
| DE-2 成员与交付 | 【实测】本批实现通过 | 入站、排队、执行、快照、RPC、实时会话和声音撤销回归保留；补 DE-R3、DE-R4。成员代次在状态发布前已失效，延迟状态通知不会再关闭新成员的新轮 | `device-events`；`python3 tests/services_finish/run.py --build build handoff actions policy isolation`；同命令换 `build-asan` |
| DE-3 时间与背压 | 【实测】本批实现通过 | 剩余 budget、排队 deadline、30 秒录音上限及声音背压保护保留；没有增加产品等待时长或放宽预算 | `device-events`；全量场景预算 |
| DE-4 能力目录 | 【实测＋实现】本批实现通过 | 全分页原子替换、不透明 cursor、同 rev、重复 cursor 和失败保留旧表；重复名称及 32 项超限拒绝，暂存表在 PSRAM。sim_hub 默认三页；完整 actions 回归保留 | `device-events`；补充 `actions` 回归 |
| DE-5 两端契约 | 【实测】合成范围通过 | 两套 hub-contract 内部各 125 项通过；hub-json-crosscheck、hub-simulator-e2e 通过。proto1 仍固定合成儿童，自报 authed 不授予正式成人权限 | 两套全量中的 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` |
| DE-6 D-105 | 【实现＋实测】部分，未完成 | asked、ask_by_hand、活动记录和 undo_ms=3000 校验保留；带自动窗口的请求或 asked 短信／拨号明确回 -32021，不能把拒绝当作三秒后执行 | `device-events`；补充 `policy` 回归 |
| DE-7 反向对照 | 【实测】26/26 检出预期失败，另有四项修前复现 | 原 19 项保留；新增 publish-epoch、wire-identity、ui-identity、ui-handoff、ui-new-turn、ui-restart、voice-handoff。每项编译、链接、模拟器均退出 0，指定断言失败且 test.fail>0；053a4cd5 的 hub 和 AI／动作源码分别重现受众串成员和真实语音旧文字；97645ea2 的两项原复现仍有效 | `python3 tests/device_events/reverse.py --build build`；加 `--baseline 053a4cd5 publish-audience voice-private`；另加 `--baseline 97645ea2 hello speech-id` |
| G1 全量、内存、预算 | 【实测】普通及 ASan+UBSan 各 103/103 | 普通 53.83 秒，内存 104.79 秒；无跳过、无 sanitizer 诊断。两套 device-events 均为重启各 94 项、握手／声音 87 项、新隔离 108 项，test.finished=1、零失败；预算、确定性和分层检查未放宽 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；同命令换 `build-asan` |
| G2 原子能力／AI 原生／分层 | 【实现】本轮符合 | 成员服务提供身份真值，连接服务管理已发送身份，AI 服务在 UI 执行器处理展示；仍通过同一能力策略切换成员。新增快照是内部机制，不含 PIN；没有新用户操作、新页面、常驻任务、锁或能力旁路 | 能力、策略、分层及场景回归；相关接口与差异核对 |
| G3 依赖状态 | 【实现】已更新 | ui-dependencies 只改“成员、PIN”状态格及报告引用，没有改设计判断 | `git diff 053a4cd5..HEAD -- docs/system/ui-dependencies.md` |
| G4／G5 范围 | 【实现＋实测】符合 | 本轮共 17 个相关文件，七个产品接口／实现文件；没有新页面。未启动带窗口 GUI；全量原有 gui-smoke 使用 SDL dummy。隔离场景通过命令行生成渲染图 | `git diff --stat 053a4cd5..HEAD`；`build/test-out/device-events/isolation/frames/` |

复现环境：配置沿用总规则；两套 INK_WERROR=ON，内存版仅另加 INK_SANITIZE=ON。ctest 实际使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`；中枢测试沿用 `work/hub-python`，依赖来自 hub/requirements.lock，pytest=8.4.2、pytest-asyncio=1.2.0，INK_HUB_PYTHON 指向该解释器。构建与全量串行，构建 `-j4`、ctest `-j3`。补充 handoff／actions／policy／isolation 两套均为 15／6／37／15 项通过，零失败，编译及运行退出 0。

日志：两套构建目录的 `device-events-identity-config.log`、`device-events-identity-build.log`、`device-events-identity-full-test.log`、`device-events-identity-services.log`；反向和基线复现分别为普通构建目录的 `device-events-identity-reverse.log`、`device-events-identity-baseline.log`、`device-events-identity-previous-baseline.log`。所有产物被忽略，可按命令重建。首次普通全量曾为 102/103：新增展示校验先关闭浮层后，旧关闭路径漏记探针；现统一计数，原 integration-listening 断言未改，最终全量通过。

## 没做到的

本轮复查的严重、高问题无未修项；没有另列中级。以下是原任务持续披露的功能缺口，本轮未扩大实现范围。

- **三秒自动撤销窗口**：只有字段检查及明确拒绝，没有“内容真正上屏后计时、到点执行、这一轮取消时撤销”。需要显示回执和确认事务接续，不能拿收到请求的时刻冒充上屏；确认结果持久化、confirm.status、confirm/request 属于本节排除范围。
- **中枢成员表**：没有 members/list 全页替换和 member.reset_pin，保留本地成员表与密码保护。真实中枢仍是受限 proto1 开发握手，正式家庭身份和成人解锁证明尚未实现。
- **同步与数据集元数据**：没有 sync/*、一致清单、原子快照安装和 history_gap 对账；完整流程依赖本节排除的块传输，本地回执不能冒充中枢备份。
- **设备末端探针上传**：本地探针保留，没有线上上传格式；设备协议第 5 节仍将格式列为第 2 步待定事项。
- **§13 后续／可选项**：内容包、远程画面新动作、真 Opus／JPEG、rt.close 的 reason／heard_frames、按计划短连均未增加；本机桥通过不等于真媒体模式，未认定这些功能不需要。
- 配对、固定公钥、地址／令牌、首次开机、确认持久化和块传输按本节排除范围未改。未在 Linux gcc、真机或真实厂商上执行；本轮新增 C 代码无 write/read/fgets 调用。

## 和设计不一致的

上述未完成功能仍与概要设计 §13 有差距，未改设计迁就代码。目录仍有 32 项上限，超限保留旧表；事件仍为 8 条内存环，可能丢旧事件，seq/boot 只用于识别缺口，不能代替持久结果库。boot 损坏或不能落盘时仍启动失败，不复用已知身份。

## 改过的已有测试

本轮没有删除测试、修改已有行为期望或提高预算。run.py 保留两次各 94 项重启及原复查场景，追加身份隔离程序并清理该次生成的旧帧，防止把旧图当新证据；review.c 补完整连接身份校验。新的成员夹具只暂停真实工作线程的状态发布，不直接改成员字段；真实语音切换走原有能力路径。原声音及握手断言全部保留。

reverse.py 适配一致快照，并增加七项反向对照及修前源码重建。hello 反向观察首个握手窗口（90 秒虚拟时间），目标断言失败，但 test.finished=0，不能视为整套夹具完成；这避免已故意破坏前置条件后继续驱动无关后续。新增七项反向均 test.finished=1；延长的仅是故意破坏保护后的模拟观察时间，不改变正常回归时长、产品期限或预算。

前轮改动继续保留，区分于本轮：

- services_finish/policy.c：补真实握手与同步前置条件，原 36 项断言不变，现 37 项。
- ai_tools.budget.json：done=1、answers=3，并要求 switch=1、旧轮取消=1；换成员取消旧轮是协议要求。其余策略和资源门槛不变。
- races.c：从实际接收连接取受众，不能把在换人前已正确交付的事件误算为串成员。
- starts.c、services_finish/actions.c：适配绑定身份的 RPC 接口，原失败及完整 actions 断言不变。
- services_finish/policy.c：请求补当前 member/epoch，asked、高风险和跨成员断言不变。
- services_finish/handoff.c：显式关闭成人认证后再锁屏，适配 D-95 的宽限；阻止外发的断言不变。

## 要用户定的

无新增产品选择，没有等待答复；未完成功能仍按原设计需要建设。

## 顺手发现

- 同类修复还包含：新语音轮次启动失败时清除旧标签／请求编号，以及延迟的旧成员回调不能关闭新成员的新问题；均有独立反向对照。
- 复用现有服务锁与 hub 的 id_lock，没有增加常驻任务、锁或状态订阅槽位；原音频成员回调继续负责撤销排队语音。
- 已检查全部本地文档链接、Python 语法和 git diff --check。相对 053a4cd5 及 f1f72ae9 的提交差异，按总规则 §6 隐私正则扫描均为 0 命中（grep 退出 1）；无本机路径、设备标识、密钥或生成产物进入提交。
- 范围记录提交 `738f2256`，代码与回归提交 `23efec1c`；最终验证以本报告的后续提交记录。仅本地提交，未 push。
