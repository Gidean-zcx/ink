# 第十二轮软件质量审计

- 审计对象：`9cf64adc9ea0a7ce54d1e8f6e05603928e7f17a5`。
- 日期：2026-11-04。
- 范围：本目录 TASK.md 指定的修复核实、变更热点、测试有效性与文档一致性。
- 方法：单人只读审计；产品代码、现有测试与文档不改动。补充实验仅写入本目录，构建使用任务指定目录；不联网、不接触设备或实机数据。

## 一页结论

**本轮未发现严重或高级别问题。按 TASK 约定的“无严重、高问题”门槛，可以合入 main。** 这一判断只适用于本次提交与下述软件审计范围，不代表设备或发布验收。

上轮要求核实的 8 项中，**6 项已修复、2 项部分修复**。原来的自动选网缺失、短信截断、跨轮关闭，以及正常结束的语音会话泄漏均已消除；剩余的自动选网恢复和语音失败收尾边界分别记为 AUD13-001、002。

新增 **4 项中级问题**，没有新增低级问题：

- **AUD13-001**：Wi-Fi 消失恰逢蜂窝重新注册时，自动模式可能持续离线；需另一事件或重设 auto 唤醒。
- **AUD13-002**：长按说话期间收到断线失败，仍采集到松键；松键又将错误覆盖为持续的“正在想”。
- **AUD13-003**：远程反显恢复可复制未完成单元的局部像素；影响限于反馈区域。
- **AUD13-004**：AI 回答面板遮住远程断网提示，现有测试计数不能证明实际可见。

这些问题有明确的触发条件或有限影响，本轮不将它们列为合入阻断。建议先跟进 001、002 的恢复行为，再修补远程提交边界与提示验证。

【复现】普通构建 **56/56**、ASan/UBSan 构建 **55/55** 通过；6 个补充实验在两种构建下完成。负对照证实大部分新增回归能抓住对应旧缺陷；`net_auto` 对 HAL 作废旧连接的覆盖不足，已用独立 HAL 实验补证。执行队列、cap、通信 AT 串行与短信时限、音频麦克风归属未发现新的严重/高问题；网络、AI、远程的具体残留见上述 4 项。

【文档核查】【复现】D-58 仍是暂定方案；Wi-Fi 换页约 1.36 s 的说明属实，800 ms 目标仍未达成。内部 RAM 的 33504 B 最低余量只属于模拟堆预算，距 32768 B 门槛为 736 B，不能据此断言设备内存足够。

## 修复核实

“已修复”针对原问题的复现路径与根因；不表示所在模块没有其他问题。详细运行摘录见 [实验说明](experiments/README.md)、[结果](experiments/results.json) 和 [负对照](experiments/mutation_results.json)。

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD12-001 | 部分修复 | 【复现】integration-net-auto 23 项检查通过；补充 net_recovery 的普通与 ASan/UBSan 运行都复现 AUD13-001，见 experiments/results.json。【代码核查】net_service.c:66-112、hal_net_sim.c:296-341。 | 普通自动切网已实现，原高问题已消除。Wi-Fi 消失与 LTE 注册重叠时仍可一直离线；残留为中级 AUD13-001。 |
| AUD12-002 | 已修复 | 【复现】integration-lifecycle 的 sms_checks（lifecycle.c:406-440）通过；【代码核查】comm_service.c:182-203、249-260。 负对照恢复 text[320] 后 lifecycle exit 1、test.fail=3。 | 109 字混合短信与 118 字正文加 20 位号码全文比对通过，119 字拒绝。4 字节字符还受参数表字节容量限制，代码明确拒绝而非截断。 |
| AUD12-003 | 已修复 | 【复现】integration-starts 13 项检查通过，含首次打开和重新登记的同步 NOMEM；【代码核查】rt.c:90-110、129-154、191-195。 去掉同步 announce 失败清理后 starts exit 1、test.fail=4。 | 首次失败返回 -1 并归还会话，重登记失败通知拥有者；后续新会话正常。 |
| AUD12-004 | 已修复 | 【复现】integration-ai-race 6 项检查通过，旧回答锁边界上开始新一轮后，新会话仍可获回答；【代码核查】ai_service.c:148-158、316-334、366-379。 恢复“解锁后关闭当前会话”后 ai_race exit 1、test.fail=2，新会话丢失。 | 同一临界区取走旧 sid，解锁后只关该 sid；flush_uplink 的收尾也匹配 stream。 |
| AUD12-005 | 已修复 | 【复现】integration-net-auto 通过；独立 hal_switch 在普通与 ASan/UBSan 下都观察到旧连接 CLOSED，恢复旧 N.type 赋值后 exit 1、closed=0。【代码核查】hal_net_sim.c:189-215、284-293，hub_link.c:419-428。 | HAL 与 hub 两层修复有效。现有 hub.link_switched 断言只证明 hub 主动关闭；恢复 HAL 缺陷时 net_auto 仍通过，不能单独作为 HAL 行为的回归证据。 |
| AUD12-006 | 已修复 | 【复现】integration-lifecycle 的 overlap_check（lifecycle.c:579-604）通过；【代码核查】remote.c:159-199、250-270。 去掉 holding 门控后 lifecycle exit 1、test.fail=2。 | 原 A 完整、B 未结束的交错与合并配额问题已修复。独立的反显恢复路径仍可提前显示局部像素，另记中级 AUD13-003。 |
| AUD12-007 | 已修复 | 【复现】integration-starts 的两个高优先级调用者和低优先级初始化者均完成；【代码核查】exec.c:63-93。 换回 c95f48c 的初始化代码后 starts 在墙钟 50 s 内无法完成，被超时终止。 | 初始化改用 g_starting 互斥；不再通过 yield 等待初始化任务。 |
| AUD11-005 | 部分修复 | 【复现】ai_turn_end、ai_turn_lost、dismiss 等场景通过；ai_lost_capture 的普通与 ASan/UBSan 运行均复现 AUD13-002。【代码核查】ai_service.c:316-334、495-510、580-593。 | 原正常结束的会话泄漏及 AUD12-004 的跨轮关闭已修复。失败发生于松键前时仍采集且松键覆盖错误提示，属于中级残留；既有 ai_turn_lost 的松键顺序未覆盖此路径。 |

## 新问题

### AUD13-001（中）未选中的蜂窝网络重新注册不发通知，自动选网可持续停在已消失的 Wi-Fi

【复现】【代码核查】。位置：`system/services/net/net_service.c:69`、`system/services/net/net_service.c:111`、`ports/sim/hal/hal_net_sim.c:332`、`ports/sim/hal/hal_net_sim.c:336`、`platform/include/ink/hal_net.h:48`。

影响与定级：Wi-Fi 消失时 LTE 恰在切卡/重新注册，服务发现两个网络都不可用后停止定时重查；LTE 随后注册成功但仅对所选 LTE 调用 relink，Wi-Fi 仍是所选类型，因而不会再决策。网络恢复后设备仍离线，需再设一次 auto 或其他网络事件唤醒。需要两种网络状态交叠，按中级；不把普通 Wi-Fi 消失时已可用的 LTE 切换误判为未实现。

复现证据：模拟器运行 experiments/net_recovery.c 与 net_recovery.script：原样产品启动、公开设置切卡，无代码失败注入。11.010 s type=wifi up=0 lte_available=0；17.000 s 与 40.000 s 均 type=wifi up=0 lte_available=1；40 s 再设 auto 后 44 s type=lte up=1。普通与 ASan/UBSan 构建均 exit 0、audit.finished=1，现象一致。命令与摘录见 experiments/README.md、experiments/results.json。

建议：蜂窝可用性变化也应通知 HAL 的链路/可用网络监听者，即使未选中 LTE；或者在自动且离线时保留有界重查。补充 Wi-Fi 消失早于蜂窝注册完成的回归。

### AUD13-002（中）按住说话时收到断线失败仍继续采集，松键会把错误覆盖成持续的“正在想”

【复现】【代码核查】【文档核查】。位置：`system/services/ai/ai_service.c:316`、`system/services/ai/ai_service.c:236`、`system/services/ai/ai_service.c:495`、`system/services/ai/ai_service.c:580`、`tests/scenarios/ai_turn_lost.script:4`、`docs/system/system-detailed-design.md:657`。

影响与定级：断线回复会清会话并丢弃后续帧，但不终止 AI.capturing；用户松键时 was_capturing 仍为真，无条件将状态改为“正在想…”。这一轮已无待回请求，网络恢复也不会改回错误。必须重按/关闭恢复；需在松键前收到失败，限于失败轮次的采集与提示，按中级。既有 ai_turn_lost 在 7 s 松键、约 10.5 s 才发现断线，没有覆盖此顺序。

复现证据：模拟器运行 experiments/ai_lost_capture.c 与 ai_lost_capture.script：3 s 按住 AI 键 14 s，5.5 s 断网 20 s。13 s session=0 capturing=1 asking=0 failures=1，提示网络断开；19.010 s 和 28.010 s session=0 asking=0，提示却为“正在想…”。原样产品代码，无锁交错注入；普通与 ASan/UBSan 构建均 exit 0、audit.finished=1，现象一致。持续状态的判断另有代码依据；实验观察截至下一次按键前。命令见 experiments/README.md。

建议：失败/会话关闭时向 UI 投递携带轮次的统一结束操作，停止该轮采集并保留失败状态；松键先判断轮次是否已失败或结束，不能再进入思考。增加长按跨过断线检测时刻的场景。

### AUD13-003（中）反显恢复直接读未完成的后画布，局部区域可在 LAST 之前显示

【复现】【代码核查】。位置：`system/apps/remote/remote.c:140`、`system/apps/remote/remote.c:154`、`system/apps/remote/remote.c:243`、`system/apps/remote/remote.c:431`、`docs/system/system-detailed-design.md:551`。

影响与定级：holding 修复了完整单元 A 等待显示时混入 B 的原问题，但 spot_restore 会直接把 back 拷到 px。若新单元已解码部分区域、尚未收到 LAST，此时上一次点按的 2 秒反显到期或用户再次点按，恢复的局部方块就含有未完成单元的像素。影响最多为反显区域，需帧间延迟或再次点击，按中级；没有发现越界或跨会话内存破坏。

复现证据：experiments/remote_feedback.c 原样包含 lifecycle.c 及产品 remote.c，用 4×4 合法画布复用现有集成夹具：仅解码 B 的首块（x=1 黑，未带 LAST），调用真实 spot_expired。输出 after_restore display=0 expected_display=255 complete=0 updates=0 owed=0；补上 LAST 后 updates=1 owed=1。普通与 ASan/UBSan 构建均 exit 0、audit.finished=1，无警告。此为组件级帧顺序实验，不声称实网发生率；命令见 experiments/README.md。

建议：保存反显前的前画布像素，或在独立于解码中的 back 的已提交画面上恢复反显。补充 LAST 延迟跨过反显超时/第二次点击的测试。

### AUD13-004（中）AI 打开的远程页面断网提示被回答面板遮住，重连测试仍声称提示已显示

【复现】【代码核查】【文档核查】。位置：`system/apps/remote/remote.c:130`、`system/apps/remote/remote.c:401`、`system/apps/remote/remote.c:496`、`system/services/ai/ai_service.c:104`、`tests/scenarios/remote_reconnect.script:4`、`tests/scenarios/remote_reconnect.budget.json:5`。

影响与定级：现有正常场景由 AI 打开远程页面后保留底部 58% 的回答面板；远程 note 是页面底部子对象，位于 lv_layer_top 的面板下面。断网期间用户看不到重连提示，预算却仅凭 remote.offline_shown=1 判定已显示。有限的状态提示与测试覆盖缺口，按中级；不会阻止重连、锁屏关闭或恢复画面，不夸大为这些能力失效。

复现证据：普通 CTest 的原 scenario-remote_reconnect-metalio-plus 已复现，无补丁或故障注入：remote.offline_shown=1，23.1276 s 的 00011_023127.6ms_GL16.png 屏幕下半部仅显示 AI 回答，未出现断线文字；已人工查看该帧及 28.9135 s 恢复帧。截图归档 experiments/remote-offline-covered.png；原场景 exit 0，预算通过。

建议：让断线提示位于用户当前能看见的位置，或打开页面后明确处理 AI 面板的遮挡；场景应在提示期间验证实际绘制结果，另覆盖面板打开与关闭两种情况。计数名称/断言区分“请求显示”和“用户已看见”。


## 分块核查记录

### 变更热点

| 范围 | 证据与判断 |
| --- | --- |
| 网络策略、设置与定时器 | 【代码核查】`net_service.c:66-118`：决策集中在定时器，后台工作读取最新目标；手动选择、15 s 失败回退、60 s 稳定等待及会话保护已实现。【复现】net_auto 的 23 个检查通过，禁用自动选择的负对照失败。可用性通知遗漏见 AUD13-001；该实验也验证了 `@0 wifi`、`@0 sim` 环境生效。 |
| HAL 切网与中枢连接 | 【代码核查】`hal_net_sim.c:189-215、284-300、1179-1210`，`hub_link.c:419-428`：保留旧链路类型判死连接；环境先初始化，启动不覆盖脚本的网络选择。【复现】独立 HAL 测试切网后收到 CLOSED；恢复旧赋值后失败。没有把 hub 主动关闭计数当作 HAL 行为证明。 |
| hub 请求重入与 RT 失败 | 【代码核查】`hub_link.c:174-188、387-404、531-539`：finish 在回调前取走旧参数与回调信息；hub 任务内直接接纳请求，避免向自己的满队列阻塞。`rt.c:90-154` 同步失败归还会话，重登记失败通知拥有者。【复现】starts、lifecycle 和会话场景通过；同步失败负对照能检出退化。未发现新的重复释放、旧参数误释放或稳定死锁。 |
| 通信服务 | 【代码核查】`comm_service.c:68-87、109-145、182-203、224-260`：状态、读短信、切卡、发短信与通话 AT 调用使用 `CM.at`；拿到锁之后再算短信剩余时限，小于 3 s 不启动发送。切卡失败有有界退避；URC 等待不发送 AT。【复现】保存卡选择、重设同值纠正、长短信全文和超限拒绝等检查通过，没有 AT 重叠警告。未接真实模组，不据模型时限保证实网短信结果。 |
| AI 一轮结束与换轮 | 【代码核查】`ai_service.c:148-158、316-334、350-379、495-510`：同锁取走 sid，flush 收尾核对 stream，旧回调不再关闭新会话。【复现】ai_race、ai_turn_end、ai_turn_lost 等通过；长按跨过失败检测时刻仍有 AUD13-002。 |
| 音频麦克风归属 | 【代码核查】`audio_service.c:208-242、628-721`：采集收尾保留占用到旧回调结束，启停由 mic_lock 协调。【复现】lifecycle 的两种采集模式及回调内重开检查通过；额外的前端启动失败交错未复现静默破坏，普通和 sanitizer 输出一致。未作硬件双核调度或真实 AFE 验证。 |
| 远程画面生命周期 | 【代码核查】`remote.c:159-270、316-426`：holding 门控修复原帧混合，new_picture 换代并清待提交状态，锁屏关闭/解锁重新开会话，重连重置画布。【复现】lifecycle 和 remote_reconnect 通过；重连场景 resynced=2、opened=closed=3。另有反显局部越过 LAST 和提示遮挡两项残留，见 AUD13-003、004。 |
| 执行队列与 UI 唤醒 | 【代码核查】`exec.c:63-93` 用互斥等待初始化；`ui_task.c:435-450、635-645` 处理丢失唤醒并计入忙状态。【复现】starts 的优先级交错通过；80 个 UI 节点塞满 24 格唤醒队列后全部按序运行，忙状态正确，未遗失。旧初始化代码的负对照超时。 |
| cap 调用生命周期 | 【代码核查】`cap.c:220-250、307-396、475-507`：空请求号按无号处理，完成、答复与持有引用分开，超时后的工作不重复答复。【复现】lifecycle 中去重交错、回调重入、延迟答复、池满/归还及截止时间测试通过。未发现本轮变更引入的新严重/高问题。 |

### 测试有效性与文档边界

【复现】六类负对照能使既有回归失败：短信缩小缓冲、RT 同步失败不清理、初始化 yield 等待、AI 关闭当前会话、远程取消 holding、禁用自动选网。具体退出码与失败摘录在 [mutation_results.json](experiments/mutation_results.json)。这是有针对性的反例核查，不是完整变异覆盖率。

【复现】【代码核查】恢复 HAL 的旧赋值后，`net_auto` 仍有 23 个通过，`hub.link_switched=4`。它验证的是 hub 主动关闭，而不是 HAL 自己会报告旧连接失效。补充的 `hal_switch` 在原实现通过，在变异实现 closed=0 并以 exit 1 失败，因此当前修复可以核实；建议把这一层验证加入原有回归。另两处现有场景的边界是 `ai_turn_lost` 先松键后发现断线，以及 `remote_reconnect` 只检查提示计数，分别对应 002、004。

【文档核查】已读取详细设计 §3、§4.1/4.3、§6、§8、§13、审计 README 第十/十一轮处理及 D-58。D-58 在 `docs/decisions/README.md:109` 明确是暂定推荐，未视作新确认需求。§6.5/§13 的“这一轮作废”和 §8 的完整单元显示，需按 002、003 的条件收窄或修复；断网提示的可见保证需按 004 修正。其余本轮重点说明未发现新的高等级虚假保证。

【复现】【文档核查】`remote_browse` 的 trace 确认 2.697663 s Wi-Fi 已连上。三个换页样本中位数 **1363.872 ms**、p95/最大值 **1368.874 ms**；实际回归上限是 **1500 ms**，`target_max=800` 为尚未达到的目标，报告不能把 CTest 通过等同于目标达成。当前约 1.36 s 的修订是诚实的；样本少且为固定模型，不构成实网时延保证。

【复现】【代码核查】本轮 PQ-BN AI 场景的内部总堆最低余量为 **33504 B**。`H`、`UI`、`REG`、`SUBS`、`LIB` 确有 `INK_PSRAM_BSS` 标记，但主机上该宏不能证明目标链接位置。补查了主机栈与 BSS 清单：单函数 JSON 解析 2640 B，cap 目录回复 1440 B；这些不是设备栈高水位。审计 README 对“堆预算不包含设备静态数据”“等待设备 map”的说明正确，未将主机 BSS 数值直接扣到模拟堆上。摘录见 [baseline_results.json](experiments/baseline_results.json)。

## 命令、边界与未查范围

- 审计对象始终为 **`9cf64adc9ea0a7ce54d1e8f6e05603928e7f17a5`**，对照 `c95f48c` 及相关第十轮修改；读取本地 git 历史与差异，未切换分支、提交、推送或合并。
- 构建命令为任务指定的本地依赖配置，加 `-DINK_WERROR=ON`；分别执行 `cmake --build build-audit -j8`、`cmake --build build-audit-asan -j8`，以及对应 `ctest --test-dir ... -j8 --timeout 300 --output-on-failure`。ASan 构建额外指定 `-DINK_SANITIZE=ON -DINK_GUI=OFF`。完整可复制命令在 [experiments/README.md](experiments/README.md)。
- 补充执行 `run_probes.py`、`mutations.py`、`stack_inventory.py`、`static_inventory.py`；编译对象与输出在 `build-audit/` 或 `build-audit-asan/`，日志在 `/tmp/ink-audit12-*.log`。交付目录只留程序、摘要与一张证据截图，无构建日志。
- 仅修改本轮审计目录的交付文件。最终检查跟踪文件的 `git diff` 为空；未跟踪项仅本轮目录，与开始时相同。JSON 格式、编号、引用路径和文件大小检查均通过；最大交付文件不足 20 KB，全部小于 1 MB。
- **未查设备和实网**：任务要求不接触设备或 ink-hw 数据；未验证实际双核并发、ESP-IDF 链接 map、目标栈高水位、真实模组/AFE、无线环境及墨水屏波形。ASan/UBSan 不等于硬件内存与时序验证，也未运行 TSan 或穷举调度。
- **未重审无关范围**：没有扩展到上两轮未改动的完整驱动、服务器实现、部署或账户系统；外部决策板、远端 Issue/PR 与 CI 状态未联网查询，遵守本轮离线范围。本次结论不能替代这些范围的证据。
