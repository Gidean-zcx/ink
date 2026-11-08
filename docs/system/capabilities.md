# 能力总表（代码生成）

由 `tools/capabilities.py` 读取 `system/`、`apps/` 的 `ink_cap_t`、`ink_setting_def_t`、`ink_state_def_t` 定义生成；请修改代码后重新生成，不直接编辑本文件。

```sh
python3 tools/capabilities.py
python3 tools/capabilities.py --check
```

当前共 196 项：可调用能力 166、事件 30；设置 16 项、状态 44 项。本表列设备固件的静态定义，不含中枢运行时的远端目录。静态目录包含按硬件条件注册的能力；实际可用性仍取决于硬件、当前页面及服务状态。CTest 另外与 Plus 模拟器启动后的完整注册表核对，不使用按成员或页面过滤的 AI 清单代替完整目录，并核对实际 C 确认策略的来源、PIN 和 asked 组合。

风险按代码的 effect 归类：read / navigate 为低，local_write 为中，local_destructive / external / system 为高；这是副作用等级，不等于所有高等级操作都弹确认。描述和 effect 必须显式填写，遗漏、未知值、重复 id 或文档过期均使检查失败。

调用和确认规则来自 `system/policy/cap.c` 的 exposed / gate / confirm_for，以及 `system/policy/settings.c` 的 setting_set。表中人指触摸、实体键；AI 指 INK_SRC_AI，外部调用还须各自的 expose 权限。AI 不能代输 PIN 或替人按确认。asked=true 只取消普通对外动作的确认，不绕过 ask_by_hand、成人 PIN、成员、页面版本或服务内部校验。

成员范围指能力闸门允许的调用成员，均绑定调用时的当前成员及代际，不表示能访问其他成员数据。非 system 能力的 child_ok 直接放行儿童，白名单按 id 前缀额外放行 child_ok=false 的能力；system 能力则只允许 child_ok=true 的人工儿童调用，白名单不能绕过。成人空间关闭时 AI、外部应用都不能调用；while_locked 只放行人工入口。数据集合、文件路径、通知、闹钟、后台作业仍检查各自的归属；设置是设备级值，儿童写权限另列。事件只能订阅，不能当工具调用，投递仍按成员隔离。

system.undo 的外层 confirm_for 无需确认，不代表整个撤销无需确认。设置撤销保留当前来源，重新经过 settings.set 及设置项的儿童、确认、版本检查；其他撤销以已确认来源调用逆操作，仍检查逆操作的成员及服务条件。

关联面按同一服务目录（policy / ui 按源文件）及该提供方源码引用的已知 id 汇集，表示服务上下文，**不表示每次调用都会修改列出的所有设置、状态或发出所有事件**。完整设置、状态和事件定义均列在后文。没有定义的关联显示 —，不据命名前缀猜测。

## 可调用能力

| 能力 id | 描述 | 谁能调 | 风险 / effect | 是否确认 | 成员范围 | 关联面 | 定义 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `activity.list` | 查看 AI 做过的事 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/policy/cap.c](../../system/policy/cap.c) |
| `ai.panel.hide` | 收起 AI 面板（继续处理） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.listen` | 用话筒问这一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.next` | 答案下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `ai.panel.prev` | 答案上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s25](#s25) | [system/ui/shell/ai_panel.c](../../system/ui/shell/ai_panel.c) |
| `alarm.cancel` | 删闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.dismiss` | 关掉正在响的闹钟 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.list` | 我的闹钟 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.set` | 设闹钟（weekdays 每周几：位 0 是周日；不给就只响一次） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.snooze` | 闹钟稍后再响 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `app.cards` | 各应用的服务卡片（和主页看到的一样） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.close` | 关掉一个应用（它会保存状态） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.list` | 这台设备上有哪些应用，各自的状态 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `app.open` | 打开一个应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s24](#s24) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `audio.record` | 开始录音，存成文件 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `audio.record_stop` | 停止录音 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `backup.manifest` | 查看备份清单、覆盖项目和未恢复项目 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.now` | 备份这位成员的资料 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.status` | 查看这位成员的备份、上传回执和恢复进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `bt.connect` | 连上耳机，声音走耳机 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.disconnect` | 断开耳机，声音回到外放 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.forget` | 忘掉一副耳机 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.list` | 配对过的耳机 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.pair` | 配对一副耳机 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `bt.scan` | 找附近的蓝牙耳机（要几秒） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `call.answer` | 接电话 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.dial` | 打电话 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.hangup` | 挂电话 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `camera.capture` | 拍一张照片 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s11](#s11) | [system/services/camera/camera_service.c](../../system/services/camera/camera_service.c) |
| `caps.domains` | 有哪些类能力（每类多少个） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/policy/cap.c](../../system/policy/cap.c) |
| `caps.find` | 按关键词查能力（带完整描述） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s03](#s03) | [system/policy/cap.c](../../system/policy/cap.c) |
| `clipboard.read` | 读剪贴板（用户在设置里打开才行） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `clipboard.write` | 写剪贴板 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `confirm.accept` | 确认（只有人在设备上能按） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s04](#s04) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.list` | 有什么在等人确认 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s04](#s04) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.page` | 翻阅确认说明（只由人在设备上操作） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s26](#s26) | [system/ui/shell/confirm_card.c](../../system/ui/shell/confirm_card.c) |
| `confirm.reject` | 取消（只有人在设备上能按） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s04](#s04) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `confirm.withdraw` | 撤回自己发起、还在等确认的请求 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s04](#s04) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `data.collections` | 有哪些数据集合 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.delete` | 删除一条数据 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.get` | 读一条数据 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.list` | 分页读一个数据集合（默认最新的在前） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.put` | 写一条数据（新的可以撤销） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `data.status` | 一条数据保存到哪一步了（saving 正在写、on_device 已存在这台设备、at_hub 已备份、failed 没写进去） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `file.copy` | 拷贝文件或目录（大的在后台做） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.delete` | 删除文件或目录（不能撤销） | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.free` | 存储卡还剩多少空间 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.index` | 按类型分页查询目录树的文件索引 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.list` | 列出目录（/me 自己的文件，/family 全家的内容） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.lookup` | 按稳定编号查询文件 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.mkdir` | 新建目录 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.move` | 移动或改名（可撤销） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.read` | 读文本文件（每次最多 2 KiB） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.stat` | 一个文件或目录的信息 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.write` | 写文本文件（新文件，或追加） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `home.next` | 主页下一屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `home.prev` | 主页上一屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `interrupt.ack` | 知道了（关掉低电量提示） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s18](#s18) | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `interrupt.list` | 现在有什么在打断（来电、闹钟、低电量），各有什么按钮 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s18](#s18) | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `job.cancel` | 停下一件后台在做的事 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s20](#s20) | [system/services/res/task.c](../../system/services/res/task.c) |
| `job.list` | 后台在做什么、做到哪了、做完的结果 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s20](#s20) | [system/services/res/task.c](../../system/services/res/task.c) |
| `keyboard.candidates` | 展开下一页候选，或 fold 收回键盘 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s27](#s27) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.edit` | 编辑选中内容：0 左、1 右、2 复制、3 剪切、4 粘贴、5 撤销 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s27](#s27) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.key` | 按输入法的键（1 符号、2 大写、8 删除、10 回车，其余是 ASCII） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s27](#s27) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `keyboard.pick` | 选输入法候选（从 0 开始） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s27](#s27) | [system/ui/shell/keyboard.c](../../system/ui/shell/keyboard.c) |
| `media.pause` | 暂停 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.play` | 放一个音频文件（Ogg Opus），不给位置就从上次听到的地方 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.resume` | 接着放 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.seek` | 跳到某个位置 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.status` | 现在放的是什么，到哪儿了 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `media.stop` | 停止 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.add` | 加一位家人 | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `member.list` | 家里有哪些人 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `member.remove` | 删除一位家人和他们的全部数据（不能撤销） | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `member.set_pin` | 设置或修改密码（只能在设备上） | 仅人（设备） | 中 / local_write | 无需 | 成人；儿童须白名单 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `member.unlock` | 输密码进入家人的空间（只能在设备上） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `message.list` | 最近的短信 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `message.send` | 发短信 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `notify.channels` | 哪些通知渠道开着 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.clear` | 清空通知 | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.dismiss` | 删一条通知 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.list` | 通知列表（新的在前） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.post` | 发一条通知（给用户看的提醒；来源记为 AI） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.read` | 标为已读（id 0 或不给：全部） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `notify.set_channel` | 开关一个通知渠道 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `pairing.accept` | 确认加入屏幕上的家庭 | 仅人（设备） | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.configure` | 设置家庭服务器地址和公钥 | 人、AI | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.hotspot_start` | 开启设备设置热点 | 人、AI | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.hotspot_stop` | 关闭设备设置热点 | 人、AI | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.import` | 从存储卡导入配对文件 | 人、AI | 高 / system | 人、AI 均在设备确认 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.reject` | 取消加入这个家庭 | 人、AI | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.restart` | 重新加入家庭 | 人、AI | 高 / system | 无需 | 仅成人 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `pairing.status` | 查看加入家庭的进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `reader.goto_page` | 跳到第几页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.list_books` | 书架上有哪些书 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.next_page` | 下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.open_book` | 打开一本书 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.prev_page` | 上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reader.read_page` | 读出这一页 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单）；页面 book | [s01](#s01) | [apps/reader/reader.c](../../apps/reader/reader.c) |
| `reminder.add` | 加提醒（at 本地时间 YYYY-MM-DD HH:MM；repeat: daily weekly monthly yearly lunar_yearly） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.cancel` | 删提醒 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.list` | 我的提醒 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `remote.open` | 打开网页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s02](#s02) | [system/apps/remote/remote.c](../../system/apps/remote/remote.c) |
| `restore.apply` | 按预览覆盖这位成员的资料 | 人、AI | 高 / local_destructive | 人、AI 均确认；已设成人 PIN 时还需 PIN | 成人、儿童（无需白名单） | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `restore.preview` | 预览恢复这位成员的备份和将被替换的资料 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `settings.get` | 查看一项设置 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s06](#s06) | [system/policy/settings.c](../../system/policy/settings.c) |
| `settings.list` | 所有设置，各自的类型、取值范围和现在的值 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `settings.set` | 改一项设置 | 人、AI | 中 / local_write | 按设置项（见设置表） | 成人、儿童（无需白名单）；设置写入另受设置项限制 | [s06](#s06) | [system/policy/settings.c](../../system/policy/settings.c) |
| `shell.back` | 返回上一层 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.close` | 收起面板 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.control` | 打开控制中心 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.display` | 显示和亮度 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.launch` | 打开主页上的应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.lock` | 锁屏 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.members` | 选择家人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.members_page` | 翻选人页面（只由人在设备上操作） | 仅人（设备） | 低 / navigate | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s28](#s28) | [system/ui/shell/member_gate.c](../../system/ui/shell/member_gate.c) |
| `shell.more` | 更多控制 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.network` | 网络和 SIM 卡 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notifications` | 查看通知 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_next` | 通知下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.notify_prev` | 通知上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.page_next` | 设置页的下一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.page_prev` | 设置页的上一页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.resume` | 接着读 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.search` | 查找应用 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单）；页面 home | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.settings` | 打开设置 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.sound` | 声音 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.wifi_next` | 附近网络下一组 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.wifi_pick` | 选附近网络（密码由人在设备上填写） | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `shell.wifi_prev` | 附近网络上一组 | 人、AI | 低 / navigate | 无需 | 成人；儿童须白名单 | [s29](#s29) | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `state.list` | 系统现在的状态（电量、网络、前台应用……） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.factory_reset` | 清空这台设备（清掉所有家人的数据，不能撤销） | 人、AI | 高 / system | 人、AI 均确认；已设成人 PIN 时还需 PIN | 仅成人 | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.full_refresh` | 刷新屏幕 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.home` | 回到主页 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.reboot` | 重启 | 人、AI | 高 / system | 无需 | 成人；儿童仅人工；锁定时仅人工入口 | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.set_volume` | 调音量 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单）；锁定时仅人工入口 | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.shutdown` | 关机 | 人、AI | 高 / system | 无需 | 成人；儿童仅人工；锁定时仅人工入口 | [s19](#s19) | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `system.switch_member` | 切换家人 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s07](#s07) | [system/policy/system_actions.c](../../system/policy/system_actions.c) |
| `system.undo` | 撤销刚才的操作 | 人、AI | 中 / local_write | 设置撤销按设置项重新判断（见设置表）；其他撤销无需 | 成人、儿童（无需白名单）；设置撤销另受设置项限制 | [s03](#s03) | [system/policy/cap.c](../../system/policy/cap.c) |
| `system.upgrade` | 安装下载好的新系统（安装时不能用设备，会重启） | 仅人（设备） | 高 / system | 人在设备确认 | 仅成人 | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `system.upgrade_from_card` | 安装存储卡上的新系统（update 文件夹里，会重启） | 仅人（设备） | 高 / system | 人在设备确认 | 仅成人 | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `text.insert` | 往有焦点的文本框里写字（等于替用户打字） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.redo` | 把撤销的改回来 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.select` | 放光标或选一段字（按字数算；不给 end 就是放光标。text.insert 写在这里，选中的被替换） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.state` | 现在有没有文本框在输入，里面是什么（密码不给看） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.submit` | 按回车（完成、发送、搜索） | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.undo` | 撤销文本框里刚才的修改 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_cancel` | 取消语音输入，什么都不写进去 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_finish` | 说完了，结束语音输入 | 人、AI | 低 / navigate | 无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `text.voice_start` | 打开语音输入：说的话变成文字进文本框 | 人、AI | 高 / external | 人无需；AI 仅 asked=true 时无需 | 成人、儿童（无需白名单） | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `time.lunar` | 公历和农历互查（不给日期就是今天） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `time.now` | 现在几点（含时区和农历） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `time.set` | 改系统时间 | 人、AI | 高 / system | 无需 | 仅成人 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `transfer.start` | 开启一次无线传文件授权（两分钟内有效） | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `transfer.status` | 查看无线传文件授权和结果 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `transfer.stop` | 关闭无线传文件 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/transfer.c](../../system/services/file/transfer.c) |
| `update.check` | 问家庭服务器有没有新版本 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.download` | 把新版本下载到存储卡（不安装） | 人、AI | 中 / local_write | 无需 | 成人；儿童须白名单 | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.status` | 系统版本和升级进度 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `usb.mount` | 把存储卡借给电脑当 U 盘 | 人、AI | 高 / system | 人、AI 均在设备确认 | 仅成人 | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `usb.status` | 是否连着电脑、存储卡是否借给了电脑 | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `usb.unmount` | 从电脑收回存储卡 | 人、AI | 中 / local_write | 无需 | 成人、儿童（无需白名单） | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `wifi.connect` | 连接一个 Wi-Fi 并记住它 | 人、AI | 高 / system | 无需 | 仅成人 | [s17](#s17) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.forget` | 忘掉一个 Wi-Fi | 人、AI | 高 / local_destructive | 人、AI 均在设备确认 | 成人；儿童须白名单 | [s17](#s17) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.list` | 这台设备记得的 Wi-Fi（只有名字） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
| `wifi.scan` | 找附近的 Wi-Fi（要几秒） | 人、AI | 低 / read | 无需 | 成人、儿童（无需白名单） | [s17](#s17) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |

## 关联的设置、状态和事件

<a id="s01"></a>

### s01 — `apps/reader`

设置：—。

状态：—。

事件：—。

<a id="s02"></a>

### s02 — `system/apps/remote`

设置：—。

状态：`net.online`、`power.state`。

事件：—。

<a id="s03"></a>

### s03 — `system/policy/cap`

设置：—。

状态：—。

事件：—。

<a id="s04"></a>

### s04 — `system/policy/confirm`

设置：—。

状态：`confirm.pending`、`interrupt.active`、`member.current`、`power.state`。

事件：`confirm.result`。

<a id="s05"></a>

### s05 — `system/policy/members`

设置：`member.idle_return_s`。

状态：`member.authed`、`member.current`。

事件：`member.changed`。

<a id="s06"></a>

### s06 — `system/policy/settings`

设置：`ai.read_clipboard`、`audio.child_max`、`audio.volume`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_style`、`display.warmth`、`member.idle_return_s`、`net.prefer`、`notify.dnd`、`power.lock_after_s`、`time.auto`、`time.zone`、`update.auto_download`、`voice.handsfree`。

状态：—。

事件：—。

<a id="s07"></a>

### s07 — `system/policy/system_actions`

设置：`ai.read_clipboard`、`audio.child_max`、`audio.volume`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_style`、`display.warmth`、`member.idle_return_s`、`net.prefer`、`notify.dnd`、`power.lock_after_s`、`time.auto`、`time.zone`、`update.auto_download`、`voice.handsfree`。

状态：`alarm.ringing`、`app.cards_rev`、`app.foreground`、`audio.listening`、`audio.mic`、`audio.now_playing`、`audio.recording`、`audio.route`、`battery.charging`、`battery.level`、`battery.low`、`bt.available`、`bt.connected`、`bt.device`、`cell.registered`、`cell.sim`、`confirm.pending`、`file.index_state`、`interrupt.active`、`job.running`、`media.state`、`member.authed`、`member.current`、`net.online`、`net.signal`、`net.type`、`notify.unread`、`pairing.state`、`power.state`、`storage.card`、`storage.free`、`storage.mode`、`storage.user_free`、`text.focused`、`text.voice`、`time.minute`、`time.synced`、`time.zone`、`update.progress`、`update.state`、`update.version`、`usb.connected`、`wifi.ssid`、`wifi.state`。

事件：—。

<a id="s08"></a>

### s08 — `system/services/audio`

设置：`audio.child_max`、`audio.volume`、`voice.handsfree`。

状态：`audio.listening`、`audio.mic`、`audio.now_playing`、`audio.recording`、`audio.route`、`media.state`、`member.authed`、`member.current`。

事件：`audio.record_done`、`media.finished`。

<a id="s09"></a>

### s09 — `system/services/backup`

设置：—。

状态：—。

事件：`backup.changed`、`backup.failed`。

<a id="s10"></a>

### s10 — `system/services/bt`

设置：—。

状态：`bt.available`、`bt.connected`、`bt.device`。

事件：`bt.changed`。

<a id="s11"></a>

### s11 — `system/services/camera`

设置：—。

状态：—。

事件：—。

<a id="s12"></a>

### s12 — `system/services/comm`

设置：`cell.sim_slot`。

状态：`cell.registered`、`cell.sim`。

事件：`call.answered`、`call.ended`、`call.incoming`、`message.received`。

<a id="s13"></a>

### s13 — `system/services/data`

设置：—。

状态：`battery.low`、`storage.user_free`。

事件：`battery.low`、`data.write_failed`。

<a id="s14"></a>

### s14 — `system/services/display`

设置：`display.light`、`display.warmth`。

状态：`power.state`。

事件：—。

<a id="s15"></a>

### s15 — `system/services/file`

设置：—。

状态：`file.index_state`、`storage.card`、`storage.free`、`storage.mode`、`usb.connected`。

事件：`file.changed`、`file.job_done`、`usb.changed`。

<a id="s16"></a>

### s16 — `system/services/hub`

设置：—。

状态：`member.authed`、`member.current`、`pairing.state`。

事件：`pairing.changed`。

<a id="s17"></a>

### s17 — `system/services/net`

设置：`net.prefer`。

状态：`net.online`、`net.signal`、`net.type`、`wifi.ssid`、`wifi.state`。

事件：`wifi.changed`。

<a id="s18"></a>

### s18 — `system/services/notify`

设置：`notify.dnd`。

状态：`battery.low`、`interrupt.active`、`member.current`、`notify.unread`。

事件：`alarm.ended`、`alarm.fired`、`alarm.missed`、`battery.low`、`call.answered`、`call.ended`、`call.incoming`、`data.write_failed`、`message.received`、`notify.posted`、`reminder.missed`、`usb.changed`。

<a id="s19"></a>

### s19 — `system/services/power`

设置：`power.lock_after_s`。

状态：`battery.charging`、`battery.level`、`battery.low`、`power.state`。

事件：`battery.low`。

<a id="s20"></a>

### s20 — `system/services/res`

设置：—。

状态：`job.running`、`net.online`。

事件：`job.finished`。

<a id="s21"></a>

### s21 — `system/services/text`

设置：`ai.read_clipboard`。

状态：`member.authed`、`member.current`、`power.state`、`text.focused`、`text.voice`。

事件：`text.voice_ended`。

<a id="s22"></a>

### s22 — `system/services/time`

设置：`time.auto`、`time.zone`。

状态：`alarm.ringing`、`time.minute`、`time.synced`、`time.zone`。

事件：`alarm.ended`、`alarm.fired`、`alarm.missed`、`reminder.fired`、`reminder.missed`、`time.changed`。

<a id="s23"></a>

### s23 — `system/services/update`

设置：`update.auto_download`。

状态：`net.type`、`storage.card`、`update.progress`、`update.state`、`update.version`。

事件：`update.available`、`update.result`、`usb.changed`。

<a id="s24"></a>

### s24 — `system/ui/app_registry`

设置：—。

状态：`app.cards_rev`、`app.foreground`、`member.authed`、`member.current`。

事件：`app.state`。

<a id="s25"></a>

### s25 — `system/ui/shell/ai_panel`

设置：—。

状态：—。

事件：—。

<a id="s26"></a>

### s26 — `system/ui/shell/confirm_card`

设置：—。

状态：—。

事件：—。

<a id="s27"></a>

### s27 — `system/ui/shell/keyboard`

设置：—。

状态：—。

事件：—。

<a id="s28"></a>

### s28 — `system/ui/shell/member_gate`

设置：—。

状态：`member.authed`、`power.state`。

事件：—。

<a id="s29"></a>

### s29 — `system/ui/shell/navigation`

设置：`audio.volume`、`cell.sim_slot`、`display.font_size`、`display.light`、`display.lock_style`、`display.warmth`、`net.prefer`、`notify.dnd`、`power.lock_after_s`、`voice.handsfree`。

状态：`audio.mic`、`audio.route`、`battery.level`、`bt.connected`、`cell.registered`、`member.authed`、`member.current`、`net.type`、`notify.unread`、`power.state`、`wifi.ssid`。

事件：—。

## 每个设置项的能力覆盖

所有已定义设置都通过 `settings.get` 读取、`settings.set` 写入，由设置表按 id 查找；`settings.list` 列出类型、范围和值。写入继续检查范围、儿童限制、确认和应用执行器，通用能力不跳过这些检查。

| 设置 id | 描述 | 类型 | 读取 / 写入能力 | 允许修改的成员 | 风险 / 确认 | 定义 |
| --- | --- | --- | --- | --- | --- | --- |
| `ai.read_clipboard` | AI 可以读剪贴板 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/services/text/text_service.c](../../system/services/text/text_service.c) |
| `audio.child_max` | 孩子的音量上限 | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `audio.volume` | 音量 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |
| `cell.sim_slot` | SIM 卡 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `display.font_size` | 字号 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `display.light` | 亮度 | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/display/light.c](../../system/services/display/light.c) |
| `display.lock_style` | 锁屏画面 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/ui/shell/navigation.c](../../system/ui/shell/navigation.c) |
| `display.warmth` | 暖光（越大越暖） | `INK_SET_INT` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/display/light.c](../../system/services/display/light.c) |
| `member.idle_return_s` | 大人锁屏多久后要重新输密码（秒） | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 高 / 人、AI 均需确认 | [system/policy/members.c](../../system/policy/members.c) |
| `net.prefer` | 上网方式 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `notify.dnd` | 勿扰 | `INK_SET_BOOL` | `settings.get` / `settings.set` | 成人、儿童（无需白名单） | 中 / 无需 | [system/services/notify/notify_service.c](../../system/services/notify/notify_service.c) |
| `power.lock_after_s` | 自动锁屏（秒） | `INK_SET_INT` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/power/power_service.c](../../system/services/power/power_service.c) |
| `time.auto` | 自动校时（听家庭服务器的） | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `time.zone` | 时区 | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/time/time_service.c](../../system/services/time/time_service.c) |
| `update.auto_download` | 自动下载新版本（流量要钱，默认关） | `INK_SET_BOOL` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `voice.handsfree` | 叫名字（什么时候在等） | `INK_SET_ENUM` | `settings.get` / `settings.set` | 仅成人 | 中 / 无需 | [system/services/audio/audio_service.c](../../system/services/audio/audio_service.c) |

## 状态

| 状态 id | 描述 | 类型 | AI 可读 | 定义 |
| --- | --- | --- | --- | --- |
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
| `cell.registered` | 4G 在线 | `INK_ST_BOOL` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `cell.sim` | 在用的 SIM 卡 | `INK_ST_STR` | 是 | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `confirm.pending` | 等人确认的请求数 | `INK_ST_INT` | 是 | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `file.index_state` | 文件索引状态（ready 可用，unavailable 不可用，changed 已换卡） | `INK_ST_STR` | 是 | [system/services/file/file_service.c](../../system/services/file/file_service.c) |
| `interrupt.active` | 正在打断（none、call、alarm、low_battery） | `INK_ST_STR` | 是 | [system/services/notify/interrupt.c](../../system/services/notify/interrupt.c) |
| `job.running` | 后台有几件事在做 | `INK_ST_INT` | 是 | [system/services/res/task.c](../../system/services/res/task.c) |
| `media.state` | 播放状态（idle、playing、paused） | `INK_ST_STR` | 是 | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.authed` | 这位家人的空间开着 | `INK_ST_BOOL` | 是 | [system/policy/members.c](../../system/policy/members.c) |
| `member.current` | 现在是哪位家人 | `INK_ST_STR` | 是 | [system/policy/members.c](../../system/policy/members.c) |
| `net.online` | 在线 | `INK_ST_BOOL` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.signal` | 信号（dBm） | `INK_ST_INT` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `net.type` | 网络 | `INK_ST_STR` | 是 | [system/services/net/net_service.c](../../system/services/net/net_service.c) |
| `notify.unread` | 没看的通知数 | `INK_ST_INT` | 是 | [system/services/notify/notify_service.c](../../system/services/notify/notify_service.c) |
| `pairing.state` | 加入家庭的进度 | `INK_ST_STR` | 是 | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
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
| `alarm.ended` | 闹钟停了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.fired` | 闹钟响了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `alarm.missed` | 错过了闹钟 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `app.state` | 一个应用的状态变了（foreground、background、frozen、stopped） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s24](#s24) | [system/ui/app_registry.c](../../system/ui/app_registry.c) |
| `audio.record_done` | 录音结束了（存好的文件、时长、为什么停） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `backup.changed` | 这位成员的备份或恢复进度有变化 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `backup.failed` | 这位成员的备份或恢复没有完成 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s09](#s09) | [system/services/backup/backup_service.c](../../system/services/backup/backup_service.c) |
| `battery.low` | 电量低了（level 1 低，2 很低，3 马上关机，0 充上电了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `bt.changed` | 蓝牙耳机的情况变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s10](#s10) | [system/services/bt/bt_service.c](../../system/services/bt/bt_service.c) |
| `call.answered` | 电话接通了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.ended` | 通话结束 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `call.incoming` | 来电 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `confirm.result` | 一个待确认的请求结束了（accepted、rejected、expired、withdrawn、member_changed，确认后还有回执） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s04](#s04) | [system/policy/confirm.c](../../system/policy/confirm.c) |
| `data.write_failed` | 数据没能写进存储 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s13](#s13) | [system/services/data/data_caps.c](../../system/services/data/data_caps.c) |
| `file.changed` | 文件变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `file.job_done` | 后台的拷贝或删除结束了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `job.finished` | 一件后台的事结束了（做好了、失败了、停下了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s20](#s20) | [system/services/res/task.c](../../system/services/res/task.c) |
| `media.finished` | 播放结束了（放完、停止、读不了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s08](#s08) | [system/services/audio/media_service.c](../../system/services/audio/media_service.c) |
| `member.changed` | 家人有变化（加了、删了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s05](#s05) | [system/policy/members.c](../../system/policy/members.c) |
| `message.received` | 收到短信 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s12](#s12) | [system/services/comm/comm_service.c](../../system/services/comm/comm_service.c) |
| `notify.posted` | 有新通知 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s18](#s18) | [system/services/notify/notify_caps.c](../../system/services/notify/notify_caps.c) |
| `pairing.changed` | 加入家庭的进度变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s16](#s16) | [system/services/hub/pairing.c](../../system/services/hub/pairing.c) |
| `reminder.fired` | 提醒到时间了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `reminder.missed` | 错过了提醒 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `text.voice_ended` | 语音输入结束了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s21](#s21) | [system/services/text/text_caps.c](../../system/services/text/text_caps.c) |
| `time.changed` | 系统时间改了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s22](#s22) | [system/services/time/time_caps.c](../../system/services/time/time_caps.c) |
| `update.available` | 发现了新版本 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `update.result` | 升级的结果 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s23](#s23) | [system/services/update/update_service.c](../../system/services/update/update_service.c) |
| `usb.changed` | 电脑连上、断开或 U 盘状态变了 | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s15](#s15) | [system/services/file/file_caps.c](../../system/services/file/file_caps.c) |
| `wifi.changed` | Wi-Fi 的情况变了（连上了、忘掉了） | 不可调用；AI 可订阅 | 低 / read | 不适用 | 按原事件成员及代际投递 | [s17](#s17) | [system/services/net/wifi.c](../../system/services/net/wifi.c) |
