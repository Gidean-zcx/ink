# 家庭中枢软件设计复核（第二轮）

审查日期：2026-11-04。审查对象：`a74029409e61960833eb233e39aaa8c772944adc`，家庭中枢设计 v0.2。状态：完成。

## 1. 一页结论

**有条件可行。v0.2 补齐了许多上一轮缺口，可以开始第 1 步的可逆骨架开发，但还不能冻结两端契约，也不能认定配对、同步恢复和异步隔离已经闭合。** 本次没有中枢实现可运行；结论针对设计及其依赖的设备接口。

- 原 25 条：**已解决 15、部分解决 9、未解决 0、改出了新问题 1**。“已解决”指设计回应了原问题，必要改造及验收前提已明确，不表示功能已经实现。唯一“改出了新问题”是 HUB-002：定义字典方向正确，但新交集依赖一个会溢出成空数组的 `actions`。
- 新问题：**严重 3、高 7、中 0、低 0**。其中包括处理原问题时留下或引入的具体缺口，不能把这 10 条与原来的 25 条相加当成独立缺陷数。
- 三个严重问题：无 pin 的电脑认领回退丢失公钥指纹（HUB2-001）；安全日志没有跨整机损毁的持久性保证（HUB2-003）；恢复旧备份时“代次加一”可以重用代次（HUB2-004）。分别关系到身份信任、吊销/忘掉是否倒退，以及同步删除与去重是否可靠。
- 最明确的现有代码证据：仅 53 个全局能力名就需要 **780 字节**，现有缓冲为 **768 字节**；生产序列化函数返回 `-1`，调用方发送 `[]`。照新交集规则接入，所有设备工具都会消失（HUB2-006）。这是函数级实测，不是完整固件联调。

### 开发前还必须定什么

这些是技术契约缺口，开发方可以在现有可逆授权内处理，不需要把每个字段变成新的用户选择。花钱、真实家庭数据和设备写入仍遵守概要 §10 的门槛。

| 时间 | 必须明确的规则 | 对应问题 |
| --- | --- | --- |
| 第 1 步可以先做 | 网关、总线、合成 Scope、假厂商、任务队列、worker 隔离和 1× 模拟器桥；测试覆盖实际能力规模，不能只放一两个假工具 | 已解决项的实现验证 |
| 第 1 步契约验收前 | 可用能力集合的完整性/溢出/更新；成员返回后的重新授权及交付代次；设备剩余请求预算和音频在途上界；确认待办与执行终态的区分。“只改模拟器”只能描述基础纵切，不能代表这些契约无需设备修改 | HUB2-006～009 |
| 第 3 步配对验收前 | 无 pin 的扫码和电脑入口如何传递信任证明；待生效凭据已落盘后的超时、重启如何恢复 | HUB2-001、002 |
| 第 4 步恢复验收前 | 安全日志的故障域与恢复开放条件；不从备份重用的恢复标识；全量清单的稳定水位、正文补齐、原子提交与增量接续 | HUB2-003～005 |
| 第 6 步自动浏览动作启用前 | 哪些已登录网页动作能自动执行，未知链接如何确认；DOM 类型不能承担最终业务授权 | HUB2-010 |

同代快照的停写/排空/完成标记、worker 权限边界、查询结果生命周期、内容包硬依赖、费用预留、真媒体与替身媒体分开验收，均已回应相应原问题。本轮没有因这些功能尚未实现而重复报错，也未把设计图或合成流当成生产验证。

## 2. 处理核实

行号以固定审查提交为准。`a81c1b8:路径:行号` 指设计引用的系统服务提交，不是工作树中的同名文件。

| 编号 | 结论 | 依据 | 说明 |
| --- | --- | --- | --- |
| HUB-001 | 已解决 | 【文档核查】docs/hub/device-protocol.md:342、344、345；docs/hub/hub-architecture.md:1188。【代码核查】a81c1b8:system/policy/cap.c:590。 | 系统服务分支保留 result；文档明确查询分页、回调生命周期和合入依赖。解决的是设计缺口，不代表当前工作树已实现。 |
| HUB-002 | 改出了新问题 | 【文档核查】docs/hub/device-protocol.md:362、365、366、368。【代码核查】a81c1b8:system/services/ai/ai_service.c:481、483；a81c1b8:system/policy/cap.c:863。【复现】experiments/discovery_probe.py：53 项的 JSON 长 780 字节，768 字节缓冲返回 -1，调用方发送空 actions。 | 累积定义字典修正了按固件缓存动态视图的错误，但以 actions 为必经交集且声称不用改设备，继承了已有容量问题并使全部设备工具消失。见 HUB2-006。 |
| HUB-003 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:224、230、233、240。 | 系统操作恢复为 AI 提案、成人确认；预算、恢复、配置都有能力入口，凭据、确认按钮和固件升级例外明确。 |
| HUB-004 | 已解决 | 【文档核查】docs/hub/device-protocol.md:186、188。 | 改为序列化后整条消息计数，每页不超过 110 词法单元，完整读取才替换目录。分页仍是待实现项。 |
| HUB-005 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:1232、1260、1264、1267；docs/hub/device-protocol.md:205、216。【代码核查】a81c1b8:system/policy/confirm.c:171、253、311。 | 独立方法、来源隔离、原动作绑定和重新校验已经补齐；但 confirm.list 只列待办，不能把查不到且事件丢失推成没有执行。见 HUB2-007；代次的传递见 HUB2-008。 |
| HUB-006 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:531、536、538、540；docs/hub/device-protocol.md:76。【代码核查】system/services/hub/hub_link.c:397、402；system/services/audio/audio_service.c:754。 | 长任务、长朗读、确认不占一轮以及文字上限已明确。倒推发送仍缺设备剩余时限和可用音频缓冲的契约，冷连接与尾帧突发不能据此验收。见 HUB2-009。 |
| HUB-007 | 已解决 | 【文档核查】docs/hub/device-protocol.md:313、326、328。 | 停说、断流、厂商无反应、总时限分别处理；听写拥有者负责断线关闭和麦克风交接，final-only 不再被新文字超时截断。 |
| HUB-008 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:952、984、986；docs/hub/device-protocol.md:148、150。 | 有 pin 的路径和完整二维码的可信控制台比对已经建立独立信任来源；仅地址、无 pin 时的电脑输入认领码回退没有携带观察到的指纹。见 HUB2-001。 |
| HUB-009 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:329、330、331、341、342、354、356、362。 | 先拉镜像、停写排空、冻结回收、同代完成标记及开放写入后的人工恢复边界已补齐。不可倒退日志没有跨主机损毁的持久性保证，恢复后安全状态仍可能倒退。见 HUB2-003。 |
| HUB-010 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:362、831、833、834；docs/hub/device-protocol.md:263、275、277、279。【代码核查】a81c1b8:system/services/data/datastore.c:885、916、932。 | 补上代次、缺口和全量对账、原子安装的方向正确，但从可回退备份中的代次加一会重用；全量对账没有稳定水位、缺失正文传输和提交 ACK 规则。见 HUB2-004、HUB2-005。 |
| HUB-011 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:673、681、682、684、685；docs/hub/device-protocol.md:166、168、170、353、356。 | Scope、接收成员、切换取消和发送前检查已补齐；旧代次一律丢弃与结果留到成员回来之间没有重新授权规则，设备入站字段仍只检查成员名。见 HUB2-008。 |
| HUB-012 | 部分解决 | 【文档核查】docs/hub/device-protocol.md:136、138、145、152、156、158、379。 | 等待 10 分钟、首次 hello 激活及 complete 丢失重发已补齐。设备已存待生效凭据但首次 hello 超过十分钟时，服务器作废而设备只能退避 hello，没有回到配对的持久状态转移。见 HUB2-002。 |
| HUB-013 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:260、275、396。 | 向导先建立可信 HTTPS 或 SSH 转发；可信之前不收秘密，无证书不降为未知自签。 |
| HUB-014 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:721、727、342、354、362；docs/decisions/README.md 的 D-93。 | 依赖关系、派生重建、禁止重新提取和原话保留选择已有约定；忘掉记录仍受不可倒退日志的异地持久性缺口影响，旧备份恢复可重新暴露它。见 HUB2-003。 |
| HUB-015 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:185、211、747。 | 独立 worker，无网络、密钥和数据库，只映射单任务输入输出，主进程校验产物。实际容器隔离仍需第 1 步实现验证。 |
| HUB-016 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:755、761、1475。 | 内容包 v1、解码样本、阅读器与播放器改造已列为第 5 步硬依赖，不再声称现有 TXT 阅读器直接支持。 |
| HUB-017 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:1387、1389、1393、1402。 | 1× 实时网络桥、替身模式、真 Opus/JPEG 模式和分阶段验收已区分，假媒体成功不等于真媒体通过。 |
| HUB-018 | 部分解决 | 【文档核查】docs/hub/hub-architecture.md:797、800、802、803、804。 | 截图截止时间、像素比较、单元不打断、输入改造已经补齐；新增的按 DOM 区分普通链接与副作用动作不能成为安全边界。见 HUB2-010。 |
| HUB-019 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:470、473；docs/decisions/README.md 的 D-79。 | 原子预留、调用上界、未知费用待对账和软硬阈值均明确，具体金额仍待用户选择。 |
| HUB-020 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:458、464。 | 保留原始厂商字段、完整组装工具调用、每模型能力表，首版限定一家一个模式。本轮未重新审计各厂商现行接口。 |
| HUB-021 | 已解决 | 【文档核查】docs/hub/device-protocol.md:243、251、257、285。 | 停等按 64 KiB 段估算，上传编号限定连接、落盘 ACK、最终哈希和 OTA 固定包已补齐。实际吞吐未测。 |
| HUB-022 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:861、868。 | 任务运行、取消、结果未知分开；副作用 unknown 不重试，持久幂等与内容复用已明确。确认事件错误推成未执行另列 HUB2-007。 |
| HUB-023 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:909、1398；docs/decisions/README.md 的 D-86。 | 合成数据可以暂定，首次真实家庭数据落盘前明确设置用户确认和备份演练门槛。 |
| HUB-024 | 已解决 | 【文档核查】docs/hub/hub-architecture.md:512、525、529、1389。 | 模型 2 秒只是必要指标，文档承认尚未计入网络与停说等待；成人/儿童、冷热连接、工具路径均量到设备末端并报 p50/p95。时延目标未实测达成；整轮截止另见 HUB2-009。 |
| HUB-025 | 已解决 | 【文档核查】docs/hub/device-protocol.md:27、40、79；docs/hub/hub-architecture.md:1387、1388、1389。 | 修正 ai.ask 文本来源、rt.open 次序和方法长度；开发第 2 步分为免费假厂商与批准后的付费真厂商。 |

## 3. 新问题

定级沿用任务说明。**【复现】分为生产函数实验与规则/算术模型，具体边界见各条及实验记录；退出 0 表示反例断言成立，不表示产品通过测试。** 推断不作为已发生的故障或攻击报告。

### HUB2-001 | 严重 | 无预置公钥时，电脑只输入认领码的路径没有带上设备观察到的指纹

**证据类型：** 【文档核查】【推断】

**位置：** `docs/hub/device-protocol.md:148`；`docs/hub/device-protocol.md:150`；`docs/hub/hub-architecture.md:952`；`docs/hub/hub-architecture.md:986`

**证据与影响：** 完整二维码携带观察到的公钥指纹，单独的八位随机认领码没有。若可信控制台仍接受只有代码的无 pin 认领，网络中间人可代理真实认领流程，使用户看到正确家庭名，却在攻击者终止的设备连接上收到配对凭据；若拒绝，则文档承诺的电脑回退不可用。这是有条件的协议攻击推演，未进行真实攻击，不适用于已有 pin 的路径。

**建议：** 无 pin 时必须把设备观察到的指纹通过独立可信的人机路径带到控制台，并绑定本次配对交易和随机数；电脑入口输入完整可信链接或另核对指纹。无法完成验证就禁止该回退路径，不能仅凭认领码跳过公钥比对。加入代理真实认领码但替换设备 TLS 公钥的负例。

这里不要求用户忽略浏览器证书警告：用户仍可在正确的可信控制台操作，缺失的是控制台对设备实际连接公钥的比对。有 pin 的连接和携带完整指纹的二维码已回应原来的自证问题。

### HUB2-002 | 高 | 设备已保存待生效凭据后跨过十分钟期限，没有自动恢复转移

**证据类型：** 【文档核查】【复现】【推断】

**位置：** `docs/hub/device-protocol.md:136`；`docs/hub/device-protocol.md:138`；`docs/hub/device-protocol.md:145`；`docs/hub/device-protocol.md:156`；`docs/hub/device-protocol.md:158`；`docs/hub/device-protocol.md:379`；`docs/hub/hub-architecture.md:1390`

**证据与影响：** pair/complete 已落盘但首次 hello 前设备断电或断网超过十分钟，中枢作废待生效凭据；设备有凭据，只能进入 hello，auth_invalid 又只要求退避重连，无法回到仅无凭据才允许的配对入口。文档没有满足每一步重启都能恢复的验收条件。状态模型复现规则缺口，并非运行中枢实现。

**建议：** 持久区分已收凭据的 COMMITTING 和已激活的 BOUND，定义按 txid 查询/重发的幂等规则、过期后的明确错误及清理/重新认领转移；保留旧已激活身份的处理边界，不能把任意认证失败都静默当成重新配对。覆盖保存成功后跨过期限再开机的场景。

`pair/expired` 对仍在配对的连接有用；缺的是已持久化凭据、重启后进入 `hello` 的设备如何识别“尚未激活的配对已过期”，而不是再次延长等待时间。

### HUB2-003 | 严重 | 不可倒退日志只排除本机回滚，未保证整台服务器丢失后仍然存在

**证据类型：** 【文档核查】【复现】【推断】

**位置：** `docs/hub/hub-architecture.md:342`；`docs/hub/hub-architecture.md:354`；`docs/hub/hub-architecture.md:358`；`docs/hub/hub-architecture.md:362`；`docs/hub/hub-architecture.md:727`；`docs/hub/hub-architecture.md:1391`

**证据与影响：** 每日异地备份后发生的设备吊销、成员删除或忘掉记录，若只持久化在原主机，整机损毁后无法从昨天的备份重放。恢复服务会重新接受已吊销身份或暴露已忘掉内容。普通 AI 数据最多丢一天的约定不能证明安全状态不倒退；状态模型验证了该失败时序。

**建议：** 明确安全日志的独立故障域、成功确认前的持久性和恢复新鲜度水位。可采用外部追加日志，或在无法证明日志完整时保持恢复实例不可访问、使旧认证失效并完成删除核对后再开放。整机损毁演练必须包含最近一次备份之后的吊销和忘掉。

### HUB2-004 | 严重 | 恢复备份后把保存的代次加一，不能保证每次恢复都是新代次

**证据类型：** 【文档核查】【代码核查】【复现】【推断】

**位置：** `docs/hub/hub-architecture.md:362`；`docs/hub/hub-architecture.md:831`；`docs/hub/hub-architecture.md:833`；`docs/hub/device-protocol.md:263`；`docs/hub/device-protocol.md:275`；`docs/hub/device-protocol.md:279`；`a81c1b8:system/services/data/datastore.c:932`

**证据与影响：** 从同一份代次为 0 的备份两次恢复并加一，都会得到 1。第二次设备仍持有 1，不再因恢复触发代次不符；第一次恢复后已 ACK 并清掉的删除历史不在设备变更集里，旧记录仍可能留在中枢。代次相同也破坏了恢复前后版本去重域必须不同的前提。模型只验证代次碰撞及指定触发条件失效，不声称现有同步实现已造成数据损坏。

**建议：** 恢复标识必须保证不从可回退快照重用，例如每次恢复生成新且足够长的随机实例标识，或依赖不可回退分配器。明确中枢实例、设备数据实例与集合游标的区别和协商方，拒绝旧代次的 push/ACK；加入连续两次恢复同一备份、设备重复恢复旧快照和迟到 ACK 的测试。

该实验只证明两次都得到 1 以及所写的代次检查失效，不是完整同步仿真。恢复后中枢 ACK 水位较低可能提供另一个异常信号，但它不能重新取出已被设备 ACK 清理的墓碑；还需要明确定义缺口识别和全量回退。`history_gap` 在现文档中特指容量挤掉墓碑，不能直接当作正常 ACK 清理的标记。

### HUB2-005 | 高 | 全量清单缺少稳定水位、正文补齐和提交 ACK，不能据此安全删除和续传

**证据类型：** 【文档核查】【代码核查】【复现】【推断】

**位置：** `docs/hub/device-protocol.md:273`；`docs/hub/device-protocol.md:277`；`docs/hub/hub-architecture.md:833`；`docs/hub/hub-architecture.md:835`；`a81c1b8:system/services/data/datastore.c:885`；`a81c1b8:system/services/data/datastore.c:916`

**证据与影响：** 只有 id/ver/hash 的清单不能补回备份后新建记录的正文，也没有定义整个清单对应哪个持久版本。现有逐条遍历不提供全局快照；上传期间继续编辑时，按结束时 last_ver 确认会吞掉清单之外的新编辑，按不完整清单做缺项删除又会误移记录。实验展示一种当前文字未排除的 ACK 错配实现，不是生产同步复现。

**建议：** 定义对账会话、完整且一致的清单范围及持久水位 V；定义缺失/不一致记录的正文与文件块补齐、失败重试和原子提交。仅确认已完整提交的 V，再续传 V 后变更。以可校验状态证明设备数据完整，不能只用用户是否刚刷机判断。明确所需设备遍历/快照接口及验收。

### HUB2-006 | 高 | 定义字典以 actions 为必经交集，但现有缓冲溢出会把全部能力变为空集合

**证据类型：** 【文档核查】【代码核查】【复现】

**位置：** `docs/hub/device-protocol.md:365`；`docs/hub/device-protocol.md:366`；`docs/hub/device-protocol.md:368`；`docs/hub/hub-architecture.md:1171`；`a81c1b8:system/services/ai/ai_service.c:481`；`a81c1b8:system/services/ai/ai_service.c:483`；`a81c1b8:system/policy/cap.c:863`；`a81c1b8:system/boot/main.c:48`

**证据与影响：** 生产序列化函数的最小 C 实验中，仅 53 个已注册、全局、默认向 AI 开放的能力名就需要 780 字节，不含结尾 NUL；现有数组只有 768 字节，返回 -1，调用方发送 actions:[]。新交集规则因此删除所有设备工具，字典已有定义也无济于事；空数组也不会触发未知能力重拉。

**建议：** 在把 actions 作为权威集合前，定义完整性、版本和容量契约：完整分页/可扩展快照或显式查询当前可用集，溢出必须是错误而非空集合；把必要设备修改列入第 1 步依赖。还需规定同一轮工具调用改变页面后如何更新可用集。用实际注册表规模和最坏字段长度验证。

实验保留 `ink_cap_ids_json` 与 JSON 写入器原函数，所选能力没有页面限制或显式 exposure 字段，并排除事件；在该子集上两项过滤恒真，与生产规则一致。通信能力要求带 modem 的配置，启动/注册路径见 `a81c1b8:system/boot/main.c:48`～`:66`。没有计入额外页面能力及其他显式开放能力；也没有把函数探针说成完整固件启动。检查时系统服务分支头仍保留 768 字节数组。

### HUB2-007 | 高 | 确认已从待办消失且事件丢失，不能推断动作没有执行

**证据类型：** 【文档核查】【代码核查】【复现】

**位置：** `docs/hub/hub-architecture.md:1232`；`docs/hub/hub-architecture.md:1267`；`docs/hub/device-protocol.md:205`；`a81c1b8:system/policy/confirm.c:171`；`a81c1b8:system/policy/confirm.c:253`；`a81c1b8:system/policy/confirm.c:285`；`a81c1b8:system/policy/confirm.c:311`

**证据与影响：** 设备执行短信等动作前已从待确认表取走记录，confirm.list 只列仍在待办的项。执行成功后结果事件若被后续八条事件挤掉，文档规定的回退却向用户报告没有执行；用户重做会重复外部副作用。对尚未收到授权、由中枢执行的动作可以保守不执行，但不能把同一判断套给设备已执行动作。

**建议：** 区分待确认查询与动作终态查询；稳定操作编号关联执行结果，终态保留到可靠 ACK 或可查询的持久历史。证据不足必须是 unknown 并禁止自动重试，不能报告没有执行。测试已执行后事件溢出、断电和重连。

### HUB2-008 | 高 | 旧代次丢弃与留待成员回来冲突，入站协议又缺少交付代次

**证据类型：** 【文档核查】【代码核查】【复现】【推断】

**位置：** `docs/hub/hub-architecture.md:681`；`docs/hub/hub-architecture.md:682`；`docs/hub/hub-architecture.md:684`；`docs/hub/hub-architecture.md:685`；`docs/hub/device-protocol.md:168`；`docs/hub/device-protocol.md:170`；`docs/hub/device-protocol.md:216`；`docs/hub/device-protocol.md:353`；`system/services/ai/ai_service.c:324`；`system/services/audio/audio_service.c:767`

**证据与影响：** 成人 A 在代次 17 发起任务，切到孩子 18，再以 A 解锁进入 19，保留结果仍绑定 17。严格丢旧代次则永远无法交付；只比 _meta.member 又会接受旧授权时期的迟到命令。模型证明相同成员名不足以验证代次。还需明确已结束会话但仍在音频队列中播放的输出归属；关闭实时会话本身不等于清除该队列。

**建议：** 区分结果产生时的 Scope 与本次交付授权：成员回来后重新鉴权，为仍有效结果创建当前代次的交付，绝不重新授权重放旧副作用。tools/call、confirm/request 和会话输出有可验证的交付代次；设备在执行、显示和播放时核验，切换屏障清理已排队输出。规定锁定、返回、重连和撤权时的持久结果策略。

音频边界的代码事实是：`ai_service.c:336` 调 `finish` 后关闭实时会话，`audio_service.c:767` 的 `finish` 允许已收队列继续播放，而 `:774` 的 `abort` 才清队列。这里要求补齐切换时的所有权/清理规则，没有宣称未来的成员切换实现已经泄露声音。

### HUB2-009 | 高 | 中枢按自身 42 秒倒推并突发尾帧，缺少设备剩余时间与缓冲余量依据

**证据类型：** 【文档核查】【代码核查】【复现】【推断】

**位置：** `docs/hub/device-protocol.md:26`；`docs/hub/device-protocol.md:76`；`docs/hub/hub-architecture.md:536`；`docs/hub/hub-architecture.md:537`；`system/services/hub/hub_link.c:397`；`system/services/hub/hub_link.c:402`；`system/services/audio/audio_service.c:56`；`system/services/audio/audio_service.c:754`；`system/services/ai/ai_service.c:319`

**证据与影响：** 设备接受请求即开始 45 秒计时，未联网时也计时；ai.listen 没有剩余期限。冷连接耗 8 秒后中枢再用 42 秒会晚 5 秒。512 帧是总容量而非空闲量，已有 200 帧时再突发 400 帧，即使尾部只有 24 秒也会超容量；满队列时音频 push 失败且 AI 接收路径忽略返回值。算术模型不是实际时延或吞吐测量。

**建议：** 在实际发包时传递可换算的剩余预算，明确排队和网络裕量；发送端必须得到播放进度/配额，或以保守上界限制全部未播放帧，不能把总容量当空闲量。定义完整句截断、长内容转作业及 30 秒录音封顶的设备依赖，并在假厂商阶段覆盖冷连接、播放暂停和慢链路。

八秒冷连接是合成时序，并非实测时延。突发期间若持续播放，实际溢出会小于简单队列模型；暂停播放或接收明显快于播放时，容量仍然不是剩余量。文档的“放不下就停止合成”需要可判断的依据，只有结束时才回报的可选 `heard_frames` 不能支撑发送中的控流。

### HUB2-010 | 高 | 按 DOM 把普通链接判成只读，不能证明已登录浏览器没有外部副作用

**证据类型：** 【文档核查】【推断】

**位置：** `docs/hub/hub-architecture.md:796`；`docs/hub/hub-architecture.md:803`；`docs/hub/hub-architecture.md:804`。

**证据与影响：** 普通 a 元素的点击也能触发祖先委托监听、链接 ping POST 或服务端带副作用的跳转。DOM 外形与 HTTP 方法都不能证明业务只读；在保留成员登录态的上下文中，未经确认的点击可能以用户身份写入或外发。出站代理只屏蔽网络地址，不能替代业务授权。此为标准支持的推断，未对真实网站执行动作。

**建议：** 缩小自动执行边界：纯阅读/滚动、已知站点适配器明确审核的动作；未知链接及已登录交互交由用户或具体动作确认，必要时把无身份内容抓取与有身份浏览分离。禁止用模型自述或 DOM 元素类型作为最终授权依据。加入祖先监听和普通链接引发外部请求的对抗用例。

标准来源（2026-11-04 查阅）：祖先捕获/冒泡监听见 [WHATWG DOM 事件传播](https://dom.spec.whatwg.org/#introduction-to-dom-events)；链接 `ping` 发送 POST 见 [WHATWG HTML 超链接审计](https://html.spec.whatwg.org/multipage/links.html#hyperlink-auditing)。它们支持上述技术前提；具体业务是否写入仍由站点语义决定，不能由 DOM 外形或 HTTP 方法证明。

## 4. 审查范围与验证记录

### 固定版本

| 对象 | 提交/状态 |
| --- | --- |
| 本轮 `git log -1` | `a74029409e61960833eb233e39aaa8c772944adc`；2026-11-04 17:34:46 +08:00；`docs: hub design v0.2 after the Codex review (HUB-001..025)` |
| 上轮对象 | `6d10d959a689a4b28749b2e6d378a6cf58cd8f6d` |
| 设计引用的系统服务基线 | `a81c1b8ed805c044bcc24a120c14bb8e3a4d011e` |
| 检查时 `claude/system-services` 分支头 | `a894ac559bf613ebdc7847fe4b0c61ca44a0530c`；未把后续新增能力混作引用基线已有 |
| 检查时 `claude/system-layer` 分支头 | `cd486a2ec1c1621ddbe7c598ed8b3ff35936fc85`；核对 R-056、D-65 |

### 看了什么

- 协作和背景：`AGENTS.md`、`README.md`、`docs/project.md`、`docs/process.md`、`docs/background.md` 相关段落、`docs/requirements.md`、`docs/ideas.md` 的中枢记录，以及本目录任务说明。
- 上轮完整报告和 `docs/audits/README.md` 的 HUB-001～025 处理表；本轮三份对象 `docs/hub/hub-architecture.md`、`docs/hub/device-protocol.md`、`docs/research/hub-server-survey.md` 及第九轮 D-73～D-94。依据为 `docs/system/foundation.md` 和 `docs/system/system-detailed-design.md` 的数据、AI、权限、传输与语音契约。
- 本工作树的 `system/services/hub/`、`system/policy/cap.c`、`system/services/ai/ai_service.c`、`system/services/audio/audio_service.c`、`system/apps/remote/remote.c`、`apps/reader/reader.c`、`ports/sim/hal/` 的相关实现；通过 `git show` 读取系统服务设计、能力、确认、数据存储及注册表、系统层 R-056/D-65。没有切换分支。
- 重点交叉检查配对信任/提交恢复、同代快照/安全日志、恢复代次/全量对账、确认终态、成员代次/接收人、语音期限/缓冲、定义字典及 worker 隔离。未发现新增的单任务 worker 挂载、无网络、无秘密约定本身有新的严重或高矛盾；运行配置待实现验证。
- GitHub 只读搜索 `repo:Gidean-zcx/ink is:open` 返回空列表，仅代表该搜索没找到开放条目。决策状态以固定提交为准，没有读取或同步外部交互决策板，也未宣称核对了尚未写入仓库的新选择。
- 为 HUB2-010 核对两份 WHATWG 官方标准，查阅日期 2026-11-04；未重新查模型/服务器价格或所有厂商现行接口。

### 跑了什么

| 命令/检查 | 结果及边界 |
| --- | --- |
| `git diff 6d10d959a689a4b28749b2e6d378a6cf58cd8f6d HEAD -- system ports apps platform` | 退出 0，空差异，本轮被审变更未改这些设备代码 |
| `python3 docs/audits/2026-11-04-codex-hub-design-r2/experiments/discovery_probe.py` | 最终运行退出 0；生产序列化函数和写入器，53 项/780 字节/768 容量返回 -1；临时 C 与二进制在本目录创建后清理 |
| `python3 docs/audits/2026-11-04-codex-hub-design-r2/experiments/state_checks.py` | 最终运行退出 0；八个规则/算术反例，覆盖配对过期、安全日志丢失、重复恢复、清单水位、确认事件、成员代次、请求期限及缓冲占用 |
| 产出检查 | 两个 JSON 解析、编号/定级/必需字段、报告交叉引用、大小和改动范围；检查方法与结果见实验记录 |

命令、输出摘要、准备阶段失败和限制见 [experiments/README.md](experiments/README.md)。本轮没有跑全套产品构建/回归：被审修改是设计文档，也没有中枢实现；全套设备测试不能证明新增协议正确。没有编写中枢实现。

### 未查与操作边界

未使用硬件、未接触设备或硬件私有目录；未验证真实音频/JPEG、真实厂商请求、网络吞吐、时延分位数、运行中容器隔离、证书签发、SQLite/restic 实际恢复和部署。没有注册、购买、付费调用、提交、推送、建分支或更改既有产品文件。本次由根代理独立完成，没有调用子代理或外部模型。

最终新增文件仅在本审查目录中，是**本地审查产出**，未同步远端。证据不含真实设备标识、凭据值或网络配置。机器可读结论见 [verification.json](verification.json) 和 [findings.json](findings.json)。
