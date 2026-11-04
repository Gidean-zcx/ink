# 第十四轮系统服务独立审计

审计日期：2026-11-04。对象：`a81c1b8ed805c044bcc24a120c14bb8e3a4d011e`；范围：`1fbf510..a81c1b8`。按 [TASK.md](TASK.md) 独立完成，只审计第 1 阶段新增或受改动影响的代码。

## 一页结论

**发现 5 个高级别、6 个中级别问题；没有本次证据足以定为严重的问题。尚不满足任务提出的“没有严重和高才继续”的条件。** 高级别问题涉及持久化失实、删除范围越界、中文读取截断、闹钟重启恢复，以及一项被明确承诺但实际缺失的测试保护。

【复现】普通 Werror 构建和 ASan/UBSan 构建都成功，CTest 各 **72/72**，分别 26.76 秒、76.24 秒。每套断电测试执行 240 个指定点，其中 233 次实际撕裂 Flash 操作，恢复检查全部通过。补充实验仍复现下面的缺陷；通过既有测试不能作为这些路径正确的证据。

| 范围 | 总体判断 | 问题 |
| --- | --- | --- |
| data / storage | 日志与快照的常规回放、压缩和断电测试有效；加载失败语义不安全 | 001 高、004 中 |
| file / USB | 路径权限和 USB 顺序流程通过；家庭根删除与中文分页不正确 | 002 高、003 高 |
| time / lunar / power | 已有时区、农历、迟到、重启、RTC 线故障测试通过；跨时区持久化和锁屏后台低电量有缺口 | 008 高、011 中 |
| notify / interrupt / audio | 事件驱动和不依赖 UI 的打断路径成立；成员与状态应用缺少事务边界 | 005、006 中 |
| confirm / cap | 来源检查、确认队列、撤销基础流程及分页已测；确认后的排队调用没有成员绑定 | 007 中；AI 读回执另见 003、004 |
| UI / app registry / shell | 生命周期和浮层基础流程通过；应用状态保存失败被吞掉 | 010 中 |
| svc / hub | 共用定时器、懒启动执行器、上线回调容量和事件转发已核查；未发现独立高/严重问题 | 见资源与架构节的边界 |
| 测试 / 文档 | 抽查的 4 条 §15 反向对照有效；缺 Schema 仍通过，与 §1.3 保证冲突 | 009 高 |

【复现】高级别问题均有补充实验；005～007 的竞争由信号量安排特定交错，不声称常见自然发生。010 需要存储队列满，011 需要锁屏持续后台 hold，因此均按中级记录。009 的高定级依据任务明确列出的“测试与文档给出虚假保证”，不是声称当前产品的 `data.get` 已经没有 Schema。

复现程序、边界安排和运行命令见 [experiments/README.md](experiments/README.md)，概要见 [experiments/results.json](experiments/results.json)；机器可读清单为 [findings.json](findings.json)、[verification.json](verification.json)。

## 新问题

以下输出中的 `status=0` 是 `INK_RC_DONE`；文件 `-3` 是 `INK_FS_ERR_NOT_FOUND`。实验的通过计数表示成功复现缺陷，不表示已修复。

### AUD15-001【高】加载失败被当成文件不存在，后续写入虚报持久化并在重载后丢失

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/data_probe.c；data_probe.c：一次日志加载返回 -1；open_success=1 count=0；新写入 durable_ver=1；重新加载 value=old，预期 new。
- 位置：【代码核查】`system/services/data/datastore.c:586`、`system/services/data/datastore.c:622`、`system/services/storage/storage_service.c:597`。
- 影响：ink_storage_load 对不存在、内存不足、读取失败都返回 -1；load 忽略失败并开放空/不完整集合继续写。版本和 ID 可从 1 重用，新帧被下次回放作为旧版本跳过。【推断】已有快照加载失败时还可能在压缩后覆盖旧数据；本实验复现的是新写入已报 durable 后重载仍读到旧值，未单独复现快照覆盖。
- 建议：区分 NOT_FOUND、NOMEM 和 IO；存在的快照/日志未完整加载时拒绝开放可写集合，保留磁盘内容。

### AUD15-002【高】删除家庭根目录会递归删除未对该路径开放的成员私有区

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/file_probe.c --template sim-files；file_probe.c：private_read=0、hidden_in_list=1；删除 /family 后 job_state=2（DONE），private_read=-3（NOT_FOUND）。
- 位置：【代码核查】`system/services/file/file_service.c:149`、`system/services/file/file_service.c:643`、`system/services/file/file_service.c:748`。
- 影响：/family 映射 /content；列表和拷贝隐藏 .m，但递归删除只排除 . 和 ..，因此删除 /family 同时销毁 /content/.m 下全体成员文件，也进入 .update。成人确认的家庭目录操作超出了界面可见范围。触发条件是对虚拟家庭根目录发起删除。
- 建议：将虚拟根目录设为不可移动/不可删除，或为根目录操作定义限定公开条目的遍历；在递归层维护与虚拟路径相同的范围规则。

### AUD15-003【高】UTF-8 分页边界被误判为文件末尾，中文文件只读出第一页

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/file_probe.c --template sim-files；file_probe.c：3000 字节中文文本，file.read status=0 next_offset=2046 end=1。
- 位置：【代码核查】`system/services/file/file_caps.c:138`、`system/services/file/file_caps.c:153`。
- 影响：默认读取 2048 字节时，三字节汉字只截得 2046 个完整字节；代码因 good < max 返回 end:true。AI 按回执停止分页会漏掉剩余正文，是常见中文文本路径。
- 建议：依据原始读取量或文件大小判断 EOF；完整字符边界仅用于 next_offset。补充多字节跨页连续读取验证。

### AUD15-004【中】合法大记录被截断后直接拼入 JSON，成功回执不是有效 JSON

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/data_probe.c；data_probe.c：3000 字节合法 JSON 记录，data.get status=0 result_bytes=2109 valid_json=0（cJSON_Parse）。
- 位置：【代码核查】`system/services/data/data_caps.c:139`、`system/services/data/data_caps.c:173`。
- 影响：存储支持 8192 字节记录，data.get/data.list 只取 2047 字节后直接写入 value；超过缓冲的合法 JSON 被截断，仍报告 DONE。影响声明 AI 可读的大记录，现有测试只覆盖小记录。
- 建议：按记录长度分配并检查输出上限；放不下时明确返回可分页/过大错误，不能把 JSON 片段当完整值返回。

### AUD15-005【中】成员检查与收件箱取引用不在同一事务，通知进入另一成员私有区

- 证据：【复现】见 experiments/README.md 的并发复现命令。concurrency_probe：notify.post result=0 recipient=owner owner_has=0 kid_has=1；只在第一次解锁后让第二任务完成真实 ink_member_switch。
- 位置：【代码核查】`system/services/notify/notify_service.c:245`、`system/services/notify/notify_service.c:272`、`system/services/notify/notify_service.c:65`。
- 影响：ink_notify_post 先确认收件人为当前成员，解锁后 inbox() 又取当前 NS.member。两者之间完成成员切换时，指定 owner 的通知被写入 kid 收件箱。可能泄露原成员的通知正文；本次为锁边界确定性交错，未声称自然高频发生。
- 建议：在首次锁区固定接收者和数据库引用，后续写入不再重新选择成员；未读统计和事件也绑定对应成员。

### AUD15-006【中】并发结束与启动打断会留下响铃和错误的活动状态

- 证据：【复现】见 experiments/README.md 的并发复现命令。concurrency_probe：interrupt id=1 active=0 actual_ring=1 published=alarm；第二任务在原任务真正调用 ink_audio_ring(true) 前结束该打断，原任务随后继续。
- 位置：【代码核查】`system/services/notify/interrupt.c:68`、`system/services/notify/interrupt.c:83`、`system/services/notify/interrupt.c:99`。
- 影响：apply_ring 在调用音频服务前更新 IT.ring_on。另一任务结束打断并关闭音频后，旧调用恢复并重新打开音频；重查只比较已提前写入的期望值，因相等直接返回。打断表已空但声音不停，旧 apply 还把 interrupt.active 写回 alarm；需要特定任务交错。
- 建议：为声音、电源和发布状态建立串行应用机制或带版本的收敛协议，区分期望值和实际完成值；在无 UI 依赖的执行器上完成。

### AUD15-007【中】确认执行未绑定原成员，排队期间切换成员会操作新成员文件

- 证据：【复现】见 experiments/README.md 的并发复现命令。concurrency_probe：confirmed owner deletion owner_stat=0 kid_stat=-3；占住 BG 后主人接受确认，再切换 kid 并放行，主人文件保留而孩子文件删除。
- 位置：【代码核查】`system/policy/confirm.c:309`、`system/policy/confirm.c:311`、`system/policy/cap.c:418`、`system/policy/cap.c:469`、`system/services/file/file_service.c:147`。
- 影响：确认时检查成员，但随后排入 BG 的 pending 调用不携带成员。执行时重新按当前成员解释 /me；成员在确认后、服务执行前切换，确认的删除落在新成员同名文件。特定执行器延迟和切换交错下发生跨成员破坏；现有换成员测试只覆盖仍在等待确认的卡片。
- 建议：让调用和确认令牌携带不可变的成员及会话代次，在 provider 开始前验证，路径解析使用绑定身份；切换时使尚未开始的旧成员调用失效。

### AUD15-008【高】改时区后的闹钟计划只更新内存，重启后按旧时区对应的 UTC 时间响

- 证据：【复现】同一个 build-audit/audit-probes/time-zone.img 连续运行 time_probe 的 write 和 read 两阶段：before reboot zone=UTC next_local_hour=9 next_s=1793610000；after reboot zone=UTC rule_hour=9 next_local_hour=1 next_s=1793581200。
- 位置：【代码核查】`system/services/time/time_service.c:436`、`system/services/time/time_service.c:601`、`system/services/time/time_service.c:662`。
- 影响：clock_moved 重算 next_s 却没有 save_alarm；启动又信任持久记录中非零的 next_s。先在上海时区设每天 09:00，再改 UTC 并等写完，内存显示 09:00；下一次启动同为 UTC、规则仍 09:00，next_s 却是 01:00。正常改时区后重启即可提前或错过闹钟，无需并发或断电注入。
- 建议：持久保存重算后的计划，明确规则/绝对提醒/错过补报的恢复契约；时区设置与计划更改应可一致恢复，或保存调度所用时区代次并在启动时识别过期计划。

### AUD15-009【高】AI 能力测试缺失 Schema 不失败，文档给出了未被实现的回归保证

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/reverse_controls.py；missing-input-schema 仅在 build-audit 副本中删除 data.get 的 input 行，原测试和预算不变，模拟器与包装脚本均退出 0、test.fail=0、test.finished=1。
- 位置：【代码核查】`tests/integration/ai_surface.c:128`、`tests/integration/ai_surface.c:131`、`docs/system/system-services-design.md:67`。
- 影响：测试仅在 c->input 非空时检查是否含 type:object，没有核查应有参数的能力缺 Schema，也不验证实际工具清单中的 Schema。删除 data.get 的完整 input 声明后，原 integration-ai-surface 仍 test.fail=0、test.finished=1；实际清单会退回默认空对象 Schema，丢失 collection/id 的必需参数描述（cap.c:853）。与文档“缺 Schema 都不通过”的明确保证不符，模型将无法获得该工具必需参数的约束。按任务将虚假测试保证定为高；不是声称当前未变异版 data.get 已缺 Schema。
- 建议：将能力的必需输入/输出契约列为可核对数据，解析真实 tools/list 的完整对象，验证必需字段与 Schema；对有参数的能力移除 Schema 的反向用例必须失败，同时收窄文档对当前检查能力的表述。

### AUD15-010【中】状态保存入队失败仍停止应用并返回成功，重新打开恢复旧状态

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/app_probe.c --template sim-apps；真实 ink_storage_hold_writes + 8 个不同路径写入占满队列，输出 close_result=0 queue_before=8 restored=old-state expected=new-state。
- 位置：【代码核查】`system/ui/app_registry.c:85`、`system/ui/app_registry.c:94`、`system/ui/app_registry.c:120`、`system/ui/app_registry.c:306`。
- 影响：save 忽略 ink_storage_write_atomic 的失败结果，stop 仍释放应用内存，ink_app_close 返回 0。真实存储队列 8 槽占满时，新状态未入队；排空并重开应用得到 old-state。需要队列满或内存不足；当前随附阅读器不使用这套 save_state，故按有限范围的失败路径定中。框架承诺不能扩展为保存已成功。
- 建议：让保存结果参与生命周期转换：入队失败保留脏状态、重试或明确报告失败；不能在唯一的新状态被释放后只保留成功回执。异步写失败也要有可观察的处置。

### AUD15-011【中】锁屏且后台保持唤醒时无限等待，低电量监测停止

- 证据：【复现】python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py docs/audits/2026-11-04-codex-services/experiments/power_probe.c --template sim-notify --end 90s；locked battery before=80 hardware=1 after75s=80 shutdowns=0 state=locked。
- 位置：【代码核查】`system/services/power/power_service.c:158`、`system/services/power/power_service.c:167`、`system/services/power/power_service.c:239`。
- 影响：locked_step 发现 AUDIO/NET/AWAKE hold 后永久等待 PS.kick；这条分支既不进入睡眠片，也不到亮屏的每分钟电量轮询。保持 hold 时电量变化不唤醒该等待，低电量通知、保存和关机均不执行。实测锁屏后台 hold，硬件电量 1%，75 秒后发布电量仍 80%、关机计数 0。限定于持续后台 hold；会直接影响第二阶段锁屏媒体等合法用法，不是要求本轮实现媒体功能。
- 建议：在保持 CPU 唤醒的锁屏分支也按电量截止时间有界等待并采样；与低电量关机等待合并，不为无后台工作的正常深睡额外增加轮询唤醒。

## 架构与 AI 原生审视（R-055）

### 模块边界与原子能力

【代码核查】【文档核查】整体划分符合设计的主方向：data 经 storage 串行访问内部 Flash；file 管虚拟路径、TF 作业和 USB；time 负责时刻、规则和唤醒，发出事件；notify 订阅通信、时间、电源事件后组合通知和打断；confirm 位于策略层；系统壳用 presenter 展示浮层。`notify_service.c:633` 起的初始化订阅、`time_service.c:302` 的调度、`system/ui/shell/confirm_card.c:31` 与 `interrupt_card.c:35` 的呈现路径可直接对应这些职责。

【代码核查】未把每一个直接 C 调用都视为绕过软总线。架构 §4.6 明确允许类型化快捷接口；data→storage、time→power/RTC、interrupt→audio/power 属于下层机制依赖。服务未直接画新增确认卡或打断卡；按钮调用确认/打断服务，后者再调用原能力。`app_registry.c` 以 UI 任务串行执行生命周期，服务存储仍交给 storage；这一任务归属本身不是“把所有机制画进页面”。

【复现】【代码核查】`layering` 和 `sdk-boundary` 通过。`tests/layering.cmake` 是服务目录的包含项/调用名静态扫描，显式放行 AI 服务的旧面板和 executor 的 `ink_ui_kick`。它能约束指定依赖，不能证明所有间接依赖、跨任务状态和事务正确，不能据此否定 005～007。

本轮应优先闭合的边界是：文件服务的虚拟命名空间必须贯穿递归删除（002）；确认、调用和路径解析必须使用一致身份（005、007）；“表里已改”与音频/电源“实际完成”要有收敛协议（006）；状态保存要进入生命周期的成功/失败契约（010）。这些都是现有一期职责，不需要等第二阶段 UI 或账户功能完成。

### AI 能否调用、读取、观察

【复现】用原 `integration-ai-surface` 的真实中枢请求路径额外捕获并解析 `tools/list`，得到 **3 页、83 个条目：65 个 call、18 个 event**，各页 JSON 有效、名字无重复。原测试和预算在捕获运行中仍通过；结构化能力定义清单见 [surface_inventory.json](experiments/surface_inventory.json)。这里是模拟中枢上的主页/成人上下文，不是实网模型验证。

【复现】【文档核查】与 §1.3 的数据、文件、时间、通知、确认、应用、系统七行逐项比较，唯一缺少的是 `system.reboot`，它列于 §13 的后续范围，按任务不算缺陷。`confirm.accept/reject` 未暴露；内部日志/压缩、RTC、锁、音频焦点等机制无需成为 AI 工具。运行时增加的 `interrupt.list/ack` 是读取/确认已有系统打断，未开放伪造来电或闹钟的 raise；不属于越权扩展。通信、阅读、相机、远程页面、成员切换等其他条目来自已有能力；reader 翻页等专属能力按当前页面过滤。

【复现】【代码核查】读结果确已走 `ink_cap_result` 和回执，但“返回 DONE”不等于数据完整或 JSON 有效：003 的错误 EOF、004 的截断 JSON 会直接误导 AI。009 说明现有清单测试不能保证契约完整，应解析实际 Schema 和必要参数，不能只查字符串名字。

【代码核查】【复现】事件按登记和 AI 可见性保留最近 8 条，经 `hub_link` 的 tick 发送 `device/event`；断网、恢复、状态快照已有集成测试。大表和事件缓冲放 PSRAM，没有为转发新建专用任务。`send_events` 分配失败/队列满留待后续，超过 1024 字节编码上限会计数并丢弃；`ink_cap_emit` 每个订阅者仍分配一份参数，即使 DIRECT 订阅也可能在内存不足时丢事件（`cap.c:719`、`cap.c:756`）。本轮没有做全局分配失败穷举，未将此实现限制误写成“所有事件可靠送达”。

【代码核查】【复现】`ui_task` 统一浮层路由、快照和内容代次；确认卡/打断卡提供文字快照；既有 notify/confirm 测试覆盖浮层出现时按着手指、按键截获、锁屏和打断让路。AI 请求和按钮最终进入同一 provider；本轮没有发现必须另建一套 AI 业务实现的情形。

### 来源、撤销与第二阶段接口

【代码核查】常规 data/time/notify 写入携带 `src`，新增对象等可逆操作能登记短时撤销；`system.undo` 查对象、期限与当前成员后执行 inverse。确认保留待确认来源供卡片读取，但真正执行时改成 `INK_SRC_CONFIRMED`（`confirm.c:311`），当前统计也计到 user（`cap.c:415`）。因此审计应区分“AI 发起、本人批准、系统执行”，不能把 confirmed 当成完整的原始来源链；建议随 007 一并明确不可变的调用身份、原始来源与批准凭证。

【代码核查】【推断】第二阶段的 PIN、多成员和后台媒体接入前，优先修复 007、010、011。应用权限目前按“当前前台应用”查询（`cap.c:113`），应用 `needs` 只查本机 `ink_cap_find`（`app_registry.c:282`），与架构 §4.6 的远端可用能力集合存在扩展边界；建议后续将调用者应用身份和远端可用性纳入接口契约。事件信封目前也只有 event/at_s/args，真实中枢接入时需明确成员归属。这些是代码可见的接口演进约束，本次未实现相关新功能、未运行真实多会话中枢，不将推断的跨云会话泄露当作已复现问题。

## 既有测试、反向对照、文档与预算

### 反向对照

【复现】所有变异只生成在 `build-audit/` 的副本，原集成测试断言和预算文件保持不变。所有测试均完成，失败不是编译失败：

| 抽查项 | 原测试 | 变异后 test.fail | 判断 |
| --- | --- | --- | --- |
| 快照帧也按递增版本筛选 | integration-data | 1 | §15 主张成立：折叠后记录丢失被检测 |
| 去掉儿童写家庭目录保护 | integration-files | 20 | §15 主张成立：首批权限断言失败；后续含连锁失败 |
| 去掉确认真人来源检查 | integration-confirm | 2 | §15 主张成立：C 接口绕过被检测 |
| 从 AI 清单隐藏 notify.post | integration-ai-surface | 1 | §15 主张成立：真实工具响应缺项被检测 |
| 删除 data.get 输入 Schema | integration-ai-surface | 0 | §1.3 保证不成立，AUD15-009 |

前四项模拟器和包装脚本退出码都是 1，最后一项都是 0；全部 `test.finished=1`。详细变异及摘录见 [verification.json](verification.json)。没有把这 4 条抽查推广为 §15 所有历史反向对照均已重新做过。

### 资源与栈

【复现】以下为普通构建的模拟器指标；`stack_used_est_bytes` 是主机估算，**不是 S31 实机栈测量**。

| 任务 | 检查到的峰值估算 / 配置字节 | 来源 |
| --- | --- | --- |
| esp_timer | 2528 / 3584 | notify、time-main |
| power | 2048 / 3072 | notify |
| storage | 3360 / 3584 | data（含存储完成回调） |
| input | 1392 / 3072 | notify、confirm |
| exec.bg | 6328 / 8192 | confirm；栈在 PSRAM |

【复现】【文档核查】`scenario-ai_ask-metalio-plus-pqbn` 的 `mem.internal_total.min_free_bytes=33192`，预算底线 32768，余量 **424 字节**，与设计 §15.1 相符。普通 AI 场景为 35856。预算目标“新增 ≤300 字节”与文档承认的实际新增 312 字节差 12；本次复现最终值，没有另构建旧提交独立重测 33504 的历史基线。新集成测试预算主要约束完成/失败/panic/warnings，不能替代上述特定 AI 场景的 RAM 预算。

【代码核查】新增小服务使用共用 svc 锁与 timer，data 另有自己的锁，因此准确表述是“两把锁、一个 timer”；§15.1 后一句“只多一把共用锁”略写了 data 锁。干净启动没有新服务专用任务，PQ-BN AI 场景指标里也没有 exec.bg；它在首次 BG 作业时创建（`exec.c:63`），栈使用 PSRAM。第一只闹钟设置 RTC、数据库压缩、文件能力、通知写入另一成员等均可触发；已有闹钟恢复时也可在启动流程触发，不能把“干净启动没有”推广到所有启动状态。

### 文档的一致性边界

【文档核查】【代码核查】`ui-dependencies.md` 明示现状基于 `dcba9b6`，本次按历史依赖规划阅读，不将其中旧的“尚未实现”误报为当前缺项。设计 §14 把 USB、RTC、FAT 改名和实机 RAM 等标成待实机验证，这一边界合理，本审计没有改动这些判断。

除 009 外，存在不影响本轮定级的小型文档漂移：设计 §3.4 描述坏帧处停止，代码 `datastore.c:553` 会逐字节寻找后续合法帧；§3/§4 伪接口的参数和 USB exclusive/MSC 命名与实际头文件不同；§15.1 的锁数简写不完整。建议下一次回填以真实头文件与恢复语义为准，本轮不因这些小项增加问题数量。

## 审计命令、产出与未覆盖边界

环境：Darwin arm64，CMake 3.28.3，任务提供的本地字体、LVGL、Python 依赖。以下两套 configure/build/ctest 均退出 0：

```bash
D=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit -j 8
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_GUI=OFF -DINK_SANITIZE=ON \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit-asan -j 8
$D/build-deps/pyenv/bin/ctest --test-dir build-audit-asan -j 8 --timeout 300 --output-on-failure
```

补充命令与预期输出完整列于 [experiments/README.md](experiments/README.md)。审计额外运行 6 类探针（time 分两次启动）、5 个反向变异和一次成功的实际清单捕获；源码包装/实验断言只存在于审计产出或允许的构建目录。最终检查 JSON、路径行号、相对链接、单文件大小与 Git 状态。运行器自动生成的超过 3 MB 的 `codex-run.log` 完整移到 `build-audit/audit-runtime-codex-run.log`，未删减，不作为交付文档。

证据边界：

- 未调用任何子代理、外部模型或在线服务；未联网查询 Issue/PR/交互决策板，按本次离线只读任务固定在指定提交，不同步决策记录。
- 未接触设备或 `ink-hw` 数据；未验证 S31 编译、真实 USB/FAT/RTC、物理断电、电池采样、实际功耗、硬件栈或长时间运行。模拟器表现不能升级为实机承诺。
- 未重审 `1fbf510` 之前全部代码；只为解释新服务接入而追到相关旧接口。文本输入、媒体/音频焦点、Wi-Fi 管理、PIN、OTA 和 §9～§13 未完成部分未作为缺陷。
- 未跑 Linux CI、GUI 人工视觉验收、TSan 或所有多核交错；ASan/UBSan 不等于数据竞争检测。并发结论只覆盖实验给出的确定性时序。
- 未穷举全部分配失败、存储错误、64 闹钟/16 应用满表组合、所有 USB 与原子写并发；未验证真实中枢的成员事件路由。报告没有把未覆盖部分写成已通过。
- 不修改产品代码、原测试或既有文档，不提交、不推送、不建分支。交付文档只在本审计目录；构建和临时日志留在任务允许的位置。
