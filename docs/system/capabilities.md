# 能力总表（代码生成）

由 `tools/capabilities.py` 读取 `system/`、`apps/` 的 `ink_cap_t`、`ink_setting_def_t`、`ink_state_def_t` 定义生成；请修改代码后重新生成，不直接编辑本文件。

```sh
python3 tools/capabilities.py
python3 tools/capabilities.py --check
```

当前共 514 项：可调用能力 473、事件 41；设置 44 项、状态 55 项。本表列设备固件的静态定义，不含中枢运行时的远端目录。静态目录包含按硬件条件注册的能力；实际可用性仍取决于硬件、当前页面及服务状态。CTest 另外与 Plus 模拟器启动后的完整注册表核对，不使用按成员或页面过滤的 AI 清单代替完整目录，并核对实际 C 确认策略的来源、PIN 和 asked 组合。

风险按代码的 effect 归类：read / navigate 为低，local_write 为中，local_destructive / external / system 为高；这是副作用等级，不等于所有高等级操作都弹确认。描述和 effect 必须显式填写，遗漏、未知值、重复 id 或文档过期均使检查失败。

调用和确认规则来自 `system/policy/cap.c` 的 exposed / gate / confirm_for，以及 `system/policy/settings.c` 的 setting_set。表中人指触摸、实体键；AI 指 INK_SRC_AI，外部调用还须各自的 expose 权限。AI 不能代输 PIN 或替人按确认。asked=true 只取消普通对外动作的确认，不绕过 ask_by_hand、成人 PIN、成员、页面版本或服务内部校验。

成员范围指能力闸门允许的调用成员，均绑定调用时的当前成员及代际，不表示能访问其他成员数据。非 system 能力的 child_ok 直接放行儿童，白名单按 id 前缀额外放行 child_ok=false 的能力；system 能力则只允许 child_ok=true 的人工儿童调用，白名单不能绕过。成人空间关闭时 AI、外部应用都不能调用；while_locked 只放行人工入口。数据集合、文件路径、通知、闹钟、后台作业仍检查各自的归属；设置是设备级值，儿童写权限另列。事件只能订阅，不能当工具调用，投递仍按成员隔离。

system.undo 的外层 confirm_for 无需确认，不代表整个撤销无需确认。设置撤销保留当前来源，重新经过 settings.set 及设置项的儿童、确认、版本检查；其他撤销以已确认来源调用逆操作，仍检查逆操作的成员及服务条件。

关联面按同一服务目录（policy / ui 按源文件）及该提供方源码引用的已知 id 汇集，表示服务上下文，**不表示每次调用都会修改列出的所有设置、状态或发出所有事件**。完整设置、状态和事件定义均列在后文。没有定义的关联显示 —，不据命名前缀猜测。

## 可调用能力

| 能力 id | 描述 | 谁能调 | 风险 / effect | 是否确认 | 成员范围 | 关联面 | 定义 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `activity.clear` | 删除全部活动记录 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人；儿童须白名单 | [s18](#s18) | [system/policy/cap.c](../../system/policy/cap.c) |
| `activity.list` | 查看 AI 做过的事 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/policy/cap.c](../../system/policy/cap.c) |
| `ai.history.clear` | 清空自己的全部对话记录 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/ai/ai_history.c](../../system/services/ai/ai_history.c) |
| `ai.history.delete` | 删除自己的一条对话记录 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/ai/ai_history.c](../../system/services/ai/ai_history.c) |
| `ai.history.list` | 列出自己的对话记录，每页四条 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/ai/ai_history.c](../../system/services/ai/ai_history.c) |
| `ai.history.next` | 对话记录下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 AiHome | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `ai.history.open` | 打开自己的一条对话记录 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/ai/ai_history.c](../../system/services/ai/ai_history.c) |
| `ai.history.prev` | 对话记录上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 AiHome | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `ai.pages.close` | 继续读 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `ai.pages.next` | 下一页 AI 记录 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `ai.pages.prev` | 上一页 AI 记录 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `ai.pages.retry` | 重新用话筒问 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `ai.panel.hide` | 收起 AI 面板（继续处理） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s49](#s49) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.listen` | 用话筒问这一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s49](#s49) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.next` | 答案下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s49](#s49) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.prev` | 答案上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s49](#s49) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.replay` | 在线重读当前回答 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s49](#s49) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `alarm.cancel` | 删闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.dismiss` | 关掉正在响的闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.list` | 我的闹钟 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.set` | 设闹钟（weekdays 每周几：位 0 是周日；不给就只响一次） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.snooze` | 闹钟稍后再响 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.update` | 修改或开关我的闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `app.cards` | 各应用的服务卡片（和主页看到的一样） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s43](#s43) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.close` | 关掉一个应用（它会保存状态） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s43](#s43) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.list` | 这台设备上有哪些应用，各自的状态 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s43](#s43) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.open` | 打开一个应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s43](#s43) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.page_action` | 操作声明页的控件（输入框无值时聚焦） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 AppPage | [s42](#s42) | [system/ui/app_packages.c](../../system/ui/app_packages.c) |
| `app.page_turn` | 翻声明应用的页面 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 AppPage | [s42](#s42) | [system/ui/app_packages.c](../../system/ui/app_packages.c) |
| `audio.record` | 开始录音，存成文件 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `audio.record_cancel` | 不要这段录音（录了的也删掉） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `audio.record_stop` | 停止录音 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `audio.transcribe` | 把一个录音交给中枢整理成文字 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `backup.manifest` | 查看备份清单、覆盖项目和未恢复项目 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.now` | 备份这位成员的资料 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.open` | 打开备份 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s50](#s50) | [system/ui/shell/backup_pages.c](../../system/ui/shell/backup_pages.c) |
| `backup.status` | 查看这位成员的备份、上传回执和恢复进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `bt.connect` | 连上耳机，声音走耳机 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.disconnect` | 断开耳机，声音回到外放 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.forget` | 忘掉一副耳机 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.list` | 配对过的耳机 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.pair` | 配对一副耳机 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.scan` | 找附近的蓝牙耳机（要几秒） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `calc.eval` | 给一个算式求值（加减乘除、百分号，十进制 8 位小数） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s02](#s02) | [system/apps/calculator/calculator.c](../../system/apps/calculator/calculator.c) |
| `calc.key` | 按计算器的一个键（back 删除一位，neg 正负号，clear 清除） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Calculator | [s02](#s02) | [system/apps/calculator/calculator.c](../../system/apps/calculator/calculator.c) |
| `calendar.list` | 按本地日期列出我的日程和提醒（from 含当天，until 不含当天；日程 ring=false；end 为结束时间，不含） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `calendar.month_next` | 月历下个月 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `calendar.month_prev` | 月历上个月 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `calendar.next` | 当天事项或接下来下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `calendar.open` | 看月历 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `calendar.prev` | 当天事项或接下来上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `calendar.select` | 选择当月一天查看闹钟和提醒 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `call.answer` | 接电话 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.dial` | 打电话 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.dtmf` | 发送一个通话按键 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.hangup` | 挂电话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.mute` | 通话麦克风静音 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.record` | 查看通话录音不可用的原因 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.route` | 切换通话耳机或喇叭 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `camera.capture` | 取一帧画面给 AI 看，不保存 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s27](#s27) | [system/services/camera/camera_service.c](../../system/services/camera/camera_service.c) |
| `camera.photo` | 拍照并保存到存储卡和相册 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 Camera | [s04](#s04) | [system/apps/camera/camera.c](../../system/apps/camera/camera.c) |
| `camera.preview` | 更新相机取景 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；页面 Camera | [s04](#s04) | [system/apps/camera/camera.c](../../system/apps/camera/camera.c) |
| `camera.review` | 查看本次锁屏拍的下一张照片 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；页面 Camera | [s04](#s04) | [system/apps/camera/camera.c](../../system/apps/camera/camera.c) |
| `camera.scan` | 打开扫码，返回类型和内容（不含 Wi-Fi 密码） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s04](#s04) | [system/apps/camera/scan.c](../../system/apps/camera/scan.c) |
| `camera.scan_apply` | 执行扫码结果中的操作 | 人、AI | 高 / external | 人、AI 均在设备确认 | 成人、儿童（无需白名单）；页面 Scan | [s04](#s04) | [system/apps/camera/scan.c](../../system/apps/camera/scan.c) |
| `camera.scan_next` | 扫码结果下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Scan | [s04](#s04) | [system/apps/camera/scan.c](../../system/apps/camera/scan.c) |
| `camera.scan_prev` | 扫码结果上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Scan | [s04](#s04) | [system/apps/camera/scan.c](../../system/apps/camera/scan.c) |
| `caps.domains` | 有哪些类能力（每类多少个） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/policy/cap.c](../../system/policy/cap.c) |
| `caps.find` | 按关键词查能力（带完整描述） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/policy/cap.c](../../system/policy/cap.c) |
| `clipboard.read` | 读剪贴板（用户在设置里打开才行） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `clipboard.write` | 写剪贴板 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `clock.draft` | 选择闹钟的时间、重复日期和开关 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ClockAlarm | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.extra_next` | 下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.extra_prev` | 上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.focus` | 输入闹钟名称 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ClockAlarm | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.new` | 加闹钟 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Clock | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.next` | 闹钟下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Clock | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.open` | 查看闹钟 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Clock | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.prev` | 闹钟上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Clock | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.save` | 保存闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockAlarm | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.stopwatch` | 秒表 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.stopwatch_lap` | 计次 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockStopwatch | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.stopwatch_pause` | 暂停 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockStopwatch | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.stopwatch_reset` | 清零 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockStopwatch | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.stopwatch_start` | 开始 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockStopwatch | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer` | 计时 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Clock | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer_cancel` | 取消计时 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockTimer | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer_dismiss` | 关闭计时响铃 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer_pause` | 暂停计时 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockTimer | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer_set` | 设定计时时长 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockTimer | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.timer_start` | 开始或继续计时 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockTimer | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.world` | 世界时钟 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.world_add` | 加城市 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockWorld | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.world_pick` | 加城市 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ClockWorld | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `clock.world_remove` | 删除 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ClockWorld | [s05](#s05) | [system/apps/clock/clock.c](../../system/apps/clock/clock.c) |
| `confirm.accept` | 确认（只有人在设备上能按） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.list` | 有什么在等人确认 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.page` | 翻阅确认说明（只由人在设备上操作） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s52](#s52) | [system/ui/shell/confirm_card.c](../../system/ui/shell/confirm_card.c) |
| `confirm.reject` | 取消（只有人在设备上能按） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.status` | 查询确认和自动动作的结果 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.withdraw` | 撤回自己发起、还在等确认的请求 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `contacts.delete` | 删除联系人 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.edit` | 打开联系人或添加联系人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.name` | 输入联系人姓名 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ContactEdit | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.next` | 联系人下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 PhoneContacts | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.note` | 输入联系人备注 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ContactEdit | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.number` | 输入联系人号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 ContactEdit | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.open` | 打开联系人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.pick` | 挑选联系人号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.prev` | 联系人上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 PhoneContacts | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.put` | 添加或修改联系人（每人一个号码） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.search` | 搜索联系人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 PhoneContacts | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.select` | 选联系人和号码，返回编号和号码 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `contacts.view` | 查看联系人详情 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/apps/contacts/contacts.c](../../system/apps/contacts/contacts.c) |
| `data.collections` | 有哪些数据集合 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.delete` | 删除一条数据 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.get` | 读一条数据 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.list` | 分页读一个数据集合（默认最新的在前） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.put` | 写一条数据（新的可以撤销） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.status` | 一条数据保存到哪一步了（saving 正在写、on_device 已存在这台设备、at_hub 已备份、failed 没写进去） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `factory.finish` | 清空后开始开机设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s53](#s53) | [system/ui/shell/factory_pages.c](../../system/ui/shell/factory_pages.c) |
| `factory.media` | 选择是否一起清空存储卡上的书和音频 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s53](#s53) | [system/ui/shell/factory_pages.c](../../system/ui/shell/factory_pages.c) |
| `factory.open` | 查看清空这台设备的范围 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s53](#s53) | [system/ui/shell/factory_pages.c](../../system/ui/shell/factory_pages.c) |
| `family.edit` | 编辑家人的称呼或添加家人 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetMembers | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.focus` | 输入家人的称呼 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetMembers | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.kid_apps` | 打开孩子的应用名单 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.kid_member` | 选择下一个孩子的设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.kid_periods` | 打开孩子的禁用时段 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.next` | 家人设置下一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.prev` | 家人设置上一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.role` | 选择添加大人或孩子 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetMembers | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `family.save` | 保存家人的称呼 | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单；页面 SetMembers | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `file.batch` | 按文件编号列表移动或拷贝，可选 all_path 的直属文件 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.cache_clear` | 清理可重新生成的缓存 | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s31](#s31) | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `file.cleanup` | 清理临时文件 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人；儿童须白名单 | [s31](#s31) | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `file.cleanup_preview` | 列出临时文件、缓存和未传完文件可清理的字节数 | 人、AI | 低 / read | 无需 | 成人；儿童须白名单 | [s31](#s31) | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `file.copy` | 拷贝文件或目录（大的在后台做） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.delete` | 删除文件或目录（不能撤销） | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.delete_many` | 删除选中的文件（不能撤销） | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.free` | 存储卡还剩多少空间 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.index` | 按类型分页查询目录树的文件索引 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.list` | 列出目录（/me 自己的文件，/family 全家的内容） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.lookup` | 按稳定编号查询文件 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.mkdir` | 新建目录 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.move` | 移动或改名（可撤销） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.read` | 读文本文件（每次最多 2 KiB） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.stat` | 一个文件或目录的信息 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.thumb` | 按文件编号读取缩略图（gray4 按行排列，每个十六进制字符表示一个灰阶） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.usage` | 读取当前成员和整台设备的书、音频、照片和其他文件字节数 | 人、AI | 低 / read | 无需 | 成人；儿童须白名单 | [s31](#s31) | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `file.write` | 写文本文件（新建、追加，或明确替换 txt） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file_picker.cancel` | 取消选文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 FilePicker | [s44](#s44) | [system/ui/file_picker.c](../../system/ui/file_picker.c) |
| `file_picker.next` | 下一页文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 FilePicker | [s44](#s44) | [system/ui/file_picker.c](../../system/ui/file_picker.c) |
| `file_picker.prev` | 上一页文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 FilePicker | [s44](#s44) | [system/ui/file_picker.c](../../system/ui/file_picker.c) |
| `file_picker.select` | 选择文件编号 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 FilePicker | [s44](#s44) | [system/ui/file_picker.c](../../system/ui/file_picker.c) |
| `files.all` | 选择当前文件夹的全部文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.browse` | 浏览文件夹 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.edit` | 编辑当前 txt 文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.mark` | 选择或取消选择文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.move` | 选移动目标文件夹 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.multi` | 进入或退出文件多选 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.name` | 编辑文件名 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.new` | 新建文件夹或文本 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.next` | 文件下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.prev` | 文件上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.preview` | 预览当前文本文件开头 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.select` | 按编号查看文件操作 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `files.up` | 返回文件上一级 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/apps/files/files.c](../../system/apps/files/files.c) |
| `gallery.next` | 相册下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Gallery | [s08](#s08) | [system/apps/gallery/gallery.c](../../system/apps/gallery/gallery.c) |
| `gallery.prev` | 相册上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Gallery | [s08](#s08) | [system/apps/gallery/gallery.c](../../system/apps/gallery/gallery.c) |
| `gallery.reload` | 手动重读当前家人的照片 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Gallery | [s08](#s08) | [system/apps/gallery/gallery.c](../../system/apps/gallery/gallery.c) |
| `games.memory.flip` | 翻开一张牌，编号从0到15，按行排列 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s11](#s11) | [system/apps/memory/memory.c](../../system/apps/memory/memory.c) |
| `games.memory.reset` | 重新洗牌开始记忆翻牌 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s11](#s11) | [system/apps/memory/memory.c](../../system/apps/memory/memory.c) |
| `games.memory.start` | 开始4×4记忆翻牌，隐藏牌面不暴露给AI | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s11](#s11) | [system/apps/memory/memory.c](../../system/apps/memory/memory.c) |
| `games.memory.state` | 读取可见牌面、步数、秒数与当前家人最好成绩 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s11](#s11) | [system/apps/memory/memory.c](../../system/apps/memory/memory.c) |
| `games.move` | 五子棋落子，行列从0开始；人机时自动接电脑的一手 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `games.reset` | 按当前模式和难度重开五子棋 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `games.snake.pause` | 暂停贪吃蛇 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.snake.reset` | 重开贪吃蛇 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.snake.resume` | 继续贪吃蛇并打开页面 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.snake.start` | 开始每半秒一步的贪吃蛇 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.snake.state` | 读取12×12蛇身、食物、分数与当前家人最高分 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.snake.turn` | 贪吃蛇转向，0上1右2下3左，每步至多转一次 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `games.start` | 开始五子棋或继续上一局，难度0简单1普通2困难，黑棋先手 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `games.state` | 读取五子棋棋盘（按行0空1黑2白）、回合、合法着法数与胜负 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `games.undo` | 悔棋，人机模式退回到人的回合 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `home.next` | 主页下一屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `home.prev` | 主页上一屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `interrupt.ack` | 知道了（关掉低电量提示） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s35](#s35) | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `interrupt.list` | 现在有什么在打断（来电、闹钟、低电量），各有什么按钮 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s35](#s35) | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `job.cancel` | 停下一件后台在做的事 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s37](#s37) | [system/services/res/task.c](../../system/services/res/task.c) |
| `job.list` | 后台在做什么、做到哪了、做完的结果 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s37](#s37) | [system/services/res/task.c](../../system/services/res/task.c) |
| `keyboard.candidates` | 展开下一页候选，或 fold 收回键盘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s54](#s54) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.edit` | 编辑选中内容：0 左、1 右、2 复制、3 剪切、4 粘贴、5 撤销 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s54](#s54) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.key` | 按输入法的键（1 符号、2 大写、8 删除、10 回车，其余是 ASCII） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s54](#s54) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.pick` | 选输入法候选（从 0 开始） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s54](#s54) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `kid.app_set` | 设置孩子能用的应用 | 人、AI | 中 / local_write | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人；儿童须白名单 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `kid.period_set` | 设置孩子的禁用时段 | 人、AI | 中 / local_write | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人；儿童须白名单 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `kid.policy_get` | 查看孩子的应用名单和禁用时段 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `media.pause` | 暂停 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.play` | 放一个音频文件（Ogg Opus），不给位置就从上次听到的地方 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.resume` | 接着放 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.seek` | 跳到某个位置 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.status` | 现在放的是什么，到哪儿了 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.stop` | 停止 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.add` | 加一位家人 | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.list` | 家里有哪些人 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.remove` | 删除一位家人和他们的全部数据（不能撤销） | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.rename` | 修改家人的称呼 | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.set_pin` | 设置或修改密码（只能在设备上） | 仅人（设备） | 中 / local_write | 无需 | 成人；儿童须白名单 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.time_add` | 给孩子今天加时间 | 人、AI | 中 / local_write | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人、儿童（无需白名单） | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.time_get` | 查看孩子今天的用时和每天的时长 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.time_set` | 改孩子每天能用多久 | 人、AI | 中 / local_write | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人、儿童（无需白名单） | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `member.unlock` | 输密码进入家人的空间（只能在设备上） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `memo.check` | 勾选或取消清单项 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.convert` | 逐行转成清单（最多 32 项） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.delete` | 删除备忘（可撤销） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.edit` | 写一条或编辑备忘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.next` | 备忘下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.open` | 查看备忘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.play` | 播放语音备忘的录音 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.prev` | 备忘上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.record` | 说一句：录一条语音备忘（联网后整理成文字） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.retry` | 重试记下备忘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.save` | 记下当前成员的长文草稿 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.undo` | 撤销删除备忘 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `memo.voice` | 把录好的音存成语音备忘，或填上中枢整理的文字 | 仅人（设备） | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/apps/memo/memo.c](../../system/apps/memo/memo.c) |
| `message.list` | 最近的短信 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `message.send` | 发短信 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `music.favorite` | 收藏或取消收藏当前歌曲 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.list` | 音乐列表和文件夹 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.next` | 下一首 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.now` | 正在播放的曲目和进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.open` | 打开音乐 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.pause` | 暂停音乐 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.play` | 播放某首、文件夹或继续播放 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.prev` | 上一首 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `music.seek` | 跳到音乐的毫秒位置 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/apps/music/music.c](../../system/apps/music/music.c) |
| `net.apn_set` | 设置接入点并重连移动数据 | 人、AI | 高 / system | 无需 | 仅成人 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `net.roaming_set` | 开关数据漫游 | 人、AI | 高 / system | 无需 | 仅成人 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `net.set_airplane` | 开关飞行模式（关掉 4G、Wi-Fi 和蓝牙，之后可以单独再开 Wi-Fi 和蓝牙） | 人、AI | 高 / system | 无需 | 仅成人 | [s34](#s34) | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.usage` | 移动流量和接入点（不含密码） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `net.usage_reset` | 清零流量统计 | 人、AI | 高 / system | 人、AI 均在设备确认 | 仅成人 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `notify.channels` | 哪些通知渠道开着 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.clear` | 清空通知 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.dismiss` | 删一条通知 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.list` | 通知列表（新的在前） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.post` | 发一条通知（给用户看的提醒；来源记为 AI） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.read` | 标为已读（id 0 或不给：全部） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.set_channel` | 开关一个通知渠道 | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `pairing.accept` | 确认加入屏幕上的家庭 | 仅人（设备） | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.configure` | 设置家庭服务器地址和公钥 | 人、AI | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.hotspot_start` | 开启设备设置热点 | 人、AI | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.hotspot_stop` | 关闭设备设置热点 | 人、AI | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.import` | 从存储卡导入配对文件 | 人、AI | 高 / system | 人、AI 均在设备确认 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.reject` | 取消加入这个家庭 | 人、AI | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.restart` | 重新加入家庭 | 人、AI | 高 / system | 无需 | 仅成人 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.status` | 查看加入家庭的进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `phone.contacts` | 从联系人选择号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.emergency` | 拨紧急电话 | 仅人（设备） | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `phone.emergency_close` | 返回锁屏 | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.emergency_hangup` | 挂断紧急电话 | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `phone.emergency_key` | 按紧急拨号键 | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.emergency_open` | 打开紧急呼叫页面 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时可打开公开页面（不含成员数据） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.key` | 按拨号键或删除一位 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Phone | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.keypad` | 打开或收起通话键盘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.next` | 通话记录下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 PhoneRecents | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.number` | 输入电话号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Phone | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.open` | 打开拨号盘，可填入号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.prev` | 通话记录上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 PhoneRecents | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.recents` | 打开最近通话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `phone.return` | 回到通话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `photo.ask` | 把当前有权查看的照片缩略图交给 AI | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Photo | [s08](#s08) | [system/apps/gallery/gallery.c](../../system/apps/gallery/gallery.c) |
| `photo.open` | 查看有权访问的图片文件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/apps/gallery/gallery.c](../../system/apps/gallery/gallery.c) |
| `power.set_saving` | 开关省电（最多 1 分钟锁屏，不改原设置） | 人、AI | 高 / system | 无需 | 仅成人 | [s36](#s36) | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `reader.add_bookmark` | 加书签 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.bookmarks` | 列出书签 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.chapters` | 列出目录 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.continue` | 接着读上次的书 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.get_layout` | 查看阅读排版 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.goto_bookmark` | 跳到书签 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.goto_chapter` | 跳到章节 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.goto_page` | 跳到第几页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.list_books` | 书架上有哪些书 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.list_next` | 书架或目录下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.list_prev` | 书架或目录上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.menu` | 阅读菜单 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.next_page` | 下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.open_book` | 打开一本书 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.open_sample` | 打开离线原创阅读示例 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.prev_page` | 上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.read_page` | 读出这一页 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.remove_bookmark` | 删书签 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.set_layout` | 修改阅读排版 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.show_chapters` | 打开目录 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reminder.add` | 加提醒或日程（at/end 本地时间 YYYY-MM-DD HH:MM；ring 默认 true，false 为日程；all_day 全天，end 不含；repeat: none daily weekly workdays monthly yearly lunar_yearly） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.cancel` | 删提醒 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.draft` | 选择提醒时间和重复 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.focus` | 输入提醒内容 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.list` | 我的提醒 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.new` | 手动加提醒 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.open` | 查看和修改我的提醒 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.save` | 保存提醒 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；页面 ReminderEdit | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.time` | 选择提醒开始或结束时间 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/apps/calendar/calendar.c](../../system/apps/calendar/calendar.c) |
| `reminder.update` | 修改我的提醒或日程（ring/all_day/end 可选；end 空串清除，不传则保留时长） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `remote.address` | 输入网址 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.close` | 关闭网页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.focus` | 输入网页文字 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.next` | 网页下一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.open` | 打开网页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.pointer` | 点按或长按网页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.prev` | 网页上一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.retry` | 重新打开网页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `remote.submit` | 完成网页输入 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 remote | [s14](#s14) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `restore.apply` | 按预览覆盖这位成员的资料 | 人、AI | 高 / local_destructive | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人、儿童（无需白名单） | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `restore.preview` | 预览恢复这位成员的备份和将被替换的资料 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `search.focus` | 输入搜索关键词 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `search.next` | 搜索下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `search.open` | 打开当前页搜索结果 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `search.prev` | 搜索上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `search.query` | 分页搜索当前家人的应用、设置、联系人、备忘和文件 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `search.show` | 打开全局搜索 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s45](#s45) | [system/ui/search.c](../../system/ui/search.c) |
| `settings.get` | 查看一项设置 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/policy/settings.c](../../system/policy/settings.c) |
| `settings.list` | 所有设置，各自的类型、取值范围和现在的值 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `settings.set` | 改一项设置 | 人、AI | 中 / local_write | 按设置项（见设置表） | 成人、儿童（无需白名单）；设置写入另受设置项限制 | [s21](#s21) | [system/policy/settings.c](../../system/policy/settings.c) |
| `shell.about` | 打开关于和更新 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.ai_ask` | 输入问题 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `shell.ai_home` | 打开对话记录 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `shell.ai_key` | 按住说话或点按听 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `shell.ai_log` | 查看 AI 做过的事 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `shell.ai_memory` | 查看 AI 记得的事 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.ai_receipts` | 查看 AI 做事的结果 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s48](#s48) | [system/ui/shell/ai_pages.c](../../system/ui/shell/ai_pages.c) |
| `shell.ai_send` | 发送输入框里的问题 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 AiAsk | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `shell.ai_stop` | 停下 AI 的朗读，保留文字 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s47](#s47) | [system/ui/shell/ai_chat.c](../../system/ui/shell/ai_chat.c) |
| `shell.back` | 返回上一层 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.bluetooth` | 打开蓝牙耳机 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s51](#s51) | [system/ui/shell/bluetooth_page.c](../../system/ui/shell/bluetooth_page.c) |
| `shell.bluetooth_next` | 耳机下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s51](#s51) | [system/ui/shell/bluetooth_page.c](../../system/ui/shell/bluetooth_page.c) |
| `shell.bluetooth_prev` | 耳机上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s51](#s51) | [system/ui/shell/bluetooth_page.c](../../system/ui/shell/bluetooth_page.c) |
| `shell.close` | 收起面板 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.control` | 打开控制中心 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.device` | 查看设备详情 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.display` | 显示和亮度 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.family` | 管理家人 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.icon_menu` | 应用快捷动作 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s64](#s64) | [system/ui/shell/today_edit.c](../../system/ui/shell/today_edit.c) |
| `shell.kid_add` | 请家长给今天加十五分钟 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.kid_allow` | 家长只允许这一次打开应用 | 人、AI | 低 / navigate | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.kid_cancel` | 算了，回到刚才的地方 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.kid_parent` | 请家长输入密码放行这一次 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.kid_phone` | 时间到后打电话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.kid_settings` | 打开孩子的设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.kid_sms` | 时间到后看短信 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s55](#s55) | [system/ui/shell/kid_pages.c](../../system/ui/shell/kid_pages.c) |
| `shell.known_networks` | 已存的网络 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.launch` | 打开主页上的应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.license` | 阅读一项开源许可 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.licenses` | 查看开源许可 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.lock` | 锁屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.lock_faces` | 选择锁屏表盘和小组件 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s67](#s67) | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `shell.member_choose` | 选择下一位家人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.member_select` | 选择这位家人（仅本人操作，密码仍需验证） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s56](#s56) | [system/ui/shell/member_gate.c](../../system/ui/shell/member_gate.c) |
| `shell.members` | 选择家人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.members_page` | 翻选人页面（只由人在设备上操作） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s56](#s56) | [system/ui/shell/member_gate.c](../../system/ui/shell/member_gate.c) |
| `shell.mobile` | 移动网络 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s58](#s58) | [system/ui/shell/mobile_network.c](../../system/ui/shell/mobile_network.c) |
| `shell.mobile_edit` | 手动接入点 | 仅人（设备） | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetMobile | [s58](#s58) | [system/ui/shell/mobile_network.c](../../system/ui/shell/mobile_network.c) |
| `shell.mobile_save` | 保存接入点 | 仅人（设备） | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetMobile | [s58](#s58) | [system/ui/shell/mobile_network.c](../../system/ui/shell/mobile_network.c) |
| `shell.more` | 更多控制 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.network` | 网络和 SIM 卡 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notifications` | 查看通知 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_next` | 通知下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_open` | 打开通知并标为已读 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_prev` | 通知上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_settings` | 通知渠道设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.page_next` | 设置页的下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.page_prev` | 设置页的上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.pin_cancel` | 收起密码页面 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.pin_forgot` | 忘记密码时请家长帮助 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.pin_parent` | 请家长在设备上输入密码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.pin_set` | 打开设置或修改密码 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.pin_submit` | 本人提交设备上的密码 | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s57](#s57) | [system/ui/shell/member_pages.c](../../system/ui/shell/member_pages.c) |
| `shell.privacy` | 打开隐私 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.records` | 查看使用记录 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.resume` | 接着读 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.return` | 离开多久要重新验证 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.runtime_records` | 查看运行记录 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.search` | 查找应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.settings` | 打开设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.settings_next` | 设置下一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.settings_prev` | 设置上一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.sound` | 声音 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.storage` | 打开存储和备份设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.tasks` | 查看正在做的事和做好了的事 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s46](#s46) | [system/ui/shell/activity_card.c](../../system/ui/shell/activity_card.c) |
| `shell.time` | 打开日期和时间 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.time_edit` | 手动设置日期和时间 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.time_next` | 时区下一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetTimeZone | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.time_prev` | 时区上一页 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetTimeZone | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.time_step` | 调整日期时间草稿 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单；页面 SetDateTime | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.time_zones` | 选择城市或地区 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s63](#s63) | [system/ui/shell/time_settings.c](../../system/ui/shell/time_settings.c) |
| `shell.today_card` | 选择今日卡片或上移 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s64](#s64) | [system/ui/shell/today_edit.c](../../system/ui/shell/today_edit.c) |
| `shell.today_edit` | 今日屏放什么 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s64](#s64) | [system/ui/shell/today_edit.c](../../system/ui/shell/today_edit.c) |
| `shell.today_next` | 今日卡片下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s64](#s64) | [system/ui/shell/today_edit.c](../../system/ui/shell/today_edit.c) |
| `shell.today_prev` | 今日卡片上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s64](#s64) | [system/ui/shell/today_edit.c](../../system/ui/shell/today_edit.c) |
| `shell.update` | 打开系统更新 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s66](#s66) | [system/ui/shell/update_pages.c](../../system/ui/shell/update_pages.c) |
| `shell.update_close` | 返回更新前的页面 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s66](#s66) | [system/ui/shell/update_pages.c](../../system/ui/shell/update_pages.c) |
| `shell.update_notes_page` | 翻阅完整更新说明 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s66](#s66) | [system/ui/shell/update_pages.c](../../system/ui/shell/update_pages.c) |
| `shell.voice` | 打开语音和 AI 设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s62](#s62) | [system/ui/shell/settings_pages.c](../../system/ui/shell/settings_pages.c) |
| `shell.wallpaper` | 打开锁屏和壁纸 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s61](#s61) | [system/ui/shell/settings_family.c](../../system/ui/shell/settings_family.c) |
| `shell.wifi_next` | 附近网络下一组 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.wifi_pick` | 选附近网络（密码由人在设备上填写） | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.wifi_prev` | 附近网络上一组 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s59](#s59) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `sms.compose` | 写短信或回复当前会话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.long` | 打开短信长文编辑页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.next` | 短信下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.pick` | 打开短信联系人选择 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 SmsCompose | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.prev` | 短信上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.recipient` | 从联系人选择收件人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 SmsCompose | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.retry` | 重新编辑明确未发出的短信 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 SmsThread | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.send` | 保存并发送短信 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `sms.text` | 输入短信 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.thread` | 打开短信会话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `sms.to` | 输入短信收件人号码 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 SmsCompose | [s15](#s15) | [system/apps/sms/sms.c](../../system/apps/sms/sms.c) |
| `state.list` | 系统现在的状态（电量、网络、前台应用……） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.export_logs` | 导出日志 | 仅人（设备） | 中 / local_write | 无需 | 成人；儿童须白名单 | [s33](#s33) | [system/services/log/log.c](../../system/services/log/log.c) |
| `system.factory_reset` | 清空这台设备（清掉所有家人的数据，不能撤销） | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.full_refresh` | 刷新屏幕 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.home` | 回到主页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.reboot` | 重启 | 人、AI | 高 / system | 无需 | 成人；儿童仅人工；锁定时仅人工入口 | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.report_problem` | 记录问题 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s33](#s33) | [system/services/log/log.c](../../system/services/log/log.c) |
| `system.set_volume` | 调音量 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.shutdown` | 关机 | 人、AI | 高 / system | 无需 | 成人；儿童仅人工；锁定时仅人工入口 | [s36](#s36) | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `system.switch_member` | 切换家人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.undo` | 撤销刚才的操作 | 人、AI | 中 / local_write | 设置撤销按设置项重新判断（见设置表）；其他撤销无需 | 成人、儿童（无需白名单）；设置撤销另受设置项限制 | [s18](#s18) | [system/policy/cap.c](../../system/policy/cap.c) |
| `system.upgrade` | 安装下载好的新系统（安装时不能用设备，会重启） | 仅人（设备） | 高 / system | 人在设备确认 | 仅成人 | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `system.upgrade_from_card` | 安装存储卡上的新系统（update 文件夹里，会重启） | 仅人（设备） | 高 / system | 人在设备确认 | 仅成人 | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `tasks.next` | 正在做的事下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Tasks | [s46](#s46) | [system/ui/shell/activity_card.c](../../system/ui/shell/activity_card.c) |
| `tasks.prev` | 正在做的事上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 Tasks | [s46](#s46) | [system/ui/shell/activity_card.c](../../system/ui/shell/activity_card.c) |
| `text.add_word` | 把选中的文字加入单词 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.ask_ai` | 问 AI 选中的文字 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.insert` | 往有焦点的文本框里写字（等于替用户打字） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.page` | 翻长文的文字页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.redo` | 把撤销的改回来 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.select` | 放光标或选一段字（按字数算；不给 end 就是放光标。text.insert 写在这里，选中的被替换） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.state` | 现在有没有文本框在输入，里面是什么（密码不给看） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.submit` | 按回车（完成、发送、搜索） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.undo` | 撤销文本框里刚才的修改 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_cancel` | 取消语音输入，什么都不写进去 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_finish` | 说完了，结束语音输入 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_start` | 打开语音输入：说的话变成文字进文本框 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `time.almanac` | 老黄历（日期不给就是系统时区的今天，民俗说法） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `time.lunar` | 公历和农历互查（不给日期就是今天） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `time.now` | 现在几点（含时区和农历） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `time.set` | 改系统时间 | 人、AI | 高 / system | 无需 | 仅成人 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `transfer.open` | 打开从手机或电脑传入页面 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s65](#s65) | [system/ui/shell/transfer_pages.c](../../system/ui/shell/transfer_pages.c) |
| `transfer.start` | 开启一次无线传文件授权（两分钟内有效） | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s31](#s31) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `transfer.status` | 查看无线传文件授权和结果 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `transfer.stop` | 关闭无线传文件 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `transfer.usb` | 打开连电脑页面 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s65](#s65) | [system/ui/shell/transfer_pages.c](../../system/ui/shell/transfer_pages.c) |
| `update.check` | 问家庭服务器有没有新版本 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.download` | 把新版本下载到存储卡（不安装） | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.status` | 系统版本和升级进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `usb.mount` | 把存储卡借给电脑当 U 盘 | 人、AI | 高 / system | 人、AI 均在设备确认 | 仅成人 | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `usb.status` | 是否连着电脑、存储卡是否借给了电脑 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `usb.unmount` | 从电脑收回存储卡 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `wallpaper.choose` | 选择锁屏壁纸 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s67](#s67) | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `weather.city` | 设置天气城市 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `weather.find` | 按名字找天气城市 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `weather.focus` | 在天气城市页输入城市名 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 WeatherCity | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `weather.get` | 查天气：现在、今天和之后几天，没联网时给上次的 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `weather.open` | 打开天气 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `weather.set_city` | 从找到的城市里选一个当天气城市 | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s17](#s17) | [system/apps/weather/weather.c](../../system/apps/weather/weather.c) |
| `wifi.connect` | 连接一个 Wi-Fi 并记住它 | 人、AI | 高 / system | 无需 | 仅成人 | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.forget` | 忘掉一个 Wi-Fi | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人；儿童须白名单 | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.list` | 这台设备记得的 Wi-Fi（只有名字） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.restore` | 撤销忘掉网络 | 仅人（设备） | 中 / local_write | 无需 | 成人；儿童须白名单 | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.scan` | 找附近的 Wi-Fi（要几秒） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.set_enabled` | 开关 Wi-Fi | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s34](#s34) | [system/services/net/net_service.c](../../system/services/net/net_service.c) |

## 关联的设置、状态和事件

<a id="s01"></a>

### s01 — `apps/reader`

设置：`reader.font_size`、`reader.footer_mask`、`reader.line_spacing`、`reader.margins`、`reader.paragraph_spacing`。

状态：`battery.level`、`storage.card`、`time.minute`。

事件：—。

<a id="s02"></a>

### s02 — `system/apps/calculator`

设置：—。

状态：—。

事件：—。

<a id="s03"></a>

### s03 — `system/apps/calendar`

设置：—。

状态：`alarm.revision`、`text.focused`。

事件：—。

<a id="s04"></a>

### s04 — `system/apps/camera`

设置：—。

状态：—。

事件：—。

<a id="s05"></a>

### s05 — `system/apps/clock`

设置：—。

状态：`alarm.revision`。

事件：`time.changed`。

<a id="s06"></a>

### s06 — `system/apps/contacts`

设置：—。

状态：—。

事件：`contacts.selected`。

<a id="s07"></a>

### s07 — `system/apps/files`

设置：—。

状态：—。

事件：`file.batch_progress`、`file.changed`。

<a id="s08"></a>

### s08 — `system/apps/gallery`

设置：—。

状态：—。

事件：`file.changed`。

<a id="s09"></a>

### s09 — `system/apps/gomoku`

设置：—。

状态：—。

事件：`games.changed`。

<a id="s10"></a>

### s10 — `system/apps/memo`

设置：—。

状态：`net.online`。

事件：`audio.record_done`。

<a id="s11"></a>

### s11 — `system/apps/memory`

设置：—。

状态：—。

事件：`games.memory.changed`。

<a id="s12"></a>

### s12 — `system/apps/music`

设置：`audio.volume`。

状态：—。

事件：`media.finished`。

<a id="s13"></a>

### s13 — `system/apps/phone`

设置：—。

状态：`audio.route`、`bt.connected`、`time.minute`。

事件：`call.answered`、`call.connected`、`call.controls`、`call.dialed`、`call.ended`、`call.incoming`、`contacts.selected`。

<a id="s14"></a>

### s14 — `system/apps/remote`

设置：—。

状态：`net.online`、`power.state`。

事件：—。

<a id="s15"></a>

### s15 — `system/apps/sms`

设置：—。

状态：—。

事件：`contacts.selected`、`message.received`。

<a id="s16"></a>

### s16 — `system/apps/snake`

设置：—。

状态：—。

事件：`games.snake.changed`。

<a id="s17"></a>

### s17 — `system/apps/weather`

设置：—。

状态：`net.online`。

事件：—。

<a id="s18"></a>

### s18 — `system/policy/cap`

设置：—。

状态：—。

事件：`confirm.result`。

<a id="s19"></a>

### s19 — `system/policy/confirm`

设置：—。

状态：`confirm.pending`、`interrupt.active`、`member.current`、`power.state`。

事件：`confirm.result`。

<a id="s20"></a>

### s20 — `system/policy/members`

设置：`member.idle_return_s`。

状态：`app.foreground`、`member.authed`、`member.current`、`member.time_up`。

事件：`member.changed`。

<a id="s21"></a>

### s21 — `system/policy/settings`

设置：`ai.read_clipboard`、`audio.alarm_ring`、`audio.call_ring`、`audio.child_max`、`audio.key_tone`、`audio.vibration`、`audio.vibration_level`、`audio.volume`、`backup.auto`、`bt.enabled`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_background`、`display.lock_reading`、`display.lock_style`、`display.lock_todo`、`display.wallpaper_file`、`display.warmth`、`member.idle_return_s`、`net.airplane`、`net.prefer`、`net.roaming`、`notify.dnd`、`power.lift_wake`、`power.lock_after_s`、`power.saving`、`reader.font_size`、`reader.footer_mask`、`reader.line_spacing`、`reader.margins`、`reader.paragraph_spacing`、`system.language`、`text.assist`、`text.google`、`text.nine_key`、`time.auto`、`time.zone`、`time.zone_auto`、`update.auto_download`、`voice.handsfree`、`voice.interrupt`、`voice.name`、`wifi.enabled`。

状态：—。

事件：—。

<a id="s22"></a>

### s22 — `system/policy/system_actions`

设置：`ai.read_clipboard`、`audio.alarm_ring`、`audio.call_ring`、`audio.child_max`、`audio.key_tone`、`audio.vibration`、`audio.vibration_level`、`audio.volume`、`backup.auto`、`bt.enabled`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_background`、`display.lock_reading`、`display.lock_style`、`display.lock_todo`、`display.wallpaper_file`、`display.warmth`、`member.idle_return_s`、`net.airplane`、`net.prefer`、`net.roaming`、`notify.dnd`、`power.lift_wake`、`power.lock_after_s`、`power.saving`、`reader.font_size`、`reader.footer_mask`、`reader.line_spacing`、`reader.margins`、`reader.paragraph_spacing`、`system.language`、`text.assist`、`text.google`、`text.nine_key`、`time.auto`、`time.zone`、`time.zone_auto`、`update.auto_download`、`voice.handsfree`、`voice.interrupt`、`voice.name`、`wifi.enabled`。

状态：`alarm.revision`、`alarm.ringing`、`app.cards_rev`、`app.foreground`、`audio.listening`、`audio.mic`、`audio.now_playing`、`audio.recording`、`audio.route`、`battery.charging`、`battery.level`、`battery.low`、`bt.available`、`bt.connected`、`bt.device`、`cell.config_readonly`、`cell.registered`、`cell.roaming`、`cell.signal`、`cell.sim`、`cell.sim_missing`、`confirm.pending`、`file.index_state`、`hub.online`、`interrupt.active`、`job.revision`、`job.running`、`media.state`、`member.authed`、`member.current`、`member.time_up`、`net.mobile_rev`、`net.online`、`net.signal`、`net.type`、`notify.unread`、`pairing.state`、`power.lifted`、`power.saving`、`power.state`、`storage.card`、`storage.free`、`storage.mode`、`storage.user_free`、`text.focused`、`text.voice`、`time.minute`、`time.synced`、`time.zone`、`update.progress`、`update.state`、`update.version`、`usb.connected`、`wifi.ssid`、`wifi.state`。

事件：—。

<a id="s23"></a>

### s23 — `system/services/ai`

设置：`net.airplane`、`voice.handsfree`、`voice.interrupt`。

状态：`app.foreground`、`member.authed`、`member.current`、`net.online`、`power.state`。

事件：`ai.history.changed`。

<a id="s24"></a>

### s24 — `system/services/audio`

设置：`audio.alarm_ring`、`audio.call_ring`、`audio.child_max`、`audio.key_tone`、`audio.vibration`、`audio.vibration_level`、`audio.volume`、`voice.handsfree`、`voice.interrupt`、`voice.name`。

状态：`audio.listening`、`audio.mic`、`audio.now_playing`、`audio.recording`、`audio.route`、`interrupt.active`、`media.state`、`member.authed`、`member.current`。

事件：`audio.record_done`、`media.finished`。

<a id="s25"></a>

### s25 — `system/services/backup`

设置：`backup.auto`。

状态：`battery.charging`、`net.online`、`net.type`。

事件：`backup.changed`、`backup.failed`。

<a id="s26"></a>

### s26 — `system/services/bt`

设置：`bt.enabled`。

状态：`bt.available`、`bt.connected`、`bt.device`。

事件：`bt.changed`。

<a id="s27"></a>

### s27 — `system/services/camera`

设置：—。

状态：—。

事件：—。

<a id="s28"></a>

### s28 — `system/services/comm`

设置：`cell.sim_slot`、`net.airplane`、`net.roaming`。

状态：`bt.connected`、`cell.config_readonly`、`cell.registered`、`cell.roaming`、`cell.signal`、`cell.sim`、`cell.sim_missing`、`net.mobile_rev`、`time.minute`。

事件：`call.answered`、`call.connected`、`call.controls`、`call.dialed`、`call.ended`、`call.incoming`、`message.received`。

<a id="s29"></a>

### s29 — `system/services/data`

设置：—。

状态：`battery.low`、`storage.user_free`。

事件：`battery.low`、`data.write_failed`。

<a id="s30"></a>

### s30 — `system/services/display`

设置：`display.light`、`display.warmth`。

状态：`power.state`。

事件：—。

<a id="s31"></a>

### s31 — `system/services/file`

设置：`net.airplane`。

状态：`file.index_state`、`member.current`、`storage.card`、`storage.free`、`storage.mode`、`usb.connected`。

事件：`file.batch_progress`、`file.changed`、`file.job_done`、`member.changed`、`usb.changed`。

<a id="s32"></a>

### s32 — `system/services/hub`

设置：—。

状态：`hub.online`、`member.authed`、`member.current`、`pairing.state`。

事件：`pairing.changed`。

<a id="s33"></a>

### s33 — `system/services/log`

设置：—。

状态：`battery.charging`、`battery.low`、`confirm.pending`、`member.current`、`net.type`、`power.state`。

事件：`battery.low`。

<a id="s34"></a>

### s34 — `system/services/net`

设置：`bt.enabled`、`net.airplane`、`net.prefer`、`net.roaming`、`wifi.enabled`。

状态：`cell.roaming`、`net.online`、`net.signal`、`net.type`、`wifi.ssid`、`wifi.state`。

事件：`wifi.changed`。

<a id="s35"></a>

### s35 — `system/services/notify`

设置：`notify.dnd`。

状态：`battery.low`、`interrupt.active`、`member.current`、`notify.unread`。

事件：`alarm.ended`、`alarm.fired`、`alarm.missed`、`battery.low`、`call.answered`、`call.ended`、`call.incoming`、`data.write_failed`、`message.received`、`notify.changed`、`notify.channels_changed`、`notify.posted`、`reminder.missed`、`usb.changed`。

<a id="s36"></a>

### s36 — `system/services/power`

设置：`power.lift_wake`、`power.lock_after_s`、`power.saving`。

状态：`battery.charging`、`battery.level`、`battery.low`、`power.lifted`、`power.saving`、`power.state`。

事件：`battery.low`。

<a id="s37"></a>

### s37 — `system/services/res`

设置：—。

状态：`job.revision`、`job.running`、`net.online`。

事件：`job.finished`。

<a id="s38"></a>

### s38 — `system/services/text`

设置：`ai.read_clipboard`、`text.assist`、`text.google`、`text.nine_key`。

状态：`member.authed`、`member.current`、`power.state`、`text.focused`、`text.voice`。

事件：`text.voice_ended`。

<a id="s39"></a>

### s39 — `system/services/time`

设置：`time.auto`、`time.zone`、`time.zone_auto`。

状态：`alarm.revision`、`alarm.ringing`、`time.minute`、`time.synced`、`time.zone`。

事件：`alarm.ended`、`alarm.fired`、`alarm.missed`、`reminder.fired`、`reminder.missed`、`time.changed`。

<a id="s40"></a>

### s40 — `system/services/update`

设置：`update.auto_download`。

状态：`net.type`、`storage.card`、`update.progress`、`update.state`、`update.version`。

事件：`update.available`、`update.result`、`usb.changed`。

<a id="s41"></a>

### s41 — `system/text/i18n`

设置：`system.language`。

状态：—。

事件：—。

<a id="s42"></a>

### s42 — `system/ui/app_packages`

设置：—。

状态：`app.foreground`、`storage.card`、`storage.mode`。

事件：`file.changed`。

<a id="s43"></a>

### s43 — `system/ui/app_registry`

设置：—。

状态：`alarm.revision`、`app.cards_rev`、`app.foreground`、`member.authed`、`member.current`。

事件：`app.state`。

<a id="s44"></a>

### s44 — `system/ui/file_picker`

设置：—。

状态：—。

事件：—。

<a id="s45"></a>

### s45 — `system/ui/search`

设置：—。

状态：—。

事件：—。

<a id="s46"></a>

### s46 — `system/ui/shell/activity_card`

设置：—。

状态：`audio.now_playing`、`job.revision`、`media.state`。

事件：`job.finished`。

<a id="s47"></a>

### s47 — `system/ui/shell/ai_chat`

设置：—。

状态：—。

事件：`ai.history.changed`。

<a id="s48"></a>

### s48 — `system/ui/shell/ai_pages`

设置：`net.airplane`。

状态：—。

事件：—。

<a id="s49"></a>

### s49 — `system/ui/shell/ai_panel`

设置：—。

状态：`audio.now_playing`、`hub.online`。

事件：—。

<a id="s50"></a>

### s50 — `system/ui/shell/backup_pages`

设置：`backup.auto`。

状态：`net.online`。

事件：—。

<a id="s51"></a>

### s51 — `system/ui/shell/bluetooth_page`

设置：`bt.enabled`。

状态：`bt.device`。

事件：`bt.changed`。

<a id="s52"></a>

### s52 — `system/ui/shell/confirm_card`

设置：—。

状态：`confirm.pending`、`interrupt.active`、`power.state`。

事件：`confirm.result`。

<a id="s53"></a>

### s53 — `system/ui/shell/factory_pages`

设置：—。

状态：—。

事件：—。

<a id="s54"></a>

### s54 — `system/ui/shell/keyboard`

设置：`text.nine_key`。

状态：—。

事件：—。

<a id="s55"></a>

### s55 — `system/ui/shell/kid_pages`

设置：—。

状态：`app.cards_rev`、`app.foreground`、`power.state`。

事件：—。

<a id="s56"></a>

### s56 — `system/ui/shell/member_gate`

设置：—。

状态：`member.authed`、`power.state`。

事件：—。

<a id="s57"></a>

### s57 — `system/ui/shell/member_pages`

设置：—。

状态：`power.state`。

事件：—。

<a id="s58"></a>

### s58 — `system/ui/shell/mobile_network`

设置：`net.roaming`。

状态：`cell.config_readonly`、`net.mobile_rev`。

事件：—。

<a id="s59"></a>

### s59 — `system/ui/shell/navigation`

设置：`display.font_size`、`net.airplane`、`system.language`。

状态：`app.cards_rev`、`audio.mic`、`bt.connected`、`cell.config_readonly`、`cell.registered`、`cell.signal`、`cell.sim_missing`、`member.authed`、`member.current`、`net.type`、`notify.unread`、`power.state`。

事件：`notify.changed`、`notify.channels_changed`、`wifi.changed`。

<a id="s60"></a>

### s60 — `system/ui/shell/settings_body`

设置：`audio.alarm_ring`、`audio.call_ring`、`audio.key_tone`、`audio.vibration`、`audio.vibration_level`、`audio.volume`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_style`、`display.warmth`、`net.airplane`、`net.prefer`、`notify.dnd`、`power.lift_wake`、`power.lock_after_s`、`voice.handsfree`、`wifi.enabled`。

状态：`audio.route`、`bt.connected`、`cell.config_readonly`、`cell.registered`、`wifi.ssid`、`wifi.state`。

事件：—。

<a id="s61"></a>

### s61 — `system/ui/shell/settings_family`

设置：`audio.child_max`、`display.lock_style`、`display.wallpaper_file`、`power.lift_wake`、`power.lock_after_s`。

状态：—。

事件：`confirm.result`、`member.changed`。

<a id="s62"></a>

### s62 — `system/ui/shell/settings_pages`

设置：`ai.read_clipboard`、`backup.auto`、`system.language`、`text.assist`、`update.auto_download`、`voice.handsfree`、`voice.interrupt`、`voice.name`。

状态：`battery.level`、`storage.card`、`storage.free`、`storage.mode`、`update.state`、`update.version`。

事件：`backup.changed`、`file.changed`。

<a id="s63"></a>

### s63 — `system/ui/shell/time_settings`

设置：`member.idle_return_s`、`time.auto`、`time.zone`、`time.zone_auto`。

状态：`time.minute`、`time.zone`。

事件：—。

<a id="s64"></a>

### s64 — `system/ui/shell/today_edit`

设置：—。

状态：—。

事件：—。

<a id="s65"></a>

### s65 — `system/ui/shell/transfer_pages`

设置：—。

状态：`storage.card`。

事件：`confirm.result`、`usb.changed`。

<a id="s66"></a>

### s66 — `system/ui/shell/update_pages`

设置：—。

状态：`battery.charging`、`battery.level`、`storage.card`、`storage.free`、`storage.mode`、`update.progress`、`update.state`、`update.version`。

事件：`confirm.result`、`update.available`、`update.result`。

<a id="s67"></a>

### s67 — `system/ui/shell/wallpaper`

设置：`display.lock_background`、`display.lock_reading`、`display.lock_style`、`display.lock_todo`、`display.wallpaper_file`。

状态：—。

事件：—。

## 每个设置项的能力覆盖

所有已定义设置都通过 `settings.get` 读取、`settings.set` 写入，由设置表按 id 查找；`settings.list` 列出类型、范围和值。写入继续检查范围、儿童限制、确认和应用执行器，通用能力不跳过这些检查。

| 设置 id | 描述 | 类型 | 读取 / 写入能力 | 允许修改的成员 | 风险 / 确认 | 定义 |
| --- | --- | --- | --- | --- | --- | --- |
| `ai.read_clipboard` | AI 可以读剪贴板 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `audio.alarm_ring` | 闹钟铃声 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.call_ring` | 来电铃声 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.child_max` | 孩子的音量上限 | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.key_tone` | 按键提示音 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.vibration` | 振动 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.vibration_level` | 振动强度 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.volume` | 音量 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `backup.auto` | 自动备份 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 高 / 无需 | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `bt.enabled` | 蓝牙 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `cell.sim_slot` | SIM 卡 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `display.font_size` | 字号 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/ui/shell/settings_body.c](../../system/ui/shell/settings_body.c) |
| `display.light` | 亮度 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/display/light.c](../../system/services/display/light.c) |
| `display.lock_background` | 表盘叠在壁纸上 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `display.lock_reading` | 锁屏继续读 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `display.lock_style` | 锁屏画面 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/ui/shell/settings_body.c](../../system/ui/shell/settings_body.c) |
| `display.lock_todo` | 锁屏显示待办 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `display.wallpaper_file` | 壁纸图片编号 | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/ui/shell/wallpaper.c](../../system/ui/shell/wallpaper.c) |
| `display.warmth` | 暖光（越大越暖） | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/display/light.c](../../system/services/display/light.c) |
| `member.idle_return_s` | 大人锁屏多久后要重新输密码（秒） | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/policy/members.c](../../system/policy/members.c) |
| `net.airplane` | 飞行模式 | `INK_SET_BOOL` | `settings.get` / `settings.set`（转给 `net.set_airplane`，不能用 `system.undo` 撤销） | 仅成人 | 中 / 无需 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.prefer` | 上网方式 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.roaming` | 数据漫游 | `INK_SET_BOOL` | `settings.get` / `settings.set`（转给 `net.roaming_set`，不能用 `system.undo` 撤销） | 仅成人 | 中 / 无需 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `notify.dnd` | 勿扰 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/notify/notify_service.c](../../system/services/notify/notify_service.c) |
| `power.lift_wake` | 拿起亮屏（平放不动 3 分钟先锁屏） | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `power.lock_after_s` | 自动锁屏（秒） | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `power.saving` | 省电 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `reader.font_size` | 正文字号 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.footer_mask` | 底栏显示项 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.line_spacing` | 行距 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.margins` | 边距 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.paragraph_spacing` | 段距 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `system.language` | 语言 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 高 / 无需 | [system/text/i18n.c](../../system/text/i18n.c) |
| `text.assist` | 打字时帮你接话 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `text.google` | 谷歌拼音 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `text.nine_key` | 九宫格拼音 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `time.auto` | 自动校时（听家庭服务器的） | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `time.zone` | 时区 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `time.zone_auto` | 自动设置时区 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `update.auto_download` | 自动下载新版本（流量要钱，默认关） | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `voice.handsfree` | 叫名字（什么时候在等） | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `voice.interrupt` | 说话能打断回答 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `voice.name` | 用什么名字叫它 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `wifi.enabled` | Wi-Fi | `INK_SET_BOOL` | `settings.get` / `settings.set`（转给 `wifi.set_enabled`，不能用 `system.undo` 撤销） | 仅成人 | 中 / 无需 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |

## 状态

| 状态 id | 描述 | 类型 | AI 可读 | 定义 |
| --- | --- | --- | --- | --- |
| `alarm.revision` | 闹钟和提醒的版本（变了就重读） | `INK_ST_INT` | 是 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `alarm.ringing` | 正在响的闹钟数 | `INK_ST_INT` | 是 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `app.cards_rev` | 服务卡片的版本（变了就重读） | `INK_ST_INT` | 是 | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.foreground` | 前台应用 | `INK_ST_STR` | 是 | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `audio.listening` | 在等人叫名字 | `INK_ST_BOOL` | 是 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.mic` | 麦克风（off 关着、local 只在设备上等人叫名字、sending 声音在送出去、recording 在录音） | `INK_ST_STR` | 是 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.now_playing` | 正在播放 | `INK_ST_STR` | 是 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.recording` | 正在录音 | `INK_ST_BOOL` | 是 | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `audio.route` | 声音从哪儿出来（speaker、bt_headset） | `INK_ST_STR` | 是 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `battery.charging` | 在充电 | `INK_ST_BOOL` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `battery.level` | 电量（%） | `INK_ST_INT` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `battery.low` | 电量低 | `INK_ST_BOOL` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `bt.available` | 这台设备能用蓝牙耳机 | `INK_ST_BOOL` | 是 | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.connected` | 耳机连着 | `INK_ST_BOOL` | 是 | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.device` | 连着的耳机 | `INK_ST_STR` | 是 | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `cell.config_readonly` | 移动网络配置只读 | `INK_ST_BOOL` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.registered` | 4G 在线 | `INK_ST_BOOL` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.roaming` | 漫游注册 | `INK_ST_BOOL` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.signal` | 移动网络信号 | `INK_ST_INT` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.sim` | 在用的 SIM 卡 | `INK_ST_STR` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.sim_missing` | 未插 SIM 卡 | `INK_ST_BOOL` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `confirm.pending` | 等人确认的请求数 | `INK_ST_INT` | 是 | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `file.index_state` | 文件索引状态（ready 可用，unavailable 不可用，changed 已换卡） | `INK_ST_STR` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `hub.online` | 家庭服务器已连接 | `INK_ST_BOOL` | 是 | [system/services/hub/hub_link.c](../../system/services/hub/hub_link.c) |
| `interrupt.active` | 正在打断（none、call、alarm、low_battery） | `INK_ST_STR` | 是 | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `job.revision` | 后台作业列表和阶段的版本 | `INK_ST_INT` | 是 | [system/services/res/task.c](../../system/services/res/task.c) |
| `job.running` | 后台有几件事在做 | `INK_ST_INT` | 是 | [system/services/res/task.c](../../system/services/res/task.c) |
| `media.state` | 播放状态（idle、playing、paused） | `INK_ST_STR` | 是 | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.authed` | 这位家人的空间开着 | `INK_ST_BOOL` | 是 | [system/policy/members.c](../../system/policy/members.c) |
| `member.current` | 现在是哪位家人 | `INK_ST_STR` | 是 | [system/policy/members.c](../../system/policy/members.c) |
| `member.time_up` | 今天的时间到了 | `INK_ST_BOOL` | 是 | [system/policy/members.c](../../system/policy/members.c) |
| `net.mobile_rev` | 移动网络设置变化 | `INK_ST_INT` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `net.online` | 在线 | `INK_ST_BOOL` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.signal` | 信号（dBm） | `INK_ST_INT` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.type` | 网络 | `INK_ST_STR` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `notify.unread` | 没看的通知数 | `INK_ST_INT` | 是 | [system/services/notify/notify_service.c](../../system/services/notify/notify_service.c) |
| `pairing.state` | 加入家庭的进度 | `INK_ST_STR` | 是 | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `power.lifted` | 刚被拿起亮屏（到下次锁屏） | `INK_ST_BOOL` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `power.saving` | 省电：最多 1 分钟锁屏，不改原设置 | `INK_ST_BOOL` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `power.state` | 使用状态（活跃 / 空闲 / 锁屏） | `INK_ST_STR` | 是 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `storage.card` | 存储卡在且能用 | `INK_ST_BOOL` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `storage.free` | 存储卡剩余空间（字节） | `INK_ST_INT` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `storage.mode` | 存储卡在哪（normal 系统用，usb 借给电脑） | `INK_ST_STR` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `storage.user_free` | 内部存储还剩多少字节（设置、通知、备忘这些放在这里） | `INK_ST_INT` | 是 | [system/services/data/datastore.c](../../system/services/data/datastore.c) |
| `text.focused` | 有文本框在输入 | `INK_ST_BOOL` | 是 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `text.voice` | 语音输入（idle、connecting、listening、finishing） | `INK_ST_STR` | 是 | [system/services/text/voice_input.c](../../system/services/text/voice_input.c) |
| `time.minute` | 本地时间（当天第几分钟） | `INK_ST_INT` | 是 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `time.synced` | 时间和家庭服务器对过 | `INK_ST_BOOL` | 是 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `time.zone` | 时区 | `INK_ST_STR` | 是 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `update.progress` | 升级进度（%） | `INK_ST_INT` | 是 | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.state` | 升级（idle、available、downloading、ready、verifying、installing、rebooting、failed） | `INK_ST_STR` | 是 | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.version` | 系统版本 | `INK_ST_STR` | 是 | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `usb.connected` | 连着电脑 | `INK_ST_BOOL` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `wifi.ssid` | 连的 Wi-Fi | `INK_ST_STR` | 是 | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.state` | Wi-Fi（idle、scanning、joining、connected、failed） | `INK_ST_STR` | 是 | [system/services/net/wifi.c](../../system/services/net/wifi.c) |

## 事件

| 事件 id | 描述 | 谁能调 / 订阅 | 风险 / effect | 确认 | 成员范围 | 关联面 | 定义 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ai.history.changed` | 自己的对话记录已改变 | 不可调用；不向 AI 转发 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s23](#s23) | [system/services/ai/ai_history.c](../../system/services/ai/ai_history.c) |
| `alarm.ended` | 闹钟停了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.fired` | 闹钟响了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.missed` | 错过了闹钟 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `app.state` | 一个应用的状态变了（foreground、background、frozen、stopped） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s43](#s43) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `audio.record_done` | 录音结束了（存好的文件、时长、为什么停） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `backup.changed` | 这位成员的备份或恢复进度有变化 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.failed` | 这位成员的备份或恢复没有完成 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s25](#s25) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `battery.low` | 电量低了（level 1 低，2 很低，3 马上关机，0 充上电了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `bt.changed` | 蓝牙耳机的情况变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s26](#s26) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `call.answered` | 电话接通了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.connected` | 拨出的电话已接通 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.controls` | 通话控制已获模组确认 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.dialed` | 电话已拨出 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.ended` | 通话结束 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.incoming` | 来电 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `confirm.result` | 一个待确认的请求结束了（accepted、rejected、expired、withdrawn、member_changed，确认后还有回执） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s19](#s19) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `contacts.selected` | 联系人号码已选好 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s13](#s13) | [system/apps/phone/phone.c](../../system/apps/phone/phone.c) |
| `data.write_failed` | 数据没能写进存储 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s29](#s29) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `file.batch_progress` | 批量文件操作逐项进度 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.changed` | 文件变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.job_done` | 后台的拷贝或删除结束了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `games.changed` | 当前成员的棋局已变化 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s09](#s09) | [system/apps/gomoku/gomoku.c](../../system/apps/gomoku/gomoku.c) |
| `games.memory.changed` | 当前家人的可见牌面或成绩改变 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s11](#s11) | [system/apps/memory/memory.c](../../system/apps/memory/memory.c) |
| `games.snake.changed` | 当前家人的蛇身或成绩改变 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s16](#s16) | [system/apps/snake/snake.c](../../system/apps/snake/snake.c) |
| `job.finished` | 一件后台的事结束了（做好了、失败了、停下了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s37](#s37) | [system/services/res/task.c](../../system/services/res/task.c) |
| `media.finished` | 播放结束了（放完、停止、读不了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s24](#s24) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.changed` | 家人有变化（加了、删了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s20](#s20) | [system/policy/members.c](../../system/policy/members.c) |
| `message.received` | 收到短信 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s28](#s28) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `notify.changed` | 通知列表有变化 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.channels_changed` | 通知渠道有变化 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.posted` | 有新通知 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s35](#s35) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `pairing.changed` | 加入家庭的进度变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s32](#s32) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `reminder.fired` | 提醒到时间了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.missed` | 错过了提醒 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `text.voice_ended` | 语音输入结束了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s38](#s38) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `time.changed` | 系统时间改了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s39](#s39) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `update.available` | 发现了新版本 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.result` | 升级的结果 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s40](#s40) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `usb.changed` | 电脑连上、断开或 U 盘状态变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s31](#s31) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `wifi.changed` | Wi-Fi 的情况变了（连上了、忘掉了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s34](#s34) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
