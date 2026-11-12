# notify-svc-1：通知服务搬 PebbleOS 的勿扰、提醒偏好和快捷回复（存储和定时换成我们的，按家人分开）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11 条；第 8 条不碰设备）。

## 为什么

用户 2026-11-12（D-129）：“移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验”。D-131：外壳（含控制中心和通知）先做到成熟可用。

现在的勿扰只是一个开关：设置 `notify.dnd`（`system/services/notify/notify_service.c:697-700`），在三处判断——高优先级提醒（`:231`、`:331`）和来电响不响（`:567`）；闹钟总是响（`:615`）。没有定时、没有例外（谁的来电能进来）、没有快捷回复；每条通知只能带一个动作（`system/include/ink/sys/notify.h:24-35`）。

两份调研一致（`docs/tasks/frameworks-research-1/SHELL.md`；主会话另一份调研第 2 节）：页面和行为照安卓（AOSP SystemUI），通知、勿扰、动作、快捷回复的服务层逻辑搬 PebbleOS——所查候选里唯一成套的 C 实现。界面是另外的任务（`notify-ui-1`、`control-1`），本任务只做服务。

## 成熟来源

- **搬代码**：PebbleOS，`github.com/coredevices/PebbleOS` 提交 `aa0c27e8330d010e9202f9a706a56002db5861ee`（只读克隆到你的 `work/`）：
  - `pebble/services/notifications/do_not_disturb.c`：手动、定时、日历忙时三种来源任一生效（242-265 行），跨午夜的时段（148-185 行）；
  - `pebble/services/notifications/alerts_preferences.c`：例外掩码（只放来电等）、提醒方式、背光、静音喇叭（36-55 行）；
  - `pebble/services/timeline/timeline_actions.c`：快捷回复和默认 5 条（795-830 行），回复菜单入口（1143-1168 行）；
  - 设置页里的用法只看不搬：`pebble/apps/system/settings/quiet_time.c:56-110`、`:604-621`。
  - `notification_storage.c` **不搬**：我们已有按家人分开的通知箱，不建第二个通知库。
- **行为对照**（只照规则，不搬代码）：AOSP 勿扰例外，`github.com/LineageOS/android_frameworks_base` 提交 `22de88b7b8eb979c816669c7963dee66ae4953cd` 的 `services/core/java/com/android/server/notification/ZenModeFiltering.java:185-267`（闹钟、来电、重复来电、消息分别判断；重复来电是同一号码 15 分钟内第二次）和 `core/java/android/service/notification/ZenPolicy.java:181-197`、`:258-278`（任何人 / 联系人 / 收藏 / 无人）。MuditaOS 提交 `2cd0e4721405c0bf263f311f27d38aadae6ac89f` 的 `module-apps/application-settings/windows/phone-modes/DoNotDisturbWindow.cpp:43-71`（勿扰时收藏联系人来电例外）。

## 要做的

1. **原文入库**：上面要搬的文件整文件拷进 `system/services/notify/vendor/pebble/`，旁边 `SOURCE.md`（来源提交、每个文件 SHA-256、改了哪几行和为什么）和许可证（以该提交根目录的 LICENSE 和文件头为准，照原样带上）。非改不可的只有接口替换：
   - 存储：Pebble 的偏好文件 → 我们按家人分开的设置或数据集合（`notify.prefs` 已有，`notify_service.c:42`）；
   - 定时：Pebble 的定时器和整点回调 → 我们的时间服务（`system/services/time/`）的定时；
   - 事件：Pebble 的事件服务 → `ink_cap_emit`；
   - 日历忙时：查我们时间服务里“现在是否在一个非全天提醒的开始和结束之间”；没有现成接口就先不接这一项，写进报告，不新建日历服务。
2. **勿扰模型（每位家人一份）**：手动开关（沿用 `notify.dnd`，含义不变，老数据照读）、多个定时时段（工作日、周末分开，可以跨午夜）、日历忙时。对外给一个状态：现在是否在勿扰、因为哪一条、到几点结束（例如“勿扰 · 到 7:00”，控制中心和锁屏会用）。
3. **例外**：来电——所有人 / 只有联系人 / 谁都不放（“只有收藏”等联系人加了收藏再接，`contacts-3` 在做，先留枚举值并写进报告）；重复来电——同一号码 15 分钟内第二次照响（照 AOSP）；闹钟——永远响（现有行为不变）。判断“是不是联系人”用现有的 `ink_contacts_match(号码, true, …)`（`system/apps/contacts/contacts.c:52`，和通知服务编在同一个库里；`contacts-3` 今晚要改 `phone.contacts` 的记录格式，但保证这个函数的用法不变，所以**不要直接读 `phone.contacts` 数据集合**）。来电事件跑在模组任务里（`notify_service.c:564` 的注释），查联系人可能碰存储：放到后台执行器里查，别在模组任务里阻塞；来电卡照常先出来，查完再决定响不响（铃声晚一点开始可以），报告写模拟器里实测晚了多少毫秒。把 `:231`、`:331`、`:567` 三处判断换成一个集中的判断函数，一个根因只改一处。
4. **通知带多个动作和快捷回复**：
   - 通知记录加最多 3 个动作（每个是：文字、能力、参数）和一个可选的“回复目标”（能力加参数模板，例如 `sms.send` 加对方号码）；`notify.inbox` 的记录版本从 1 升到 2，老记录照读（`notify.h` 现有字段和函数保持源码兼容）。
   - 默认 5 条快捷回复，照 Pebble 默认那 5 条的意思译成自然的中文（例如“好的”“是”“不是”“给我回个电话”“我晚点给你回电话”），每位家人可以改（存在自己的偏好里）。
   - 新能力：执行某条通知的第几个动作（例如 `notify.act`）、回复某条通知（例如 `notify.reply`，用第几条快捷回复或一段文字）、读和改勿扰时段与例外。按钮和 AI 调同一个能力；回复时把发起者原样传下去：人在屏幕上点的，属于人亲口要求，直接发（D-105）；AI 发起的，照现有策略闸门（发消息是对外动作，没有 `_meta.asked` 就弹确认卡）。
   - 接上现有来源：短信通知（`notify_service.c:637-645`）加“回复”（回复目标 `sms.send`，对方号码）和“打开会话”（`sms.thread`）；未接来电（`:590-596`）加“回电”（`call.dial`）和“发短信”（`sms.compose`）。能力名以代码里实际有的为准。
   - 清掉一条通知只删通知箱里这一条，不动短信原文。

## 范围和预算

- 只改：`system/services/notify/`、新增 `system/services/notify/vendor/pebble/`、`system/include/ink/sys/notify.h`（加字段，旧接口不变）、时间服务里接定时需要的最少几行（列进报告）、通知相关测试、CMake 里新目录的那几行、`docs/tasks/notify-svc-1/`。
- 不碰：控制中心和状态栏（`control-1`）、通知列表页面（以后 `notify-ui-1`）、`system/services/comm/comm_service.c`（`phone-3`）、短信和电话应用（`sms-3`、`phone-3`）、联系人应用（`contacts-3`）。
- 胶水不超过 400 行（拷来的原文不算）；固件增量不超过 20 KB。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。

## 验收

1. **照安卓的勿扰走一遍**（时间注入，不盲等）：设定工作日 22:00–07:00 勿扰、例外“只有联系人”和“重复来电”，然后：
   - 周三 23:30 联系人来电 → 响；陌生号码来电 → 不响，但来电卡照常出现、未接后进通知箱；同一陌生号码 10 分钟后再打 → 响；
   - 06:59 仍在勿扰，07:00 结束；周六 23:30（周末另设了时段或没设）按周末规则；
   - 勿扰中闹钟照响；
   - 手动开勿扰盖过定时；状态里写清“因为哪一条、到几点”。
   反向对照：去掉跨午夜处理，23:30 和 06:59 的用例必须失败。
2. **家人分开**：大人 A 设了时段，换成孩子 B 不受影响；B 自己的设置换回 A 后不影响 A；重启后都还在。
3. **快捷回复**：收到一条短信通知 → 用 `notify.reply` 选第 1 条快捷回复（来源是触摸）→ 短信服务收到 `sms.send`，号码和文字都对，没有确认卡；同样的调用来源是 AI 且没有 `_meta.asked` → 弹确认卡（D-105）；未接来电通知的“回电”调的是电话能力。按钮和 AI 调同一个能力，结果一样。
4. **老数据**：用升级前格式写的通知箱记录读出来字段齐全；`notify.dnd` 老设置照旧生效。
5. 这是服务任务，没有页面改动：“改前改后对照”用一张表代替截图——同一组来电和通知（联系人、陌生、重复、闹钟、短信）在改前和改后响不响、进不进通知箱，写进报告。
6. 主工程普通全量 ctest 过；ASan 跑通知和来电相关测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、产品代码行数、`gcccheck` 结果，以及给 `notify-ui-1` 和 `control-1` 用的接口清单（状态名、能力名、字段）。
