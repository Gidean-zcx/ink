# phone-gap-3：R-068 收尾检查

基线：`b796b9fd6268e4dda1c1a38fd441de2478b68dc8`，2026-11-11。按 [R-068](../../requirements.md#L121)、[phone-gap-2](../phone-gap-2/REPORT.md) 和 [阶段完成界限](../backlog.md#L249) 检查。**目前不能认定系统层达到这一阶段的完成线**：普通和内存测试全绿，但仍有模拟器可验的缺项。首要的是 G1：获准使用文件应用的孩子，其 AI 能读共享诊断路径上的文件。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 14 个领域已查 | 【实现／实测】逐领域表、AI 原生对照和实际截图，另核性能和端口 | `git log --first-parent --oneline 4099f390..b796b9fd`；普通全量日志 |
| 2 | 已分类 | 【实现】下文单列能用模拟器验证的缺项，逐项有严重度和补法；真机、中枢、真实通话短信分开 | `rg -n -e 暂不能 -e 暂时读不到 system/ui/shell`，其余见对应代码与用例 |
| 3 | 普通与内存全量通过 | 【实测】两轮均 `100% tests passed out of 363`，0 跳过；未改代码或测试 | 见“验证与复现”；`git diff --stat b796b9fd..HEAD` |

## 逐领域复评

沿用上轮尺度：**A** 本领域的当前模拟器验收功能可用，**B** 主体可用但有具体缺项，**C** 只有一部分。本轮模拟器为 **6 A、8 B**。每行的“有／没有”指模拟器范围，真实条件另列；上轮未单独打分时引用其条目编号和原状态。源码为【实现】，CTest 名用于复现，不从用例名称推断未覆盖的行为。

| 领域 | phone-gap-2 → 本轮 | 模拟器：有／没有 | 代码位置；对应 CTest |
| --- | --- | --- | --- |
| 通话 | B → **A** | **有**：拨接挂、记录、静音、DTMF、路由、分钟显示、锁屏和飞行模式下的紧急流程。**没有**：真实音频和录音，后者明确返回不可用，见真机类。 | [电话页及分钟订阅](../../../system/apps/phone/phone.c#L436)、[通信能力](../../../system/services/comm/comm_service.c#L645)；`phone-calls`、`emergency-call`、`small-gaps-1` |
| 短信 | B → **A** | **有**：成员会话、联系人选收件人、草稿、收发分段和乱序／缺段标识、未知／失败回执；长短信发送有明确上限，AI 的正文参数与人的长文草稿不是同一容量。 | [页面与送出](../../../system/apps/sms/sms.c#L116)、[成员集合及分段](../../../system/services/comm/comm_service.c#L56)、[NT26 PDU](../../../drivers/chips/nt26/nt26.c#L314)；`sms`、`sms-multipart`、`unit-nt26-pdu` |
| 联系人 | B13 有 → **A** | **有**：每人一个号码，分页、新增、修改、删除、供电话和短信选用；孩子只读写自己成员的数据。 | [成员集合](../../../system/apps/contacts/contacts.c#L13)、[能力](../../../system/apps/contacts/contacts.c#L243)；`contacts`、`phone-calls`、`sms` |
| 相机相册 | B25 部分／B26 有 → **B** | **有**：取景、拍照存卡、锁屏拍和本次回看、扫码、九宫格、看图和删除。**没有**：“问这张图”给模型的照片内容链路；深睡不能直接触摸相机入口，先唤醒。 | [相机及锁屏页](../../../system/apps/camera/camera.c#L128)、[扫码](../../../system/apps/camera/scan.c#L98)、[相册单图动作](../../../system/apps/gallery/gallery.c#L144)；`camera-gallery`、`lock-camera`、`qr-scan` |
| 时钟日历提醒 | B14 有／B15–16 部分 → **B** | **有**：闹钟和贪睡、倒计时、秒表、世界时钟、月历、手动增改删提醒、日期与时区设置。**没有**：独立于提醒的日程记录；自动时区缺中枢字段。 | [时钟页](../../../system/apps/clock/clock.c#L456)、[月历与提醒页](../../../system/apps/calendar/calendar.c#L149)、[提醒能力](../../../system/services/time/time_caps.c#L348)；`clock-alarm`、`sound-clock`、`clock-extras`、`calendar`、`reminder-edit`、`time-settings` |
| 文件 | B20 部分／B21–23 有 → **B** | **有**：文件索引和类型、缩略图、文件选择、拷贝移动删除、新建文件夹和小文本、草稿恢复、多选、U 盘和无线传输的模型。**没有**：从文件页打开 EPUB/PDF 的阅读闭环；从中枢导书另归中枢。 | [文件页动作](../../../system/apps/files/files.c#L249)、[文件能力](../../../system/services/file/file_caps.c#L489)、[文件类型](../../../system/services/file/file_service.c#L1594)；`files-app`、`files-ops`、`file-index-3`、`thumb-wiring`、`transfer-pages`、`integration-wireless-transfer` |
| 设置 | B → **B** | **有**：网络、显示声音、勿扰、存储、隐私、手动备份与恢复夹具、语言、日期时间、离开多久重验、省电重启保留、设备详情、许可、更新和清空。**没有**：叫法可改、打字接话、自动备份设置、更新说明完整路径。 | [设置各组](../../../system/ui/shell/settings_pages.c#L19)、[时间页](../../../system/ui/shell/time_settings.c#L98)、[备份页](../../../system/ui/shell/backup_pages.c#L84)、[更新说明](../../../system/ui/shell/update_pages.c#L126)；`settings-b`、`settings-gaps`、`settings-small`、`small-gaps-1`、`time-settings`、`airplane-mode`、`backup-pages`、`update-pages`、`factory-pages` |
| AI | B（设备端）／C（整体） → **B**（设备端）／**C**（真实后端） | **有**：模拟语音和打字问、工具、回执、确认、离线提示、长回答分页、替身朗读和打断；上限 32 KiB。**没有**：能回看的对话记录、当前答案重播，真实记忆／运行记录也未接。 | [聊天入口和空记录](../../../system/ui/shell/ai_chat.c#L15)、[长度合同](../../../system/include/ink/sys/ai.h#L18)、[问答上下文](../../../system/services/ai/ai_service.c#L514)；`ai-chat`、`ai-pages`、`ai-answer-long`、`ai-speech-long`、`integration-ai-surface`、`hub-simulator-e2e` |
| 多成员和孩子 | B03 部分／B04 有／C20 部分 → **A** | **有**：成员与 PIN、孩子主页、每日限时、应用名单、禁用时段／星期、话短信保留、独立重启状态；找回密码与重新绑定依赖中枢。 | [家长设置页](../../../system/ui/shell/settings_family.c#L181)、[孩子应用规则](../../../system/policy/members.c#L385)；`kid-1`、`kid-2`、`kid-3`、`kid-apps`、`audit14-ownership`、`integration-member-wake` |
| 锁屏和安全 | B02 有／B03 部分／D07 无 → **B** | **有**：PIN、超时重验、锁屏相机、紧急呼叫、确认卡、休眠模型中拿起亮屏／静止省电。**没有**：成员门头像选择的本人能力；导出的诊断文件也缺成员访问边界。本人密码与确认按设计不交给 AI。 | [锁屏](../../../system/ui/lock_mark.c#L55)、[成员门回调](../../../system/ui/shell/member_gate.c#L65)、[诊断导出](../../../system/services/log/log.c#L208)、[能力策略](../../../system/policy/cap.c#L85)；`time-settings`、`accel-use`、`emergency-call`、`lock-camera`、`confirm-durable`、`semantics-shell-member`、`unit-log` |
| 网络 | Wi-Fi A／蓝牙 B／4G B → **A** | **有**：Wi-Fi 扫描、连接、管理未连已存网，蓝牙耳机模型，网络偏好、在线切 SIM、飞行模式、APN／漫游／流量。**没有**：真实模组和蓝牙接线验证；热点、音箱和翻页器已明确排除。 | [已存 Wi-Fi](../../../system/ui/shell/navigation.c#L1333)、[移动网络页](../../../system/ui/shell/mobile_network.c#L87)、[通信服务](../../../system/services/comm/comm_service.c#L794)；`integration-net-auto`、`bluetooth-settings`、`airplane-mode`、`settings-small`、`mobile-network` |
| 通知 | C15 部分 → **A** | **有**：成员收件箱、分组未读、勿扰、逐条删除、渠道开关，重启保留；关闭渠道不等于关闭来电或闹钟。 | [通知能力](../../../system/services/notify/notify_caps.c#L149)、[成员收件箱](../../../system/services/notify/notify_service.c#L65)；`integration-notify`、`integration-notify-lowbat`、`notify-manage` |
| 语言 | C25 无 → **B** | **有**：中英切换、重启保留、增语言用词表，已迁应用、系统壳和服务文案。**没有**：英文全状态覆盖；未经过翻译函数的系统文字不计入缺译计数。 | [语言设置与回退](../../../system/text/i18n.c#L20)、[控制中心未翻译文字](../../../system/ui/shell/navigation.c#L1030)、[记录页](../../../system/ui/shell/settings_family.c#L120)；`i18n-lang`、`i18n-apps`、`i18n-memo`、`i18n-shell`、`i18n-shell-remaining`、`i18n-services`、`i18n-tables` |
| 应用框架 | B → **B**（C 开发 **A**） | **有**：注册、生命周期和后台打开，能力、语义、输入、分页、文件选择、活动和卡片、全局搜索、应用列表超过 16 个。**没有**：完整声明页运行时、脚本 SDK、应用包装卸；C 写应用的验收有可跑示例，后续形态仍待定范围。 | [框架及例子](../../system/app-framework.md)、[注册与上限](../../../system/ui/app_registry.c#L29)、[语义导出](../../../system/ui/ui_semantics.c#L45)；`app-framework-example`、`app-framework-gaps`、`ui-semantics`、`ui-file-picker`、`fw-f8`、`calculator` |

### R-068 的共用能力和资源

| 项 | 模拟器结论 | 证据 |
| --- | --- | --- |
| 备忘与录音 | **有**文字、清单、离线 Opus、播放、待整理；真转写另列。 | [备忘能力](../../../system/apps/memo/memo.c#L480)；`memo`、`memo-record`、`integration-media` |
| 输入法和主页 | **有**拼音、候选、学词、长文草稿、文字重做、输入法和主页语义、继续读进度与插拔卡。听写设备端有替身测试，真实听写另列。 | [文本能力](../../../system/services/text/text_caps.c)、[继续读能力](../../../apps/reader/reader.c#L249)；`ime-learning`、`text-long`、`settings-small`、`reader-continue`、`semantics-shell2-keyboard`、`integration-dictation` |
| 计算器、天气、远程画面 | **有**定点计算、天气及城市和离线旧值、远程页面和点按的替身。**没有**真实天气部署验证和浏览器节点。 | [计算器](../../../system/apps/calculator/calculator.c#L313)、[天气](../../../system/apps/weather/weather.c#L290)、[远程页面](../../../system/apps/remote/remote.c#L651)；`calculator`、`weather`、`remote-app` |
| 流畅与确定性 | **有**预算和确定性回归；**没有**达到部分 SRS 响应建议值，见 G6。 | `perf-latency`、`determinism`、`determinism-reader`、`storage-order`；[前任务实测](../open-app-latency/REPORT.md#前后对比)模拟释放到可读 p50：短信 1591→809 ms、设置 945 ms、相册 1430 ms；相册仍有两次刷新。 |
| 硬件使用 | **有**模拟触摸、键、前光、声振、显示、RTC、加速度计、卡、电量和芯片故障注入；真机接入与验证见后。 | [板级](../../../boards/metalio_plus/)、[模拟自检](../../../tests/simulator/selftest.c)；`selftest-board-wiring`、`selftest-board-wiring-pqbn`、`vendor-haptic`、`accel-use`、`integration-time-rtc_lost` |

本轮保留的模拟器截图（仓库根目录相对路径，均为测试生成，不提交；带随机后缀的目录重跑会换名）：

| 领域 | 截图路径 |
| --- | --- |
| 通话／短信／联系人 | `build/test-out/small-gaps-product/phone-zh/snap-call-3-minutes.png`；`build/emergency-call-0t0zkgnc/airplane/snap-active.png`；`build/test-out/sms-multipart/compose/snap-compose-parts.png`；`build/test-out/contacts/write/snap-scene.png` |
| 相机相册／锁屏 | `build/qr-scan-screens/snap-url-en.png`；`build/lock-camera-9vust28l/lock/snap-restricted.png`；`build/camera-gallery-compare/Camera/snap-scene.png`；`build/camera-gallery-f_exr85c/many/final.png` |
| 时钟日历提醒／设置 | `build/clock-extras-evidence/sim-clock-extras/create/snap-world-zh.png`；`build/test-out/reminder-edit-90muigki/create/snap-detail.png`；`build/time-settings-evidence/sim-time-settings/snap-time-en.png`；`build/test-out/small-gaps-product/pages-zh/snap-device.png` |
| 文件／AI | `build/files-ops-g698dqnw/recover/snap-MultiSelect.png`；`build/test-out/ai-chat/snap-AiLong.png`；`build/test-out/ai-chat/snap-AiHome.png`（空历史） |
| 多成员和孩子／网络／通知 | `build/kid-apps-y8uks0r8/policy/snap-SetKidPeriods.png`；`build/test-out/mobile-network/main/snap-mobile-zh.png`；`build/notify-manage-evidence/sim-notify-manage/main/snap-channels-zh.png` |
| 语言／应用框架 | `build/test-out/i18n-lang/restart/snap-Settings-en-restart.png`；`build/test-out/i18n-shell-remaining/snap-UpdateDone-en.png`；`build/test-out/calculator-apps20/snap-AppsSecond.png`；`build/jot-hg9bv2i0/write/final.png` |

## 相对 phone-gap-2 补上的项目与 AI 原生检查

【实现】分层仍由应用／系统服务／平台目标和软总线隔开。常用按钮调用原子能力；AI 工具到同一关口并检查页面版本、角色、成员代次和确认。运行时在导出前后核对成员与代次，锁屏浮层另给脱敏快照。参见 [能力关口](../../../system/policy/cap.c#L102)、[AI 工具入口](../../../system/policy/cap.c#L1538)、[上下文快照](../../../system/ui/ui_task.c#L436)；普通全量的 `layering`、`capability-table-policy`、`integration-ai-surface`、`integration-member-wake` 通过。快照存在不代表每个动作已开放，也不代表模型已经读到媒体内容。

| 相比上轮的新增／修正（合入） | 人与 AI 的能力；快照 | 隔离和验收边界 |
| --- | --- | --- |
| 通话分钟、锁屏／飞行模式紧急呼叫及拨号提示（`e802ae57`、`bcde51f9`、`90920db0`） | 通话 `call.*`；AI 可 `phone.emergency_open`，实际拨挂及拨号键是本人能力；`PhoneCall`、`PhoneEmergency` 有快照。 | [通话归属检查](../../../system/apps/phone/phone.c#L341)、[紧急能力规则](../../../system/services/comm/comm_service.c#L645)；`small-gaps-1`、`emergency-call`、`small-gaps-2-phone`。锁屏不泄露普通记录。 |
| 长短信收拼接、发分段；短信冷开少一次刷新（`9b0282dd`、`b796b9fd`） | `message.send/list`；`SmsThread`、`SmsCompose` 等有快照，发信的分段数进入确认说明。 | 成员集合、草稿引用和发信关口；`sms-multipart`、`sms`。冷开只影响绘制节奏；数据仍通过原服务。 |
| 锁屏相机、扫码、相机／相册入口图标（`35e72018`、`162d4067`、`3ba30618`、`90920db0`） | 普通 `camera.photo/scan/scan_apply`、`photo.open` 可经 AI；锁屏拍与本次回看使用能力，锁定时不允许 AI 操作；Scan、Camera、Photo 有快照。 | 扫码密码只交给对应能力，不进快照；换人丢弃迟到结果；锁屏不见旧照片，孩子拍照归自己。`qr-scan`、`lock-camera`、`camera-gallery`；像素传递缺口见 G4。 |
| 日期、时间、时区、重验时长；提醒编辑、复用选择器、秒表和世界时钟（`90f94594`、`619824da`、`5a144bde`、`90920db0`） | `time.set`、`settings.set`、`reminder.add/update/cancel`、`clock.stopwatch*`、`clock.world*`；设置、提醒、时钟有快照。 | [提醒写入用调用成员](../../../system/services/time/time_caps.c#L162)，[秒表和城市分成员](../../../system/apps/clock/clock.c#L307)，系统时间／重验限制儿童；`time-settings`、`reminder-edit`、`clock-extras`、`small-gaps-2-picker`。 |
| 文件新建／编辑／多选、文件页后台索引重画修复（`50a84493`、`2b66bf38`、`3ba30618`） | `file.mkdir/write/batch/delete_many`；`Files` 及编辑／多选有快照；后台索引属于服务，无独立操作。 | `/me` 绑定成员，孩子不能改共享文件，批删经本人确认；`files-ops`、`files-app`、`audit14-ownership`。 |
| 32 KiB 长回答、长朗读、说话打断开关、AI 图标（`27fd3740`、`d56cde2a`、`e802ae57`、`a439081b`） | AI 面板与人共用 `ai.panel.next/prev`、`shell.ai_stop`，打断用 `settings.set`；回答和开关有快照，图标无新增操作。 | 换人结束旧轮，不接收旧帧；`ai-answer-long`、`ai-speech-long`、`small-gaps-1`、`semantics-shell2-ai`。 |
| 孩子应用名单和禁用时段；电量页换家人（`a72102b5`、`3ba30618`、`e802ae57`） | `kid.app_set/period_set` 及电量页 `settings.set`；应用名单、时段、电量页有快照。 | 家长验证本人完成；孩子不能修改或读取另一孩子的规则。`kid-apps`、`audit14-ownership`、`semantics-shell2-battery`。 |
| 通知渠道和逐条删除；已存 Wi-Fi、飞行模式、APN／流量／漫游（`33899109`、`fee20f04`、`61f55c19`、`bcde51f9`、`90920db0`） | `notify.dismiss/set_channel`、`wifi.list/connect/forget`、`net.set_airplane`、移动网络能力；各设置与收件箱有快照。 | 通知按成员；网络为设备设置，敏感值不进 AI；APN 密码仅本人填，改网络受权限控制。`notify-manage`、`settings-small`、`airplane-mode`、`mobile-network`。 |
| 设备详情与许可、双语、文字重做（`e802ae57`、`ab505bba`、`8a4a3674`、`bc13de46`、`5cc0a6eb`、`fee20f04`） | `shell.device/licenses`、`settings.set(system.language)`、`text.redo`；设置及键盘有快照；改语言只换词表和设备设置。 | 标识和密码脱敏；文字重做按当前编辑会话，敏感输入不向 AI 开放。`small-gaps-1`、`settings-small`、`i18n-lang`、`semantics-shell2-keyboard`。 |
| 两批系统壳语义迁移；继续读、插拔卡、应用目录分页（`8093e65a`、`f7ceca75`、`59faf152`、`90920db0`、`ddb9a5fd`） | 壳浮层和主页、键盘、成员、AI、电量统一从语义声明导出；有兼容文本格式；`reader.continue`、`app.list/open` 和 `shell.launch` 可调用。 | 过期动作拒绝，阅读卡分成员；本地头像选择是 G5 的例外。`semantics-shell-*`、`semantics-shell2-*`、`reader-continue`、`calculator`、`app-framework-example`。 |
| 录音备忘、计算器、天气（`bc96c23e`、`ddb9a5fd`、`1e32b7c0`） | `memo.record/play`、`calc.eval/key`、`weather.get/find/set_city`；备忘、计算器、天气和选城市有快照。 | 录音与草稿按成员；计算器临时页；天气城市设备共用、孩子不能改，今日卡位按成员。`memo-record`、`calculator`、`weather`。 |
| 加速度计、触觉、端口装配与日志（`31307330`、`04068eb5`、`2ad203ca`、`e5d736eb`、`bf47b0bc`、`5775ceea`、`aa9d9a9e`、`f452a6ed`、`a0e058ad`） | 拿起／静止和触觉是平台事件和反馈；不开新的 AI 硬件入口。记录问题 `system.report_problem` 可调用，原始导出 `system.export_logs` 仅本人；电源菜单有快照。 | 导出产物没有跟随访问边界，见 G1。`accel-use`、`vendor-haptic`、`unit-log`、`integration-power-menu`；端口不等于实机实测。 |

[ui-deps-refresh](../ui-deps-refresh/REPORT.md) 补文档出处；`8253c8d7` 修合并后的两处时序；`5aee142b`、`33b4ca60`、`4c64ae16`、`3ba30618`、`8fff0412` 包含批次审查修复。C 应用接入、联系人和主要系统应用在上轮已有，本轮没有重复记为新增。

## 没做到的

### 模拟器可验、且不属于三类排除项

严重度：高＝破坏权限或数据边界；中＝核心功能、承诺界面或响应未闭环；低＝窄范围缺项。下表是发现和补法建议，不是在本分支加任务。

| 编号 | 严重度 | 模拟器里没有什么；证据 | 建议怎么补 |
| --- | --- | --- | --- |
| G1 | **高** | **导出后仍隔离诊断文件。**【实现】`system.export_logs` 禁 AI 发起，但[导出代码](../../../system/services/log/log.c#L208)把日志和原始转储写进 `/content/ink-logs`；[文件解析](../../../system/services/file/file_service.c#L230)把它当 `/family/ink-logs`。【实测】家长已允许文件应用的合成孩子，其 AI 用 `file.read` 读共享诊断路径返回 `done`，读成人私有路径返回 `rejected`；对照见下。未读取真实转储，不推断某条凭据已泄露。 | 原始诊断不进可由普通文件能力读取的共享空间；导出只走成人本人入口。用合成诊断标记验成人导出后，孩子的文件列表、文件读能力和 AI 均取不到；另外验正常共享资料仍可读。 |
| G2 | **中** | **回看历史和重播。**【实现】[AiHome](../../../system/ui/shell/ai_chat.c#L24)总是空记录；[面板动作](../../../system/ui/shell/ai_panel.c#L195)有收起、翻页和再问，没有当前回答重播。`ai-chat` 覆盖空页和单轮，`ai-speech-long` 覆盖首播及停止。 | 用合成历史先接读列表和按成员打开；给当前回答一个共用重播动作并核对换人；真实历史存储和合成服务另验。 |
| G3a | **中** | **更改叫法。**【实现】[语音设置](../../../system/ui/shell/settings_pages.c#L19)这一行没有设置或能力，仅“暂不能更改”。叫名字和语音打断的用例不能替代更改叫法验收。 | 先定可改的规则，用模拟唤醒走设置、读取和权限；真机唤醒词识别另验。 |
| G3b | **中** | **打字接话。**【实现】[同页](../../../system/ui/shell/settings_pages.c#L25)没有可切换设置或接受候选动作；`ime-learning` 是输入法词频，不是 AI 续写。 | 先用合成候选接普通文本框，测试开关、接受／拒绝、密码框禁用和换人；真实模型生成另验。 |
| G4 | **中** | **把当前照片交给可读它的 AI。**【实现】[照片快照](../../../system/apps/gallery/gallery.c#L165)只有文件路径和状态，按钮只唤起 AI；[问答请求](../../../system/services/ai/ai_service.c#L550)只打包页面文本等上下文。`camera-gallery` 验按钮和隔离，不验图片交付。 | 用带成员授权的照片引用和合成图验交付、过期和越权，或在视觉链路可用前说明入口范围；视觉模型按中枢计划后置。 |
| G5 | **低** | **成员门头像选择走原子能力。**【实现】[头像点按](../../../system/ui/shell/member_gate.c#L69)直接开本人验证或调用成员服务；已有 `system.switch_member` 并非这个按钮的路径。`semantics-shell-member` 验脱敏和版本，不能证明共路。 | 给成员门选择加仅本人可用的原子入口，保留现有验证和过期拒绝；密码、本人确认继续不开放给 AI。 |
| G6 | **中** | **达到部分模拟响应目标。**【实测】全量日志报 `SRS GAP`：录音提示 p95 505.1／621.8 ms（目标 300）、首句 p95 3258.2 ms（3000）、翻页 p95 668.7／702.7 ms（600）、可见反馈 p95 447.4 ms（350）、解锁最大值 822.8 ms（800）、远程点按 p95 1352.3 ms（800）。这些是 [SRS 建议值](../../system/srs.md#L11)或场景标注，预算测试均通过，非真机数字。 | 优先用现有 `scenario-ai_ask-*` 和 `scenario-reader_basic-*` 拆提示、送屏、波形；保留预算，实机校准后再判是否达标。 |
| G7 | **中** | **英文覆盖剩余状态。**【实现】[控制中心的飞行模式按钮](../../../system/ui/shell/navigation.c#L1030)、[隐私记录页](../../../system/ui/shell/settings_family.c#L120)和[长回答截断提示](../../../system/include/ink/sys/ai.h#L19)仍直接给中文；`i18n-shell` 等通过只代表已测路径。 | 收口这几条固定文案及对应布局，做 English 页面和快照反例；用户输入、已存通知和诊断标识保持原值。 |
| G8 | **中** | **日历独立日程。**【实现】[月历](../../../system/apps/calendar/calendar.c#L149)与[时间服务](../../../system/services/time/time_caps.c#L319)管理闹钟和提醒；手动提醒已完成，没有独立日程的编辑和读取。`calendar`、`reminder-edit` 对应现有范围。 | 若阶段门仍要求独立日程，先定义与提醒的区分，再以成员集合、能力和月历编辑页补齐；验是否响铃、跨日和换人。 |
| G9 | **低** | **本地备份自动选项和更新说明界面。**【实现】[备份](../../../system/ui/shell/backup_pages.c#L84)和[更新](../../../system/ui/shell/update_pages.c#L126)明确提示暂不可用；有手动备份／模拟更新，不等于本地选项和说明已接。 | 有约定后，用合成备份计划及更新说明先验设置、显示和失败态；真实同步和固件源留中枢。 |
| G10 | **低** | **首启可直接问的示例内容。**【实现】[BootHello](../../../system/ui/shell/boot_flow.c#L205)只有提示、跳过和打开书架；[boot-flow](../boot-flow/REPORT.md)已标暂定 D-99 的内容缺失。 | 若保留 D-99 暂定方案，提供一段许可清楚的示例正文，离线可看，可跳过，用模拟 AI 验当前页上下文。 |
| G11 | **中** | **可动态加载的应用形态。**【实现】[R-008](../../requirements.md#L62)还要求动态加载安装；[C 接入说明](../../system/app-framework.md#1-注册应用与启动顺序)明确没有应用包加载器、完整脚本 SDK 和产品声明页运行时；`app-framework-example` 只证明 C 应用。[script-s31](../script-s31/REPORT.md)仅是测量原型。 | 按 D-08／D-111 分期，先验声明页的权限、快照和加载失败回退，再定包的签名、安装／卸载验收；不把 C 链接或整机刷写当作安装。 |

### 只在后续条件下才能核实

| 类别 | 没有／未验证 | 边界与证据 |
| --- | --- | --- |
| **上真机** | 基线整机驱动、耗电、光学刷新和真栈水位；摄像头、蓝牙、4G 模组及部分 HAL；浅睡、USB 恢复、RTC 时区与唤醒；FAT／USB／OTA 的真实链路；确认通话音频路径后才谈录音。 | [S31 桩](../../../ports/esp32s31/hal_stubs.c#L38)仍含相机／蓝牙／WebSocket／蜂窝等，`RTC_LOCAL_OFFSET_S` [固定 +8h](../../../ports/esp32s31/s31_board.c#L118)。[试用记录](../../handoff/trial-guide.md)证明更早的 `g1390e166` 已能起机和使用一部分功能；本轮没有重验。 |
| **真实家庭中枢** | 真模型、STT／TTS／备忘转写、账号和重绑定／找回、历史／记忆／运行记录、备份同步、自动时区、固件源、浏览器节点和视觉模型；真实天气部署及放书协议。 | [中枢 README](../../../hub/README.md)、[中枢 hello](../../../hub/ink_hub/gateway/server.py#L370)未发 `time_zone`，设备[已读字段](../../../system/services/hub/hub_link.c#L401)。`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`、`weather` 用合成数据，天气已有真实数据源代码但本轮不访问它。 |
| **真实通话／短信** | 实际拨通、VoLTE、DTMF／静音兼容性、真实 PDU 拼接、回执、无 SIM 或飞行模式紧急呼叫与射频恢复。 | [蜂窝合并报告](../cell-features-merge/REPORT.md)及 `emergency-call`、`sms-multipart` 仅证明模型；不联系任何号码，不发短信。 |

用户已有取舍：[R-037](../../requirements.md#L91) 不做完整 EPUB/PDF 兼容；文件页目前只走文本开头预览、音频或图片，后续若要求就地读书，应走原有内容准备／阅读流程。蓝牙音箱和翻页器不做，热点与无障碍先不做（[R-071](../../requirements.md#L124)）。声明页和受限脚本按 D-08、D-111 分期，目前只有测量原型，C 框架已能开发；应用装卸和签名仍无产品实现。邮件、通用浏览器和应用商店仍按[系统应用分类](../../product/system-apps.md#L24)所列范围处理。上述不当成新需求或本轮完成项。

## 和设计不一致的

- [界面规范 §3.5](../../product/system-ui-spec.md#35-锁屏)仍写“相机没有入口”，D-121 已改为唤醒后的受限拍照；浅睡直接触摸不唤醒。`lock-camera` 验的是醒着锁定的行为。
- [界面规范的 AiHome](../../product/system-ui-spec.md#L283)要能找回以前问过的内容；当前总是空页（G2）。R-069 的英文目标仍有 G7；R-061／G-002 的权限与共路缺口见 G1、G4、G5。
- [设计中的首启示例](../boot-flow/REPORT.md)仍未接（G10）。[天气任务](../weather.md#L13)原写“设置里”，最终改为在天气应用里选，仍经同一能力。
- 【实现】端口参照原厂 `690a023` 的 `main/boards/common/power_policy/README.md:73–85,141–169` 做睡眠／恢复顺序；当前默认关闭浅睡，不能据源码当作真机验证。原厂 `main/boards/metalio-e-ink4-plus/periph/metalio_rtc.cc:33–38` 按低有效读 RTC 线；项目按[既有同板测量](../../hardware/2026-11-10-selftest.md#L98)用高有效。2026-11-11 核对本机原厂只读副本，没有改参数或接触设备。

## 改过的已有测试

无。未改任何源码、测试、阈值、依赖、设计或共享文档；没有修复需要做撤改反证。诊断发现的对照仅用合成数据运行既有模拟器。

## 要用户定的

- 阶段门若继续按“模拟器可验均有”，优先收口 G1，再处理 G2–G7。G8–G11 和文件就地打开的范围，建议在现有设计和任务边界内明确；本轮不擅自扩成下一批开发。
- 真机与真实中枢阶段的上机、真实凭据和花钱验证仍等后续指示。本轮未调用真实模型、短信或电话。

## 顺手发现

- [phone-gap-2](../phone-gap-2/REPORT.md)的“整个端口是桩”已过时；最新试用文档与端口代码可区分已接和仍为桩。不能把历史试用当作指定提交已上机。
- `open-app-latency` 的普通及内存轮曾在 `integration-dictation` 失败；本轮两轮全量该项和免按键用例均通过。保留历史差异，不能据此判断时序问题已根除。
- [阶段门](../backlog.md#L249)列的开发项在基线合并历史中可找到；`i18n-2` 以 `i18n-2b` 和合并补修收口。已有 batch12–14 审查及修复记录，未在基线内找到所有后续合并完成后的独立整批审查报告。远端 main 和基线 CI 的具体提交分开核对，见验证节。
- 决策板链接返回网页外壳，未拿到 `decisions` 集合；D-112～D-126 以本地[已定记录](../../decisions/README.md#L135)为准，没有改写。GitHub 连接器按仓库查未结 Issue 和 PR，均返回空。

## 新增的机制

无。产品代码 0 行，测试代码 0 行；无新增任务、线程、状态机、持久文件格式、配置项或框架。只提交本报告。

## 要改的文档

由合并会话同步 backlog 的阶段门状态、共享界面依赖中落后的端口／AI 快照状态，并按 D-121 更新界面规范锁屏说明。本任务只写本报告，不改这些文件。

## 验证与复现

普通全量：配置、构建及 CTest 均退出 0。原话 `100% tests passed out of 363`、`Total Test time (real) = 366.39 sec`，363 通过、0 失败、0 跳过。`gui-smoke` 使用 SDL dummy 驱动，没有启动有窗口的模拟器。日志 `build/verification/{configure,build,ctest}.log`、`build/Testing/Temporary/LastTest.log`。

ASan/UBSan 全量：独立配置、完整构建、同 363 项均退出 0，原话 `100% tests passed out of 363`、`Total Test time (real) = 577.84 sec`，0 跳过。实际编译参数含 `-Werror -fsanitize=address,undefined`；检查了全量输出、`LastTest.log` 和构建日志，未见 ASan、LeakSanitizer 或 UBSan 错误诊断。日志 `build-asan/verification/{configure,build,ctest}.log`、`build-asan/Testing/Temporary/LastTest.log`。宿主链接有既有 `libink_port_sim.a` 重复库提示。

在仓库根目录设置任务指定的 `INK_DEPS`；设计稿不参与构建。宿主构建和两轮全量依次运行，按总规则限制在 utility 优先级、`-j2`。中枢 Python 依赖从只读检出复用，三个中枢测试实际运行。

```sh
py="$INK_DEPS/build-deps/pyenv/bin/python"
export PYTHONDONTWRITEBYTECODE=1 SDL_VIDEODRIVER=dummy
export PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$py" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" \
  --test-dir build --output-on-failure -j2
```

内存检查复用上面配置，目录换 `build-asan`，增加 `-DINK_SANITIZE=ON`；运行时设置 `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`，不设置截图保留变量。领域表中的截图均来自普通轮。

G1 的合成对照：【实测】默认关闭孩子文件应用时两路径都拒绝。用普通轮 `files-ops` 输出的 `build/files-ops-g698dqnw/flash.img`，该测试[以家长确认开启了文件应用](../../../tests/files_ops/files_ops.c#L19)。每次用它的新副本，先通过模拟触摸选择孩子，再让模拟中枢发一次 `file.read`。两个文件均为同一段合成文本，分别放在夹具 `content/ink-logs/synthetic/logs.bin` 和 `content/.m/owner/synthetic.txt`。两进程各退出 0，`members.switches=1`、`action.ai.file.read=1`；共享路径 `tools.calls.done=1`，成员私有路径 `tools.calls.rejected=1`。这轮只核实访问路径，没有导出或读取真实日志／转储。

复跑本机留存夹具：

```sh
scope=build/verification/log-isolation
for variant in shared private; do
  cp build/files-ops-g698dqnw/flash.img "$scope/$variant-repeat.img"
  taskpolicy -c utility build/sim-configured --profile simulator/profiles/metalio-plus.json \
    --script "$scope/permitted.script" --hub "$scope/$variant-permitted.hub.json" \
    --assets build/assets --content "$scope/content" --flash-image "$scope/$variant-repeat.img" \
    --out "$scope/$variant-repeat" --quiet --trace --no-random-outages
done
```

夹具若被清理，可用 `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^files-ops$' --output-on-failure -j1` 重新生成合成闪存。脚本在 5 s 点 `(190, 930)` 选择孩子，12 s 开始合成麦克风并按住 AI 键 1200 ms，22 s 结束。中枢 JSON 沿用 `tests/scenarios/ai_tools.hub.json` 的 `hello` 和一轮 `ai.listen` 形状，仅将工具换为 `file.read`，`path` 分别填 `/family/ink-logs/synthetic/logs.bin` 和 `/members/owner/synthetic.txt`。本次证据：`build/verification/log-isolation/shared-permitted/metrics.json`、`build/verification/log-isolation/private-permitted/metrics.json`。

S31：【实测】ESP-IDF v6.1 `fff9895c` 配置及 `ink-s31.elf` 编译链接退出 0；`file` 验为 32 位 RISC-V ELF，编译数据库含显示、音频、Wi-Fi 和浅睡桥各一份。浅睡关闭；0 条编译器警告，SDK 有 5 条配置警告和 1 条 Bash 补全警告。只构建 ELF，不打包固件，不调用设备工具。日志 `build-s31/verification/{configure,elf,export}.log`。复现使用 [S31 构建脚本](../../../ports/esp32s31/build.sh)相同环境，在构建目录复制默认 `sdkconfig` 及只读依赖里的 `build-s31/esp-opus-1.0.5.zip`；加载 SDK 环境，设置 `INK_LVGL_SOURCE_DIR`、`INK_UPSTREAM` 和 `IDF_COMPONENT_MANAGER=0`，然后从端口目录执行：

```sh
b="$(cd ../.. && pwd)/build-s31"
taskpolicy -c utility idf.py --preview -B "$b" -DIDF_TARGET=esp32s31 \
  -DSDKCONFIG="$b/sdkconfig" -DINK_S31_LIGHT_SLEEP=OFF reconfigure
taskpolicy -c utility cmake --build "$b" --target ink-s31.elf -j2
```

GCC：【实测】`gcccheck: 22 compiled, 0 with findings`，退出 0，无跳过。检查使用普通构建的编译数据库和 RISC-V GCC 15.2.0，覆盖下列代表性源文件，不能代替 Ubuntu 的原生编译测试。日志 `build/verification/gcccheck.log`。

```sh
taskpolicy -c utility "$py" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  system/apps/{phone/phone,sms/sms,contacts/contacts,camera/camera,camera/scan,clock/clock,calendar/calendar,files/files,calculator/calculator,weather/weather}.c \
  system/services/{ai/ai_service,comm/comm_service,log/log,notify/notify_caps,file/file_service}.c \
  system/policy/{cap,members}.c system/text/i18n.c \
  system/ui/{ui_task,app_registry,shell/navigation,shell/settings_pages}.c
```

远端只读核对：【实测】查阅时 GitHub `claude/system-layer` 为指定基线；`main` 为 `bc13de46`，[该提交的 sim 工作流](https://github.com/Gidean-zcx/ink/actions/runs/42917748054)绿。[基线 sim 工作流](https://github.com/Gidean-zcx/ink/actions/runs/42926007435)的 macOS 与 hub 已过，Ubuntu `build-test` 仍在跑。不能用较早的 main CI 或本机测试替代基线的 Linux 结果。

收尾检查：【实测】表格、用例名、相对链接及截图可读性通过。`git diff --stat b796b9fd..HEAD` 只有本报告，`git diff --check b796b9fd..HEAD` 无问题。对基线到 HEAD 的差异执行总规则第 6 节原式隐私扫描，grep 退出 1、0 匹配。产物、夹具和截图只留在忽略的构建目录；未推送或开 PR。
