# device-events 执行报告

日期：2026-11-06；基线：`f1f72ae9`。证据范围是本机 macOS 模拟器、`sim_hub` 和合成家庭中枢，没有真机或真实厂商验证。范围按[任务书](../backlog.md#device-events)、[两端约定](../../hub/contracts-step1.md)和[设备协议](../../hub/device-protocol.md)核对。

本轮复修基线：`97645ea2`。按[独立复查](REVIEW.md)处理 DE-R1、DE-R2（严重 0、高 2，未单列中级）。下表保留上一轮证据；成员握手隔离和声音失败提示的原“通过”已被复查推翻，本轮正在补修和验证，全量、内存及反向对照结果完成后更新。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| DE-1 事件 | 【实测】通过 | `device/event` 带 seq、boot、member；boot 原子落盘，重启递增；保留最近 8 条直到 ACK；丢 ACK 重连用同一身份重发，错 boot／超前 ACK 无效 | `ctest --test-dir build --output-on-failure -j3 -R '^device-events$'` |
| DE-2 成员与交付 | 【实测】通过 | hello 带实际成员、authed、epoch；切成员和认证变化推进代次；入站缺少／不匹配 `_meta.member/epoch` 回 -32023。校验到分配、UI 队列、执行和回执均保留原身份；A→B→A 不恢复旧请求；出站快照、RPC、会话和帧不重新绑定成员；换人关闭会话，排队语音被清除 | 同上；`python tests/services_finish/run.py --build build handoff actions policy isolation` |
| DE-3 时间与背压 | 【实测】通过 | 截止时间从请求创建计，不因等队列重置；budget_ms 在实际发送时写入，最多 45000。按键不松开也在 30 秒结束录音；下行队列拒收后中止该段声音并显示失败，不再静默丢帧 | 同上 |
| DE-4 能力目录 | 【实测＋实现】通过 | 已测全分页原子替换、不透明 cursor、同 rev、重复 cursor 和失败保留旧表；代码另拒绝重复名字和超限，暂存表放 PSRAM。sim_hub 默认分 3 页。保留基线完整 actions，未改为空数组；actions_more 可选优化未做 | 同上；`actions` 回归仍要求完整能力列表超过 768 字节 |
| DE-5 两端契约 | 【实测】通过 | 假设备保存并检查交付代次；中枢接受 hello epoch，ACK 事件、检测缺口和重连重复；proto1 仍固定合成儿童，不因 authed 自报而授予成人权限；保留原 result／MCP 回执、语音、听写和错误测试 | `ctest --test-dir build --output-on-failure -j3 -R 'hub-contract|hub-json-crosscheck|hub-simulator-e2e'` |
| DE-6 D-105 | 【实现＋实测】部分 | 保留基线 asked、ask_by_hand、活动记录；校验 undo_ms 必须为 3000。自动撤销窗口尚未实现：带窗口的请求，或 asked 的短信／拨号，明确回 -32021，不能忽略窗口后立即执行 | `device-events`；`policy` 回归 |
| DE-7 反向对照 | 【实测】12/12 | event、member、accept、epoch、directory、replay、budget、scope、undo、deadline、capture、downlink；每次窄撤回一个保护，编译和链接均为 0，指定断言失败且 test.fail>0；模拟器本身可退出 0，不冒充进程失败 | `python tests/device_events/reverse.py --build build` |
| G1 全量与内存 | 【实测】两套均 103/103 | 普通 54.13 秒、ASan+UBSan 88.73 秒；中枢契约内部 125 项也全过，无跳过。包含预算、确定性、重启和无窗口 GUI 检查；全部命令退出 0 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；同命令换 `build-asan` |
| G2 原子能力／AI | 【实现】通过 | 未增加用户操作或页面；工具仍走同一能力策略关口。新增的是传输身份绑定接口，不让界面直接修改服务状态 | 能力表 runtime／policy 和现有场景测试 |
| G3 依赖状态 | 【实现】已更新 | 只更新 ui-dependencies 的“成员、PIN”状态格，并链接本报告 | `git diff -- docs/system/ui-dependencies.md` |
| G4／G5 范围 | 【实现】符合 | 无新页面；只改服务、策略、模拟替身和测试。没有启动带窗口的 GUI；全量原有 gui-smoke 使用 SDL dummy | `git diff --stat f1f72ae9` |

复现环境：构建参数按总规则，两个构建均使用 INK_WERROR=ON，内存构建另加 INK_SANITIZE=ON。中枢 Python 在 `work/hub-python`：运行依赖从 `hub/requirements.lock` 用 `--require-hashes` 安装，pytest=8.4.2、pytest-asyncio=1.2.0；环境的模块搜索路径指向本仓库 hub。CMake 的 INK_HUB_PYTHON 指向该解释器，未跳过中枢测试。详细原始输出在被忽略的 `work/` 和构建目录；上表命令可重新生成。

## 没做到的

- **三秒自动撤销窗口**：只有字段检查和明确拒绝，未实现“内容真正上屏后计时、到点执行、这一轮取消时撤销”。需要显示回执和确认事务接续，不能拿收到请求的时刻冒充上屏。本次不实现确认结果持久化、confirm.status、confirm/request。
- **中枢成员表**：未实现 members/list 的全页替换和 member.reset_pin；保留设备原有成员表和本地密码保护。当前真实中枢仍是受限 proto1 开发握手，没有正式家庭身份和成人解锁证明，不把自报 authed 当证明。
- **同步客户端和数据集元数据**：未实现 sync/*、一致清单、原子快照安装和 history_gap 对账；完整流程依赖本节排除的块传输，不能把本地保存回执冒充中枢备份。
- **设备末端探针上传**：现有本地探针仍在；没有新增线上上传格式，设备协议第 5 节仍把格式列为第 2 步待定事项。
- **§13 其余后续／可选项**：未增加内容包、网页新动作、真 Opus／JPEG 模式、rt.close 的 reason／heard_frames、按计划短连。现有本机中枢桥已经通过 e2e；这不等于真媒体模式。未自行认定这些功能不需要。
- 配对、固定公钥、地址／令牌存放、首次开机、确认持久化和块传输按本节排除范围未改。未在 Linux gcc 或硬件上运行；新增代码未使用 write/read/fgets。

## 和设计不一致的

上列未完成部分仍与概要设计 §13 有差距，未改设计迁就实现。目录沿用 32 项固定上限，超限保留旧表并记失败，不静默截断；事件的 8 条环仍可能丢旧事件，seq/boot 用于识别缺口，不能当作持久确认结果。boot 文件损坏或无法落盘时启动失败，不复用已知启动身份。

## 改过的已有测试

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
- 已核对差异范围和本地链接（报告 3 条、依赖状态表 11 条），Python 语法和 `git diff --check` 通过。`git diff f1f72ae9..HEAD` 按总规则 §6 的 grep 表达式扫描：0 命中，grep 退出 1；没有本机路径、设备标识或密钥进入提交。代码验收提交为 `b4c71d45`（前一里程碑 `0053ef9e`）；报告提交后再核对同一范围。
