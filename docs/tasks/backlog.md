# 开发任务清单（2026-11-05 起）

用户要求：先夯实系统层的能力缺口和质量，再补齐系统级别的应用与应用框架，**暂不开发具体应用**（阅读、听、报纸、单词都不做），都做完再做底层性能优化。总规则见 [codex-rules.md](codex-rules.md)。

**状态（2026-11-09）：** 已合入 `services-mid`、`member-wake`、`capability-table`、`device-events`、`pairing`、`backup-restore`、`confirm-durable`、`file-index`（拆成 `file-index-1`、`file-index-2`）、`wireless-transfer`，应用框架的 F1、F2、F5（`ui-semantics`、`ui-input-host`、`ui-cards`），以及只写报告的 `phone-gap`（对照完成标准 R-068 查缺口）和 `speech-cache`（DE-R6 早已修好）。下午又合入：框架 F4、F9（`ui-list-paging`、`ui-file-picker`）、`text-long`、`thumb-wiring`，系统应用 `boot-flow`、`contacts`、`clock-alarm`、`ai-chat`、`phone-calls`、`sms`、`memo`、`settings-a`、`power-pages`。`member-quiesce` 做完但不合入，原因见该节。应用框架的选择已定（D-111）。晚上又合入：`kid-1`、`update-pages`、`calendar`、`confirm-pages`、`search`（F7）、`files-app-1`、`camera-gallery`、`backup-pages`、`ai-pages-2`、`settings-b`；从 batch3 起每个任务由主会话复查、合并后整批交 astra 审一轮（1 小时以内）。batch4 已合入：`activity-card`（F6）、`remote-app`、`fw-f8`、`kid-2`、`storage-usage`、`transfer-pages`，以及 batch3 审计的九条修复 `audit3-fix`；batch4 审计的三条修复 `audit4-fix` 已合入。**2026-11-10：** batch5（`privacy-records`、`kid-3`、`file-index-3`、`factory-pages`、`fw-f13`、`port-skeleton`）和 batch6（`perf-memory`、`perf-latency`、`script-s31`）都已合入，两批合在一起的 astra 审计（batch5，高 1、中 2）的修复 `audit5-fix`（B5-01、B5-03、S31 等待换算）和 `index-durable`（B5-02）已合入。batch7（`storage-order`、`fw-gaps`、`sound-clock`、`phone-2`、`ui-fidelity`）全部合入，它的 astra 审计在跑。batch7 审计（高 2、中 2）的修复 `audit7-fix` 在跑。batch9（`ime-lexicon` 正式词库、`set-bluetooth` 蓝牙设置页、`index-cost` 索引代价报告）已合入，它的 astra 审计在跑。batch10（`index-compact` 索引记录按实际长度存、`ime-quality` 简拼和学词、`settings-gaps` 补设置入口）和 batch9 审计的修复 `audit9-fix` 已合入，它们的 astra 审计（batch10）在跑。batch11（`confirm-hooks`、`index-prune`）、batch10 审计修复 `audit10-fix`、batch11 审计（中 2）的修复 `audit11-fix` 都已合入。之后的安排见“2026-11-10 起：Claude 子代理接手”。合并时记下的后续事项见文末“合并时记下的”。

**只做你被分配的那一节。** 每节按一小时左右规划，最多两小时；做不完的写进报告的“没做到的”，不要半成品冒充完成。报告写到 `docs/tasks/<节名>/REPORT.md`。

每节都要满足：

- G1 全量测试和内存检查都过，预算不放宽。
- G2 新的用户操作都进能力表，按钮和 AI 走同一条路；新页面有 AI 快照，符合[规范](../product/system-ui-spec.md)第 10 节。
- G3 把 `docs/system/ui-dependencies.md` 对应行改成实际状态（只改状态，不改判断）。
- G4 页面任务：画板在 `$INK_DESIGN/png/`，先看图；只用规范第 1 节的规格；不做动画；接真实服务，服务没有的显示规范写的空状态，示例数据只放测试夹具；用 `tools/ui_compare.py` 出对照分数，目标每屏不超过 12/255，超了写清差在哪；遵守 `docs/tasks/ui-shell.md` 的“不能违反的设计规则”。
- G5 服务层任务只改服务、策略和测试；页面任务只改界面、框架和测试，服务缺小接口时可以增量加，列进报告。

中枢第 2 步以后还没做：涉及中枢的都用 `sim_hub` 替身和 `hub/` 的合成身份，不花钱，不碰真实数据。

## 系统层

## services-mid

修遗留的中低级问题：`docs/tasks/services-finish/REPORT.md` 列的 AUD18-106、206、207、402、404，该任务 `REVIEW.md` 里还没修的中级，`docs/tasks/hub-step1/REVIEW.md` 的 R4。验收：每条修好并配测试和反向对照，或写明为什么不成立。

## member-wake

按 D-95 改：大人 2 分钟内回来直接回原页；超过走 `MembersBack` → 密码 → 回到原页和原位置。现在是闲置 120 秒或一锁屏就要。用户没有最后确认，时长做成一处可改的值。验收：时序测试覆盖 1 分 59 秒、2 分 01 秒、锁屏后电源键和 AI 键唤醒、孩子不受影响、密码错误、回到原页和原位置；`system-services-design.md` §11.4 同步。

## capability-table

能力总表（R-061）：从代码生成 `docs/system/capabilities.md`：能力 id、谁能调（人、AI）、风险等级、要不要确认、成员范围、对应的设置、状态和事件。ctest 校验生成结果和提交的文件一致，每个能力都有描述和风险等级，每个设置项都有能力。列出没有能力的用户操作，能小改的补上，其余写进报告。

## device-events

设备端协议第一批：`docs/hub/contracts-step1.md`“要设备改的”和 `docs/hub/hub-architecture.md` §13 里，除配对、确认持久化、块传输以外的项目（`device/event` 的 `seq`、`boot`、`member`，`ink/member`，`_meta`，`budget_ms` 等）。验收：`sim_hub`、`hub/` 的假设备和契约测试都同步并通过；旧行为有的测试不删。

## member-quiesce

预算 150 行产品代码。`docs/tasks/services-finish/REVIEW.md` 的 R6-01、R6-02 和 device-events 的 DE-R5 是同一个根因：换成员时还有在途调用、事件和上传。**不要逐个服务传递身份。** 在换成员这一处集中解决：切换时先不接新调用，等在途能力调用和排队的事件、上传结束（有超时，超时的取消并如实回执），再切换。已按成员绑定的长作业（文件、下载）不动。验收：这三条的复现不再成立；换成员的额外时延【实测】写进报告；列出哪些逐服务的代次检查因此可以删（只列，不删）。

**结果（2026-11-09）：不合入。** 这三条在当前主线上已经复现不出来：各服务按原成员打开数据、按指定成员发通知、页面快照查归属，成员编号也不再复用。Codex 做的集中收尾（分支 `claude/codex-member-quiesce`，[报告](member-quiesce/REPORT.md)）让 6 个已有测试失败：后台忙时干脆不让换人，有一处等待会卡住。有在途操作时，换人还要多等 100～180 毫秒；各服务现有的检查也一个都删不掉。所以保留现在逐服务的检查，集中收尾以后需要时再拿出来。

## confirm-durable

预算 300 行产品代码。确认结果落盘、`confirm.status`、`confirm/request`，自动待执行动作的持久标识和结果查询（中枢设计 §7.5）。验收：断电和重启后能查到结果，不重复执行；绑定成员和内容版本的检查不放松。

**结果（2026-11-09）：已合入。** 先建在 member-quiesce 上做完，后来移到主线（[报告](confirm-durable/REPORT.md)），300 行。

## pairing

配网和家庭绑定的设备端：配对状态机（未配对到已吊销）、配网和令牌分开（设备协议 §3.2、§4）、设备热点和小网页在模拟器里的模型、存储卡配对文件（中枢设计 §6.1、§13）。验收：每一步掉线、确认丢失、重启都能恢复；吊销后退回未配对且数据按设计处理。

## backup-restore

备份和恢复的设备端（R-023，D-01）：备份清单、上传回执、恢复预览、覆盖确认、没恢复的项目。验收：串联场景“备份 → 清空设备 → 恢复”在模拟器里走通，中途断电和断网都能继续；按成员隔离。

## file-index

预算 500 行产品代码。文件服务缺口（`ui-dependencies.md` §4.1）：稳定的文件编号和类型、媒体索引、缩略图、图片解码和抖动到 16 灰阶。验收：改名和移动后编号不变，卡拔出和换卡有明确状态，索引不占内部 RAM，缩略图有上限。

**结果（2026-11-09）：已合入，拆成两半。** [file-index-1](file-index-1/REPORT.md)：编号、类型、媒体索引、换卡状态，218 行。[file-index-2](file-index-2/REPORT.md)：解码、抖动、缩略图，191 行，加上 LVGL 自带的 stb_image（两处补丁，记在 third_party/stb/README.md）。还没做：缩略图和文件编号的接线；第一次建索引对每个文件单独写盘和逐条查重，留到性能一节。

## wireless-transfer

预算 400 行产品代码。无线传输：一次性数字授权、只放一个目录、超时断开、和 U 盘模式互斥、按成员。服务在模拟器里真能传文件。验收：传输过程断开、授权过期、空间不足、文件名不合法都有测试。

**结果（2026-11-09）：已合入**（[报告](wireless-transfer/REPORT.md)），315 行。合并时补了一条：授权码连续错 3 次就关闭这次授权。页面在 files-app 做。

## text-long

预算 300 行产品代码。长文本翻页（`ImeLong`）、草稿自动保存并归属页面、选区动作（问 AI、加单词）做成能力。验收：长文本的字节和字符换算有测试，草稿断电不丢，换成员不串。

## 应用框架

## app-framework-1

服务卡片（`Today`、长按图标；界面和 AI 读同一份，更新节流）；活动卡（`ActivityCard` `MainPlaying` `ControlPlaying` `LockPlaying` `MainInCall`）；任务列表 `Tasks`；全局搜索（`Search` `SearchEmpty`：按成员、分页、结果有类型和返回位置）；`IconMenu` `TodayEdit`。

**进度（2026-11-09）：** 今日屏的服务卡片已合入（F5，[ui-cards](ui-cards/REPORT.md)）；长按图标、活动卡、任务列表、搜索、`IconMenu`、`TodayEdit` 还没做。按 [应用框架设计](../system/app-framework-design.md) §9 的 F6～F8 拆。

## app-framework-2

系统文件选择器 `FilePicker`；通用列表和分页组件；页面状态恢复（草稿、导航）；控件带语义，AI 快照自动生成，敏感字段遮蔽。写一份简短的 `docs/system/app-framework.md`：系统应用怎么写，配一个测试里的最小示例。

**进度（2026-11-09）：** 控件语义合同（F1，[ui-semantics](ui-semantics/REPORT.md)）和短输入框（F2，[ui-input-host](ui-input-host/REPORT.md)）已合入；通用列表和分页（F4）的任务书是 [ui-list-paging](ui-list-paging.md)。文件选择器（F9）、页面状态恢复、`app-framework.md`（F13）还没做。

## 系统应用

系统应用的范围见 `docs/product/system-apps.md`。

## settings-all

`SetBluetooth` `SetVoice` `SetMembers` `SetStorage` `SetAbout` `SetPrivacy` `SetKid` `Wallpaper` `Records` `AiMemory`，接上后端。蓝牙“当音箱用”“翻页器”、锁屏相机入口是收起来待用户定的，接口保留，不要删。

## ai-pages

`AiHome` `AiAsk` `AiListenTap` `AiLong` `AiSpeak` `AiOffline` `AiTimeout` `AiBudget` `AiLog` `AiReceipts` `ConfirmWeb` `ConfirmParent` `ConfirmCall` `ConfirmSetting` `ConfirmExpired` `StressConfirm`。

## boot-flow

`FirstBoot` `BootWifi` `BootWifiPass` `BootJoining` `BootJoinFail` `BootClaim` `BootNoHub` `BootConfirm` `BootRestore` `BootHello` `BootRevoked`，接 `pairing` 的状态机。

## kid-member

`MemberSwitch` `PinWait` `PinForgot` `PinSet` `FactoryReset` `FactoryDone` `KidHome` `KidToday` `KidApps` `KidControl` `KidLimit` `KidTimeUp`。

## power-update-pages

`BatteryLow` `BatterySaving` `Charging` `PowerOffEmpty` `BootRecovery` `Update` `UpdateReady` `UpdateBlocked` `UpdateFail` `UpdateInstall` `UpdateDone` `Backup` `BackupFail` `Restore` `RestoreRun` `RestoreDone` `RestoreFail` `LockImage` `LockPlaying`。

## phone-sms

`Phone` `PhoneRecents` `PhoneContacts` `PhoneCall` `Sms` `SmsThread` `SmsCompose`，联系人、通话记录、短信会话存进数据服务。只到模拟器的 AT 模型，不打真电话。

## memo-calendar-clock

`Upcoming` `Memo` `MemoDetail` `MemoEdit` `MemoRecord` `MemoPending` `MemoChecklist` `MemoUndo` `MemoSaveFail` `Calendar` `Clock`。

## files-app

`Files` `FileList` `FileActions` `FileMove` `Transfer` `TransferLive` `TransferUsbBusy` `TransferUsbOn` `TransferUsbFail` `Import` `ImportFind` `ImportJob` `ImportFail`。

## camera-gallery

`Camera` `Gallery` `Photo`，以及规范第 12 节列的相机取景和扫码结果。照片存卡，进媒体索引。

## remote-app

`Remote` `RemoteLoading` `RemoteOffline` `RemoteAddress` `RemoteType` `RemoteBlocked`，在现有 `system/apps/remote` 上补地址编辑、翻页、长按、提交文字，用 `sim_hub` 替身。

**系统应用的进度（2026-11-09）：** 已合入 `boot-flow`（BootHello 的离线示例正文、就地问 AI 教程、回到上次阅读位置没做）、`contacts`、`phone-calls`、`sms`（phone-sms 三块）、`clock-alarm`、`memo`（MemoRecord、MemoPending 没做）、`ai-chat`（ai-pages 前五页）、`settings-a`（设置前四页）、`power-pages`（电量四页）；晚上 `kid-1`（切换成员和密码四页）、`update-pages`、`calendar`、`confirm-pages`、`search`、`files-app-1`（文件四页）、`camera-gallery`、`backup-pages`（备份恢复六页）、`ai-pages-2`（AI 错误页、AiLog、AiReceipts）、`settings-b`（家人、孩子、锁屏和壁纸、记录、AI 记忆）。

## 合并时记下的（2026-11-09）

- 【已做】存储页的分类占用和清理（`storage-usage`）。新卡上要等整张卡的索引建好才有数，后台建索引在 batch5 的 `file-index-3`。
- 【建议】通用列表只有一页时不画“1/1”分页条（画板里没有）。
- 【要用户定】省电开关现在重启后回到关闭，要不要保留。
- 【性能】长文本每打一个字就整篇写盘并落盘；缩略图解码时一直拿着文件服务的锁；第一次建文件索引时每个文件单独写盘、逐条查重。归到下面的性能一节。
- 【真机要看】`INK_ARGS_TEXT` 从 384 加到 416 字节（为整条长短信），每个参数结构大 32 字节；模拟器两套构建的内存余量都过了。
- 【上限】能力表上限从 256 提到 512、状态订阅表从 32 提到 64（都在外部内存）；每个订阅用 64 位记变化，所以状态表、设置表各自最多 64 项，状态现在 46 项。
- 【上限】应用表从 16 提到 32（系统应用已有 15 个），`app.list` 的输出缓冲 8 KB；`app.cards` 仍是 2 KB，卡片超长要明确失败。“全部”页的工具组最多列 16 个已注册应用（`tools()` 里的 `count < 16`），装第三方应用前要改。
- 【合并时改的】“全部”页查找只排匹配项、从第一页排起（原来只是把不匹配的藏起来，结果可能在第二页、第一页空着）；日历注册为 `upcoming` 后不再在工具组重复出现；`AiLog`、`Backup` 不是应用，分别从 SetVoice／Records／对话记录和 SetStorage 进。
- 【在做，batch5 privacy-records】`Records` 按画板做成汇总页：AI 替你做过的事（进 `AiLog`）、和 AI 说过的话（进对话记录）、AI 记得的事、运行记录、保留多久的说明，入口在 `SetPrivacy`；`SetPrivacy` 还没做。现在的 `Records` 自己分页列 AI 记录，和 `AiLog` 重复。下一批做（privacy-records）。
- 【已做】SetKid 的“每天能用多久”已接 `kid-2` 的用时策略。
- 【上限】事件订阅表从 32 提到 64（系统里已有 27 处订阅，测试再加就满，相机和相册曾因此起不来）。
- 【合并时改的】孩子的控制中心不画“正在进行”卡片和“正在做的事”（规格给 KidControl 四样，画了以后孩子的场景测试一直停不下来）。
- 【已做】batch3 审计（docs/audits/batch3）九条由 `audit3-fix` 修完：相机 PNG 编码在内存失败时不再写坏内存、没卡时搜索保留能搜的、AI 触发的备份失败也更新页面、两个拍照能力说清楚哪个保存、文件里的图片进大图、改名按字节核对等。
- 【缺后端】`AiMemory`：设备和中枢都没有列出、删除记忆的接口，页面如实显示不可用；运行记录同样没有。
- 【等中枢】导入书（`Import` 四页）要等中枢的放书协议接上。
- 【建议】`MemoRecord`、`MemoPending` 要先有“录成文件”（`audio.record`，界面依赖表第 52 行）；编码按依赖表写的 Opus 需要引入 libopus，先评估体积和内部 RAM 再定。
- 【做法】CI 改成 `make -k`，一次列出所有编译错误；合并时扫 gcc 才报的写法不能截断输出（这次漏了一条就是因为只看了前 5 条）。

## 合并时记下的（2026-11-10）

- 【已做】`file-index-3`：开机、插拔卡、出现新文件时后台自动建整卡索引，分批让路，建好就停（不再轮询）；`file.usage` 不会把旧数当成已就绪；卡身份文件一次打不开不再被覆盖。索引上限 512→1024 条。合并时改了：只有出现新路径才重扫（改名、删除、覆盖写不重扫），见 [复核记录](file-index-3/REVIEW.md#主会话复核修复轮2026-11-10)。
- 【后续，batch7 storage-order】存储写盘先来先写，后台写盘会排在有人在等的落盘前面；写闪存时任务停顿，碰上短按或开始录音会丢输入、丢开头的声音。hub 端到端测试的第一下点按因此从 3 秒改到 7 秒，camera_gallery、media 两个测试加了“先等落盘再按键”，storage-order 要把这三处改回去。
- 【已知】`index_save` 不再等落盘：改名、删除后马上断电，重启会留下旧路径的记录（编号不复用，查询时找不到文件）。
- 【已知，问用户】阅读器回主页就释放阅读状态，今日屏的“继续读”卡片随之消失（kid-3 报告记下的，`apps/reader/reader.c`）。阅读属于内置应用，要不要现在修等用户定。
- 【已做，batch7 fw-gaps 补】`fw-f13` 写好了“怎么写一个应用”（docs/system/app-framework.md）和能跑的示例 examples/jot；还没有的两样：后台打开集合、取消订阅。
- 【已知】MicroQuickJS 在 UBSan 下异常退出（script-s31 记下）；脚本候选只测量，不进出厂固件。
- 【真机要看】perf-memory 把显示协调任务的栈放到 PSRAM（模拟器里最紧场景的内部 RAM 余量 33,272→37,376 字节）；真机端口要用同样的栈分配并核实性能。主机估算接近上限的栈（main 4000/4096 等）等真机高水位再定。
- 【已知】factory-pages：页面只看当前家人的备份状态；FactoryReset、FactoryDone 与画板对照 51.6、27.6（batch7 ui-fidelity 处理）。privacy-records：对话没有读取历史的后端，页面如实写“暂时读不到次数”。
- 【已做】port-skeleton：ESP32-S31 端口能编译、链接（固件约 1.2 MB），每次相关推送都在 CI 里编一遍（`s31-port.yml`）；HAL 还是空实现，驱动接上要上机。

## 合并时记下的（2026-11-10，batch7）

- 【已做】`storage-order`：有人在等的落盘排在后台写盘前面（同一路径仍按先后）；输入、录音、通话之后 500 ms 内不开始后台写盘，最多等 10 秒，队列过半照写；整卡索引 1000 个文件写盘 39→3 次。hub 端到端测试改回 3 秒。
- 【已做】`phone-2`：通话页静音、拨号键盘、蓝牙耳机切换；AI 发拨号音要人确认。录音没做：证明不了通话声音经过本机音频芯片。
- 【已做】`fw-gaps`：应用在后台打开集合（`ink_app_db_open_async`），状态、设置、事件订阅可以退订（只在界面、媒体、后台三个执行器上保证）。
- 【已做】`sound-clock`：按键提示音、振动、来电和闹钟铃声（程序生成的三种音色）、时钟的倒计时（重启取消）。顺带：输入任务改为最高优先级并固定在启动核（屏幕送数据任务降一级），80 ms 短按不再被屏幕刷新挤掉；开始录音前先暂停写闪存，开头不再丢 90 ms。
- 【已做】`ui-fidelity`：145 个有画板的页面全部量过，分组行的图标白底、分隔线、装饰圆无描边做进共用组件。
- 【已做】`index-durable`：卡上的“索引不全”标记加记录里的大小和修改时间（格式 2，仍 216 字节），文件改动后马上断电也不出错，改名后编号不变；覆盖写、删除不触发任何后台工作。每批建索引 16 个文件。
- 【要用户定】画板是界面统一放大 1.2 倍以前的尺寸，145 页没有一页能到 12/255：按现行规格重出画板，还是改用布局和位置的对照。计时页没有画板，截图在 docs/tasks/sound-clock/。
- 【后续】文件索引集合 1024 条、约 220 KB 放在内部闪存，压缩一次整体重写在模拟器里要 4.7 秒，期间别的写盘都要排队。考虑把卡的索引放到卡上，或缩小记录。
- 【真机要看】输入任务最高优先级、固定在 0 号核，刷新时有输入屏幕会不会出瑕疵，0 号核上的 Wi-Fi 受不受影响；后台写盘静默期 500 ms 和上限 10 秒要按真机校准；NT26 的 `AT+CMUT`、`AT+VTS` 要核对；S31 端口实现目录读取时要填 `mtime`。
- 【后续，架构】通用确认策略 `system/policy/confirm.c` 里按能力名特判了 `call.dtmf`（audit7-fix，还引用了通信服务的头文件）和 `system.factory_reset`（audit5-fix 的清卡说明）。应在能力定义里加钩子，让能力自己在确认前绑定上下文、补说明，策略层只调钩子（confirm-hooks）。
- 【已知】`ink_ui_list_row` 按父容器是否灰底决定画不画分隔线；`ink_shell_bind` 把可点的白圆补上描边。都是样式推断，换主题时要一起看。

## 合并时记下的（2026-11-10，batch9、batch10 和两轮审计修复）

- 【已做】`ime-lexicon`、`ime-quality`、`audit9-fix`：输入法有了正式词库（968,735 字节，23,783 项全部能用随包字体显示），90 条测试首选 83、全拼首选 71/73、简拼首选 12/16；每位家人的选词会被记住（512 条，密码框不学，AI 读不到）；候选栏不会再把“更多”挤出屏幕。词库在构建时下载并按 SHA-256 核对，CI 要能联网。
- 【已做】`set-bluetooth`：蓝牙设置页（开关、连接、忘记、搜索配对）；`audit9-fix` 修了 AI 重开蓝牙后的状态提示。
- 【已做】`settings-gaps`：接上“忘掉这个网络”“清除通知”“拷贝文件”；清单在 docs/tasks/settings-gaps/INVENTORY.md。
- 【已做】`index-cost`（报告）、`index-compact`：索引记录按路径实际长度存，首次建索引写盘 252 KB→92 KB，有人在等的写盘排在压缩后面多等的时间 4.4 秒→1.66 秒（模拟器）。界面绘制分条 96→128 行（多用 21,888 字节 PSRAM）。
- 【已做】`audit7-fix`：拨号音确认绑定到那一通电话；通话键盘按键是秘密字段、事件不带按键值；借卡前写“索引不全”标记；改目录名不再逐个读卡。
- 【要用户定】settings-gaps 清单里没有画板或数据设计的：日期时间和时区选择页、通知渠道、逐条删通知、新建文件夹、写文件、提醒的编辑和删除、文字重做、没连上的已存 Wi-Fi 管理、离开多久要重新验证（D-95）、语音备忘录音和备忘怎么关联。
- 【后续】确认策略按能力名特判（batch11 confirm-hooks 在做）；索引旧记录占上限（batch11 index-prune 在做）。
- 【做法】Linux CI 的 gcc 会报 clang 不报的截断和有无符号混用，S31 不编测试文件也抓不到：合并时用 ink-codex/tools/gcccheck.py（本机的 RISC-V gcc 按 compile_commands.json 编改动的 C 文件）。CI 只检出一个提交，注册进 ctest 的脚本不能用 git 历史。

## 2026-11-10 起：Claude 子代理接手（batch12 起）

Codex 额度用完，任务改由 Claude 子代理（思考等级 medium）在各自检出里做，主会话复核合并，每批合完由一个子代理独立审一轮。通用规矩在 `ink-codex/briefs/subagent-rules.md`（仓库外），任务书仍放本目录。依据：[phone-gap-2](phone-gap-2/REPORT.md) 和用户 11-10 的回答（D-112～D-126，R-069～R-071）。

**batch12 已合入：** `ai-answer-long`（一轮回答最多 32 KB，按页显示，超出明确截断）、`reader-continue`（每位家人记住上次读的书，“继续读”卡片和 AI 共用 `reader.continue`）、`ai-icon`（编织立方体，用户四轮选定，D-109）、`calculator`（十进制定点计算器，“全部”页不再限 16 个应用、应用表 40）、`airplane-mode`（飞行模式和省电保留）；合并时修了状态栏 4G 图标（`net.type` 是 `lte` 不是 `mobile`）。batch12 的独立审计在跑（`docs/audits/batch12/TASK.md`）。

**batch13 已合入：** `port-hal-1`（S31 的 I2C、UART、GPIO 中断、板级接线，只编译）、`memo-record`（录音备忘，Opus，D-112）、`i18n-1`（中英两种语言的机制，语言选项在“关于和更新”）。**在做：** `sms-multipart`、`weather`、`audit12-fix`、`dictation-flake`（CI 上听写测试失败）、`accel-use`。D-124 改为先用厂家现成的屏幕输出层，排队里加一项 `port-display-1`（把现成的输出层接到 `hal_display` 后面，只编译）。

**batch12 留下的后续：** `ai-speech-long`（长回答整篇朗读：中枢 `ANSWER_BYTES = 1800`、朗读 450 帧上限、一轮 45 秒，要改中枢和协议）；“继续读”卡片的百分比、停在今日屏时插拔卡不刷新、卡片标题“在读”和规范的“继续读”；时钟、相机、相册在“全部”页没有自己的图标；计算器在 120% 字号下算式区可能挤；拨号页提示文字压住键盘；点“关闭飞行模式”后面板不自动收起；飞行模式的 `CFUN=4`、Wi-Fi 和蓝牙真正断电、开机窗口期要上机核实；`device-protocol.md` 里“中枢一轮 ≤ 1.8 KB”等 ai-speech-long 一起改。

**排队（大致按这个顺序，互相改同一批文件的先后做）：**

| 任务 | 内容 | 依据 |
| --- | --- | --- |
| `sms-multipart` | 收到的分段长短信拼成一条，超过 70 字的发送分段 | phone-gap-2 A2；等 airplane-mode 合入（都改通信服务和 NT26 模型） |
| `memo-record` | 先量 Opus 和 ADPCM 的体积、内存、处理器占用、文件大小，按数字定编码（D-112）；录音成文件，MemoRecord、MemoPending | D-112 |
| `i18n-1` | 多语言机制：字符串表、语言设置、运行中切换，中文原文作键、缺译回退中文；先迁一小块页面 | R-069、D-116 |
| `i18n-2` 起 | 其余页面迁移和英文译文，英文下的排版 | 等页面类任务少的时候成批做 |
| `port-hal-1` 起 | S31 端口的 HAL：I2C、GPIO 中断、UART、板级芯片接线，只编译不上机；屏幕输出层自己写（D-124），先评估工作量 | phone-gap-2 C；只改 `ports/esp32s31/` |
| `weather` | 家庭中枢取免费天气数据（测试用假数据，不联网），设备只给城市；今日屏卡片、天气页、能力 | D-125 |
| `time-settings` | 日期时间、时区设置页（含时区数据），离开多久要重新验证（D-95）的控件 | D-117、INVENTORY |
| `clock-extras` | 秒表、世界时钟（用 time-settings 的时区数据） | D-125 |
| `emergency-call` | 锁屏不解锁、飞行模式下都能拨紧急电话 | D-119；等 airplane-mode |
| `lock-camera` | 锁屏相机入口，只能拍、看不了以前的照片 | D-121 |
| `accel-use` | 拿起亮屏、静止省电 | D-122 |
| `mobile-network` | 接入点设置、流量统计、漫游开关 | D-125；等 sms-multipart |
| `kid-apps` | 孩子的应用名单和禁用时段 | D-126 |
| `qr-scan` | 先评估识别库（识别率、内存、固件增量），再接进相机 | D-125 |
| `notify-manage`、`files-ops`、`reminder-edit`、`wifi-saved`、`text-redo` | 通知渠道和逐条删除；新建文件夹、写文件、多选；提醒的添加修改删除；没连上的已存 Wi-Fi；文字重做 | D-117、INVENTORY |
| `semantics-shell-1`、`semantics-shell-2` | 剩下手写快照的壳页面迁到控件语义 | phone-gap-2 A3、A4 |
| `voice-interrupt-setting`、`about-details-licenses`、`call-minutes`、`power-pages-member` | 设置里“暂不能”的两项、设备详情和开源许可、通话分钟数、电量页换家人 | phone-gap-2 A6～A9 |
| `open-app-latency` | 打开应用约 1 秒的时间去处，只留有实测收益的改动 | phone-gap-2 A10，最后单独做 |
| `ui-deps-refresh` | ui-dependencies.md 的过时状态 | phone-gap-2 A12 |

不做或先不做（用户 11-10 定）：蓝牙音箱和翻页器、无障碍、热点。

## 合并时记下的（2026-11-10 晚上，batch14）

- 【已做】`time-settings`：设置里的日期和时间、按城市选时区（九个时区，美国 2007 年前的规则也有）、手动设置、“离开多久要重新验证”；`reminder-edit`：提醒能手动添加、修改、删除；`port-hal-2`：S31 的按键和电源 HAL，只编译。
- 【后续】中枢的 hello 要下发 `time_zone`（按家庭设置的 IANA 名称），否则“自动设置时区”一直显示等待；现在 Python 中枢只发 `now_ms`。
- 【后续】现在有两个日期时间选择（时间设置的 SetDateTime 和提醒编辑页），并进 `small-gaps` 统一成一个。
- 【后续】时间设置页对 `time.` 下所有状态变化都整页重画（闹钟改动也会），按规则第 8 节只对影响这一页的事件重画，放进这批的审查。
- 【做法】硬件先查原厂代码（用户 11-10 晚上）：写进 codex-rules 第 9 节和 docs/hardware/README.md 规则第 6 条。port-hal-2 对照原厂查出的问题由 `port-sleep-vendor` 修。
- 【设备】11-10 自检第二、三轮的浅睡把 USB 弄丢，要人按键救；19:10 复原全部核实（全片按区域和备份一致）。第四、五轮只跑安全项，第五轮起跑完停在下载模式，连接保持（[记录](../hardware/2026-11-10-selftest.md)）。
- 【端口，照原厂】电源服务判断“插着电”不能看充电芯片的输入状态码（插着线恒压充电时也是 0），照原厂看电量计电流（> +5 mA 且不满为充电，< -5 mA 为放电）；加速度计每次完整初始化（CTRL1～CTRL6、AOI1 照原厂 `sc7a20h.cc`），不能假设出厂状态；字库和图片大块读改用内存映射前先实测速度（驱动接口读 7.4 MB/s）。

## 试用版之后（2026-11-11 记）

- 【已做】“插着电”照原厂看电量计电流（`s31_board.c`）；试用版的其它合并修正见[接续记录](../project.md#2026-11-10-夜接续记录试用版)。
- 【端口】时钟芯片存本地时间，现在按默认时区 +8 小时换算；设置里改时区还不会传到板级（要一个 HAL 钩子，时间服务改时区时告诉板级）。
- 【端口】ipc0 栈余量 424 B（原厂也是 1024 B）；Wi-Fi 跑起来以后再看一次健康快照，紧了就照原厂以外的理由加大并写明。
- 【端口】配网热点（`hal_provision_*`）还是桩，首次设置的“用手机设置”一步会失败；第一页已加“先用起来”。
- 【端口】摄像头、蓝牙、4G 模组（电话短信）、唤醒词、TLS/WebSocket（中枢）还是桩；浅睡要用户在场再上机（port-sleep-vendor-2 已按原厂对齐，未合）。
- 【用户定】AI 键按下不震（试用版这样做：按住说话会录进马达声）；首次设置第一页的“先用起来”。
- 【合并】audit14-fix、emergency-call、mobile-network、port-sleep-vendor-2 已做完未合（和今晚的合并有冲突）；kids-phone-research、obook-research 的报告待读。
- 【性能】屏幕刷新期间，存储任务每次碰闪存（连查文件大小也算）都要等在途波形做完（`storage_service.c` 申请显示互斥，`display_service.c`）；添加成员第一次打开 `member.retired` 要查三个文件，撞上三次整屏刷新就多 2.6 秒（cell-features-merge 报告）。查大小、读小文件能否不等波形，值得看。
- 【测试】中枢端到端脚本在固定时刻点按（`tests/hub/e2e.script` 的 `@3s tap`），夹具晚一点就点错页面；应改成等就绪再点。`ink_power_state()` 不是纯读取，能力闸门里省掉这次读取就改变了调度（audit14-merge-fix）；移动网络里到底哪一行改了时序还没缩到单行。
- 【测试】CI 上 `integration-races` 的 `/user` 闪存工作比本机多（擦除 66 对 57 次，卡住 5.2 对 4.3 秒）；准备步骤已放宽到 6 秒，为什么 CI 上写得更多还没查。`boot-flow` 的“从中枢恢复”场景在 macOS CI 上（交付版提交）和本机合入 audit14-fix 后都会失败：恢复后活动卡片反复读 `job.list`，界面停不下来，audit14-merge-fix 在查。

## 2026-11-11 夜：移植复用一批（用户：“明天早上可以获得一个把各种现有能复用的轮子都改造集成好的版本”）

- 【已合入，2026-11-12 早上】底层换原厂：4G（`port-cell-3`）、电源（`port-power-2`）、蓝牙和声音（`port-bt-audio-2`）、板级（`port-board-2`）、Wi-Fi（`port-wifi-2`）、屏幕输出层（`port-display-3`）、相机连续取景（`camera-fps-1`，真机修补见下）。移植：EPUB（`reader-epub-1`）、键盘（`ime-reuse-1`）、输入框（`textfield-1`）、阅读设置（`reader-settings-1`）、书架收件箱和书签（`reader-more-1`）、传书（`transfer-s31-1`）、五子棋（`games-1`）、记忆翻牌和贪吃蛇（`games-2`）、音乐（`music-1`+`music-2`）、锁屏表盘（`lock-faces-1`+`lock-faces-2`）、短信联系人电话流程（`comm-ux-2`、`sms-thread-2`）、界面审计改造七批（`ui-fix-a`～`ui-fix-g`）、键盘短信打磨（`polish-1`）、固件瘦身 345 KB（`fw-size-1`）、设备自动测试（`device-autotest-1`）。
- 【已做，主会话】锁屏时分钟照走（时间服务原来锁屏就停分钟，表盘的钟不走）；密码框之后的短输入框按新输入类型量键盘（原来被普通键盘挡住）；能力表上限 512 → 1024（多 2 KB 外存）。
- 【已做，真机】相机取景：保留会话里停下的控制器再开始就不出帧（0 字节、隔一帧超时、超时后重写 586 条寄存器约 2.7 秒），改成每帧只重建控制器：30 秒 24 帧全成功、每帧 277 ms，约每秒 0.9 张（原来 30 秒 4 张好画面）。瘦身前后真机对比：翻页、备忘键盘、开书耗时没有可测的差别。
- 【已做】闪存排空：`/user` 和日志环每次读写擦都持原厂 I2C1 闪存锁（`s31_flash_begin`，板级移植留下的缺口）。
- 【用户定】计时器：原厂 Plus、E-Ink4、CrossPoint 一系、kiikoread 都没有计时器可移植；现有能力已支持秒，只是选择和显示取整到分钟（审计 UX02，约 30 行）。老黄历宜忌：CrossMux 是按日支循环选词，不是真黄历，先不显示。Game Boy：卡带版权。
- 【待修】新键盘：切换中英文后约 2 秒内按下的第一个键会丢（切换后键盘连刷三次，最后一次刷新期间的那下点击没有产生按键动作；4 秒后正常）。设备自动测试的备忘脚本先等 4 秒。
- 【待修】备忘边打字边存：屏幕一直在刷时存储要等空闲窗口，模拟器里一次保存 3 秒超时（`memo.save result=7`），提示“备忘没存上”；和“存储等波形做完”的性能项同源。
- 【告诉用户】板级照原厂后开机时 P14 共用电源复位一次，4G 晚几秒注册；前光最大亮度照原厂限在 10%。
- 【待修】大图标只按名单打包：设备资源包里 106 像素图标只留 22 个名字（`tools/asset_pack.py`），模拟器读的是没筛过的原始文件，以后有页面用了名单外的大图标，测试看不出来、真机上缺图。办法：模拟器加载资源时按同一名单筛。
- 【待修】音乐音质上限：`music-1` 把解码后的声音再编成 16 kHz 单声道 Opus 走现有播放通路，音乐听着闷；应改成按原采样率直接送声音输出。另外 `show_values` 每次刷新都给播放/暂停按钮追加一次绑定（`music-2` 报告）。
- 【待修，低】安装页 `system/ui/app_packages.c` 第 124、145 行改 `-Os` 时 GCC 报路径可能截断（只会导致读不到，不会读错文件），所以这个文件还留 `-O2`。
- 【已知，测试环境】`unit-s31-cell*` 在机器满载时偶发失败：原厂 100 ms 应答超时在宿主机替身里来不及；安静时连跑全过，真机不受影响。
- 【盯着】内部内存：自动测试版空闲 115 KB（最低 79 KB，最大块 56 KB），换装 18 是 127 KB；正式版数字见试用记录换装 19。

## 用户的决定（2026-11-12 早上）

需求（用户原话在前）：
- “修啊 这些都是问题 不是新的需求任务”：已知问题直接修，不受完成界限限制。派出：`ime-switch-key`（切换中英文丢第一个键）、`memo-save-busy`（屏幕一直在刷时存储超时）、`music-pcm`（音乐按原采样率直接播放）、`txt-gbk`（GBK 编码的 TXT）、`cell-double-1`（4G 测试替身偶发超时）；大图标名单的检查并进 `fw-size-2`。
- 计时器：“其他项目没有现成的时钟 app 移植吗 没有的话你就改”。已固定的原厂、CrossPoint 一系、kiikoread、CrossMux 副本里没有计时器可移植（现在的计时器是我们自己的时钟应用），所以改我们的：`timer-seconds`。
- 老黄历宜忌：“可以做 你自己研究有没有公开的 api”。在线接口要账号或收费，且设备只连中枢（D-04）；离线开源库 cnlunar、6tail lunar（都是 MIT）能直接算，移植：`almanac-1`。
- Game Boy 模拟：“可以考虑移植 或后续再做”，先不排。
- 测试短信、电话：“可以 给运营商10000 发或者打电话都可以”。下次装机时用自动测试版做。
- 资源挪到字库分区：“你自己看着办 有利就做”。分区表不能改（硬规则），字库分区日志环前只剩 726 字节，挪不进；改为压缩嵌入资源包腾出应用空间：`fw-size-2`。
- 装机：“你准备好了就刷入 我来试用”。

## 规划 v3：自研系统，成熟应用整块嫁接，每天一版（2026-11-12 下午，D-129）

用户：“我没有说要全部用mogo 阅读也不是全部的主要功能 后续也没有说要完全跟进他的更新 只是让你尽量复用 和参考成熟设计 我们系统还是要自研的 我更接受你提出的移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等） 并且mogo 感觉字体可能也太小了 微信读书账号 这些我都可以提供 你可以先做 中枢有必要启动配合开发的也可以搞 但是我需要你的规划策略 至少每天可以交付给我一个迭代的可用版本 让我及时试用和反馈 避免需要经常性的推倒重来”。

- 规划页（同一链接，v3）：https://claude.ai/artifact/F3wbvUBYivJ6X68S3KDTR1 。要点：平台和外壳自研；每个功能选最成熟的项目整块搬，接同一个嫁接接口（画面、输入、按家人的数据、暂停和切换、能力），套我们的外壳、文字大小和 AI 能力；不跟上游；每天晚上一版试用固件加说明；先定接口、先出预览、新旧并存、测试一起搬、定了不翻。
- 字号：Mogo 界面正文 38 px（和我们一样），读书默认 48 px（18–90）；搬来的应用一律跟系统“文字大小”走。
- 第 1 天（11-12）派出：`fixes-1112`（ime-switch-key、timer-seconds、memo-save-busy、cell-double-1，进今晚试用版 20）、`graft-mogo-1`（嫁接接口 + 新阅读第一版，先模拟器）、`hub-link-1`（S31 加密 WebSocket、配网热点缺口、中枢接 OpenAI 兼容和 Anthropic 两种真模型接法）、`fw-size-2`（压缩嵌入资源包）、`frameworks-research-1`（输入法、通知栏和控制中心、设置的最成熟来源；另有一个代理并行查同一题）。
- 上午暂停的 `txt-gbk`、`music-pcm`、`almanac-1` 不再单做：GBK 随 Mogo 阅读，音质随新音乐（kiikoread 改版 + MyPaperS31 声音输出），老黄历随 Mogo 锁屏和日历数据。
- 之后的顺序（按反馈调）：新阅读书架和统计、传书、设备上的 AI（11-14）；锁屏样式、U 盘、输入法（11-15）；通知栏和控制中心、设置、音乐、电话短信联系人、时钟、日历、天气、相册相机、备忘文件、微信读书。
- 中枢模型用户选了 DeepSeek（D-130）。`hub-link-1` 合入时补：有 `DEEPSEEK_API_KEY` 就默认用 DeepSeek（`deepseek-chat`），中枢启动时读 `ink-hw/private/hub.env`；语音识别先接本机 Whisper。
- **交付计划 v4**（项目经理角色排，[delivery-plan.md](delivery-plan.md)）：外壳 9 个部件、17 个现有应用，11-13 试用版 21、11-14 试用版 22 全部换完，11-15 机动；磁盘才是并行上限（同时约 5 个开发任务）。外壳预览页（设计语言、主屏三方案、锁屏、下拉面板、键盘、AI 面板，可点选）：https://claude.ai/artifact/XuRj3y8DCZv4S5zxrQVJWS ，选择存在它的 `choices` 集合。
- **优先级（D-131，用户 13:1x）**：先把外壳和现有应用（移植或参照成熟应用重写）做到成熟可用，加上真机能用所需的平台缺口；新增功能的移植排后；要被替换的模块不再修（`fixes-1112` 里键盘、计时器两项已做完，随今晚这版带上，不再追加）。交付计划由项目经理角色排（`.claude/agents/project-manager.md`）。
- 外壳重做（用户 2026-11-12 下午：“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”）：主会话看了八个外壳页面的截图，问题是到处粗边框、圆圈和胶囊，空状态占主位，控制中心没有开关格，键盘按键大小不一，返回键/输入框/按钮多种样式。做法：先换一套照成熟墨水屏和手机设计定的设计语言（改主题令牌，全系统一起变），再逐个部件重做；主屏布局（D-60、D-106）给新方案让用户重选。调研：`shell-research-1`（Codex）加一个代理看 Kindle、文石、Mudita、Light Phone、Pebble、长辈模式；输入法、通知栏和控制中心、设置在 `frameworks-research-1`。11-13 出改前改后预览，11-14 起每天改一块（先设计语言，再控制中心和通知、主屏、锁屏、键盘、对话框、AI 面板）。规划页 v3 已加这一节。
- DeepSeek 的 Key 用户已存进 `ink-hw/private/hub.env`（权限 600），用不扣费的模型列表接口验证过可用（HTTP 200）。
- 等用户的：每天装机的时间（默认晚上插 USB）；微信读书到时扫码登录。

## 2026-11-12 晚：合并时查出、待修的

- **执行方式（2026-11-12 22:1x 起）**：Codex 额度快用完（用户：“后续先不派给 codex 了”），外壳改造的任务书改由 Claude 子代理在独立工作区执行，主会话复核每个任务、跑合并门禁：今晚先做 `home-1`（一页主屏）和 `lock-1`（盖住内容的锁屏），两者文件不重叠、并行。

- **充电显示慢**（`update-pages` 合并时暴露）：插上充电后，`battery.charging` 在设备醒着时约 5 秒、空闲时 30 秒以上才变（模拟器同一流程实测；空闲时更新页靠电量自然涨到 30% 才放行）。原因在电源服务：照原厂 `cx25601n` 每 2 或 10 秒轮询充电芯片，锁屏时暂停，`ink_power_battery_policy_start` 只重读电量、不立刻查充电芯片。手机的体验是一插线就显示在充电。要修：插线（VBUS 变化）当作一次用户活动并立刻查一次充电芯片，补“插线到显示充电 ≤ 1 秒”的测试。`update-pages` 先改成等电源服务报出充电后、更新页 3 秒内切回。
- **原厂新输出层（874538e）先不接**：试用版 21 退回 690a023 后用户确认条纹消失。要再接（它带 S31 的中止、DMA 结束和精确 CKV 修正），得单独做一版只换输出层的固件，对比 684/688 行、有无行首占位、CKV 周期三项，各请用户看屏，过了才进试用版。
- **4G 宿主测试偶发失败**（`unit-s31-cell-*`）：原厂 4G 程序在假模组上按真实时间跑，机器忙时 AT 应答超时就中止。今天本机门禁两次（started、nat）、Linux CI 一次（baud，c6469124），单独重跑都过。要修：测试用的系统时间和假模组改成可控的虚拟时间，或者把超时按测试放宽系数统一放大，不改原厂逻辑。
- **重画会吞掉正在进行的点按（框架层）**：页面在手指按下到抬起之间整页重画，会把被按住的控件删掉，这一下点按就丢了。勿扰重合时查出（备份页每秒查状态，`small-gaps-3-backup` 只差 20 ms 就撞上），这次只在备份页按“按着时推迟同页重画”修了，新加 `ink_ui_pressing()`。“切换中英文后第一个键会丢”很可能同类。要在框架里统一：按着时推迟同页的被动重画，或重画时保住被按住的控件。
- **备份完成时连刷两次**：备份页约 0.2 秒内重画两次（先状态、再摘要），墨水屏闪两下，合成一次。
- **重新加入时第一页先闪一下再跳**（11-13 门禁修复轮查出）：点“重新加入”后，已知网络和家庭服务器，按界面规格 §7.1 应该跳过第一页直接出新的加入码；现在先画第一页（墨水屏刷两次，约 1.65 秒），新加入码约 1.7 秒后到了再跳到加入码页，人这时点“在设备上设置”会被跳走。改法：`boot.restart` 里已知网络和服务器时直接 `show(CLAIM)`，等加入码时写“正在取新的加入码”。测试 boot-flow 已按规格改成点完“重新加入”等加入码页。
- **模拟器读目录的顺序跟着主机走**（11-14 Linux CI 查出）：`hal_storage_sim.c` 直接用主机 readdir 的顺序，ext4 是按名字哈希排，换一台 CI 机器测试结果就可能变。模拟器应给出确定的顺序（设备上的 FAT 是写入顺序）；产品里凡是给人看的列表都该自己排序，不靠读目录的顺序。
- **旧音乐列表和“下一首”按读目录的原始顺序走**（同上）：打乱顺序时约 10% 有 2～5 项检查失败，下一首可能落到打不开的文件上，报错文字还写着过时的“not an Ogg Opus file”。旧音乐按 D-141 要换成 music-3，合并 music-3 时核对新音乐按名字排序、下一首跳过放不了的文件，并改掉那句过时的报错。
- **这批合并在 Linux 上还没跑过 ASan**：前三次 Linux CI 的全量测试失败，后面的 Sanitizers 一步被跳过；11-14 修好后看那一步的结果。
- **字体分区快满了**：font_data 可用 4608 KiB，Mac 上装完只剩约 1.5 KB，Linux（apt 的 Noto CJK 稍大）这一轮已放不下界面粗体 ui-2-53。现在放不下的字体会不压缩地进程序镜像（每次约 +485 KB，应用分区余约 800 KB），只是止血。要规划：界面字体的字集、几套字号能否合并、大号正文字体能否按 D-138 放到卡上开机补齐。 → 2026-11-13 傍晚 almanac-2 加 8 个字就把粗体整套挤进了程序镜像（+706,912 B，应用分区只剩 1%），改成任务 [font-pack-1](font-pack-1.md)：字形无损压缩，打包不再整串挤出。
- **文件索引的两个慢点**：首次建索引时 index_renamed 每碰到一个新路径就把整库读一遍（有一次占锁 321 ms）；低优先级后台拿着 edits 时去等数据库锁。（Linux CI 的 index-prune 查出，测试已按真实前提改好。）
- **新阅读要放字体**：卡上 `/content/fonts/InkSC.ttf`（全家一份，家人自己的 `/me/fonts/InkSC.ttf` 优先）；下次接设备时用 U 盘模式拷进去。
- **新阅读没接完**：书架封面和分类、设置子面板（滑杆、四边距、页眉页脚槽位）、目录快跳、书签管理、完整结构化快照（graft-mogo-4 报告）。
- **黄历（almanac-2）没合**：固件多 178 KB（预算 60 KB），大头是 12 万字节中文宜忌文字，先压缩再合。
- **键盘（keyboard-2）没合**：`keyboard-pages`、`sms-multipart`、`autotest-cap` 失败，九键读音选择没做。
- **勿扰（notify-svc-1）撤回待重合**：和 `small-gaps-3-backup` 冲突，查清再合。
- **中枢语音**：本机 Whisper 常驻后首句文字 17.7 秒（2 线程、高负载、只用 CPU），要用 Metal 重测；Silero 和 Paraformer 模型下载等用户定。

## 暂停，先重新规划（2026-11-12 中午）（已由上面的 v3 取代：不再原样托管 Mogo、不跟上游）

用户：“我觉得你要重新先规划一下了 先别做了 哪些应用应该抛弃之前的工作 如何整合三方资源 如何用哪些应用满足哪些功能 这些应该先想清楚 给我一个设计和评估 告诉我最优解是什么 而不是因为之前做过了 所以就在之前的基础上做”；“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源 先满足我的使用体验为主”。

- 10:16 派出的七个任务（`ime-switch-key`、`memo-save-busy`、`music-pcm`、`txt-gbk`、`cell-double-1`、`timer-seconds`、`almanac-1`）已停，半成品留在各自克隆里，规划定了再决定续做还是放弃。下面“功能清单的剩余项”里的派发顺序作废，等新规划。
- 评估标准：功能成熟度和使用体验优先，移植工作量只在打平时参考；开源协议不作限制，只看有没有源码（闭源的当设计参照）；原厂只在底层驱动优先。
- 调研完成（五路代理 + 五个 Codex：`docs/tasks/replan-*/`），设计和评估页：https://claude.ai/artifact/F3wbvUBYivJ6X68S3KDTR1 。建议：平台保留；阅读、传书、锁屏样式整块换成 Mogo（chopper1026/mogo-e4p，同机 S31）并保留它的界面（托管，下面垫兼容层）；音乐用 kiikoread 改版播放器加 MyPaperS31 的 S31 声音输出；电话类系统应用照 Fossify、AOSP 等重做流程；先补真机连中枢、配网、U 盘。等用户定方向后从第 0 期（模拟器里托管 Mogo、量体积）开始。

## 功能清单的剩余项和自写应用（2026-11-12 中午，用户：“所有的应用和系统应用都是移植的最优秀的三方项目吗 我怎么感觉不太像呢 我勾选的需求哪些满足了”）

- 逐项状态见 [picks-status.md](picks-status.md)：勾选 72 项，有 17、部分 18、没有 37（27 项有可移植来源，10 项没有）。
- 勾选的就是需求：有来源的按 `reuse-crosspoint-1/port-queue.json` 的区域拆任务，修复这批空出名额就派（最多 7 个）：fw-size-2 先行（腾空间）；然后 R05 插图和漫画、R04/R11 排版和主题、R06 方向和自动翻页、R08 封面分类和详情、R07/L03 书摘和锁屏句子/迷你书库、R09/R12 统计打卡手账、M08 录音机、T02 U 盘、M05/M06 蓝牙重连和键盘、O01/O02 OPDS 和坚果云（经中枢）、M07 网络电台、A02 语音遥控电脑。
- 自写的系统应用（电话、短信、联系人、时钟、计算器、日历、天气、文件、相册、备忘、远程网页、相机界面、桌面、设置、AI 面板）在“优先移植”之前写成；昨晚的调研只对照了阅读器固件，这些固件没有这类应用。再做一轮更大范围的调研（LVGL 生态的手机、手表、平板等成熟开源项目，许可要兼容），比我们的好就换：`app-reuse-2`。
- 待用户定：微信读书四项（要账号）、原厂云推送和 WorkBuddy（走原厂云和账号）、当蓝牙音箱、联网升级写出厂分区；Game Boy 用户说以后再做。

## 这一阶段的完成界限（2026-11-10 晚上定，用户要求“明确完成界限，不要无休止的执行”）

**目标**：模拟器里的系统层达到 R-068（相当于一台没装应用的手机）。下面的任务做完就停，向用户汇报，等新的指示；不再往里加新任务，除非用户要求。

**完成条件（全部满足）**：
1. 下表任务全部合入 `claude/system-layer`；
2. 每批合完都审过一轮，查出的高、中问题都修完；
3. main 上 CI 全绿；
4. 最后一次缺口检查（phone-gap-3）对照 R-068，凡是模拟器里能验证的都是“有”，剩下的只属于下面“不在这一阶段”的三类。

**任务表（共 27 项）**：

| 状态 | 任务 |
| --- | --- |
2026-11-11 早上的状态：

| 状态 | 任务 |
| --- | --- |
| 已合入（25） | `boot-db-stall`、`time-settings`、`reminder-edit`、`port-hal-2`、`files-ops`、`lock-camera`、`kid-apps`、`ai-speech-long`、`clock-extras`、`qr-scan`、`notify-manage`、`sms-multipart-2`、`emergency-call`、`mobile-network`（这两项以 `cell-features-merge` 合入）、`settings-small`、`i18n-2`（以 `i18n-2b` 合入）、`i18n-3`、`i18n-4`、`semantics-shell-1`、`semantics-shell-2`、`small-gaps-1`、`small-gaps-2`、`port-display-1`、`open-app-latency`、`ui-deps-refresh` |
| 审查修复 | `port-sleep-vendor`、`port-sleep-vendor-2`、`audit14-fix`、`audit14-merge-fix` 已合入；最后一轮整批审查已做完（[batch15 报告](../audits/batch15/report.md)），中等问题 3 项已修，“英文覆盖不完整”1 项等 phone-gap-3 量化后请用户定 |
| 已做完 | `phone-gap-3`（[报告](phone-gap-3/REPORT.md)）：14 个领域 6 A、8 B。完成条件 1～3 满足；**第 4 条没满足**，还有模拟器能验的缺口 G1～G11。G1（高：导出的诊断文件孩子能经文件能力读到）已修（`3b55256b`）；G2～G11 是功能缺口或要先定规则，按完成界限停下，等用户定 |

**phone-gap-3 留下、等用户定的缺口**（严重度照报告）：G2 中，AI 对话记录回看和当前回答重播；G3a 中，改叫法；G3b 中，打字接话；G4 中，把当前照片交给 AI；G5 低，成员门头像选择走原子能力；G6 中，模拟响应没达到 SRS 建议值（录音提示、首句、翻页、可见反馈、解锁、远程点按）；G7 中，英文剩余文案（控制中心飞行模式、设置→家人等）；G8 中，日历独立日程；G9 低，本地自动备份选项和更新说明；G10 低，首启示例内容；G11 中，可动态加载的应用形态（R-008，按 D-08、D-111 分期）。

**补齐 G2～G11（用户 2026-11-11：“都并行做吧，你自己按最合理的、工作量较小的方式来补齐这些”）**：同时最多 7 个，按最小做法拆成 7 项，另有试用后用户点名的两项在跑。

| 任务 | 补哪项 | 最小做法 | 状态 |
| --- | --- | --- | --- |
| `port-cell-1` | 用户点名：真机 4G | 照原厂移植帧协议，AT 通道、上网、状态栏信号；不改模组配置 | 已合入（d2d61e22，超时由主会话接手） |
| `port-cell-2` | 用户追问“4G 参考原厂最新代码了吗”：port-cell-1 有两处没跟 v2.0.77 | 收发改用原厂 UHCI DMA 组件；共享电源 P14 的放电 500 ms、稳定 80 ms 和断电前后通知 4G | 已合入（461ff10d） |
| `port-camera-1` | 用户试用：“拍照也点不进去”；S31 相机是占位 | 照原厂 v2.0.77 移植 GC2145 DVP 驱动，探测不到时相机应用给说明页 | 已合入（1b237839；存储块操作的屏幕门由主会话撤掉） |
| `ime-keyboard-1` | 用户试用：“没有看到中英文切换的按钮” | 键盘照 `ImePinyin` 设计稿补中英切换、中文逗号句号键、大写和删除图标；词库进闪存由主会话做 | 已合入（14be0d9b） |
| `board-vendor-1` | 原厂对比第 57、74～76 行没补 | 充电管理照原厂（VREG 4400 mV 定时核对、终止、再充、没插电 3.1 V 切 BATFET），前光每 5 ms 一级渐变 | 已合入（cffd7b2f）；复核退回修了 7 项回归 |
| `port-bt-1` | 原厂对比第 59、60 行没补 | 照原厂 BH1098 协议文档和代码接蓝牙耳机（模式 2），不做音箱和翻页器（D-120） | 已合入（10860b99）；协议缺口（断开事件、芯片端忘记、独立配对）原厂文档未定义 |
| `registry-research-1` | 用户点名：调研 CloudZao 固件仓库（含社区移植），找可复用、该补齐、该参考的 | 只读调研，不下载固件；报告分三类加建议清单 | 已合入（f9cbd130）：52 个固件、156 个版本；没有能整套并入的；可复用候选 U1～U5（阅读、字体、位置映射），新缺口候选 N1～N4 都要用户定（见下） |
| `ui-borders` | 用户点名：灰底改边框 | 主题里集中改白底细边框 | 已合入（a9a3eeb1，已装机） |
| `ai-history-photo` | G2、G4 | 按成员存最近 50 轮；重播请中枢读；照片缩成小图作附件 | 派出 |
| `voice-name-assist` | G3a、G3b | 从前端支持的唤醒词里选；接话放进候选条，默认关 | 派出 |
| `calendar-events` | G8 | 日程就是不响铃的提醒，同一套存储和编辑页 | 派出 |
| `small-gaps-3` | G5、G9、G10 | 头像走原子能力；自动备份用现有作业；首启示例是自写短文 | 派出 |
| `app-runtime-1` | G11 | 只做页面描述应用（D-111），卡上文件夹装卸，权限走现有闸门 | 派出 |
| `i18n-5` | G7 | 加“直接写中文”的检查并扫尾 | 排队（等界面任务合完） |
| `perf-latency-2` | G6 | 先拉近离建议值最远的两三项 | 已合入（c26a1e40）：远程更新 p95 1373.8→1252.8 ms，PQ-BN 录音提示 623.0→569.3 ms（模拟器）；其余几项仍离建议值远，要真机数据再动 |
| `port-display-2`（主会话，要上真机） | 原厂 v2.0.77 的屏幕输出层修复 | 改 `display_upstream.cmake` 的固定提交到 `874538e`，真机看刷新日志和灰阶（用户看） | 用户 2026-11-11：等补缺任务都合完再做；补缺任务 2026-11-11 18:4x 全部合完，可以开始 |

**试用反馈 4 和功能清单（2026-11-11 19:4x 起，用户：“我非常不满意 这些都要重新搞”；“所有开发的非底层功能优先移植成熟项目”）**：背景 `docs/tasks/ui-feedback-4.md`；用户在功能清单页勾了 71 项（`docs/tasks/reuse-crosspoint-1/picks.md`）。

| 任务 | 做什么 | 状态 |
| --- | --- | --- |
| `ui-audit-2` | 每个页面对照现代 UI/UX 审计，出统一组件规格和改造拆分 | 派出（19:51） |
| `comm-ux-2` | 短信（直接输号码）、联系人（独立应用）、电话照手机标准流程 | 派出（19:51） |
| `ui-input-2` | 键盘和输入框 | 19:54 停掉：用户要求键盘复用现成的（LVGL `lv_keyboard`、`lv_ime_pinyin` 含九宫格，或开源项目里的），等 `reuse-crosspoint-1` 结论再派 |
| `reuse-crosspoint-1` | CrossPoint、CrossInk、crosslink、CrossMux、固件仓库链接的 13 个源码仓库、原厂三块板的仓库：许可证、模块、71 项对照、输入法结论、推倒重来的建议、移植队列 | 派出（19:59） |
| `camera-fps-1` | 取景每 3 秒一帧且每帧重建采集；改照原厂连续会话 | 派出（20:34） |
| 计时器按秒 | 用户：先找现成的移植 | 暂停，等 `reuse-crosspoint-1` |
| 短信没收到 | 系统记录已发出（模组 2.7 s 回成功）；下一版记短信回执结果，再请用户同意发一条测试短信 | 等装机 |
| 4G 通话 | `call-research-1` 已合入：呼叫控制大概率可用，双向声音未知；要用户同意一次受控通话 | 等用户 |

**底层换成原厂代码（用户 2026-11-11：“这种级别的底层流程和驱动 你能复用就直接复用 拷贝过来都行”；审计 `driver-audit-1a/1b/1c` 已合入）**：公共规矩 `docs/tasks/vendor-common.md`。磁盘只剩约 50 GB，分两批，每批三个。

| 任务 | 换什么 | 状态 |
| --- | --- | --- |
| `port-cell-3` | 4G 模组驱动换成原厂 `uart_eth_modem` 加 iot_eth 原文，允许列表和日志脱敏放在原厂发送边界 | 派出（19:36） |
| `port-display-3` | 屏幕输出层换到 874538e（即 `port-display-2`），屏幕电源照原厂 | 已合入（be44b32a，20:4x 门槛全过）；共享 IO 扩展的睡眠屏蔽归 `port-board-2`；灰阶等用户看 |
| `port-board-2` | I2C 总线、开机清理、P14 共享电源、前光 10% 上限、触摸、按键、加速度计、马达照原厂 | 派出（19:36） |
| `port-wifi-2` | Wi-Fi 照原厂 `wifi_station`（关 Wi-Fi 真停射频、AP 回来能切回） | 排队（等第一批腾出磁盘） |
| `port-bt-audio-2` | BH1098 模式、应答重试、SCO、I2S 照原厂 | 排队 |
| `port-power-2` | 充电、电量计、时钟照原厂驱动 | 排队 |
| `call-research-1` | 4G 通话能不能打通、声音走哪（只调研） | 派出（19:16） |
| 主会话 | 开机崩溃：中断入口调到闪存里的 `pxPortGetCoprocArea`（ESP-IDF v6.1 的缺口，审计 C22），放进 IRAM（`0220613e`） | 已提交，等下次装机验证 |

**registry-research-1 留下、等用户定的候选**（报告 `docs/tasks/registry-research-1/REPORT.md`，都还不是需求）：
- N1 外部书库入口：先自有 OPDS 书目，再考虑网盘（原型 3～5 人日）；接哪个外部服务、要不要账号由用户定。
- N2 跨阅读器的进度和批注互通（KOReader、微信读书记录或自建，先做位置换算实验 2～3 日）；上传哪些阅读记录由用户定。
- N3 电脑往设备当前输入框发普通长文本（1～2 日，依赖 S31 传文件端口）；开放哪些输入框由用户定。
- N4 有声书：先在电脑或中枢转成现有的 Opus，再接章节跳转（1～2 日）；要不要设备直接放 M4B 由用户定。
- 另外两条事实：原厂列表上已有 2.0.78（2026-11-10），公开源码还停在 v2.0.77；可复用代码 U1～U5 都核过许可证，用到时按文件补查。

**原厂对比复查（2026-11-11 下午，用户问“昨天做了一个原厂对比的 codex 任务 你都完成补齐了吗”）**：对照 `vendor-feature-audit` 报告盘点表逐行核对。没有全部补齐。

- 已补：触感三档（第 54 行）、冷暖前光（56）、存储卡读写（62，真机能用）、喇叭和麦克风（58，模式 1；通话的声音通路未验证）、4G 帧协议（77，`port-cell-1`）、触摸虚拟键（79）、日志和崩溃现场（83，改成闪存日志环加 coredump，可导出到卡）。浅睡照原厂已合入（95，`port-sleep-vendor-2`），试用版没开，要用户在场上机。
- 已合入（18:4x 前）：摄像头（64，`port-camera-1`，真机探测到 GC2145）、充电管理和前光渐变（57、74～76，`board-vendor-1`）、4G 照 v2.0.77（`port-cell-2`，真机已注册、能上网）、蓝牙耳机（59、60，`port-bt-1`）。
- 在查：开机偶发崩溃（2026-11-11 换装 12，13 次开机 1 次）：核 1 上某个读闪存时也会跑的中断碰了缓存区；下一版开机打印中断分布来锁定（见试用记录）。
- 端口缺口，还没排（S31 上 HAL 仍是占位）：存储卡借给电脑当 U 盘（63）、Wi-Fi 传文件和手机配网（70）、唤醒词（S31 能否跑 ESP-SR 未核）、连家庭服务器的加密连接（AI 要用，中枢本身不在这一阶段）。电量显示原厂用电压平滑估算（72），要完整充放电样本标定。
- 已定：设置页手选切换 SIM 卡（D-128，2026-11-11 接上，只放行原厂切卡的三条命令）。
- 要用户定：OTA 在线升级（93：现在的分区里升级要写出厂固件所在的 ota_0，违反“不碰 ota_0”）；马达软 PWM（55）；设备当蓝牙音箱（61）；外接 USB 摄像头（65）；壁纸图库（66，连同锁屏默认 A/B/C）；自选关机图片（67）；设备端导入字体（68）；原厂云推送和账号（69）；日志上传到哪（71，现在能导出到卡）；充电电流 0.5/1/1.5 A 可选（73，原厂有，默认 1 A，我们固定 1 A）；自动旋转（78）；自检和电池测试页开放范围（80、81）；WorkBuddy（92）；原厂五套主题（94）；电池健康度和温度（101）；RTC 硬件倒计时和时钟输出（102、103，暂不需要）。

**不在这一阶段（到这里就停下问用户）**：
- 上真机：烧录、驱动调试、真机性能实测（每次写设备都要先整片备份、征得同意）。
- 真实家庭中枢：真模型、听写朗读、账号、备份同步、浏览器节点、固件源（要服务器和密钥，可能花钱）。
- 真打电话、发短信（花钱）。

## 性能

## perf-memory

内部 RAM 和 PSRAM：找最大的几项，有实测收益才改。存储任务的栈 2026-11-09 按主机估算从 3584 加到 4096，能编 S31 后用 `-fstack-usage` 核实，能降回就降。验收：各场景内部 RAM 最低余量和 PSRAM 峰值的前后对比表【实测，模拟器】；行为不变，测试和确定性都过；没有收益的尝试写明，不留改动。

**结果（2026-11-10）：已合入**（[报告](perf-memory/REPORT.md)）。显示协调任务的栈放到 PSRAM，最紧的正常场景（AI 工具调用）内部 RAM 最低余量多出 4,104 字节；产品代码 4 行。栈大小都没动：当时还没有 S31 端口，拿不到完整调用链。

## perf-latency

点按到反馈、翻页、开机到主页、打开应用的时延：同样要前后对比，只留有实测收益的改动。真机才能确认的标清楚。

**结果（2026-11-10）：已合入**（[报告](perf-latency/REPORT.md)）。点名的四处：活动卡、AiLog 计数、缩略图解码持锁改了，长文本每字落盘量下来不值得改；整体 p50/p95 基本不变（点按到第一帧约 234 ms，翻一页到可读约 601 ms，模拟器）。
