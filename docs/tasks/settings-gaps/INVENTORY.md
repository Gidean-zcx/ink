# 设置与数据变更能力逐项清单

【实现】按基线静态能力表逐项核对，共 89 个非只读副作用能力、22 个设置项；另检查 navigate 类中的录音停止及用户操作入口。查阅日期：2026-11-10。只核查本地代码和设计导出，不宣称实机已用。

“只给 AI 的”在这里指没有独立人工编辑器的 AI／应用接口，不修改 expose 权限，也不把没有设计的功能说成 AI 专用。除三项接线外，其余条目不宣称新增完成。

## 接上的

| 能力 | 位置 | 原因与处理 |
| --- | --- | --- |
| `wifi.forget` | SetNetwork 的 Wi-Fi 段 | 原来只展示已连名称；补“忘掉这个网络”，沿用原确认卡。任务书明确指定此位置。 |
| `notify.clear` | Notifications 底部 | 画板有“全部清除”，代码只有“全部已读”；补独立按钮与快照动作。 |
| `file.copy` | FileActions → FileMove | 画板底部有“拷贝”，代码只有移动、删除；复用目标文件夹页，最终调用 file.copy。 |

Wi-Fi 仅给当前已连网络提供动作。未连的已记网络管理没有画板，列入待定；不会把扫描到的陌生网络当成已保存网络。

## 只给 AI 的通用接口

| 能力 | 人工对应位置 | 不另加按钮的依据 |
| --- | --- | --- |
| `data.put` | 各应用自己的编辑页 | 通用集合写接口；现有业务编辑器负责字段和校验，没有任意集合编辑器的设计。 |
| `data.delete` | 各应用自己的详情页 | 通用集合删接口；现有业务删除动作负责对象选择，没有任意集合编辑器的设计。 |
| `notify.post` | Notifications 展示结果 | 目录明确为 AI 发给用户的通知，不是让用户给自己发通知的编辑器。 |
| `clipboard.write` | ImeEdit／文字选择菜单 | 任意文本写剪贴板供 AI／应用使用；人的复制、剪切已经从选择菜单进入相同剪贴板服务。 |

没有查到“其余缺失操作一律只供 AI”的设计授权。下面的缺口全部留给用户决定。

## 要用户定的

| 能力／设置 | 应在何处 | 目前为什么没有／本次边界 |
| --- | --- | --- |
| `audio.record` | Memo → MemoRecord（已有画板） | 尚无录音页和语音备忘数据关联；单接按钮只能留下文件，不能实现画板的“记好了／不要了”和备忘回看。新增持久字段或录音生命周期不属于本任务允许的接线；没做。 |
| `file.mkdir` | FileList（位置待定） | 画板、规范没有新建文件夹按钮和命名流程；保留能力，待补设计。 |
| `file.write` | 文件文本编辑页（未设计） | FileActions 只有打开、拷贝、移动、删除；没有写文件的页面和保存流程。 |
| `notify.dismiss` | Notifications 单条通知（位置待定） | 画板只有打开、回复／回拨及全部清除，没有逐条删除控件。 |
| `notify.set_channel` | 通知渠道设置（未设计） | SetSound 有勿扰；没有渠道列表及各渠道开关。 |
| `reminder.add` | Upcoming 的“加一件”／MemoDetail 的“加提醒” | Upcoming 已有 AI 语音入口；MemoDetail 尚未生成候选事项，规范又禁止编造时间。无手动时间输入画板，不能凭空补表单；未补直接入口。 |
| `reminder.cancel` | Upcoming 事项详情（未设计） | 现有事项行只读且不可点；没有删除入口或详情画板，不能自行画页。 |
| `text.redo` | ImeEdit（位置待定） | 画板和现有菜单有撤销，没有重做控件；不自行加入一组操作。 |
| `time.set` | 日期时间设置（未设计） | 现有设置目录和画板都没有日期时间选择页。 |
| `audio.record_stop` | MemoRecord（已有画板） | 录音入口未实现，停止、保存、放弃及回看需一并接通；同 audio.record，没做。 |
| `settings.set(time.auto)` | 日期时间设置（未设计） | 自动校时能力已具备，没有开关位置。 |
| `settings.set(time.zone)` | 时区选择页（未设计） | 没有时区列表和选择交互。 |
| `settings.set(member.idle_return_s)` | SetMembers／安全设置（位置待定） | 有自动返回策略，没有对应超时选择控件；不与自动锁屏混为一项。 |

推荐：先补三项时间／安全设置和通知渠道的画板；语音备忘另给独立任务，明确数据关联及“不要了”的保存／删除语义。现有能力继续保留。

## 全部非只读能力复核

表中的“已有”只表示人工操作或设计规定的自动流程存在；不替代该功能原任务的验收。定义入口以[能力总表](../../system/capabilities.md)为准。

| 能力 | 结论／位置 | 代码依据 |
| --- | --- | --- |
| `activity.clear` | 已有：Records | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L139) |
| `alarm.cancel` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L138) |
| `alarm.dismiss` | 已有：interrupt_card | [system/ui/shell/interrupt_card.c](../../../system/ui/shell/interrupt_card.c#L57) |
| `alarm.set` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L317) |
| `alarm.snooze` | 已有：interrupt_card | [system/ui/shell/interrupt_card.c](../../../system/ui/shell/interrupt_card.c#L57) |
| `alarm.update` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L72) |
| `audio.record` | 待定／未做：Memo → MemoRecord（已有画板） | [system/services/audio/media_service.c](../../../system/services/audio/media_service.c) |
| `backup.now` | 已有：backup_pages／factory_pages | [system/ui/shell/backup_pages.c](../../../system/ui/shell/backup_pages.c#L58) |
| `bt.forget` | 已有：bluetooth_page | [system/ui/shell/bluetooth_page.c](../../../system/ui/shell/bluetooth_page.c#L79) |
| `bt.pair` | 已有：bluetooth_page | [system/ui/shell/bluetooth_page.c](../../../system/ui/shell/bluetooth_page.c#L69) |
| `call.answer` | 已有：PhoneIncoming | [system/services/notify/notify_service.c](../../../system/services/notify/notify_service.c#L566) |
| `call.dial` | 已有：confirm_card／phone | [system/ui/shell/confirm_card.c](../../../system/ui/shell/confirm_card.c#L40) |
| `call.dtmf` | 已有：phone | [system/apps/phone/phone.c](../../../system/apps/phone/phone.c#L243) |
| `camera.capture` | 已有：camera | [system/apps/camera/camera.c](../../../system/apps/camera/camera.c#L105) |
| `camera.photo` | 已有：camera | [system/apps/camera/camera.c](../../../system/apps/camera/camera.c#L19) |
| `clipboard.write` | AI／应用接口：ImeEdit／文字选择菜单 | [system/services/text/text_caps.c](../../../system/services/text/text_caps.c) |
| `clock.save` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L170) |
| `clock.timer_cancel` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L279) |
| `clock.timer_dismiss` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L232) |
| `clock.timer_pause` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L255) |
| `clock.timer_set` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L249) |
| `clock.timer_start` | 已有：clock | [system/apps/clock/clock.c](../../../system/apps/clock/clock.c#L252) |
| `contacts.delete` | 已有：contacts | [system/apps/contacts/contacts.c](../../../system/apps/contacts/contacts.c#L53) |
| `contacts.put` | 已有：contacts | [system/apps/contacts/contacts.c](../../../system/apps/contacts/contacts.c#L53) |
| `data.delete` | AI／应用接口：各应用自己的详情页 | [system/services/data/data_caps.c](../../../system/services/data/data_caps.c) |
| `data.put` | AI／应用接口：各应用自己的编辑页 | [system/services/data/data_caps.c](../../../system/services/data/data_caps.c) |
| `family.save` | 已有：SetMembers | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L151) |
| `file.cache_clear` | 已有：settings_pages | [system/ui/shell/settings_pages.c](../../../system/ui/shell/settings_pages.c#L359) |
| `file.cleanup` | 已有：settings_pages | [system/ui/shell/settings_pages.c](../../../system/ui/shell/settings_pages.c#L29) |
| `file.copy` | 接上：FileActions → FileMove | [system/apps/files/files.c](../../../system/apps/files/files.c#L259) |
| `file.delete` | 已有：gallery／files | [system/apps/gallery/gallery.c](../../../system/apps/gallery/gallery.c#L96) |
| `file.mkdir` | 待定／未做：FileList（位置待定） | [system/services/file/file_caps.c](../../../system/services/file/file_caps.c) |
| `file.move` | 已有：files | [system/apps/files/files.c](../../../system/apps/files/files.c#L186) |
| `file.write` | 待定／未做：文件文本编辑页（未设计） | [system/services/file/file_caps.c](../../../system/services/file/file_caps.c) |
| `keyboard.edit` | 已有：keyboard | [system/ui/shell/keyboard.c](../../../system/ui/shell/keyboard.c#L122) |
| `keyboard.key` | 已有：keyboard | [system/ui/shell/keyboard.c](../../../system/ui/shell/keyboard.c#L112) |
| `keyboard.pick` | 已有：keyboard | [system/ui/shell/keyboard.c](../../../system/ui/shell/keyboard.c#L116) |
| `member.add` | 已有：settings_family | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L260) |
| `member.remove` | 已有：settings_family | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L273) |
| `member.rename` | 已有：settings_family | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L260) |
| `member.set_pin` | 已有：SetMembers → PinSet | [system/ui/shell/member_pages.c](../../../system/ui/shell/member_pages.c#L1) |
| `member.time_add` | 已有：kid_pages | [system/ui/shell/kid_pages.c](../../../system/ui/shell/kid_pages.c#L92) |
| `member.time_set` | 已有：settings_family | [system/ui/shell/settings_family.c](../../../system/ui/shell/settings_family.c#L53) |
| `memo.check` | 已有：memo | [system/apps/memo/memo.c](../../../system/apps/memo/memo.c#L195) |
| `memo.convert` | 已有：memo | [system/apps/memo/memo.c](../../../system/apps/memo/memo.c#L91) |
| `memo.delete` | 已有：memo | [system/apps/memo/memo.c](../../../system/apps/memo/memo.c#L90) |
| `memo.save` | 已有：memo | [system/apps/memo/memo.c](../../../system/apps/memo/memo.c#L78) |
| `memo.undo` | 已有：memo | [system/apps/memo/memo.c](../../../system/apps/memo/memo.c#L88) |
| `message.send` | 已有：sms | [system/apps/sms/sms.c](../../../system/apps/sms/sms.c#L88) |
| `notify.clear` | 接上：Notifications 底部 | [system/ui/shell/navigation.c](../../../system/ui/shell/navigation.c#L1348) |
| `notify.dismiss` | 待定／未做：Notifications 单条通知（位置待定） | [system/services/notify/notify_caps.c](../../../system/services/notify/notify_caps.c) |
| `notify.post` | AI／应用接口：Notifications 展示结果 | [system/services/notify/notify_caps.c](../../../system/services/notify/notify_caps.c) |
| `notify.read` | 已有：navigation | [system/ui/shell/navigation.c](../../../system/ui/shell/navigation.c#L1347) |
| `notify.set_channel` | 待定／未做：通知渠道设置（未设计） | [system/services/notify/notify_caps.c](../../../system/services/notify/notify_caps.c) |
| `pairing.accept` | 已有：boot_flow | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L90) |
| `pairing.configure` | 已有：FirstBoot 手机扫码设置页 | [system/services/hub/pairing.c](../../../system/services/hub/pairing.c#L609) |
| `pairing.hotspot_start` | 已有：boot_flow | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L156) |
| `pairing.hotspot_stop` | 已有：boot_flow | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L281) |
| `pairing.import` | 已有：首次开机的存储卡配对导入 | [system/services/hub/pairing.c](../../../system/services/hub/pairing.c#L971) |
| `pairing.reject` | 已有：boot_flow | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L90) |
| `pairing.restart` | 已有：boot_flow | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L293) |
| `power.set_saving` | 已有：power_pages | [system/ui/shell/power_pages.c](../../../system/ui/shell/power_pages.c#L83) |
| `reminder.add` | 待定／未做：Upcoming 的“加一件”／MemoDetail 的“加提醒” | [system/services/time/time_caps.c](../../../system/services/time/time_caps.c) |
| `reminder.cancel` | 待定／未做：Upcoming 事项详情（未设计） | [system/services/time/time_caps.c](../../../system/services/time/time_caps.c) |
| `restore.apply` | 已有：backup_pages／boot_flow | [system/ui/shell/backup_pages.c](../../../system/ui/shell/backup_pages.c#L110) |
| `settings.set` | 已有：各设置页 | [system/ui/shell/bluetooth_page.c](../../../system/ui/shell/bluetooth_page.c#L93) |
| `shell.today_card` | 已有：TodayEdit | [system/ui/shell/navigation.c](../../../system/ui/shell/navigation.c#L1846) |
| `sms.send` | 已有：confirm_card | [system/ui/shell/confirm_card.c](../../../system/ui/shell/confirm_card.c#L51) |
| `system.factory_reset` | 已有：factory_pages | [system/ui/shell/factory_pages.c](../../../system/ui/shell/factory_pages.c#L104) |
| `system.reboot` | 已有：power_menu | [system/ui/shell/power_menu.c](../../../system/ui/shell/power_menu.c#L70) |
| `system.set_volume` | 已有：Control／SetSound／音量键 | [system/ui/shell/navigation.c](../../../system/ui/shell/navigation.c#L1) |
| `system.shutdown` | 已有：power_menu | [system/ui/shell/power_menu.c](../../../system/ui/shell/power_menu.c#L69) |
| `system.undo` | 已有：ai_pages | [system/ui/shell/ai_pages.c](../../../system/ui/shell/ai_pages.c#L136) |
| `system.upgrade` | 已有：update_pages | [system/ui/shell/update_pages.c](../../../system/ui/shell/update_pages.c#L159) |
| `system.upgrade_from_card` | 已有：Notifications 的存储卡升级通知 | [system/services/update/update_service.c](../../../system/services/update/update_service.c#L520) |
| `text.add_word` | 已有：textfield | [system/ui/textfield.c](../../../system/ui/textfield.c#L297) |
| `text.ask_ai` | 已有：textfield | [system/ui/textfield.c](../../../system/ui/textfield.c#L293) |
| `text.insert` | 已有：键盘和输入框 | [system/ui/shell/keyboard.c](../../../system/ui/shell/keyboard.c#L1) |
| `text.redo` | 待定／未做：ImeEdit（位置待定） | [system/services/text/text_caps.c](../../../system/services/text/text_caps.c) |
| `text.undo` | 已有：ImeEdit／文字选择菜单 | [system/ui/textfield.c](../../../system/ui/textfield.c#L1) |
| `text.voice_start` | 已有：keyboard／navigation | [system/ui/shell/keyboard.c](../../../system/ui/shell/keyboard.c#L272) |
| `time.set` | 待定／未做：日期时间设置（未设计） | [system/services/time/time_caps.c](../../../system/services/time/time_caps.c) |
| `transfer.start` | 已有：transfer_pages | [system/ui/shell/transfer_pages.c](../../../system/ui/shell/transfer_pages.c#L97) |
| `transfer.stop` | 已有：transfer_pages | [system/ui/shell/transfer_pages.c](../../../system/ui/shell/transfer_pages.c#L107) |
| `update.download` | 已有：update_pages | [system/ui/shell/update_pages.c](../../../system/ui/shell/update_pages.c#L161) |
| `usb.mount` | 已有：transfer_pages | [system/ui/shell/transfer_pages.c](../../../system/ui/shell/transfer_pages.c#L110) |
| `usb.unmount` | 已有：transfer_pages | [system/ui/shell/transfer_pages.c](../../../system/ui/shell/transfer_pages.c#L111) |
| `wifi.connect` | 已有：boot_flow／navigation | [system/ui/shell/boot_flow.c](../../../system/ui/shell/boot_flow.c#L125) |
| `wifi.forget` | 接上：SetNetwork 的 Wi-Fi 段 | [system/ui/shell/navigation.c](../../../system/ui/shell/navigation.c#L1251) |

## 全部设置项复核

| 设置 | 人工入口／缺口 |
| --- | --- |
| `ai.read_clipboard` | SetPrivacy |
| `audio.alarm_ring` | SetSound |
| `audio.call_ring` | SetSound |
| `audio.child_max` | SetKid |
| `audio.key_tone` | SetSound |
| `audio.vibration` | SetSound |
| `audio.volume` | SetSound／Control／音量键 |
| `bt.enabled` | SetBluetooth |
| `cell.sim_slot` | SetNetwork |
| `display.font_size` | SetDisplay |
| `display.light` | SetDisplay／Control |
| `display.lock_style` | Wallpaper |
| `display.wallpaper_file` | Wallpaper → 相册选择 |
| `display.warmth` | SetDisplay／Control |
| `member.idle_return_s` | 待定：没有超时选择位置 |
| `net.prefer` | SetNetwork |
| `notify.dnd` | SetSound／Control |
| `power.lock_after_s` | SetDisplay／Wallpaper |
| `time.auto` | 待定：没有日期时间页 |
| `time.zone` | 待定：没有时区页 |
| `update.auto_download` | SetAbout |
| `voice.handsfree` | SetVoice／Control |

补查的 navigate 能力：`audio.record_stop` 未接；`bt.connect`／`bt.disconnect`、`call.hangup`／`call.mute`／`call.route`、`media.play`／`media.pause`／`media.resume`／`media.seek`／`media.stop`、`job.cancel`、`system.full_refresh`、`system.switch_member` 均有既有控件或实体键入口。`reminder.add` 的人工语音入口不等于离线手动编辑器。
