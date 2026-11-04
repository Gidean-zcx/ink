# 第十一轮软件质量审计

审计日期：2026-11-04。审计对象：`c95f48c5bc398178b512c3babc19e475217fbe18`；上一轮对象：`ebf05c8fad460b4231fe4be2cb58c44d45e9f8a4`。

本报告按核查进展持续更新，现已完成 TASK.md 指定范围的本轮审计。审计独立、离线进行；未调用子代理或外部模型，未接触设备与私有实机数据，未修改产品源码、现有测试或其他文档，未提交或推送。

## 1. 一页结论

**不建议合入 main。** 本轮范围内未发现严重问题；新增 **7 条问题：高 3、中 4、低 0**。高级问题都有独立运行证据，现有全量测试通过不能覆盖这些路径：

- **AUD12-001**：自动选网没有实现，当前网络失去覆盖后，即使另一条网络可用也持续离线。
- **AUD12-002**：合法的 109 字混合短信被截短，模拟模组实际保存的发送内容与请求不同，仍回执成功。
- **AUD12-004**：旧回答收尾与下一次 AI 按键交错时，旧轮误关新会话，麦克风继续采集却不能上传。

上一轮要求核实的 **11 项中，10 项已修复、1 项部分修复**（AUD11-005：原会话泄漏消除，但关闭路径新增 AUD12-004）。这里的“已修复”针对原问题和当前模拟器/便携源码范围，不代表设备端验收。

【复现】普通构建 **52/52**、ASan/UBSan 构建 **51/51** 测试通过；其中 `systest` 144 项、`lifecycle` 92 项检查通过。补充实验有 7 个运行模式：6 个问题实验支持上述 7 条发现，另 1 个 UI 拥塞实验通过；全部在普通与 ASan/UBSan 构建运行，后者未报内存/未定义行为错误。并发和失败实验采用原样包含产品源码、精确安排任务交错或注入特定返回值的方式，**不将实验安排的发生率当作真实故障发生率**。

执行节点归属、回执槽位归还、排队超时、去重和 UI 唤醒总体比上一轮可靠；主要剩余阻断项在服务语义和 AI 换轮。实时会话内存失败恢复、网络模型切换、远程更新单元提交及执行器首次启动另有中级问题。SDK、语音前端与真实中枢仍处于文档已说明的部分实现/模型阶段。资源预算仅证明本机模型的运行堆余量；不能由绿色预算推导静态数据、设备栈或真实 PSRAM 段合格。

建议先处理 3 条高问题并加入针对性回归，再复审合入；中级问题保留明确修复记录。此次只交付审计证据，没有修改实现。

## 2. 修复核实

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD-007/F-6 | 已修复 | 【复现】ai_page_version 的过期工具调用与重复请求检查通过；integration-lifecycle 的两个去重锁边界通过。【代码核查】apps/reader/reader.c:258；system/ui/ui_task.c:98、363；system/policy/cap.c:113、217、260。 | 内容版本与输入消息页面代次分离，翻页递增内容版本、回执带新版本；请求号查重/占槽及完成/缓存原子完成。保留当前协议约定的有界去重窗口和可选 page_gen 语义。 |
| AUD11-001 | 已修复 | 【复现】普通 CTest 52/52；integration-lifecycle 拥塞检查通过；补充 UI 实验 wake_queue=24、80 个节点 FIFO 执行且运行中 busy=1。【代码核查】exec.c:96-110、cap.c:449-463、settings.c:210-226、watch.c:27-51。 | 原队列容量导致的回执、apply、通知丢失已消除；节点归工作对象所有，批量积压与合并通过。首次启动存在另一条较少见的调度活锁，单列 AUD12-007；不将它混作旧容量问题仍未修复。 |
| AUD11-002 | 已修复 | 【复现】integration-lifecycle 的 deadline_checks 通过。【代码核查】cap.c:329-400、407-446；comm_service.c:173-190。 | 截止时间覆盖排队和阻塞：未开始的过期不再运行，已开始的先回超时、迟到结果丢弃。 |
| AUD11-003 | 已修复 | 【复现】integration-lifecycle 的 sim_checks：预存 slot=1 开机应用、外部改成 slot=0 后再次设置同值纠正，普通与 ASan 均通过。【代码核查】system/services/comm/comm_service.c:117、131、139、262；system/policy/settings.c:210。 | 启动延迟查询和失败退避重试已有实现；同值设置仍提交 apply。有限次失败后放弃并计数，后续设置可重新启动。 |
| AUD11-004 | 已修复 | 【复现】integration-lifecycle 的 capture_checks，在独立采集和监听两种模式下均检查结束中的重开被拒绝、最终回调一次、下一次采集正常。【代码核查】system/services/audio/audio_service.c:219、233、625、683。 | 编码器与回调先取走、清理、通知，最后才清 cap_on；mic_lock 保护麦克风归属。没有把本轮 AI 的换轮关闭竞态混作此采集资源问题。 |
| AUD11-005 | 部分修复 | 【复现】ai_turn_end、ai_dismiss、ai_dismiss_held、ai_mic_unavailable 均通过；新增 ai_race_probe 在真实回复与 UI 新按键交错时得到 current_session=0 capturing=1 up_open=1 rt_count=0。【代码核查】system/services/ai/ai_service.c:147、310、326。 | 原正常结束不关会话的泄漏已消除；新关闭路径按解锁后的当前 sid 操作，可误关下一轮，见高问题 AUD12-004，尚不能整体验收。 |
| AUD11-006 | 已修复 | 【复现】integration-lifecycle 的 dedupe_checks 两个锁边界重发检查通过。【代码核查】cap.c:217-242、260-268。 | 查重与占槽、最终回执与去重环写入分别在同一次持锁完成。 |
| AUD11-007 | 已修复 | 【复现】remote_reconnect、integration-lifecycle 两次断线、恢复配额和恢复后的点按通过；lifecycle 比较图像（JPEG 替身区域除外）。【代码核查】system/apps/remote/remote.c:104、281、300；system/services/hub/rt.c:102、118。 | 重连回调清后画布、递增代次、重新发配额并全屏重绘；配额发送失败保留 owed 重试。JPEG 区域不在逐像素一致检查内，不能称全画面逐像素验证。 |
| AUD11-008 | 已修复 | 【复现】integration-lifecycle 的 stale_region_check 在解码中换代，旧区域不写画布、不投递 UI；立即关闭重开及最终资源释放通过。【代码核查】system/apps/remote/remote.c:137、185、217、328、343。 | back 与 px 分离，解码提交前重新确认代次，UI 任务才写 px；延迟释放也检查 active。对原跨会话写入/读写冲突问题成立，更新单元的提交边界另查。 |
| AUD11-009 | 已修复 | 【复现】integration-lifecycle 的 early_checks 通过。【代码核查】cap.c:247-255、360-400、489-507。 | finished、answered、refs 三个条件共同决定回收；run 返回前或 invoke 等待期间不复用槽位。 |
| AUD11-010 | 已修复 | 【代码核查】system/services/comm/comm_service.c:40-52：rx、rx_args、ev_args 均已移入 CM INK_PSRAM_BSS；platform/include/ink/osal.h:91-96 目前只提供可覆盖的空宏，注释指定未来设备端使用 EXT_RAM_BSS_ATTR。 | 源码层漏标记的 1392 B 已消除；当前仓库尚无目标端宏映射和链接段验收，模拟器仍不计静态区。结论限于本次模拟器/便携源码修复，不能称已验证设备上的 PSRAM 放置。 |

## 3. 新问题

### AUD12-001（高）net.prefer=auto 没有应用动作，当前链路消失后不会使用可用的另一条网络

【复现】【代码核查】【文档核查】位置：`system/services/net/net_service.c:24`、`system/services/net/net_service.c:29`、`system/services/net/net_service.c:35`、`ports/sim/hal/hal_net_sim.c:215`、`ports/sim/hal/hal_net_sim.c:238`、`docs/system/system-detailed-design.md:308`。

影响：先选 LTE 再改回自动，设置值为 auto，实际仍固定在 LTE；蜂窝覆盖消失而 Wi-Fi 可用时 net.online/hub 均离线，手动选 Wi-Fi 才恢复。默认 auto 也没有网络变化时的选网逻辑。已对外登记的正常设置不能实现其含义。

证据：experiments/service_probe.c 的 net 模式，配 services.script：auto_cell_down_wifi_available prefer=0 type=2 up=0 hub=0；manual_wifi prefer=1 type=1 up=1 hub=1。详见 experiments/README.md。

建议：明确并实现自动选网策略及 HAL 的可用网络输入；切回 auto 立即重算，网络变化后也重算。加入双网络可用、当前网络消失、手动切回自动的测试；若暂不支持，先移除 auto 选项并准确说明。

### AUD12-002（高）合法的长中文短信被截断后仍回执发送成功

【复现】【代码核查】位置：`system/services/comm/comm_service.c:173`、`system/services/comm/comm_service.c:229`、`system/util/ink_args.c:104`、`system/util/ink_args.c:180`。

影响：Schema 按字符允许最多 300 字，cap_send 的 320 字节缓冲只容纳 319 字节，并忽略 ink_args_get_str 返回的原长度。106 个“中”加 abc（109 字、321 字节）通过参数检查，实际送到模拟模组的是 106 个“中”加 a，HAL 成功且回执“短信发出去了”；接收内容与请求不同。截在 UTF-8 中间时还会使本应合法的输入发送失败。

证据：AUDIT_CASE=sms service_probe：requested_bytes=321 requested_chars=109 args=0/0 schema=0 accepted=0；actual_bytes=319 ends_abc=0 ends_a=1 hal_return=0；final_receipt=done；modem_sent=1 stored_bytes=319 equals_requested=0。无真实短信或设备访问。

建议：发送前校验完整字节长度，容量不足明确拒绝；或直接使用有界的完整参数字符串并与驱动编码、短信分段能力统一限制。补充中文/混合文本的边界测试，核对实际发出的全文。

### AUD12-003（中）rt.open 同步返回内存不足时 inflight 永不清除，重连也不再登记

【复现】【代码核查】位置：`system/services/hub/rt.c:89`、`system/services/hub/rt.c:106`、`system/services/hub/rt.c:178`、`system/services/hub/hub_link.c:507`。

影响：announce 忽略 ink_hub_call 的同步负返回；该失败不触发 open_reply，留下 used/inflight 和后台作业、hub keep 引用。经过 12 秒并恢复内存后，重连回调仍跳过该会话，既不 ready 也不通知关闭，直至所有者显式关闭。少见的内存压力条件下会话不可用、资源长留。

证据：AUDIT_CASE=rt service_probe：仅对第一次 announce 注入文档化的 INK_HUB_ERR_NOMEM（不调用回调）；sid=1 open=1 inflight=1 calls=1 closed=0；12 秒后调用已注册的 on_online，仍 open=1 ready=0 inflight=1 calls=1 closed=0。此为定点失败注入，不声称随机 OOM 复现。

建议：处理同步失败并走与异步拒绝一致的清理/通知或有界重试路径；定义好 open 返回值与同步 on_closed 的次序，避免调用方尚未保存 sid 时丢失关闭。

### AUD12-004（高）旧轮回复解锁后关闭当前 session，可误关刚由 UI 开启的新一轮语音

【复现】【代码核查】位置：`system/services/ai/ai_service.c:147`、`system/services/ai/ai_service.c:310`、`system/services/ai/ai_service.c:326`、`system/services/ai/ai_service.c:504`。

影响：on_reply 在锁内确认旧轮仍为 current，但解锁后 close_session 再读 AI.session。期间 UI 响应新一次按键、关闭旧会话并开启新 sid 后，旧回复会关闭新 sid。用户仍看到正在听，采集和 up_open 仍为真，却已没有实时会话，不能上传该轮声音。属于 AUD11-005 修复新增的换轮竞态；无需 OOM、坏输入或断网。

证据：ai_race_probe.c 原样 include AI 和启动源码；真正的 hub 回复在 AI.lock 解锁处暂停，让 UI 任务调用 ink_ai_key(true)，再让旧回复继续。输出 old_sid=1 new_sid=2 seq=2 mic_failed=0 rt_count=1；after_old_reply current_session=0 capturing=1 up_open=1 rt_count=0。为定点并发交错，非随机发生率测量。

建议：在确认轮次的同一临界区取出并清除属于该轮的 sid，解锁后只关闭取出的 sid；跨锁异步收尾均携带并再次匹配轮次/会话标识。为回复完成与下一次按键重叠添加回归测试。

### AUD12-005（中）Wi-Fi 切到已注册 LTE 时旧 WebSocket 被错误保留，绕过断线与重新握手

【复现】【代码核查】位置：`ports/sim/hal/hal_net_sim.c:187`、`ports/sim/hal/hal_net_sim.c:194`、`ports/sim/hal/hal_net_sim.c:278`。

影响：hal_net_use 先覆盖 N.type 再 relink，使 set_link 的 old != t 检查恒为假。两条网络都可用时 Wi-Fi→LTE 不让连接失效、不清 DNS/无线状态；旧 TCP/WebSocket 无需重新连接就改用 LTE 时延/带宽。切网场景的连接连续性、恢复时延和资源开销因此失真。范围限于模型的跨网络切换，未据此宣称现有所有时延预算无效。

证据：service_probe 的 net 模式：wifi 与 wifi_to_lte 两次输出均 hub=1 online_count=1。最终普通构建 Trace 在 5.200103 s 切到 LTE，却只有约 2.70 s 的首次 connect；18 s 真正移除覆盖后才于 23 s connection lost。27.73 s 手动切回 Wi-Fi 后才第二次 connect。

建议：保持旧类型直到 set_link 原子完成切换；切换时失效旧连接、清连接相关状态，测试强制切网期间丢失/重连/会话重登记及相应成本。

### AUD12-006（中）完成标记只记录连接代次，UI 会把下一单元尚未完成的区域提前显示

【复现】【代码核查】【文档核查】位置：`system/apps/remote/remote.c:137`、`system/apps/remote/remote.c:148`、`system/apps/remote/remote.c:219`、`system/apps/remote/remote.c:227`、`docs/system/system-detailed-design.md:544`。

影响：有两份初始配额时，完整更新 A 的 LAST 已排队等 UI，媒体任务还可以把更新 B 的首区域写入同一个 back/dirty。show_update 只检查 complete_gen==gen，随后复制所有 dirty，连尚未收到 LAST 的 B 也显示。可出现短暂的半页/混合画面；若两个 LAST 合并到同一 show_job，还只返还一份配额。原 AUD11-008 的跨会话内存安全修复仍成立，本条是同会话更新原子性遗漏。

证据：remote_batch_probe.c：在 UI 忙时，媒体任务先解码 A（x=0 黑，LAST），再解码 B 首块（x=1 黑，无 LAST），随后放行真实 show_update。4×4 画布输出 shown_A=0 shown_partial_B=0 updates=1 owed=1；B 应仍为白色 255。定点合法帧顺序实验，无畸形数据。

建议：将正在解码单元与已完成待显示单元的像素/脏区边界隔离；LAST 提交一个不可变快照或缓冲所有权，后续单元不可改写它，并按完整消费单元累计配额。

### AUD12-007（中）等待启动状态只调用 yield，高优先级并发调用可饿死初始化任务

【复现】【代码核查】位置：`system/services/res/exec.c:62`、`system/services/res/exec.c:74`、`ports/sim/osal/osal_sim.c:285`、`simulator/kernel/vtk/vtk.c:953`。

影响：低优先级任务把 state 从 1 改为 2 后若被抢占，后来的高优先级调用者会在 state==2 时不断 yield；固定优先级调度不会因此运行低优先级初始化者。双核上两个高优先级等待者即可占满两核，使三次 ready/post 都无法返回。首次启动时需特定并发交错，未在常规场景自然触发，按中级登记；其后影响可持续到重启。

证据：exec_start_probe.c 原样 include exec.c，state=2 后唤醒优先级 5/6 的两个调用者，初始化者为优先级 3；三者 core=-1，在原双核 metalio-plus 档运行。200 ms 后 state=2 low_done=0 high_done=0，至 1 s 结束仍未建成；两个等待者持续消耗 CPU。仅注入抢占次序，无 OOM。

建议：用可阻塞并带优先级继承的初始化互斥/完成通知，或在明确的初始化阶段建立执行器，避免高优先级纯让出式自旋等待低优先级任务；不要仅在自旋中增加无界重试。

## 4. 分范围核查记录

| 范围 | 状态 | 证据与结论 |
| --- | --- | --- |
| 执行队列、UI 唤醒与调用生命周期 | 已核查 | 【代码核查】`res/exec.c`、`ui/ui_task.c`、`policy/cap.c`：节点发布/摘链、调用前清 queued、回调可释放节点、FIFO、合并、start/answer 两节点、finished/answered/refs 回收条件及截止时间。【复现】`lifecycle`；另将 UI 唤醒队列塞到 24，投递 80 个任务，全部按序执行，执行中 busy=1，无剩余节点。首次启动活锁见 AUD12-007。 |
| 状态、设置、通知与参数 Schema | 已核查 | 【代码核查】`policy/state.c`、`settings.c`、`watch.c`、`util/ink_args.c`：类型读取、合并通知、限频、同值 apply、持久化提交、typed/JSON 参数容量与 Schema 子集。【复现】`systest`、`lifecycle`。短信的字符/字节限制不一致见 AUD12-002；不将该子集当作完整 JSON Schema 引擎。 |
| 服务接入与资源管理 | 已核查 | 【代码核查】`power/power_service.c`、`net/net_service.c`、`display/light.c`、`camera/camera_service.c`、`comm/comm_service.c`、`res/res.c`：状态与设置接入、持锁/睡眠约束、相机缓存和回收、切卡/退避、短信/通话、配额/准入/压力/作业。【复现】服务、相机、待机、压力、切卡测试通过；选网和短信见高问题。相机只保留最新 PSRAM 照片、连拍排队串行是已声明的阶段限制。 |
| 实时会话、语音与打断 | 已核查 | 【代码核查】`hub/rt.c`、`hub/hub_link.c`、`ai/ai_service.c`、`audio/audio_service.c`：连接队列/五档发送、会话登记与重连、采集结束、下行丢旧轮、面板关闭、打断、引用归还。正常语音/免按键/取消通过；同步 OOM 与跨任务关闭分别见 AUD12-003、004。 |
| 远程画面与解码 | 已核查 | 【代码核查】`system/apps/remote/remote.c`、`util/lz4.c`、`simulator/environment/sim_remote.c`、`hal_jpeg_sim.c`：区域边界、4 位展开、LZ4 输出界限、后画布与显示画布、连接代次、配额与重连。【复现】LZ4 既有往返/坏输入检查、远程场景、换代/重开实验通过；同会话未完成单元提前显示见 AUD12-006。JPEG 是 SJ 替身格式，未验证真实 JPEG 解码。 |
| 应用边界与模拟模型 | 已核查 | 【代码核查】`ink/sys/app.h`、`ui/app.c`、`tests/sdk_boundary.cmake`、`apps/home/home.c`、`apps/reader/reader.c`；网络/AFE HAL、`sim_hub.c`、handsfree 档案。现有应用经框架调用；SDK 只做源码检查，尚无头文件隔离/完整生命周期。网络切换模型见 AUD12-005。AFE 按脚本与估计参数输出识别结果，不能证明 ESP-SR 精度和实机耗电。 |
| 静态内存、栈、文档与预算 | 已核查 | 【复现】【代码核查】【文档核查】主机对象 BSS 清单、`-fstack-usage`、场景预算、详细设计 §13 逐项对照，见下；未将历史阶段的检查数量或已声明未实现功能重新定为高问题。 |

### 4.1 静态数据与栈的账

【复现】`experiments/static_inventory.py` 用 `xcrun nm -nm` / `otool -l` 检查当前 **arm64 主机构建对象**。下列为符号区间字节数，可能含相邻填充；不是 RV32 的 `sizeof` 或目标链接图，也不是模拟堆统计。

| 对象 | 主机符号区间 / B | PSRAM 标记 / 判断 |
| --- | ---: | --- |
| settings `SE` / state `ST` | 4552 / 4544 | 均有 `INK_PSRAM_BSS` |
| comm `CM` / rt `RT` | 5184 / 1328 | 均有标记；原遗漏的三个通信缓冲已并入 CM |
| res `R` / remote `RS` | 1800 / 1000 | 均有标记 |
| reader `LIB` | 4104 | 无标记；16 项书名/路径表，目标静态账应单列 |
| hub `H` / display `DS` / UI `UI` | 2216 / 1672 / 1088 | 无标记；不能遗漏在目标内部静态区预算之外 |
| cap `REG` / `SUBS` | 792 / 640 | 无标记；含指针，目标布局会与主机不同 |

【代码核查】`platform/include/ink/osal.h:91-96` 现在只定义可覆盖的空宏，注释说明目标端应映射到 `EXT_RAM_BSS_ATTR`；仓库还没有对应设备端映射。上表仅确认“应搬往 PSRAM”的源代码意图。按当前目标端尚未建设的范围，不把这个已知前提重复列为新高问题；移植时必须验证宏、配置、链接段和所有未标记数据。**不能把这些主机字节数直接从模型的 96 KiB 堆假设里相减**：档案将其定义为静态区之后的剩余堆，目标真实剩余量尚未测定。

【复现】`experiments/stack_inventory.py` 以现有编译参数另加 `-fstack-usage` 编译原始系统/应用源码，输出对象只放在 `build-audit/audit-stack/`。较大主机栈帧：`ink_json_parse` 2640 B、`dir_reply` 1440 B、阅读排版的 `render_into` / `find_page_start` 各 1376 B、AI `request` 960 B、远程 `show_update` 768 B。完整 ≥512 B 清单见 [results.json](experiments/results.json)。这些是单函数编译估计，不能替代完整嵌套路径或 RV32 的栈分析。

【复现】【代码核查】普通产品场景中，内部栈 `storage` 配置 3584 B，观测最大估计 3168 B（余 416 B）；`power` 为 3072/1680 B；`main` 为 4096/2560 B。UI 10240 B、hub 6144 B、comm 8192 B、media/bg 各 8192 B、audio 26624 B 均声明外部栈。当前已跑场景未出现栈超限；存储栈余量较小，设备构建需继续核对，不能将主机高水位当作安全余量保证。

### 4.2 文档、预算与证据范围

- 【复现】最紧的**普通产品场景**为 `ai_ask-metalio-plus-pqbn`：`mem.internal_total.min_free_bytes=33816`，比 32768 门限多 1048 B；`mem.dma.min_free_bytes=29240`。这是包含可供内部申请使用的保留区的模型总量，不能与常规 internal 单池自由空间混用。内存耗尽自检故意跌至 3136 B，不拿该自检数字当普通场景的最低值。
- 【复现】【文档核查】`remote_browse` 首屏 1879.765 ms；点按整页更新 p50=1472.388 ms、p95/max=1805.823 ms，预算放行上限 1900 ms，同时保留 `target_max=800`。详细设计 §13 已承认未达到原目标，本轮不把“预算通过”写成“800 ms 达标”。生命周期测试的像素比较**跳过 JPEG 替身区域**；历史处理表的“逐像素相同”须按这一实际边界理解。
- 【复现】【文档核查】免按键场景的本机打断指标本轮为 68.686 ms，低于 200 ms；这是脚本行为模型中的单次样本。`metalio-plus-handsfree.json` 采用估计的 AFE CPU/内存/检测延迟、`echo_false_per_min=0`。没有验证残留回声误打断、真实唤醒准确率、全天监听功耗或 ESP-SR 端口。
- 【文档核查】§13 已说明的缺项仍存在：语音旧帧超 300 ms 丢弃与断网作废场景、预录缓冲、AI 缓存/下行逐帧分配、完整应用生命周期/编译隔离、危急时停后台/重启、媒体持久化、部分能力与真实中枢。它们是范围限制，不因本轮 CTest 通过而升级为完成；本轮也未借这些已公开缺项凑高级问题。

## 5. 命令、证据边界与未覆盖项

- 【复现】`git rev-parse HEAD` 确认上述提交；初始工作区仅有未跟踪的本轮审计目录，内含 `TASK.md`。
- 按本轮只读、离线要求，不读取在线决策板或远端 Issue/PR，不同步决策；以检出版本的文档及历史处理记录为准。
- 本机模拟器和主机测试不能代替目标固件链接、设备资源测量或实机验收。

- 【复现】按 TASK.md 的本地依赖配置，`INK_WERROR=ON` 普通构建成功；CTest **52/52** 通过（23.22 s）。日志在 `/tmp/ink-audit11-{config,build,ctest}.log`，不放入交付目录。
- 【复现】ASan/UBSan 构建（`INK_SANITIZE=ON, INK_GUI=OFF`）成功；CTest **51/51** 通过（14.56 s）。补充实验复用了原始产品源码及已构建库，仅在审计文件中观察参数、注入可辨认的失败返回。

可复查的主要命令（从仓库根目录运行）：

```sh
audit_deps=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT="$audit_deps/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$audit_deps/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$audit_deps/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$audit_deps/build/_deps/imgui-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$audit_deps/build/_deps/sdl2-src"
cmake --build build-audit -j8
"$audit_deps/build-deps/pyenv/bin/ctest" --test-dir build-audit -j8 --timeout 300 --output-on-failure
```

内存检查采用同一配置命令，目录换为 `build-audit-asan`，增加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`；构建和 CTest 命令也换用该目录。上述配置、构建、CTest 均 exit 0。没有联网下载依赖。

补充实验的编译/运行命令、输出解释和控制条件见 [experiments/README.md](experiments/README.md)，摘录指标见 [experiments/results.json](experiments/results.json)。所有最终版本的补充模式在普通及 ASan/UBSan 下均正常结束；退出 0 只表示实验运行完成，**问题实验的错误业务状态仍存在**。补充 ASan 运行设置 `ASAN_OPTIONS=detect_leaks=0`，不声称 LSan 泄漏检测或真实 SMP 的随机竞态覆盖。

未做：设备编译/烧写/硬件测量、目标链接 map、真实 JPEG/ESP-SR/网络中枢、远端 CI/Issue/PR、在线决策板、长期随机并发/耗尽组合或模糊测试。本轮按任务离线且不接触设备；新增问题以明确触发路径为证据，不给未覆盖路径作无缺陷保证。

【复现】收尾检查 `git diff --stat` 为空，`git status --short` 仅显示本轮审计目录；产出仅在本目录，构建对象在指定构建目录，日志在 `/tmp`。没有提交、推送或合并操作。
