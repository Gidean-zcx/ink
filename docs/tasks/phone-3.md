# phone-3：电话照 Fossify Phone 重写——输号码就列出匹配的联系人、最近通话按人和时间归组、通话中的控制和真实状态（含对方忙、没人接）；匹配规则对照 Lineage Dialer；LilyPhone 的号码比对、来电查名和通话显示整函数搬进来

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 2、11 条；第 8 条不碰设备）。

**D-140（总规则第 11 节）**：本任务是“照着重写，片段可搬”。

- **第一步**：把对照表提交成 `docs/tasks/phone-3/对照表.md`，单独一个提交，写代码之前。主会话随本任务书给了一份草稿，可以补行、改行，但每一行都要有决定；改了决定写明理由。
- **LilyPhone 片段必须拷原文**（下面“搬什么”列的整函数）。`system/apps/phone/vendor/lilyphone/SOURCE.md` 记项目、固定提交、文件和 SHA-256、改了哪几行、为什么。不许看完原文按自己的风格重写一遍。
- **其余照对照表重写**。参照安卓的地方按第 11 节为这台设备改；做完逐行验收，每行配截图或测试名。

## 为什么

用户 2026-11-12：“所有的应用和系统应用都是移植的最优秀的三方项目吗 我怎么感觉不太像呢”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；“我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131）。2026-11-13：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”（D-140）。

调研（`docs/tasks/replan-sysapps/APPS.md` A01、`CURRENT.md` A01）的建议：

- 搬 Fossify 的拨号、最近通话、通话中三段工作流；
- 搬 LilyPhone 里能拆出来的显示逻辑；
- Lineage 的匹配和状态逻辑当行为对照；
- 保留现有 `call.*`、紧急呼叫和家人规则；
- 不能把模组回 OK 显示成已接通。

交付计划第 8 节并进来的已知问题：“拨号页提示文字压住键盘”（现状见下面，原来那种压住已经修过一半）。

### 现状（基线 `16e7be5f`，已含 design-lang-2、home-1、lock-1、control-1、settings-1、notify-svc-1、graft-mogo-4、audio-out-2）

**`system/apps/phone/phone.c`（490 行，一个文件）**

有四个页面：`Phone`（拨号）、`PhoneRecents`、`PhoneCall`、`PhoneKeypad`，另有锁屏紧急呼叫浮层 `PhoneEmergency`（`:48-140`）。

- **拨号页**（`create` 的最后一段，`:416-444`）：
  - 号码框在上，12 个键（`INK_DIGIT` 宽）在下；拨打是黑色圆圈图标（`ink_ui_circle(... "phone" ...)`，`:441`），右下角是“删除”。
  - 联系人匹配只给**一个**结果：`ink_contacts_match` 拿到第一个就返回（`system/apps/contacts/contacts.c:52-60`），名字显示在 `P.match`。
  - `P.match` 和出错提示**是同一个标签**（`P.match = P.notice`，`:425`），所以出错时联系人名字被盖掉，点出错文字还会打开 `contacts.view`。
  - 每按一个键都走 `phone.key` → `key_cap` → `ink_ui_redraw()` 整页重建（`:265`）。拨号回执成功后也整页重建（`:219`）。
- **“提示压住键盘”的现状**：
  - small-gaps-2 已经把提示挪到拨打键左边（`:435-436`），`tests/small_gaps_2/ui.c:58-80` 在默认和最大字号下断言“没有完成，请重试”不压任何数字键。
  - 还剩三个问题：一是上面说的和名字共用一个标签；二是这一栏只有约 198 px 宽（`(684-163)/2-38-24`），长一点的回执（比如“锁屏时只能拨紧急电话”、飞行模式那句）会折成 4～5 行，往屏幕底边外跑，测试只测了那句短的；三是拨打键是圆圈。
- **最近通话**（`:403-415`）：
  - 4 行一页的 `ink_ui_list`，每行写号码和“打来 · N 分钟 14:05”。点一行是 `phone.open`（把号码填进拨号盘）。
  - 缺的东西：没有联系人名字，没有日期（不同天同一时刻看不出来），不归组，不能删，没有详情。
  - 行后面垫了一张 `ink_ui_card`（`:413`）。
  - `bind_item` 每画一行都从头走游标（`:270`）。
- **通话记录格式**（`:166-170`）：
  - 字段是 `{"number","kind","minutes","at"}`，`kind` 存的是中文原文 `打来/未接/打出`（`TR_NOOP`）。`minutes` 向下取整，59 秒记成 0 分钟。
  - 集合 `phone.calls` 的 schema 是 1（`:21-22`），没设 `max_records`/`drop_oldest`：存满 256 KiB 默认上限后，写入会被拒。
  - 注意：备份恢复 `ink_db_import` 要求快照的 schema 和集合定义一致（`system/services/data/datastore.c:1118`，`backup_service.c:1009` 调用）。**把集合 schema 升到 2，旧备份里的通话记录就恢复不回来了。**
- **通话页**（`:342-402`）：
  - 号码上面有一个头像圆圈（`:350`）。只显示号码，不查联系人名字。下面是状态行和“N 分钟”（`minute_changed` 每分钟只改那一块，`:206-214`）。
  - 四个格子（静音、键盘、耳机/耳机未连、录音），每个都是圆圈图标套在框里（`:387-397`）。挂断是黑色圆圈（`:359`）。
  - 状态只有四种：来电、正在拨号、通话中、通话已结束。结束后没有时长，也没有原因。
  - 状态一变就 `ink_ui_redraw()` 整页重建（`:203`）。
  - “录音”点了只说明不能录（`:401`），参照里没有这个按钮。
- **“已受理”和“已接通”**：现在已经分开了——`call.dialed` 显示“正在拨号”，`call.connected` 才显示“通话中”（`:197-198`）。但没有反向对照测试守着。
- **外面依赖电话的地方**：
  - `ink_phone_activity`（`:486-490`）拿 `P.status` 跟 `"通话中"` 比，状态栏（`system/ui/shell/status_bar.c:40`）和“正在进行”卡（`activity_card.c:80`，按钮 `phone.return`）都在用。所以接通后状态文字必须仍是“通话中”，结束文字不能叫“通话中”。
  - `ink_phone_tabs` 联系人页在用（`contacts.c:209`）。
  - `ink_shell_emergency_button` 锁屏和家人页在用（`lock_mark.c:257/265/307`、`member_pages.c:140`、`member_gate.c:185`；lock-styles-1 这批会动 `lock_mark.c`）。

**`system/services/comm/comm_service.c`（840 行，本批只归本任务）**

- 拨号受理后发 `call.dialed`（`:608`）。
- 模组报 `+CLCC` 去电接通时发 `call.connected`（`:418`）。
- `NO CARRIER` 时发 `call.ended`（`:419-430`），**不带原因**。
- 来电卡在通知服务（`system/services/notify/notify_service.c:594-640`）：接听、挂断；没接的发“未接来电”通知，带“回电”“发短信”。

**模组层只认 `NO CARRIER`**

- 驱动：`ports/sim/hal/hal_cellular_sim.c:78-81`、`ports/esp32s31/hal_cellular_s31.c:136-140`。真机的 URC 分类也没有 `BUSY`、`NO ANSWER`（`ports/esp32s31/s31_modem.c:48-50`）。
- 模拟器的 AT 模型：去电只会被接通或挂断（`sim_net_remote_answer/hangup/auto_answer`，`ports/sim/hal/hal_net_sim.c:1154-1166`），结束一律推 `NO CARRIER`（`:826-845`）。
- S31 模组宿主测试 `ports/esp32s31/tests/cell.c`（`:112-114` 已断言 `HAL_CELL_EV_CALL_ENDED`）：新加的 BUSY/NO ANSWER 在这里加断言。
- 原厂固件没有通话代码（`ink-hw/upstream/Metalio-E-INK4-Plus` 的 690a023 和 874538e 都搜不到 `NO CARRIER`/`BUSY`）。所以现在**分不出对方拒接和没人接**。

**汉字转拼音**：系统没有公开的反查接口。`system/services/text/pinyin.c` 只有拼音到字的 `py_lookup`（和 `py_lookup_nine`），声明在文本服务内部的 `text_internal.h`。

**联系人**

- 联系人集合可以只读不改文件地打开：`ink_db_find_def("phone.contacts")` 加 `ink_db_open_cached_member`（`system/include/ink/sys/data.h:89,99`）。
- `contacts.edit` 只收 `id`，不收号码（`contacts.c:316`）。所以“加为联系人”现在没法把号码带过去。
- `sms.compose` 收 `number`（`system/apps/sms/sms.c:457`）。
- **contacts-3 会改记录格式**：它要把 `phone.contacts` 记录改成多号码（记录版本加一，老记录读成一个“手机”号码），同时承诺 `ink_contacts_match(query, exact, name, number)` 的签名和含义不变。它的任务书没提 `contacts.edit` 收号码。本任务读联系人的代码要经得住这个变化，见“共同要求”。

**直接 `#include` phone.c 或改 phone.c 字符串的测试**

- 直接 include：`tests/phone_calls/phone_calls.c:3-5`、`tests/small_gaps/phone.c:3-6`、`tests/emergency_call/emergency_call.c:3`。
- 按原文替换做反向对照：`tests/phone_calls/reverse.py:11`、`reverse_controls.py:39`、`tests/activity_card_reverse.py:18`、`tests/small_gaps/reverse.py:29`、`tests/small_gaps_2/reverse.py:64`、`tests/design_lang_2_reverse.py:31`、`tests/audit7_fix/run.py:49`、`tests/emergency_call/run.py:51,67`。
- 拆文件或改这些原文时，这些测试要跟着改，而且一样严格（见验收 8）。

## 来源（固定提交）

**Fossify Phone**（第一参照，照着重写；GPL-3.0，不拷代码），`github.com/FossifyOrg/Phone` 提交 `25804aa81061868c0f5d5b4ad4eefc544507380c`，路径前缀 `app/src/main/kotlin/org/fossify/phone/`：

- `activities/DialpadActivity.kt`：
  - `:295-360` `dialpadValueChanged`：号码包含或名字按键字母包含都算匹配，号码命中的排前面；输入为空时列出全部联系人；
  - `:339-342` 点候选直接拨；
  - `:362-376` `initCall`：号码为空时按拨打，填上最近一次的号码；拨出后 `clearInputWithDelay`；
  - `:392-401` `speedDial`；
  - `:254` 加为联系人；
  - `:204-207` 删除、长按清空。
- `activities/CallActivity.kt`：
  - `:469-500` 声音去向；`:501-512` 静音；`:513-566` 通话键盘（`:523-566` 带缩放动画）；
  - `:270-360` 滑动接听（带循环箭头动画）；
  - `:574-651` 对方名字和用的卡；
  - `:653-678` 按通话状态切页面；`:732-750` 拨号中、来电、接通；
  - `:760-790` 结束：有时长显示“01:05 (Call Ended)”，3 秒后关页；
  - `:820-829` 每秒计时。
- `helpers/RecentsHelper.kt:90-125`：归组规则——连续、同一张卡、同一天、号码相同（另有“名字都有且不同不归组”，我们的记录不存名字，用不上）；`:287-310` 删除。
- `fragments/RecentsFragment.kt:134-168`：空状态、点一行回拨。
- `adapters/RecentCallsAdapter.kt`：`:351-445` 行菜单（打电话、发短信、查看联系人、加为联系人、复制、通话详情、拉黑、删除）；`:460-540` 行内容（名字和次数、类型、时间、时长）。
- `dialogs/ShowGroupedCallsDialog.kt`：通话详情。
- `activities/ManageSpeedDialActivity.kt:64-100`：快捷拨号设置。
- `activities/MainActivity.kt:276-285`：清空通话记录。
- 文案在 `res/values/strings.xml`。

**Lineage Dialer**（规则对照），`github.com/LineageOS/android_packages_apps_Dialer` 提交 `3b9a8e35d2bc2c804ee1c64f9b291630a00e333d`：

- `java/com/android/dialer/smartdial/util/SmartDialNameMatcher.java:117-181`：号码从开头匹配，或跳过国家码后从开头匹配。注意它**不是**“任意位置包含”，旧任务书写反了。
- 同一文件 `:400-434`：名字按键匹配必须从词首开始（“846 能匹配 Phillips Thomas，466 不能”）。**本任务名字匹配采用这条**，不用 Fossify 的“包含”。
- `java/com/android/incallui/CallCardPresenter.java:394-446`：按真实状态和断开原因显示（`setDisconnectCause`，`:420`）。
- `incallui/call/state/DialerCallState.java:22-38`：DIALING、ACTIVE、DISCONNECTED 分开。

**LilyPhone**（片段可搬），`github.com/dparnell/lilyphone1` 提交 `de10fc3882292b9be1cfcd14632726590fdef262`，C++/Arduino、LVGL 8：

- **许可证**：仓库里没有许可证文件，GitHub 显示 license 为空，两个源文件也没有版权头。按 D-129 用户原话“不需要在乎开源协议”照搬，`SOURCE.md` 如实写明这一点并保留作者和出处。

## 搬什么、怎么编进来（第 11 节）

原文从固定提交拷。shallow clone 后核对 SHA-256：`src/apps/phone_store.cpp` = `026492851b85169a6b7c16fe251b06c55ac336cd090c715f93864847a0e8db0d`（和 `replan-sysapps/sources.json` 一致），`src/ui/ui_phone1.cpp` = `4515a8a8f64b8ed35dbacf6ddc6e278aa175ca3997e467b872ab01b6ed3240cb`。（评审时已核对：两个哈希和行号都对得上。）

| 原文（整函数） | 放到 | 用途 | 怎么编 |
| --- | --- | --- | --- |
| `phone_store.cpp:111-138` `phone_number_match` | `system/apps/phone/vendor/lilyphone/phone_store.c` | 两个号码是不是同一个人：通话页、最近通话、来电查名字；也当最近通话归组的“号码相同”（代替安卓的 `PhoneNumberUtils.compare`） | 纯 C 写法，按 C 编译（文件名从 `.cpp` 改 `.c`，`SOURCE.md` 记一笔）；去掉 `Arduino.h`、`FS.h`、`SPIFFS.h`，换成 `<ctype.h> <string.h> <stdbool.h>` |
| `phone_store.cpp:190-196` `contacts_find_by_number`、`:198-203` `contacts_display_name` | 同上 | 按号码找联系人、显示名字 | 原文读全局表 `contact_list`/`contact_num`。胶水层在 PSRAM 里放这张表（最多 200 人，和 `phone.contacts` 的上限一样），页面打开或联系人变了时从当前家人的集合填一次 |
| `ui_phone1.cpp:3262-3279` 三个静态变量（`scr8_1_status`、`scr8_1_answer`、`scr8_1_shown_state`）和两个按键回调 `scr8_1_answer_event`、`scr8_1_hangup_event`（`scr8_1_timer` 变量不搬） | `system/apps/phone/vendor/lilyphone/call_card.c` | 下面两个函数依赖它们才能编过；接听、挂断的按键入口 | C 编译。`scr8_1_hangup_event` 里的 `scr_mgr_pop(false)` 必须删（结果留在通话页，不自动关） |
| `ui_phone1.cpp:3280-3318` `scr8_1_render` | 同上 | 通话状态变成文字；“接听”只在来电时出现 | C 编译。LVGL 8 的调用（`lv_obj_clear_flag` 等）靠 LVGL 9.5 自带的 `lv_api_map_v8.h`（`build/_deps/lvgl-src/src/` 里有）编过。原文 default 分支“Call ended”由胶水加的“已结束”状态接管 |
| `ui_phone1.cpp:3334-3383` `create8_1` | 同上 | 通话页上半部：名字（查名）、名字来自通讯录时另起一行写号码、状态行、接听/挂断那一排 | 同上 |

**老实写在 `SOURCE.md` 里的边界**：搬来的只是名字、号码、状态文字、接听和挂断这一段。结束原因、按秒的结束时长、“再打一次/完成”、静音/键盘/声音去向三个控制、通话中分钟刷新，原文里都没有，是我们新写的胶水（算产品代码行数）。

**替身头文件** `vendor/lilyphone/lily_compat.h`，算胶水：

- `phone_store.h`：`contact_t`；长度改成我们的 97 和 25，`CONTACTS_MAX` 改成 200。
- `modem_call_state_t`、`ui_phone_get_call_state/number/duration`、`ui_active_number`：接到 `P` 里的通话状态。
- `ui_phone_answer`、`ui_phone_hang_up`：接到 `call.answer`、`call.hangup`。
- `FONT_BOLD_SIZE_20/16/15`：换成 `ink_ui_font_px(INK_FONT_TITLE/BODY/NOTE, …)`。
- `DECKPRO_COLOR_FG`：换成 `ink_ui_ink()`。
- `scr_action_bar_create`、`scr_bar_btn_create`：换成 `ink_ui_pill`。
- `ui_disp_full_refr`：换成空操作。改文字时 LVGL 只让那个标签的区域失效，局刷自然只刷它；需要时对该标签调一次 `ink_ui_hint_obj(label, INK_REFRESH_FAST)` 给波形意图（它不决定刷哪块，`system/ui/ui_task.c:340`）。

**原文里必须改的行**（`SOURCE.md` 逐行写原文、改成什么、为什么；改动合计不超过 80 行，超了先停下说明）：

- `MATCH_DIGITS 7` 改成 `11`：中国手机号 11 位，末 7 位相同会把 138xxxx5678 和 139xxxx5678 当成同一个人；改成 11 位后，`+8613812345678` 和 `13812345678` 照样算同一个，10086 这类短号照旧要整串相同。
- 英文字符串换成 `TR("正在拨号")` 等，不然中文界面出英文，翻译检查也过不了。
- 写死的坐标（`62`、`126`、`lv_pct(92)`、高 18/48/58）换成主题令牌加 `ink_ui_px`（`tools/ui_scale_check.py` 要过）。
- `Connected %02u:%02u` 按秒走，改成“通话中”加分钟（第 11 节：墨水屏不跑秒）。
- 状态变了整屏刷（`ui_disp_full_refr`），改成只改状态行。
- 删掉 `scr_back_btn_create` 那一行（返回由外壳负责）；删掉 `scr8_1_hangup_event` 里的 `scr_mgr_pop(false)`。

**不搬**（写进 `SOURCE.md`）：

- `contacts_reorder`、`contacts_add`（`phone_store.cpp:207-251`）：改通讯录是联系人应用的事（contacts-3）。
- SPIFFS 存储。
- `peri_modem.cpp` 的模组层：只拿它 `:508-510`、`:615-616` 认 `BUSY`/`NO ANSWER` 的做法当证据。
- 3 秒定时重画：`scr8_1_timer_event`（`:3322`）和 `entry8_1` 里的 `lv_timer_create`。原因是不轮询，只在 `call.*` 事件和每分钟时更新。
- 拨号页那一行名字 `scr8_update_name`（`:3124-3133`）：被 Fossify 的多候选取代。

**测试**：LilyPhone 没有主机测试（`test/` 只有 README），所以给 `phone_number_match` 新写主机单测，覆盖：

- +86、0086、带空格和横线的写法；
- 10086 要整串相同；
- 8 位座机和带区号的写法不算同一个；
- 138 和 139 后 8 位相同不算同一个（改位数前这条会算成同一个，留作反向对照）。

## 要做的

1. **对照表先提交**：用草稿起步，逐页、逐操作、逐状态写清照做、按设备改（怎么改、为什么）还是不做（为什么）。少做的功能列进“要用户定”。

2. **拨号页**（照 Fossify `DialpadActivity`，规则对照 Lineage）：
   - 布局三段：号码框在上（用共用输入框，自带 108 px 清除键，代替“长按删除清空”）；中间是候选区（默认字号两行、最大字号一行，翻页写页码，盖板上一页、下一页键能翻）；下面是 12 个键（高不低于 108 px）。最底下一排：“删除”和唯一的主按钮“拨打”（文字按钮，不套圆圈）。
   - **候选**：联系人的号码包含所输数字，或者名字按键字母从词首开始匹配（拉丁字母，照 Fossify 的键位表，词首规则照 Lineage），就算匹配。排序照 Lineage：号码从开头命中的（包括跳过 +86、0086 后从开头命中）排在中间命中的前面，号码命中排在名字命中前面。之后是最近通话里不在通讯录的号码（参照没有，旧任务书要求，标“最近通话”）。输入为空时列出全部联系人（照 Fossify）。
   - **点候选**：只把号码填进号码框，不直接拨出，再按“拨打”才拨。这是按设备改：墨水屏刷新慢，候选在按键之后才换，手指落下时那一行可能刚换成别人，直接拨出就打错了，而且撤不回。
   - 号码为空时按“拨打”，填上最近一次通话的号码（照 Fossify）。
   - **中文名字**：没有反查拼音的接口，只做号码匹配和拉丁字母名字匹配，写进“要用户定”。
   - **按键**：原地改号码框和候选区，一次操作只刷一次（两块合在一次局刷）。不再 `ink_ui_redraw`。`phone.open` 在拨号页已经开着时原地填号，不重建页面。
   - **拨出后**：Fossify 延时清空号码框；这里不用定时器，进入通话页时随换页一起清空。
   - **出错和说明**：放在候选区自己的一行，不和联系人共用标签，不压键盘，最大字号下也在屏幕内。长回执折行，放不下就截短，全文进快照。提示留到下一次操作时一起消失，不自动消失。飞行模式照旧弹 `ink_shell_airplane_card`。
   - **卡和网络**：拨号页一行字写当前用的卡和状态（“卡 1”“内置卡”“没有卡”“没有信号”“飞行模式”，读 `cell.*` 状态）。拨号不弹选卡（D-128）。
   - **加为联系人**：有号码又没匹配到联系人时，出现文字按钮“加为联系人”，调 `contacts.edit`。号码现在带不过去，写进报告交 contacts-3，不改联系人文件。

3. **拨出到挂断，状态要真实**（照 Fossify `CallActivity`、Lineage `CallCardPresenter`，显示部分用搬来的 `create8_1`/`scr8_1_render`）：
   - 受理后显示“正在拨号”；模组报接通后才显示“通话中”。**模组回 OK 不能显示成已接通**，加反向对照。
   - **结束原因**（`comm_service.c` 的 `call.ended` 加 `reason`）：

     | 情况 | 怎么认 | 显示 |
     | --- | --- | --- |
     | 接通过 | — | “通话结束 · 1 分 05 秒” |
     | 对方忙或拒接 | `BUSY` | “对方忙或拒接” |
     | 没人接 | `NO ANSWER` | “没人接” |
     | 接通前断了，又不是自己挂的 | 其他 `NO CARRIER` | “没有接通”（号码不通等都落在这里，模组不报原因就分不出） |
     | 接通前自己挂了 | `cap_hangup` 先到 | “已取消” |

   - **模组层**（为结束原因放宽的范围，只加不改）：
     - `platform/include/ink/hal_cellular.h`：`hal_cell_event_t` 加结束原因字段；
     - 两个 HAL 认 `BUSY`、`NO ANSWER`；`s31_modem.c:48` 的 URC 列表加这两项；
     - 模拟器的 AT 模型加“对方忙”“对方不接”两个钩子，推标准的 `BUSY`、`NO ANSWER`，声明加在 `simulator/sim.h` 末尾（单独一个带注释的块）；
     - `ports/esp32s31/tests/cell.c` 加对应断言。
     - 真机 NT26 会不会发这两个，不能凭推断，列进“上机先核对清单”。不发的话，两种情况都显示“没有接通”，页面不出错。
   - **结束后**：结果留在通话页，下面两个按钮“再打一次”和“完成”（回最近通话）。不照 Fossify 3 秒自动关页：不为关页多刷一次屏。

4. **通话页**：
   - 上半部是 LilyPhone 的名字、号码、状态。名字靠 `contacts_display_name` 查，现在只显示号码。
   - 去掉头像圆圈。下面一行写用的哪张卡。
   - 控制是三个写着字的按钮：静音/已静音、键盘、声音去向。声音去向在耳机连着时在“喇叭/耳机”间切（`call.route`），没连时写“耳机未连”，点了打开蓝牙页（现有）。状态靠字和实心、空心，不靠颜色；等模组确认以后才改字（现有 `call.controls`）。
   - 唯一的主按钮是宽的“挂断”文字按钮。
   - 来电响铃时如果打开了通话页，显示“接听”（LilyPhone：只在来电时有）。
   - 通话中时长显示“N 分钟”，每分钟只刷那一块（`P.duration` 的文字格式不变，`tests/small_gaps/phone.c` 照旧过）；结束时给到秒。
   - 通话键盘照旧走 `call.dtmf`，是独立页面，不做 Fossify 那种缩放淡入动画。
   - 状态变化原地改，不再整页重建。
   - “录音”按钮写进“要用户定”：参照没有，现在点了只说不能录。建议从页面拿掉，`call.record` 能力保留；用户定之前先不动。

5. **最近通话和详情**：
   - **归组**：照 Fossify `RecentsHelper:90-125`——连续、同一张卡、同一天、号码相同（用搬来的 `phone_number_match`）归一组，行里写次数。
   - **每行**：有联系人写名字，否则写号码；第二行用字写类型（“未接”“打来”“打出”，加线条图标；未接用粗体，不靠颜色），加日期时间（“今天 14:05”“昨天 09:10”“11月10日”，不插日期分隔行），没接的不写时长。
   - **点一行**：把这一组的号码填进拨号页，不拨出（保持现在的行为）。Fossify 是点一行直接回拨；这里按设备改：翻页后列表刷新有延迟，手指落下时那一行可能已经换了，直接拨会打错号还产生电话费，撤不回。想直接拨的，进详情页点“打电话”或在拨号页按“拨打”。“点一行直接回拨”是否要，列进“要用户定”。
   - **详情**：行右边有写着“详情”的按钮（不小于 108 px），代替 Fossify 的三点菜单。注意 `ui_list` 每行只能登记一个动作（`system/include/ink/sys/ui_list.h:11-13`），“详情”要另外登记，快照里两个都要列出来。
   - **详情页**：独立页面，不是弹窗。列这一组每一条（时间、类型、时长、用的卡）。动作有打电话、发短信（`sms.compose`）、查看联系人（`contacts.view`）或加为联系人、删除这一组。
   - **删除和清空**：删一组可以撤销（D-105：能撤销就不弹确认），照 `ink_cap_undoable`，页面上“已删除 · 撤销”留到下一次操作。清空全部记录照 Fossify 放在最近通话页，属于找不回来的删除，走确认卡。时间不够就不做，写进“没做到的”。
   - **记录格式**：**不升集合 schema**（会让旧备份恢复失败，见现状），只加字段：`seconds`（按秒）、`result`（结束原因）、`sim`（用的卡）。`minutes` 照旧写，给老读者和 AI 看。读老记录时，没有 `seconds` 就按 `minutes × 60`。集合加 `max_records = 500` 加 `drop_oldest`（安卓通话记录也只留最近 500 条；`drop_oldest` 按 `sort` 即通话时间挤掉最旧的）。
   - 归组索引在 PSRAM 里，刷新时建一次，不再每行从头走游标。

6. **快捷拨号**（照 Fossify 的 1～9，排在最后，时间不够就写进“没做到的”，对照表对应行标“未做”）：
   - 不用长按：只输入一位、而这一位设了快捷拨号时，候选第一行写“快捷拨号 2 · 妈妈”，点了填号，再按“拨打”。
   - 设置页列 1～9，点一位从联系人里选（`contacts.pick`），每行有“清除”。
   - 每位家人自己一套，存新的成员集合 `phone.speed`。在 BG 上用 `ink_db_open_member` 读，不走 `data.list`：孩子用完时间以后，`data.list` 只放行三个集合（`system/policy/cap.c:150-153`，即 `phone.contacts`、`phone.calls`、`sms.messages`）。

7. **紧急呼叫**（D-119）：锁屏不解锁、飞行模式下都能拨 110、120、119、112 的流程和限制不变。`ink_shell_emergency_button` 签名不变。样式要动就照新设计语言，`emergency-call` 测试照过。

8. **能力和快照**：
   - 每个点按都是能力，按钮和 AI 走同一个入口。现有能力名（`phone.*`、`call.*`、`contacts.selected`）不改名。
   - 新增的能力（建议 `phone.detail`、`phone.delete`、`phone.clear`、`phone.speed_set`，名字可以调）进 `tests/integration/ai_surface.c` 两张表。
   - 快照写清可见内容和动作：
     - 拨号页：号码、候选（名字、号码、来源）、卡和网络、提示；
     - 通话页：名字、号码、状态、结束原因、时长、静音、声音去向、卡、按键位数（按键内容照旧保密）；
     - 最近通话：组、次数、类型、日期、时长；
     - 详情页：每一条和动作。

## 按设备改（汇总；逐行见对照表）

- **墨水屏**：
  - 不跑秒、不自动关页、不定时重画、不做缩放淡入动画；
  - 候选和列表按页翻，有页码，盖板键能翻；
  - 按键、接通、静音、每分钟都只刷变了的那一块；
  - 不放按键音、不振动，按下不反色（D-31）。
- **灰阶**：未接、打来、打出写字，静音、耳机靠字和实心空心，不用 Fossify 的红绿蓝。
- **触摸**：所有目标不小于 108 px。长按换成看得见的按钮：清空号码用输入框的清除键；快捷拨号用候选行；选卡不做；“+”号用号码框的键盘输入。三点菜单换成“详情”页。不画头像圆圈，不用圆圈图标。会拨出电话的点按只有“拨打”和详情页的“打电话”，候选行和最近通话行都只填号。
- **提示**：不用会自动消失的提示条，留到下一次操作。
- **资源**：联系人表、归组索引放 PSRAM；不后台轮询。

## 共同要求

- **来源**：功能、页面、状态、出错处理照对照表一个不落，不自己发挥。不照搬的写理由，例如安卓 Telecom 才有的保持、多方通话、选卡、距离传感器。
- **设计语言**：
  - 只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c`）。
  - 一屏最多一个主按钮：拨号页是“拨打”，通话页是“挂断”。
  - 行间细分隔线、组间留白；图标不套圆圈；不垫重框（去掉 `:413` 的卡片底）。
  - 整页空才用共用空状态。不写死像素和颜色（`tools/ui_scale_check.py`）。
- **不变的规矩**：
  - 家人隔离：通话记录、快捷拨号按家人分开。
  - 孩子照现有 `child_ok`。
  - 确认卡只给 D-105 的高风险动作：人自己按拨打就直接拨，AI 替人打电话照现有策略（要人确认）。
  - 少刷新（D-127）；按下反馈（D-31）；按键内容和密码不给 AI、不进日志。
- **读联系人（和 contacts-3 的接缝）**：读联系人、填 200 人的 PSRAM 表只在一个函数里做，别处不碰记录格式。先看 contacts-3 合没合入：没合入就读老格式（一个号码）；合入了要同时读得懂新格式（多号码，每个号码一个候选，来源写名字），并按 contacts-3 的老记录兼容约定（老记录读成一个“手机”号码）测两种。号码比较的规则两边不同（这里是末 11 位，contacts-3 是规整前缀），对中国号码结果一致，报告里写一句。`contacts.edit` 要收号码也在报告里交给 contacts-3，本任务不改联系人文件。
- **字号**：跟系统“文字大小”一起缩放（`ink_ui_px`），放不下就少放行、分页，不缩字、不裁按钮。
- **原地重写**：旧页面代码删掉，不留两套。`phone.c` 可以拆成几个文件，但要照验收 8 把引用它的测试一并改好。
- **不往卡上加文件**：本任务没有要放进存储卡的文件（auto-provision-1 在改按清单放卡文件）。
- **共享文件**（尽量只在末尾追加本任务的行，合并冲突由主会话处理）：
  - `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；
  - 新文案的英文加进 `assets/i18n/en.tsv`；
  - 新的 `.c` 里有 `TR()` 的，加进根 `CMakeLists.txt` 的翻译源列表（`:470-480`，phone.c 在列表中间一行，新文件加在它旁边）和 `ports/esp32s31/components/ink_system/CMakeLists.txt:26-35`；
  - S31 那边 `system/apps/*.c` 是按通配收进来的（同文件 `:2`、`:17`，`GLOB_RECURSE`，`vendor/lilyphone/*.c` 也在内），不用另加源文件行；
  - `simulator/sim.h` 末尾、根 `CMakeLists.txt` 翻译源列表这两处可能和别的任务（music-3、camera-touch-bus 等）改到相邻位置，预计合并时有文本冲突，由主会话处理。
- **截图**：放 `docs/tasks/phone-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。每页三套：默认字号、最大字号、孩子身份。

## 范围和预算

- **只改**：
  - `system/apps/phone/`：可以拆文件；新的 `vendor/lilyphone/` 和它的 `SOURCE.md`。
  - `system/services/comm/comm_service.c`：本批只归本任务；动的是结束原因、本地挂断标记。
  - 为结束原因放宽的模组层：`platform/include/ink/hal_cellular.h`、`ports/sim/hal/hal_cellular_sim.c`、`ports/sim/hal/hal_net_sim.c`、`simulator/sim.h`（末尾加声明）、`ports/esp32s31/hal_cellular_s31.c`、`ports/esp32s31/s31_modem.c`（URC 列表那一处）、`ports/esp32s31/tests/cell.c`（加断言）。这几个文件本批别的任务基本不碰（`sim.h` 末尾见上）；只加不改，旧行为不变。
  - 电话相关测试，以及验收 8 列出的引用 `phone.c` 的测试。
  - 根 `CMakeLists.txt` 里新文件、新测试、翻译源那几行；`ports/esp32s31/components/ink_system/CMakeLists.txt` 翻译源那一处。
  - `tests/integration/ai_surface.c`（末尾追加）、`assets/i18n/en.tsv`（末尾追加）、`docs/system/capabilities.md`（重新生成）、`docs/tasks/phone-3/`。
- **不碰**：
  - `system/apps/contacts/`、`system/apps/sms/`：contacts-3、sms-3 在 2b 接着做，缺的参数写进报告；
  - `system/services/notify/`：来电卡和未接通知照旧；
  - `system/services/text/`：拼音反查不在这里加；
  - 主题和共用控件文件；`system/policy/cap.c`；
  - `lock_mark.c`（lock-styles-1）；`system/ui/shell/activity_card.c`、`status_bar.c`（只经 `ink_phone_activity` 接，返回值的意思不变）；
  - 别的应用目录。
- **代码量**：
  - 产品代码（胶水加重写，含 `comm_service.c` 和模组层新增）不超过 **500 行**，删掉的旧代码不抵；模拟器 AT 模型的钩子另计，不超过 60 行；
  - LilyPhone 原文整函数照拷不计，对原文的改动不超过 **80 行**；
  - 超了先停下说明（旧 phone.c 四页就是 490 行，这个预算很紧，别硬压成难读的代码，也别悄悄砍功能）。
- **固件和内存**：固件增量不超过 **15 KB**；内部静态 RAM 不增加（新表都放 PSRAM，`INK_PSRAM_BSS`）。
- **起步**：主会话给的分支（当前 `claude/system-layer`，已含上面列的合入）。第一波 reader-one-1、auto-provision-1、settings-d137、camera-touch-bus 可能在这期间合入，和本任务没有共用文件（只可能在 `sim.h`、根 `CMakeLists.txt` 附近有文本冲突）；先合入的，合并时照常重跑全量。almanac-2、music-3、files-3、memo-3、gallery-3、lock-styles-1 同样只在 `ai_surface.c`、`en.tsv`、`capabilities.md`、根 CMake 上有末尾追加的重叠。只做到模拟器的 AT 模型，不打真电话，不碰设备。

## 验收

1. **照 Fossify 的参照路径走一遍**（模拟器脚本，每步截图并断言）：
   - 拨号页输 `138`，候选列表出现两个以上的联系人，开头命中的排在中间命中的前面；
   - 再输两位，候选变少；点一个，号码填好，**没有拨出**；
   - 按拨打，显示“正在拨号”。这时模组已回 OK，页面**不能**显示“通话中”，快照也一样；
   - 模拟器让对方接通，显示“通话中”和“0 分钟”；推进一分钟，只刷时长那一块；
   - 静音（等模组确认后才变成“已静音”），切到蓝牙耳机，打开键盘按 1（`call.dtmf`）；
   - 挂断，显示“通话结束 · 1 分 05 秒”，结果留在页上，不自动关；点“完成”回最近通话，最上面一组是这次，时长按秒。
   - 另走四条：对方忙（`BUSY`）、没人接（`NO ANSWER`）、接通前断（`NO CARRIER`）、自己接通前挂断。四种结束说明各不相同，最近通话里的 `result` 也各不相同。
   - 名字匹配从词首：联系人 “Phillips Thomas”，按 846 命中，按 466 不命中。
   - 同一条路径用能力调用（来源是 AI，照现有策略要人确认）走一遍，结果和快照一样。
   - **反向对照**：把“受理就算接通”放回去，接通前那条断言必须失败；把 `BUSY` 当成普通结束，“对方忙或拒接”那条必须失败。
   - 刷新次数和“只刷那一块”：用现有的刷新次数探针；模拟器若报不出按区域的统计，就断言次数（按键 1 次、接通 1 次、每分钟 1 次等）并在报告里写明没有按区域断言，不放宽次数。
2. **号码比对**（搬来的原文）：`phone_number_match` 单测过（见“测试”）。把 `MATCH_DIGITS` 改回 7，“138/139 后 8 位相同不算同一个”必须失败。通话页、最近通话、详情里的名字都来自 `contacts_display_name`：联系人存成 `138 1234 5678`，打进来的是 `+8613812345678`，也显示名字。
3. **最近通话**：
   - 同一号码同一天连着打三次，显示成一组写“(3)”；隔一天再打，另成一组；
   - 点一组：号码填进拨号页，**没有拨出**（`sim_net_call` 仍为 0）；点“详情”看到三条，有“打电话”“发短信”“加为联系人”（或“查看联系人”），点“打电话”才拨；
   - 删一组，再撤销，记录回来；清空走确认卡（如果做了）；
   - 老格式记录（只有 `minutes`）时长读对；存满 500 条以后最旧的那条被挤掉；
   - 旧备份（schema 1）照样能恢复：集合 schema 没变。
4. **家人和孩子**：A 的通话记录和快捷拨号 B 看不到；孩子拨号照现有限制；孩子用完时间后电话照样能用。
5. **紧急呼叫**：`emergency-call` 现有测试照过，锁屏下进紧急呼叫页拨 110。
6. **拨号页的提示不压键盘**：默认和最大字号下，一句短回执、一句最长回执（飞行模式那句）、一个很长的联系人名字，分别断言不压任何键、不出屏幕、和联系人名字不重叠。`small-gaps-2-phone` 照过：它的点按位置要改成按“拨打”键的绑定找，压键断言不放宽。
7. **截图，改前改后并排**：拨号页（空、输入中带候选、没匹配、出错）、通话中、通话键盘、通话结束（四种原因各一张）、最近通话、通话详情、快捷拨号设置。默认、最大字号、孩子各一套。测试断言所有点按区不小于 108 px、都在屏幕内、页面上没有 `ink_ui_circle`。
8. **全量**：
   - 主工程普通全量 ctest 过（含 `phone-calls`、`emergency-call`、`small-gaps-*`、`ui-scene-*`、`design-lang-*`、`ui-scale`、`i18n-*`、`integration-*`）；
   - ASan 跑电话、联系人匹配和 `ui-scene-*` 过；S31 宿主机测试过（含 `ports/esp32s31/tests/cell.c` 新断言）；
   - `ports/esp32s31/build.sh` 编译通过，无新警告。

   报告写：每个主要操作刷几次屏（按键、点候选、拨打、接通、每分钟、静音、开关键盘、挂断、回最近、翻页、删除和撤销）；固件增量；产品代码行数和原文改动行数；`gcccheck` 结果；对照表做到几行、哪几行没做；改名或删除的能力（应为没有）；上机先核对清单（NT26 发不发 `BUSY`/`NO ANSWER`）；以及**改过的每一个已有测试**。预计会改的：
   - `tests/phone_calls/phone_calls.c`：最近通话的 `"total"` 变成组数，另加记录条数的断言，一样严格；点行仍是填号（不变），新增详情里拨号的断言；
   - `tests/small_gaps_2/ui.c`：拨打键位置；
   - 原文替换型的反向对照：`tests/phone_calls/reverse.py`、`reverse_controls.py`、`tests/activity_card_reverse.py`、`tests/small_gaps/reverse.py`、`tests/small_gaps_2/reverse.py`、`tests/design_lang_2_reverse.py`、`tests/audit7_fix/run.py`、`tests/emergency_call/run.py`。原文搬了文件或改了写法，就改指新位置，失败条件不变；
   - 直接 include `phone.c` 的三个测试载体：拆文件后照样能编；
   - `tests/integration/ai_surface.c`：只追加。

   只改定位方式和页面结构，不改业务断言；测试不放宽、不删、不加等待来凑。

## 要用户定（主会话派出前问清，或者写进报告）

1. **中文名字按拼音首字母匹配**（“张三”按 9、7）：系统没有汉字反查拼音的接口。要在文本服务加一个，另起任务；本任务只做号码和拉丁字母名字。
2. **拉黑号码、复制号码、最近通话搜索、导出通话记录、多选批量删除、号码归属地**：Fossify 有，本任务不做。拉黑要 `comm_service.c` 在来电时直接挂断，可以下一轮做；导出已经由备份包含；归属地要离线数据，固件放不下（C6）。
3. **通话页的“录音”按钮**：参照没有，现在点了只说不能录。建议从页面拿掉、能力保留。
4. **行为改动，请用户知道**：
   - 点候选只填号不拨（和 Fossify 不同，理由见上）；
   - 最近通话点一行**仍是填号**，不照 Fossify 直接回拨（理由：翻页刷新延迟加误拨有电话费）；要不要改成直接回拨，请用户定；
   - 通话中时长按分钟、结束时给秒（墨水屏不跑秒）。
5. **LilyPhone 没有许可证文件**：按 D-129 照搬，仓库是公开的，在这里记一笔。
