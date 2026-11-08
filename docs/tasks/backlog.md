# 开发任务清单（2026-11-05 起）

用户要求：先夯实系统层的能力缺口和质量，再补齐系统级别的应用与应用框架，**暂不开发具体应用**（阅读、听、报纸、单词都不做），都做完再做底层性能优化。总规则见 [codex-rules.md](codex-rules.md)。

**状态（2026-11-09）：** 已合入 `services-mid`、`member-wake`、`capability-table`、`device-events`、`pairing`、`backup-restore`，以及只写报告的 `phone-gap`（对照完成标准 R-068 查缺口）和 `speech-cache`（DE-R6 早已修好）。`member-quiesce` 做完但不合入，原因见该节。`confirm-durable`、`file-index`（拆成 `file-index-1`、`file-index-2`）、`wireless-transfer`，以及应用框架的 F1、F2、F5（`ui-semantics`、`ui-input-host`、`ui-cards`）已由 Codex 做完、审过一轮，正在收尾。应用框架的选择已定（D-111），`text-long`、应用框架和系统应用各节不再等待。

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

## pairing

配网和家庭绑定的设备端：配对状态机（未配对到已吊销）、配网和令牌分开（设备协议 §3.2、§4）、设备热点和小网页在模拟器里的模型、存储卡配对文件（中枢设计 §6.1、§13）。验收：每一步掉线、确认丢失、重启都能恢复；吊销后退回未配对且数据按设计处理。

## backup-restore

备份和恢复的设备端（R-023，D-01）：备份清单、上传回执、恢复预览、覆盖确认、没恢复的项目。验收：串联场景“备份 → 清空设备 → 恢复”在模拟器里走通，中途断电和断网都能继续；按成员隔离。

## file-index

预算 500 行产品代码。文件服务缺口（`ui-dependencies.md` §4.1）：稳定的文件编号和类型、媒体索引、缩略图、图片解码和抖动到 16 灰阶。验收：改名和移动后编号不变，卡拔出和换卡有明确状态，索引不占内部 RAM，缩略图有上限。

## wireless-transfer

预算 400 行产品代码。无线传输：一次性数字授权、只放一个目录、超时断开、和 U 盘模式互斥、按成员。服务在模拟器里真能传文件。验收：传输过程断开、授权过期、空间不足、文件名不合法都有测试。

## text-long

预算 300 行产品代码。长文本翻页（`ImeLong`）、草稿自动保存并归属页面、选区动作（问 AI、加单词）做成能力。验收：长文本的字节和字符换算有测试，草稿断电不丢，换成员不串。

## 应用框架

## app-framework-1

服务卡片（`Today`、长按图标；界面和 AI 读同一份，更新节流）；活动卡（`ActivityCard` `MainPlaying` `ControlPlaying` `LockPlaying` `MainInCall`）；任务列表 `Tasks`；全局搜索（`Search` `SearchEmpty`：按成员、分页、结果有类型和返回位置）；`IconMenu` `TodayEdit`。

## app-framework-2

系统文件选择器 `FilePicker`；通用列表和分页组件；页面状态恢复（草稿、导航）；控件带语义，AI 快照自动生成，敏感字段遮蔽。写一份简短的 `docs/system/app-framework.md`：系统应用怎么写，配一个测试里的最小示例。

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

## 性能

## perf-memory

内部 RAM 和 PSRAM：找最大的几项，有实测收益才改。验收：各场景内部 RAM 最低余量和 PSRAM 峰值的前后对比表【实测，模拟器】；行为不变，测试和确定性都过；没有收益的尝试写明，不留改动。

## perf-latency

点按到反馈、翻页、开机到主页、打开应用的时延：同样要前后对比，只留有实测收益的改动。真机才能确认的标清楚。
