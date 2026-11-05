# device-events 执行报告

日期：2026-11-06；基线：`f1f72ae9`。证据范围是本机 macOS 模拟器、`sim_hub` 和合成家庭中枢，没有真机或真实厂商验证。范围按[任务书](../backlog.md#device-events)、[两端约定](../../hub/contracts-step1.md)和[设备协议](../../hub/device-protocol.md)核对。

本轮复修基线：`97645ea2`。按[独立复查](REVIEW.md)处理 DE-R1、DE-R2（严重 0、高 2，未单列中级）。两项及同类边界已修，两套全量均 103/103；19 个反向对照和两项基线复现均检出预期失败。原报告的握手隔离和声音提示证据不足，以下以本轮结果为准；原任务未完成项仍保留。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| DE-R1 握手隔离（高） | 【实现＋实测】已修，两套通过 | 保存本连接 hello／member 已发送的 epoch，握手成功后先同步身份再上线、发送排队请求和调用回调；身份同步失败断开重连。覆盖首次握手、重连、双向换人、A→B→A、同成员解锁／关闭认证、普通在线换人；观测实际接收连接的成员和代次 | `ctest --test-dir build --output-on-failure -j3 -R '^device-events$'`；同命令换 `build-asan` |
| DE-R2 声音失败（高） | 【实现＋实测】已修，两套通过 | 两种声音错误通知统一用提问轮次匹配，不再混用 rt sid。真实文字提问后接语音，在实际下行首次 push／创建音频流时各拒收一次；声音中止、回答完成后仍显示失败。另测扬声器失败回调、旧通知不串新轮，以及失败早于首段文字时提示保留 | 同上；新增 `sim-device-events-review` 由同一 ctest 执行 |
| DE-1 事件 | 【实测】通过 | `device/event` 带 seq、boot、member；boot 原子落盘，重启递增；保留最近 8 条直到 ACK；丢 ACK 重连用同一身份重发，错 boot／超前 ACK 无效 | `ctest --test-dir build --output-on-failure -j3 -R '^device-events$'` |
| DE-2 成员与交付 | 【实测】本批实现两套通过 | 原入站、队列、执行、快照、RPC、实时会话和声音撤销回归保留；补 DE-R1 的握手窗口，并在序列化分配后、最终发送前再查成员代次。故障注入在分配期间换人，旧私密请求得到 LOST，不能发送 | 同上；`python3 tests/services_finish/run.py --build build handoff actions policy isolation`；同命令换 `build-asan` |
| DE-3 时间与背压 | 【实测】本批实现两套通过 | 原 budget、排队 deadline、30 秒录音上限保留。补 DE-R2 的真实下行拒收、提示文案和完成后保留断言；同类的音频流创建失败也终止声音并提示，不再静默丢帧 | 同上 |
| DE-4 能力目录 | 【实测＋实现】通过 | 已测全分页原子替换、不透明 cursor、同 rev、重复 cursor 和失败保留旧表；代码另拒绝重复名字和超限，暂存表放 PSRAM。sim_hub 默认分 3 页。保留基线完整 actions，未改为空数组；actions_more 可选优化未做 | 同上；`actions` 回归仍要求完整能力列表超过 768 字节 |
| DE-5 两端契约 | 【实测】通过 | 假设备保存并检查交付代次；中枢接受 hello epoch，ACK 事件、检测缺口和重连重复；proto1 仍固定合成儿童，不因 authed 自报而授予成人权限；保留原 result／MCP 回执、语音、听写和错误测试 | `ctest --test-dir build --output-on-failure -j3 -R 'hub-contract|hub-json-crosscheck|hub-simulator-e2e'` |
| DE-6 D-105 | 【实现＋实测】部分 | 保留基线 asked、ask_by_hand、活动记录；校验 undo_ms 必须为 3000。自动撤销窗口尚未实现：带窗口的请求，或 asked 的短信／拨号，明确回 -32021，不能忽略窗口后立即执行 | `device-events`；`policy` 回归 |
| DE-7 反向对照 | 【实测】19/19，另有两项基线复现 | 原 12 项全部保留；新增 hello、send-boundary、speech-id、speech-notify、speaker-id、speech-open、speech-delta。每次窄撤回一个保护，编译／链接／模拟器退出均为 0，目标断言失败且 test.fail>0；只撤掉声音通知也必失败。修前 hub／AI 源码各自放入新夹具，均复现目标失败 | `python3 tests/device_events/reverse.py --build build`；`python3 tests/device_events/reverse.py --build build --baseline 97645ea2 hello speech-id` |
| G1 全量与内存 | 【实测】普通、ASan+UBSan 均 103/103 | 普通 56.79 秒、内存 90.85 秒，无跳过；两套中枢契约内部也均 125 项通过。预算、确定性和分层检查全部保留。两套 device-events 均为两次启动各 94 项、复查场景 83 项；补充 handoff／actions／policy／isolation 各 15／6／37／15 项通过 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；同命令换 `build-asan`；全部退出 0 |
| G2 原子能力／AI | 【实现】通过 | 边界仍是连接服务处理身份、AI 服务投递错误、UI 执行器更新既有浮层；工具走同一能力策略关口。本轮无新用户操作或页面，不新增应开放给 AI 的能力，不增加常驻任务或资源预算 | 能力表 runtime／policy、分层和现有场景测试 |
| G3 依赖状态 | 【实现】已更新 | 只更新 ui-dependencies 的“成员、PIN”状态格，并链接本报告 | `git diff -- docs/system/ui-dependencies.md` |
| G4／G5 范围 | 【实现】符合 | 无新页面；只改服务、策略、模拟替身和测试。没有启动带窗口的 GUI；全量原有 gui-smoke 使用 SDL dummy | `git diff --stat f1f72ae9` |

复现环境：构建参数按总规则，两个构建均使用 INK_WERROR=ON，内存构建另加 INK_SANITIZE=ON；上表 ctest 实际使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`。中枢 Python 沿用 `work/hub-python`：运行依赖来自 `hub/requirements.lock`，pytest=8.4.2、pytest-asyncio=1.2.0；模块搜索路径指向本仓库 hub，CMake 的 INK_HUB_PYTHON 指向该解释器。所有构建与全量测试串行，构建 `-j4`、ctest `-j3`；gui-smoke 为 SDL dummy，无窗口。

本轮日志在两个构建目录的 `device-events-full-build.log`、`device-events-full-test.log`、`device-events-services.log`；反向对照和基线复现分别在 `build/device-events-reverse.log`、`build/device-events-baseline.log`。修复后的全量、定向和补充回归均无断言失败，反向对照及基线副本均检出目标失败。所有产物被忽略，上表命令可重新生成。

## 没做到的

本轮复查问题无未修项。以下保留原任务的功能缺口，不因本轮通过而视为完成。

- **三秒自动撤销窗口**：只有字段检查和明确拒绝，未实现“内容真正上屏后计时、到点执行、这一轮取消时撤销”。需要显示回执和确认事务接续，不能拿收到请求的时刻冒充上屏。本次不实现确认结果持久化、confirm.status、confirm/request。
- **中枢成员表**：未实现 members/list 的全页替换和 member.reset_pin；保留设备原有成员表和本地密码保护。当前真实中枢仍是受限 proto1 开发握手，没有正式家庭身份和成人解锁证明，不把自报 authed 当证明。
- **同步客户端和数据集元数据**：未实现 sync/*、一致清单、原子快照安装和 history_gap 对账；完整流程依赖本节排除的块传输，不能把本地保存回执冒充中枢备份。
- **设备末端探针上传**：现有本地探针仍在；没有新增线上上传格式，设备协议第 5 节仍把格式列为第 2 步待定事项。
- **§13 其余后续／可选项**：未增加内容包、网页新动作、真 Opus／JPEG 模式、rt.close 的 reason／heard_frames、按计划短连。现有本机中枢桥已经通过 e2e；这不等于真媒体模式。未自行认定这些功能不需要。
- 配对、固定公钥、地址／令牌存放、首次开机、确认持久化和块传输按本节排除范围未改。未在 Linux gcc 或硬件上运行；新增代码未使用 write/read/fgets。

## 和设计不一致的

上列未完成部分仍与概要设计 §13 有差距，未改设计迁就实现。目录沿用 32 项固定上限，超限保留旧表并记失败，不静默截断；事件的 8 条环仍可能丢旧事件，seq/boot 用于识别缺口，不能当作持久确认结果。boot 文件损坏或无法落盘时启动失败，不复用已知启动身份。

## 改过的已有测试

本轮没有删除测试、修改既有行为断言或预算。`device-events/run.py` 仅追加复查夹具（83 项检查），原两次持久 Flash 启动仍各为 94 项；`reverse.py` 适配新的拒收分支并新增七个反向对照及可选的基线源码编译。HELLO 回执在新夹具中受控延迟 1 秒，以覆盖真实握手窗口；未加 sleep 掩盖产品时序。提前声音失败通知是额外注入，push／创建音频流拒收另走实际下行帧。

`services_finish/policy.c` 原来没有连接中枢就直接注入远程 `tools/call`，新增的连接身份检查使其先被拒收；本轮仅补真实握手及身份同步前置检查，原 36 项断言不变，现为 37 项。修前夹具本次失败 7 项；补前置条件后全部通过，没有放松产品检查或策略断言。

上一轮已改测试如下，继续保留：

- `ai_tools.budget.json`：行为计数改为精确断言 done=1、answers=3，并新增 switch=1、旧轮取消=1。协议 §3.3 要求换成员取消旧轮，所以不能继续要求旧成员回答完整播放。原页面过期、儿童拒绝、翻页和全部资源上限保留。
- `races.c`：判断“告诉当前成员的 AI”时核对模拟中枢接收时的连接成员；受众由连接上下文记录，独立于事件自报 member。换人前已经正确交付给旧成员的事件不能算作串给新成员。原收件箱、通知和竞态断言保留。
- `starts.c`、`services_finish/actions.c`：故障注入适配带原成员身份的 RPC 接口，原失败／完整 actions 断言不变。
- `services_finish/policy.c`：测试请求补当前 member/epoch，asked、字符串伪造、高风险和跨成员断言不变。
- `services_finish/handoff.c`：测试夹具显式关闭成人认证后再锁屏；基线 D-95 已有 120 秒宽限，不能再用锁屏动作隐含“立即关空间”。原阻止事件外发的断言不变，未增加 sleep 或放宽等待。

## 要用户定的

无新增产品选择；没有等待答复，也没有把暂定建议写成需求。未完成的功能按原设计继续需要实现。

## 顺手发现

- 本产品的状态订阅固定槽位已很紧；本次复用 hub 的成员通知给实时会话，没有增加常驻任务或提高槽位／内存预算。
- 换成员时清除排队语音，基线音频服务已经在直接成员回调里完成；新增回归保留这项保证，没有重复加一层中止逻辑。
- 本轮差异 12 个文件，仅两个产品服务、必要的测试／构建登记、依赖状态格和本报告。已检查本地链接（报告 4 条、依赖状态表 11 条）、Python 语法和 `git diff --check`；新增 C 代码没有 write/read/fgets 调用。两段提交差异 `97645ea2..HEAD`、`f1f72ae9..HEAD` 按总规则 §6 的 grep 表达式均为 0 命中，grep 均退出 1；没有本机路径、设备标识或密钥进入提交。
- 本轮代码提交 `348a6bbf`，策略夹具接续提交 `add4d080`；上一轮 `0053ef9e`、`b4c71d45` 的结果仍保留。仅本地提交，未 push；最终报告提交后再核对上述两段差异。
