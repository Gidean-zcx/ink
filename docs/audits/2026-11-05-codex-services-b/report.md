# 第十七轮 B 路审计：声音和文字

审计日期：2026-11-05。状态：已完成本轮限定范围的只读审计。

## 一页结论

对象是 `4d23fc2a62f9ae39cc1e1bb0a1f1d98ce2cbb27f`。新增 **9 条：严重 0、高 4、中 5、低 0**。现有相关测试在常规及 ASan/UBSan 构建下各 **32/32 通过**，但补充实验仍发现录音与关机、焦点与执行器、路由与暂停、文字快照与编辑之间的边界缺陷。现有回归不能作为这些组合行为已正确的保证。

优先处理四条高等级问题：来电后仍继续收音、OOM 后旧重做覆盖新输入、录音中关机丢失已接受的待持久化记录、耳机断开后暂时外放。后三者分别复现了文字被覆盖、同镜像重启后记录不存在、约 990 ms 外放；第一条只证明采集与上传继续，没有把模型结果说成真实通话声道泄漏。

指定的 AUD17-005、AUD16-002/003，以及 AUD15-006 的旧缺陷均已修复；暂停位置、换成员收帧和打断 apply 的三个反向对照都能使相应检查失败。新发现 AUD18-201、205 与旧修复的边界不同，应分别处理。

【复现】【代码核查】PQ-BN AI 场景内部 RAM 最低可用量确为 **33192 字节**，比 32768 字节底线仅高 **424 字节**；该场景未启动 `exec.bg`。这不是组合最坏情况的余量，也不是真机内存或栈验收。正式 UI、真实中枢协议、正式词库、ESP32S31 移植和 TASK.md 列出的未实施项不计问题。

| 编号 | 等级 | 新问题 |
| --- | --- | --- |
| AUD18-201 | 高 | 来电取得焦点后，停止收音依赖拥堵的执行器，录音和听写仍继续采集 |
| AUD18-202 | 高 | 快照分配失败后仍保留旧重做分支，重做会覆盖刚输入的文字 |
| AUD18-203 | 高 | 录音未停时关机，写入仍被音频延后，5 秒后直接断电丢失已接受的数据 |
| AUD18-204 | 高 | 耳机断开先切外放、后异步暂停，已缓冲的媒体会外放 |
| AUD18-205 | 中 | 来电覆盖闹钟后，闹钟继续响但不再持有焦点，录音可以在其下启动 |
| AUD18-206 | 中 | 面板供电失败仍被记为关机图已经显示 |
| AUD18-207 | 中 | 选区的显示位置映射错误：点击偏后一字，长选区重复显示 |
| AUD18-208 | 中 | text.submit 漏查 ai_hidden，AI 仍能提交隐藏输入框 |
| AUD18-209 | 中 | 初始文字按字节直接截断，会留下不完整的 UTF-8 字符 |

## 新问题及复现证据

以下均已复现；探针、日志和完整命令见 [实验说明](experiments/README.md)，机器可读清单见 [findings.json](findings.json)。补充探针以“缺陷出现”为断言目标的退出 0，不表示产品通过验收。

### AUD18-201【高】来电取得焦点后，停止收音依赖拥堵的执行器，录音和听写仍继续采集

【复现】【代码核查】媒体执行队列忙 2 s 时录音从来电前约 1.21 s 延长到 3.24 s。UI 忙时听写在通话焦点取得并等待 600 ms 后又上传了 26 帧。违反打断后停止收音、通话声音不进听写的保证。实验验证采集和上传继续，未模拟或声称验证真实通话声道。

位置：`system/services/audio/audio_service.c:905`；`system/services/audio/media_service.c:569`；`system/services/audio/media_service.c:670`；`system/services/text/voice_input.c:428`；`system/services/text/voice_input.c:449`。

建议：焦点改变时同步标记采集截止代次或样本边界，音频任务按边界拒绝新帧；文件收尾和 UI 更新再异步完成。覆盖 UI 与媒体执行器分别阻塞的用例。

复现：python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/record_call_probe.c --template sim-media --script docs/audits/2026-11-05-codex-services-b/experiments/audio.script；另见 experiments/voice-call.log，复跑上轮 voice_boundary_probe.c。

### AUD18-202【高】快照分配失败后仍保留旧重做分支，重做会覆盖刚输入的文字

【复现】【代码核查】输入 ABC，撤销到 AB；下一次快照分配失败时输入 D 得 ABD，但 can_redo 仍为真。重做回到 ABC，D 被覆盖。新修改后不能重做的约定在内存不足时失效。 常规及 ASan/UBSan 探针结果一致。

位置：`system/services/text/text_service.c:115`；`system/services/text/text_service.c:130`；`system/services/text/text_service.c:352`；`system/services/text/text_service.c:498`。

建议：任何新编辑都必须使旧 redo 失效；明确快照分配失败策略，拒绝编辑或清理、降级历史并通知，不能继续宣称旧历史可用。

复现：python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/text_oom_probe.c --template sim-textinput --extra docs/audits/2026-11-05-codex-services-b/experiments/text_observer.c；日志 experiments/text-oom.log。观察器仅令原样 include 的文字服务下一次分配返回 NULL。

### AUD18-203【高】录音未停时关机，写入仍被音频延后，5 秒后直接断电丢失已接受的数据

【复现】【代码核查】未注入故障：录音中接受一条 INK_DB_LAZY 记录，再正常调用 system.shutdown。请求断电时 recording=1 capture=1 db_idle=0 storage_pending=1 durable=1 latest=2；用同一 Flash 镜像重启，记录返回 -5（不存在）。关机只发 flush 并等待最多 5 s，音频写入 hold 则可延后 30 s；没有先停止采集、结束录音或暂停媒体。低电量路径也走同一写入超时逻辑，且仍显示东西都存好了。后者为代码核查。

位置：`system/services/power/power_service.c:125`；`system/services/power/power_service.c:277`；`system/services/audio/audio_service.c:565`；`system/services/storage/storage_service.c:403`。

建议：建立统一关机收尾流程：停止新采集和编辑、收尾录音、释放音频写入 hold、等待持久化回执，再画关机图；普通关机遇失败应可重试或明确告知。低电量必须有受电量约束的收尾与失败表述。

复现：shutdown_audio_probe.c 两轮运行，共用 build-audit/audit-shutdown.img 和 audit-shutdown-sd；第二轮设置 INK_AUDIT_REOPEN=1。完整命令和输出见 experiments/shutdown-audio.log、shutdown-audio-reboot.log。

### AUD18-204【高】耳机断开先切外放、后异步暂停，已缓冲的媒体会外放

【复现】【代码核查】媒体执行器暂忙时，模拟耳机离线触发真实 BT 服务切回 speaker；音频任务立即继续播放缓冲帧，暂停仍在媒体队列里。复现额外外放 990 ms，media_state=playing。现有 integration-media 在已等到 PAUSED 后才记录外放基线，不能验证断连到暂停之间没有外放，设计文档给出的保证不成立。

位置：`system/services/audio/audio_service.c:645`；`system/services/audio/audio_service.c:679`；`system/services/audio/media_service.c:435`；`system/services/bt/bt_service.c:56`。

建议：切到 speaker 之前同步停止或静音媒体输出并清理在途输出；将暂停位置和通知交给媒体执行器。测试从耳机断开前开始记录外放样本，并覆盖媒体队列忙的情况。

复现：python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/headset_probe.c --template sim-media --script docs/audits/2026-11-05-codex-services-b/experiments/audio.script；输出 experiments/headset.log。

### AUD18-205【中】来电覆盖闹钟后，闹钟继续响但不再持有焦点，录音可以在其下启动

【复现】【代码核查】闹钟响铃持有 ALERT；CALL acquire 将其删除。来电和剩余闹钟均 ringing 时，打断服务不再调用 ink_audio_ring(true)，旧 ring_focus 没有恢复。结束来电后 interrupt=alarm、focus=0，500 ms 内仍播放 495 ms 响铃；audio.record 返回 done 并开始采集。闹钟对低档音频的阻挡失效。

位置：`system/services/audio/audio_service.c:931`；`system/services/audio/audio_service.c:1113`；`system/services/notify/interrupt.c:79`；`system/services/notify/interrupt.c:96`。

建议：让闹钟焦点在 CALL 下暂停并恢复，或让 ring 管理者在焦点丢失和恢复时重新取得有效 token；不能仅用 ring_on 的布尔变化维护焦点生命周期。

复现：python3 docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/audio_focus_probe.c --template sim-media --script docs/audits/2026-11-05-codex-services-b/experiments/audio.script；输出 experiments/audio-focus.log。

### AUD18-206【中】面板供电失败仍被记为关机图已经显示

【复现】【代码核查】给模拟 PMIC 注入持续 I2C 故障，关机时 display.power_failures=2、ui.off_picture_on_panel=1、late=0。off_picture 仅等绘图作业和 busy 清空，没有关联成功刷新的回执；失败也会清空 busy，旧画面仍可能留在墨水屏。现有正常关机测试只看提交数增长及空闲，不能排除此情形。没有声称验证真实面板。

位置：`system/ui/shell/off_picture.c:56`；`system/services/display/display_service.c:115`；`tests/integration/power_menu.c:145`。

建议：让关机图绑定具体帧的成功完成结果；区分已绘制、已提交、已显示和失败，在剩余电量及时间允许时有限重试，失败不得计 on_panel。补面板供电和刷新失败用例。

复现：python3 docs/audits/2026-11-05-codex-services-b/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/power_picture_probe.c --template sim-power-menu --sim-arg=--i2c-faults --sim-arg=1.0:pmic；见 experiments/power-picture.log。

### AUD18-207【中】选区的显示位置映射错误：点击偏后一字，长选区重复显示

【复现】【代码核查】正文 abcdef 选择 bc，标签为 a[bc]def；真实模拟触摸点在 d 上，光标变成 4 而非 3，输入 X 得 abcdXef。compose_line 加两个选区标记，place_of 只减一个。100 字全选时，窗口从第 70 字开始，但格式化起点仍用选区起点，负精度让尾部 30 字又出现在选区之前，标签长达 132 字。两种结果在常规及 ASan/UBSan 构建一致。属于现有编辑行为错误，不是占位 UI 的外观缺口。

位置：`system/ui/textfield.c:51`；`system/ui/textfield.c:119`。

建议：保留显示字符到正文位置的完整映射，统一处理两个选区标记、组合串和滚动窗口；增加选区两侧、组合串前后及长选区的点击用例。

复现：python3 docs/audits/2026-11-05-codex-services-b/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/text_boundary_probe.c --template sim-textinput；见 experiments/text-boundary.log。

### AUD18-208【中】text.submit 漏查 ai_hidden，AI 仍能提交隐藏输入框

【复现】【代码核查】【文档核查】普通文字框设置 ai_hidden=true 后，AI text.insert 正确拒绝，但 text.submit 返回 done，并调用一次 entered 回调。与该类输入框 AI 碰不到的约定不一致。本轮只验证回调被触发，没有把占位回调推断成真实消息发送、付款或空间权限突破。

位置：`system/services/text/text_caps.c:81`；`docs/system/system-services-design.md:588`。

建议：在所有文字操作能力执行处统一核查输入框类型、ai_hidden 和调用来源，至少让 submit 与 insert/select/undo 的限制一致；核查 voice_finish/cancel 的相同边界。

复现：同 text_boundary_probe.c；输出 hidden_submit status=done entered_delta=1，见 experiments/text-boundary.log。

### AUD18-209【中】初始文字按字节直接截断，会留下不完整的 UTF-8 字符

【复现】【代码核查】以 initial=中文、max=4 建立文字会话，结果为 e4 b8 ad e6，最后一个字只剩首字节，字符光标仍算作 2。后续显示、提交或持久化会接收无效 UTF-8。常规追加路径会回退到完整字符边界，初始值路径没有同样保护。 常规及 ASan/UBSan 结果一致；此例不依赖快照 OOM 注入。

位置：`system/services/text/text_service.c:79`；`system/ui/textfield.c:227`。

建议：初始会话和控件缓存复用追加路径的 UTF-8 边界裁剪；对过长初始值明确拒绝或按完整字符截断，添加多字节字和容量交界用例。

复现：text_oom_probe.c 后半段不注入分配失败，直接建立 max=4 的会话；输出 initial_4_bytes hex=e4b8ade6 cursor=2，见 experiments/text-oom.log。

## 修复核查

判定针对旧报告中的具体失败路径，不代表模块整体无缺陷。完整条目见 [verification.json](verification.json)。

### AUD17-005：已修复

【复现】【代码核查】原 record_end_probe.c 在当前提交得到 recording=0 capture=0 focus=0 lost=15 completed=1；它断言旧缺陷仍存在的两条检查失败，runner 退出 1。当前 integration-media 常规和 ASan/UBSan 均通过。media_service.c:538 的 ending 标志不占帧队列位置。见 experiments/record-end-original.log。

边界：核查的是队列满时结束标记丢失；来电及时停止的不同问题见 AUD18-201。本轮未撤回此修复做变异。

### AUD16-002：已修复

【复现】【代码核查】【文档核查】media_service.c:81 无流时使用暂停位置。两种构建 integration-media 通过；仅从构建目录副本删除该分支，原测试失败 (19200, 23280)，test.fail=1、runner 退出 1，与设计 §15.3.1 相符。见 experiments/reverse-media-pause.log。

边界：包括暂停后停止、重新播放的原回归；未修改产品文件。

### AUD16-003：已修复

【复现】【代码核查】【文档核查】media_service.c:545 在音频任务检查成员，:563 在媒体执行器收尾，:491 的结束事件携带成员。原测试媒体队列忙 1 s 的场景两种构建均通过；副本只去掉收帧成员检查，原测试得到 2820 ms，test.fail=1、runner 退出 1，与设计表一致。见 experiments/reverse-media-member.log。

边界：限换成员收音边界；不能据此保证通话焦点边界正确。

### AUD15-006：已修复

【复现】【代码核查】【文档核查】notify/interrupt.c:70 的 applying/apply_again 保证单执行者最终重算。原 integration-races 两种构建均通过；独立边界探针当前为 active=0 ring=0 state=none。仅在副本绕过单执行者检查，同探针变为 active=0 ring=1 state=alarm，两条断言失败、runner 退出 1，与 §15.2 所述失败一致。见 experiments/interrupt-fixed.log、reverse-interrupt.log。

边界：此修复保证 apply 并发顺序，不保证 CALL 覆盖后 ring 的焦点生命周期；后者为 AUD18-205。

### AUD15-011：已修复

【复现】【代码核查】power_service.c:169 的 locked_step 在后台持有唤醒时按 next_battery_us 有界等待并轮询；integration-notify-lowbat 常规及 ASan/UBSan 均通过。日志见 experiments/ctest.log、ctest-asan.log。

边界：未重做此项反向变异；轮询恢复不等于关机数据收尾已正确，后者见 AUD18-203。

### AUD16-008：已修复

【复现】【代码核查】integration-listening 两种构建均通过，覆盖成人空间关闭后结束 AI 轮次和停止上传；本轮 voice_lifecycle_probe 的收尾再锁屏为 voice=0 rt=0，未把待定听写写回。

边界：验证的是现有空间关闭及正常 UI 执行路径，没有把它推广为任意拥堵执行器下的同步停止证明。

### AUD17-004：已修复

【复现】【代码核查】integration-races、integration-media 两种构建均通过；media_service.c:194 的 media.finished 和 :491 的 audio.record_done 带成员，换成员后事件按归属处理。

边界：本条限本轮音频事件分支和已运行回归；下载、更新、清理等其他分支不作独立完整验收。

## 分项检查与证据边界

### 音频、文件与蓝牙

【代码核查】`audio_service.c` 管理 PCM、采集、路由及焦点；`media_service.c` 在媒体执行器上处理文件、位置和录音写入；`ogg.c` 管理容器；BT 只使用抽象 HAL。这个分工可保留，关键缺口是跨服务的同步截止条件：焦点变更目前先修改焦点表，再在调用者选择的执行器发通知；录音的焦点回调在 MEDIA、听写的焦点回调在 UI。老的 stream 与 ring 接口仍维护各自状态，因此“最高焦点”“实际上在采集/发声”不能视为同一件事（201、205）。

【复现】现有 `integration-media` 覆盖优先级抢占/返回、暂停续播、正常录音、最长录音、卡满、损坏及截断容器、播放时拔卡、音量、耳机和来电。补充 `record_card_probe` 验证：录音持有卡时 U 盘模式返回 `-9/BUSY`；拔卡后以 `write_failed` 完成并释放采集；插回后既有完整页仍可打开播放；再借给电脑成功，新录音被拒绝。未把“剩余完整页可播放”称为拔卡前所有样本均已落盘；卡突然移除无法补写结尾页。

【代码核查】`ogg.c:14` 限制页缓冲，`:82` 检查 CRC，`:127` 用 64 位 granule 换算，`:231` 在拼包前检查容量；音频包/容器异常用例和 ASan/UBSan 基线未见新增越界。未做随机模糊测试或所有 Ogg Opus 变体的兼容性验收。

【文档核查】【代码核查】真机蓝牙没有协议手册，本轮未发现 BT 服务臆造具体 AT 命令。`hal_bt_sim.c` 的扫描、配对、连接、断连与成功延迟是模型假设；真实设备的握手、错误分类、通话路由和硬件切换时延未验证。不能把本轮模型通过转写为真机蓝牙可用。

### 听写、AI 按键和收尾

【复现】【代码核查】`voice_input.c:261` 的 finish 先转 `finishing` 并请求停采集；`:274` 的 interrupt 释放听写焦点，再收尾。`:252` 由采集结束通知切换帧队列所有权，随后发 end；ASR 会话仍保留以接收尾结果。AI 按键路径 `ai_service.c:593` 等待麦克风释放后开启自己的轮次。本轮将 ASR 末结果延迟，实测 `voice=finishing`、两条 RT 会话并存；此时 `capture=1`、VOICE 焦点属于新 AI 轮次。关闭原输入框只取消旧 ASR，AI 仍采集；释放并关闭 AI 后 RT 和采集归零。另一轮反复 interrupt 后锁屏，没有提交旧听写，原值保持 `base`。见 `voice-lifecycle.log`。

【代码核查】结果任务以 sid 隔离轮次，以 seg 过滤已经确认的段；result 与 done 都由同一 hub 路径投递 UI，执行器按入队顺序执行。现有测试覆盖模型支持的结果/结束顺序、失败、断网和迟到结果；未注入一个违反当前模型约定的真实服务器协议，也未声称覆盖任意多生产者重排。UI 忙时来电的停止边界并不由这些检查保证，见 201。

【复现】【代码核查】`quiet_frames` 由音频任务每帧原子更新（有声归零、静音加一），UI 只读取计数乘帧时长（`:143–154`、`:401`）；不再用跨任务两次读时钟相减。`integration-dictation`、handsfree 及相关场景在两种构建均通过。此结论限模拟器帧流，不涉及真实麦克风噪声下的阈值质量。

### 文字、词库、剪贴板与 32 位边界

【代码核查】【复现】编辑服务内部以字节位置保存 UTF-8 光标，外部 select/cursor 以字符位置表达；追加与语音追加会退回完整 UTF-8 边界。现有测试涵盖中文组合、候选、选区、撤销、普通点击、长文本光标窗口以及无卡/坏词库。新问题 202、207、209 说明 OOM、带选区标记的命中测试、跨窗口选区及初始值截断未被原测试充分覆盖；补充文字探针在两种构建结果一致，没有 ASan/UBSan 访问错误。

【代码核查】`text_service.c:20–23` 将正文限制为 4095 字节，历史深度为 16；undo/redo 转移快照，正常稳定状态下历史正文负载总计至多 64 KiB，当前值另占至多 4 KiB，一次新增快照可短暂再占 4 KiB，均在 PSRAM；这些数字不含分配器和元数据。OOM 时不是无界增长，而是历史分支正确性错误（202）。剪贴板上限 4 KiB、仅在 PSRAM；敏感内容到期清空、AI/REMOTE 不可读，普通内容也需成员设置许可，成员变更同步清空；现有文字测试通过。未枚举定时器回调与每一次成员切换的所有交错。

【代码核查】`pinyin.c:63–80` 对文件段边界、条目范围和 CRC 校验，乘法和加法先升到 `uint64_t`；加载上限 1 MiB。此处未发现 32 位 size_t 导致的文件偏移环绕。当前词库校验不证明键排序或文本语义正确，正式词库仍不在范围内。文字服务 cap 已夹到 4095；`textfield.c:215` 的控件配置容量依赖调用者，未做不受信任配置入口或任意 `size_t` 的穷举证明。本机为 64 位构建，未运行 32 位二进制、交叉编译或目标栈检查。

### 音量键、电源菜单与关机图

【代码核查】【复现】`input_service.c:179` 的音量键路径调用 `system_actions.c:67`，只读取小状态字符串、原子累计步数并投递 UI；参数分配、能力调用与设置持久化在 UI 后续执行。普通播放、锁屏播放、音量边界和重叠按键的现有测试通过。响铃使用固定音量（`audio_service.c:326`），不是媒体音量曲线；真实蜂窝通话音量与蓝牙链路行为未验证。

【复现】电源菜单测试覆盖长按、松手不误锁、取消、Home、锁屏关闭、来电优先、重启和关机、低电量以及关闭空间下的关机图。补充实验将 UI 阻塞 9 s，在关机等待期间两次注入 2% 电量并请求锁屏：人工关机与低电量路径分别在约 6059/7089 ms 请求 HAL，计 `late=2`、`on_panel=0`，未死锁。低电量触发计数为 1。此处证明有界等待及失败标记，不保证图片已经显示；最初探针完成计数为 2 的测试助手问题已单独记录在实验说明。

【代码核查】`power_service.c:125` 的 `powering_off` 防止同一低电量流程反复开始，电源任务在关机图等待期间不能同时继续它自己的电量轮询；独立 BG 人工关机可以与之重叠。共享关机收尾状态仍应统一，不将上述单个交错推广为所有重复调用均安全。极低电量下实际 PMIC 会否在绘图前切断、电压是否够完成刷新只能靠后续硬件验证。已实证的数据收尾与刷新回执问题分别见 203、206。

### 内部 RAM、执行器与栈

【复现】【文档核查】`ai_ask-metalio-plus-pqbn/metrics.json` 的 `mem.internal_total.min_free_bytes=33192`，运行过程中最大连续空闲块的最低值为 28616 字节；前者是合计，不能当作一块同等大小的连续可分配内存。相对文档底线余量 424 字节属实；没有重建历史提交独立测量“比基线少 312 字节”。

【代码核查】【复现】`exec.c:61–78` 在第一次 ready/post 时创建执行器任务和信号量；新服务复用已有音频/UI/媒体/后台执行器。干净 standby 与上述 AI 场景中均无 `task.exec.bg` 或 `task.exec.media`；文字词库首次请求、蓝牙能力和存量闹钟任务会启动 BG，所以不能把“干净启动无新任务”扩展到带用户数据的所有启动。听写/媒体组合测试内部 RAM 约 34 KiB，但不是所有功能同时活跃的最坏情况。

从本轮 39 份常规测试指标汇总得到以下已观察最大值，原值见 [baseline-metrics.json](experiments/baseline-metrics.json)：

| 任务 | 配置栈（字节） | 最大估算使用（字节） | 对应场景 |
| --- | ---: | ---: | --- |
| hub | 6144 | 3392 | AI / dictation / handsfree |
| esp_timer | 3584 | 2528 | notify |
| power | 3072 | 2192 | power_menu/battery |
| input | 3072 | 1392 | 多个场景 |
| exec.bg | 8192 | 6328 | lifecycle |
| exec.media | 8192 | 4048 | media |

【代码核查】`simulator/kernel/vtk/vtk.c:809` 将宿主栈消耗减基准、按比例换算再加固定开销；`ports/sim/osal/osal_sim.c:874` 输出的是估算。它们不是真机高水位；ASan 会放大宿主栈，不能拿 ASan 结果替代目标检查。本轮没有观察到这些任务的栈预算告警，但未验证 RISC-V `-fstack-usage` 或硬件中断叠加余量。

### 模块划分、软总线与 AI 可调用性（R-061）

【代码核查】媒体、录音、文字会话、语音输入、剪贴板、BT、音量和关机都有公共能力、状态或事件；UI 与 AI 大体复用相同业务入口。`layering`、`sdk-boundary` 及 `scenario-ai_tools` 通过，未发现为修复本轮问题必须合并整个子系统的依据。内部 PCM、指针、焦点 token 不需直接变成模型可调接口；需要开放的是有清晰权限、输入、完成结果和失败原因的用户操作。

主要架构风险是跨服务契约不够强：焦点表变化不等于采集已截断，route 变化不等于旧缓冲已静音，flush 请求不等于已持久化，busy 清空不等于关机帧成功显示。建议在对应所有者提供可核验的截止代次/样本边界和完成回执，由上层协调；不要用 UI 队列回调代替底层即时保护。文字 AI 能力需要统一权限判定（208），而非每个回调各写一份近似检查。本轮未重新拉取并穷举完整工具 Schema 清单，不声称所有未来能力已向 AI 开放。

## 对象、执行命令与未查范围

- 提交：`4d23fc2a62f9ae39cc1e1bb0a1f1d98ce2cbb27f`，`docs: the next audit in three narrower parts`，2026-11-05 11:03:08 +0800。
- 构建、两组 32 项测试、全部补充实验及三个反向对照的完整命令、退出码、日志链接见 [experiments/README.md](experiments/README.md)；19 份有效补充日志的断言与指标摘要见 [probe-results.json](experiments/probe-results.json)。副本头文件引用和测试完成计数两项实验准备问题另有记录；未将准备失败算成产品证据。
- 只读产品代码、测试和现有文档；审计交付均在本目录。目录外仅生成任务准许的 `build-audit/`、`build-audit-asan/`。反向对照使用副本，不需恢复产品源文件。
- 开始时已有 `docs/audits/2026-11-04-codex-services-s2/experiments/pin_cut_probe.c` 的工作区删除，未修改或恢复。原有自动会话日志 `codex-run.log` 随运行增长超过 3 MB，最终完整移至 `build-audit/audit-runtime-codex-run.log`，未删减，不作为交付证据。没有提交、推送、建分支或声称已远端保存。
- 遵守 TASK.md：未联网、未读线上决策板/Issue/PR、未调用子代理或外部模型、未接触设备或实机数据，未进行被明确排除的实验。未运行其余 53 项 CTest、所有历史反向变异、32 位构建、模糊测试或实机测试。
- 正式 UI 外观、真实中枢协议/正式词库、ESP32S31 移植、D-105、配对/备份/无线传输及 D-95 计时等有意未完成项不计缺陷。本文只评价现有实现和已宣称可用的行为。
